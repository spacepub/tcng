/*
 * field.c - Packet field definition
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Network Robots
 */


#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "util.h"
#include "error.h"
#include "data.h"
#include "op.h"
#include "field.h"


static FIELD_ROOT *field_roots = NULL;
static FIELD *fields = NULL;
static int level = 0;


/* ----- Field roots ------------------------------------------------------- */


FIELD_ROOT *field_root(int offset_group)
{
    FIELD_ROOT *root;

    for (root = field_roots; root; root = root->next)
	if (root->offset_group == offset_group) return root;
    root = alloc_t(FIELD_ROOT);
    root->offset_group = offset_group;
    root->next = field_roots;
    field_roots = root;
    return root;
}


int offset_group_taken(int offset_group)
{
    const FIELD_ROOT *root;

    for (root = field_roots; root; root = root->next)
	if (root->offset_group == offset_group) return 1;
    return 0;
}


/* ----- Field definition -------------------------------------------------- */


void field_begin_scope(void)
{
    level++;
}


void field_end_scope(void)
{
    FIELD **field,**next;

    for (field = &fields; *field; field = next) {
	next = &(*field)->next;
	if ((*field)->level == level) {
	    data_destroy((*field)->condition);
	    data_destroy((*field)->access);
	    if ((*field)->outer) {
		FIELD *outer = (*field)->outer;

		(*field)->condition = outer->condition;
		(*field)->access = outer->access;
		(*field)->level = outer->level;
		(*field)->outer = outer->outer;
		free(outer);
	    }
	    else {
		FIELD *this = *field;

		*field = this->next;
		next = field;
		free((void *) this->name);
		free(this);
	    }
	}
    }
    level--;
}


FIELD *field_find(const char *name)
{
    FIELD *field;

    for (field = fields; field; field = field->next)
	if (!strcmp(field->name,name)) break;
    return field;
}


void field_set(const char *name,DATA access,DATA condition)
{
    FIELD *field;

    if (field_find(name)) yyerrorf("field \"%s\" already exists",name);
	/* the parser actually already disallows this */
    field = alloc_t(FIELD);
    field->name = stralloc(name);
    field->condition = condition;
    field->access = access;
    field->level = level;
    field->outer = NULL;
    field->next = fields;
    fields = field;
}


static void is_known(DATA d,const FIELD *horizon)
{
    if (d.type == dt_field) {
	const FIELD *scan;

	for (scan = horizon; scan; scan = scan->next)
	    if (scan == d.u.field) return;
	yyerrorf("field \"%s\" defined after redefined field",d.u.field->name);
    }
    if (!d.op) return;
    is_known(d.op->a,horizon);
    is_known(d.op->b,horizon);
    is_known(d.op->c,horizon);
}


void field_redefine(FIELD *field,DATA access,DATA condition)
{
    is_known(access,field->next);
    is_known(condition,field->next);
    if (field->level == level) {
	data_destroy(field->access);
	data_destroy(field->condition);
    }
    else {
	FIELD *outer;

	outer = alloc_t(FIELD);
	outer->name = NULL;
	outer->access = field->access;
	outer->condition = field->condition;
	outer->level = field->level;
	outer->next = NULL;
	outer->outer = field->outer;
	field->outer = outer;
	field->level = level;
    }
    field->access = access;
    field->condition = condition;
}


/* ----- Field expansion --------------------------------------------------- */


static void do_field_expand(DATA *d)
{
    if (d->type == dt_none) return;
    while (d->op && d->op->dsc == &op_access && d->op->a.type == dt_field) {
	FIELD *field = d->op->a.u.field;
	DATA access = data_clone(field->access);

	data_destroy_1(d->op->a);
	/*
	 * disallow things like
	 *   field foo = bar+5
	 *   field baz = foo[0]
	 */
	if ((d->op->b.type != dt_none || d->op->c.u.unum) &&
	  field->access.type != dt_none &&
	  (!field->access.op || field->access.op->dsc != &op_access))
	    yyerrorf("access not allowed for field \"%s\"",field->name);
	if (field->condition.type == dt_none) d->op->a = access;
	else {
	    DATA condition;

	    condition = data_clone(field->condition);
	    do_field_expand(&condition);
	    d->op->a = op_binary(&op_precond2,access,condition);
	}
    }
    if (!d->op) return;
    do_field_expand(&d->op->a);
    do_field_expand(&d->op->b);
    do_field_expand(&d->op->c);
}


static void bubble_precond(DATA *d,DATA *root,int adjust_root)
{
    DATA old = *d;

    if (!d->op) return;
    if (d->op->dsc == &op_precond) {
	DATA new = data_unum(1);

	bubble_precond(&d->op->a,&new,0);
	new = data_clone(new);
	data_destroy(*d);
	*d = new;
	return;
    }
    if (d->op->dsc == &op_precond2) {
	*root = op_binary(&op_logical_and,d->op->b,*root);
	bubble_precond(&root->op->a,root,1); /* was d->op->b */
	/* resolve pointer alias */
	if (root == d) d = &root->op->b;
	bubble_precond(&d->op->a,root,adjust_root);
	*d = d->op->a;
	data_destroy_1(old);
	return;
    }
    if (adjust_root &&
      (d->op->dsc == &op_logical_or || d->op->dsc == &op_logical_not)) {
	bubble_precond(&old.op->a,&old.op->a,1);
	bubble_precond(&old.op->b,&old.op->b,1);
    }
    else {
	/*
	 * d->op->b before d->op->a to put preconditions of a || b in the right
         * order if adjust_root is not set (i.e. when processing precond() )
	 */
	bubble_precond(&old.op->b,root,adjust_root);
	bubble_precond(&old.op->a,root,adjust_root);
	bubble_precond(&old.op->c,root,adjust_root);
    }
}


static void reduce_access(DATA *d)
{
    while (d->op && d->op->dsc == &op_access) {
	DATA old;

	/*
	 * Change access(root,-,0) to access(root,-,8) and stop reducing in all
	 * cases of access(root,-,*)
	 */
	if (d->op->a.type == dt_field_root && d->op->b.type == dt_none) {
	    if (!d->op->c.u.unum) d->op->c.u.unum = 8;
	    break;
	}

	/*
	 * Reduce access(X,-,0) to X unless X is dt_field_root
	 */
	if (d->op->b.type == dt_none && !d->op->c.u.unum) {
	    old = *d;

	    *d = d->op->a;
	    data_destroy_1(old);
	    continue;
	}

	/*
	 * Reduce
	 *   access(access(D,O2,S2),O1,S1)
	 * to
	 *   access(D,O1+O2,S1 ? S1 : S2)
	 */
	if (!d->op->a.op || d->op->a.op->dsc != &op_access) {
	    if (!d->op->c.u.unum) d->op->c.u.unum = 8;
	    break;
	}
	old = d->op->a;
	if (d->op->b.type == dt_none)
	    d->op->b = data_clone(d->op->a.op->b);
	else if (d->op->a.op->b.type != dt_none)
	    d->op->b = op_binary(&op_plus,d->op->b,d->op->a.op->b);
	if (!d->op->c.u.unum) d->op->c.u.unum = d->op->a.op->c.u.unum;
	if (!d->op->c.u.unum) d->op->c.u.unum = 8;
	d->op->a = d->op->a.op->a;
	data_destroy_1(old);
    }

    /* make life easier for later processing stages */
    if (d->op && d->op->dsc == &op_access && d->op->b.type == dt_none)
	d->op->b = data_unum(0);
    if (d->op) {
	reduce_access(&d->op->a);
	reduce_access(&d->op->b);
	reduce_access(&d->op->c);
    }
}


DATA field_expand(DATA d)
{
    do_field_expand(&d);
    debug_expr("BEFORE bubble_precond",d);
    bubble_precond(&d,&d,1);
    reduce_access(&d);
    return d;
}


DATA field_snapshot(DATA d)
{
    do_field_expand(&d);
    return d;
}
