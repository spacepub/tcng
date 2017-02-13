/*
 * ifact_db.c - Action database handling
 *
 * Written 2001,2002,2004 by Werner Almesberger
 * Copyright 2001,2002 by Bivio Networks, Network Robots
 * Copyright 2004 Werner Almesberger
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "data.h"
#include "op.h"
#include "iflib.h"
#include "ext_all.h"


static struct action *root = NULL;


#define CLASS_TO_QDISC_NUM(c) ((c)->parent.qdisc->number)
#define CLASS_TO_DECISION(c) ((CLASS_TO_QDISC_NUM(c) << 16) | (c)->number)


/* ----- Step 1: Build the bit tree ---------------------------------------- */


static struct action *make_action(DATA d,struct action *parent)
{
    struct action *action;

    action = alloc_t(struct action);
    if (d.op && d.op->dsc == &op_conform) {
	action->type = at_conform;
	action->u.conform = d.op->a.u.police;
    }
    else if (d.op && d.op->dsc == &op_count) {
	action->type = at_count;
	action->u.count = d.op->a.u.police;
    }
    else if (!d.op && d.type == dt_decision) {
	switch (d.u.decision.result) {
	    case dr_class:
		action->type = at_class;
		action->u.decision = d.u.decision.class;
		break;
	    case dr_drop:
		action->type = at_drop;
		break;
	    case dr_continue:
		action->type = at_unspec;
		break;
	    default:
		dump_failed(d,"unsupported decision");
	}
    }
    else abort();
    action->parent = parent;
    action->c[0] = action->c[1] = NULL;
    action->number = -1;
    action->dumped = 0;
    action->next = NULL;
    action->referers = NULL;
    return action;
}


static struct action *add_actions(DATA d,DATA next,DATA otherwise,
  struct action *parent)
{
    struct action *action;

    if (debug > 1) debugf("add_actions");
    if (d.op && d.op->dsc == &op_logical_or) {
#if 0
	assert(!next.op && next.type == dt_none);
	assert(!otherwise.op && otherwise.type == dt_none);
#endif
	return add_actions(d.op->a,data_none(),d.op->b,parent);
    }
    if (d.op && d.op->dsc == &op_logical_and) {
#if 0
	assert(!next.op && next.type == dt_none); /* solution ? @@@ */
#endif
	return add_actions(d.op->a,d.op->b,otherwise,parent);
    }
    if (d.op && d.op->dsc == &op_conform) {
	action = make_action(d,parent);
	action->c[1] = add_actions(next,data_none(),otherwise,action);
	action->c[0] = add_actions(otherwise,data_none(),data_none(),action);
	return action;
    }
    if (d.op && d.op->dsc == &op_count) {
	action = make_action(d,parent);
	action->c[1] = add_actions(next,data_none(),otherwise,action);
	return action;
    }
    if (!d.op && d.type == dt_decision)
	return make_action(d,parent);
    dump_failed(d,"unknown action");
    return NULL; /* not reached */
}


/* ----- Step 2: Eliminate nodes repeating previous decisions -------------- */


static void free_1(struct action *a)
{
    free(a);
}


static void free_subtree(struct action *a)
{
    if (a) return;
    free_subtree(a->c[0]);
    free_subtree(a->c[1]);
    free_1(a);
}


static void repeated_actions(struct action *a)
{
    if (!a) return;
    if (a->type == at_conform) {
	struct action *walk;

	for (walk = a; walk; walk = walk->parent) {
	    struct action **anchor;
	    int value;

	    if (!walk->parent || walk->parent->type != at_conform ||
	      walk->parent->u.conform != a->u.conform)
		continue;
	    value = walk->parent->c[0] == walk ? 0 : 1;
	    anchor = a->parent->c[0] == a ? &a->parent->c[0] : &a->parent->c[1];
	    *anchor = a->c[value];
	    a->c[value]->parent = a->parent;
	    free_subtree(a->c[1-value]);
	    free_1(a);
	    repeated_actions(*anchor);
	    return;
	}
    }
    repeated_actions(a->c[0]);
    repeated_actions(a->c[1]);
}


/* ----- Hash table for leaf nodes ----------------------------------------- */


#define HASH_ENTRIES 47


static struct hash_list {
    struct action *action;
    struct hash_list *next;
} *hash[HASH_ENTRIES];


static int action_eq(const struct action *a,const struct action *b)
{
    if (a->type != b->type) return 0;
    switch (a->type) {
	case at_class:
	    return a->u.decision == b->u.decision;
	case at_drop:
	case at_unspec:
	    return 1;
	case at_conform:
	    return a->u.conform == b->u.conform;
	case at_count:
	    return a->u.count == b->u.count;
	default:
	    abort();
    }
}


static struct action *hash_find(struct action *a)
{
    struct hash_list *walk,*new;
    int key;

    assert(a->type == at_class || a->type == at_drop || a->type == at_unspec);
    key = a->type == at_class ?
      CLASS_TO_DECISION(a->u.decision) % HASH_ENTRIES : 0;
    for (walk = hash[key]; walk; walk = walk->next)
	if (action_eq(walk->action,a)) return walk->action;
    new = alloc_t(struct hash_list);
    new->action = a;
    new->next = hash[key];
    hash[key] = new;
    return a;
}


/* ----- Step 3: Merge common subtrees ------------------------------------- */


static void add_referer(struct action *action,struct action *referer)
{
    struct action_referer **here;

    for (here = &action->referers; *here; here = &(*here)->next);
    *here = alloc_t(struct action_referer);
    (*here)->action = referer;
    (*here)->next = NULL;
}


static struct action *unique_action(struct action *a);

static struct action *_unique_action(struct action *a)
{
    struct action_referer *referer,*walk;

    switch (a->type) {
	case at_class:
	case at_drop:
	case at_unspec:
	    return hash_find(a);
	case at_conform:
	    a->c[0] = unique_action(a->c[0]);
	    a->c[1] = unique_action(a->c[1]);
	    for (referer = a->c[0]->referers; referer; referer = referer->next)
		if (action_eq(referer->action,a))
		    for (walk = a->c[1]->referers; walk; walk = walk->next)
			if (referer->action == walk->action)
			    return referer->action;
	    add_referer(a->c[0],a);
	    add_referer(a->c[1],a);
	    return a;
	case at_count:
	    a->c[1] = unique_action(a->c[1]);
	    for (referer = a->c[1]->referers; referer; referer = referer->next)
		if (action_eq(referer->action,a)) return referer->action;
	    add_referer(a->c[1],a);
	    return a;
	default:
	    abort();
    }
}


static struct action *unique_action(struct action *a)
{
    struct action *w;

    if (debug > 2) {
	for (w = a; w; w = w->parent) fprintf(stderr,"  ");
	fprintf(stderr,"ENTER unique_action: %p\n",a);
    }
    a = _unique_action(a);
    if (debug > 2) {
	for (w = a; w; w = w->parent) fprintf(stderr,"  ");
	fprintf(stderr,"EXIT unique_action: %p\n",a);
    }
    return a;
}


static void to_unique(struct action *a)
{
    struct action *res,*walk;

    res = unique_action(a);
    for (walk = root; walk; walk = walk->next)
	if (res == walk) return;
    res->next = root;
    root = res;
}


/* ----- Dump a subtree ---------------------------------------------------- */


static void dump_items(FILE *file,QDISC *qdisc,const struct action *a,int top)
{
    if (a->number != -1 && !top) {
	fprintf(file," action %d",a->number);
	return;
    }
    switch (a->type) {
	case at_conform:
	    fprintf(file," conform %u",(unsigned) a->u.conform->number);
	    dump_items(file,qdisc,a->c[1],0);
	    dump_items(file,qdisc,a->c[0],0);
	    break;
	case at_count:
	    fprintf(file," count %u",(unsigned) a->u.count->number);
	    dump_items(file,qdisc,a->c[1],0);
	    break;
	case at_class:
	    if (dump_all_decision(file,qdisc,a->u.decision->number)) break;
	    fprintf(file," class %u:%u",CLASS_TO_QDISC_NUM(a->u.decision),
	      a->u.decision->number);
	    break;
	case at_drop:
	    fprintf(file," drop");
	    break;
	case at_unspec:
	    fprintf(file," unspec");
	    break;
	default:
	    abort();
    }
}


static void dump_subtree(FILE *file,QDISC *qdisc,struct action *a,int top)
{
    switch (a->type) {
	case at_conform:
	    dump_subtree(file,qdisc,a->c[0],0);
	    /* fall through */
	case at_count:
	    dump_subtree(file,qdisc,a->c[1],0);
	    /* fall through */
	case at_class:
	case at_drop:
	case at_unspec:
	    break;
	default:
	    abort();
    }
    if (!a->dumped) {
	a->dumped = 1;
	fprintf(file,"action %d =",a->number);
	dump_items(file,qdisc,a,1);
	fputc('\n',file);
    }
}


#define INITIAL_PREFERRED_SIZE 8

static int *preferred_table; /* table of registered preferred numbers */
static int max_preferred; /* current array size */
static int preferred; /* number registered preferred numbers */


static void init_preferred(void)
{
    debugf2("init_preferred");
    max_preferred = INITIAL_PREFERRED_SIZE;
    preferred_table = alloc(max_preferred*sizeof(int));
    preferred = 0;
}


static void add_preferred(int index)
{
    debugf2("add_preferred(%d)",index);
    preferred_table[preferred++] = index;
    if (preferred == max_preferred) {
	max_preferred *= 2;
	preferred_table = realloc(preferred_table,max_preferred*sizeof(int));
	if (!preferred_table) {
	    perror("realloc");
	    exit(1);
	}
    }
}


static void sort_preferred(void)
{
    debugf2("sort_preferred[preferred %d]",preferred);
    preferred_table[preferred] = -1;
    qsort(preferred_table,preferred,sizeof(int),&comp_int);
    preferred = 0; /* use as pointer now */
}


static void release_preferred(void)
{
    free(preferred_table);
}


static int number_available_slow(int index)
{
    int i;

    debugf2("number_available_slow(%d)",index);
    for (i = 0; i < preferred; i++)
	if (preferred_table[i] == index) return 0;
    return 1;
}


static int number_available_fast(int index)
{
    debugf2("number_available_fast(%d)[preferred %d]",index,preferred);
    if (preferred_table[preferred] != -1 &&
      preferred_table[preferred] < index)
	preferred++;
    debugf2("  ... [preferred %d -> %d]",preferred,preferred_table[preferred]);
    return preferred_table[preferred] != index;
}


static void assign_indices(const struct action *root,struct action *a,
  int preferred,int *index)
{
    switch (a->type) {
	case at_conform:
	    assign_indices(root,a->c[0],preferred,index);
	    /* fall through */
	case at_count:
	    assign_indices(root,a->c[1],preferred,index);
	    /* fall through */
	case at_drop:
	case at_unspec:
	    break;
	case at_class:
	    if (a->number == -1 && preferred &&
	      number_available_slow(a->u.decision->number & 0xff)) {
		a->number = a->u.decision->number & 0xff;
		add_preferred(a->number);
	    }
	    break;
	default:
	    abort();
    }
    if (a->number != -1 || preferred) return;
    while (!number_available_fast(*index)) (*index)++;
    a->number = (*index)++;
}


/* ----- Debugging tree dump ----------------------------------------------- */


static void debug_subtree(FILE *file,const struct action *a,int level)
{
    const struct action_referer *r;

    fprintf(file,"%*s[%p] %d ",level*4,"",a,a->number);
    switch (a->type) {
	case at_conform:
	    fprintf(file,"conform %u (0, then 1)\n",
	      (unsigned) a->u.conform->number);
	    break;
	case at_count:
	    fprintf(file,"count %u\n",(unsigned) a->u.count->number);
	    break;
	case at_class:
	    fprintf(file,"class %u:%u\n",
	      (unsigned) CLASS_TO_QDISC_NUM(a->u.decision),
	      (unsigned) a->u.decision->number);
	    break;
	case at_drop:
	    fprintf(file,"drop\n");
	    break;
	case at_unspec:
	    fprintf(file,"unspec\n");
	    break;
	default:
	    abort();
    }
    fprintf(file,"%*s| parent %p, next %p\n",level*4,"",a->parent,a->next);
    fprintf(file,"%*s| referers:",level*4,"");
    for (r = a->referers; r; r = r->next)
	fprintf(file," %p",r->action);
    fputc('\n',file);
    switch (a->type) {
	case at_conform:
	    debug_subtree(file,a->c[0],level+1);
	    /* fall through */
	case at_count:
	    debug_subtree(file,a->c[1],level+1);
	    /* fall through */
	case at_class:
	case at_drop:
	case at_unspec:
	    break;
	default:
	    abort();
    }
}


static void debug_action_tree(FILE *file)
{
    struct action *walk;

    for (walk = root; walk; walk = walk->next)
	debug_subtree(file,walk,0);
}


/* ------------------------------------------------------------------------- */


void register_actions(DATA d)
{
    struct action *action;

    action = add_actions(d,data_none(),data_none(),NULL);
    repeated_actions(action);
    to_unique(action);
}


void write_actions(FILE *file,QDISC *qdisc)
{
    int index = 0;
    struct action *walk;

    init_preferred();
    /* assign preferred numbers */
    for (walk = root; walk; walk = walk->next)
	assign_indices(root,walk,1,&index);
    sort_preferred();
    /* assign all other numbers */
    for (walk = root; walk; walk = walk->next)
	assign_indices(root,walk,0,&index);
    /* dump the thing */
    for (walk = root; walk; walk = walk->next)
	dump_subtree(file,qdisc,walk,1);
    release_preferred();
}


const struct action *get_action(DATA d)
{
    struct action *action;

    if (debug > 1) debug_action_tree(stderr);
    action = add_actions(d,data_none(),data_none(),NULL);
    repeated_actions(action);
    if (debug > 1) fprintf(stderr,"in-action %p\n",action);
    action = unique_action(action);
    if (debug > 1) {
	fprintf(stderr,"out-action %p\n",action);
	print_expr(stderr,d);
    }
    return action;
}


int action_number(DATA d)
{
    return get_action(d)->number;
}


void free_actions(void)
{
    /* tricky: walk tree while == 1 referer */
    root = NULL;
    memset(hash,0,sizeof(hash));
    /*
     * @@@ Okay, this is a rather crude hack. But since all this code is doomed
     * anyway, it's pretty pointless to try to make it neat and tidy.
     */
}
