/*
 * iflib_off.c - Add and optimize explicit offset operators
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Werner Almesberger
 */


#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "data.h"
#include "op.h"
#include "field.h"
#include "tc.h"
#include "iflib.h"

#include "tccmeta.h"


/* ----- Move offset from access() to offset() ----------------------------- */


static void use_offset(DATA *d)
{
    if (d->op && d->op->dsc == &op_access) {
	DATA new;

	if (d->op->a.type != dt_field_root) abort();
	if (d->op->b.type != dt_unum) abort();
	if (d->op->b.op) { /* don't extract constant offset */
	    new = op_binary(&op_offset,*d,d->op->b);
	    d->op->b = data_unum(0);
	    *d = new;
	}
	use_offset(&d->op->b);
	return;
    }
    if (!d->op) return;
    use_offset(&d->op->a);
    use_offset(&d->op->b);
    use_offset(&d->op->c);
}


/* ----- Extract and combine offsets --------------------------------------- */


static int have_offset(DATA d)
{
    if (!d.op) return 0;
    if (d.op->dsc == &op_access) return 0;
    if (d.op->dsc == &op_field_root) return 1;
    if (d.op->dsc == &op_offset) return 1;
    return have_offset(d.op->a) || have_offset(d.op->b) || have_offset(d.op->c);
}


static int offset_in_sum(DATA d,DATA offset)
{
    if (expr_equal(d,offset)) return 1;
    if (!d.op || d.op->dsc != &op_plus) return 0;
    return offset_in_sum(d.op->a,offset)+offset_in_sum(d.op->b,offset);
}


/*
 * Check if all branches have a common offset (or don't care).
 * If we enter the region *except, we don't check the offset there, assuming
 * that this region will be removed later (i.e. it is the offset candidate
 * under consideration).
 */

static int have_common_offset(DATA *d,DATA offset);

static int do_have_common_offset(DATA *d,DATA offset)
{
    if (!d->op) return 1;
    if (expr_equal(*d,offset)) return 1;
    if (d->op->dsc == &op_access) return 0;
    if (d->op->dsc == &op_field_root) return 0;
    if (d->op->dsc == &op_offset) {
	int terms;

	if (!have_common_offset(&d->op->b,offset)) return 0;
	terms = offset_in_sum(d->op->b,offset);
	return terms == 1 || (terms > 1 && !have_offset(offset));
	    /* @@@ I suppose, it *could* be recursive ... */
    }
    return have_common_offset(&d->op->a,offset) &&
      have_common_offset(&d->op->b,offset) &&
      have_common_offset(&d->op->c,offset);
}


static int have_common_offset(DATA *d,DATA offset)
{
    int res;

    if (debug > 1) {
	debug_expr("have_common_offset on ...",*d);
	debug_expr("... with offset",offset);
    }
    res = do_have_common_offset(d,offset);
    if (debug > 1) printf("RESULT have_common_offset = %d\n",res);
    return res;
}


static int remove_offset_from_sum(DATA *d,DATA offset)
{
    DATA curr = *d;

    if (!d->op || d->op->dsc != &op_plus) return 0;
    if (expr_equal(d->op->a,offset)) {
	if (debug > 1) printf("#### DROP ####\n");
	*d = d->op->b;
	data_destroy_1(curr.op->a);
	return 1;
    }
    if (expr_equal(d->op->b,offset)) {
	if (debug > 1) printf("#### DROP ####\n");
	*d = d->op->a;
	data_destroy_1(curr.op->b);
	return 1;
    }
    if (remove_offset_from_sum(&d->op->a,offset)) return 1;
    return remove_offset_from_sum(&d->op->b,offset);
}


static void remove_offset(DATA *d,DATA offset)
{
    if (!d->op) return;
    if (d->op->dsc == &op_access) return;
    if (d->op->dsc == &op_offset) {
    if (debug > 1) {
	debug_expr("remove_offset offset ...",offset);
	debug_expr("... from",d->op->b);
    }
#if 0 /* @@@ looks useless and wrong - check */
	if (have_offset(d->op->b)) {
	    remove_offset(&d->op->b,offset);
	    return;
	}
#endif
	if (expr_equal(d->op->b,offset)) {
	    DATA next;

	    if (debug > 1) printf("remove_offset: expr_equal\n");
	    data_destroy(d->op->b);
	    d->op->b = data_unum(0);
	    next = d->op->a;
	    data_destroy_1(*d);
	    *d = next;
	    return;
	}
	else {
	    if (debug > 1) printf("remove_offset: remove_offset_from_sum\n");
	    (void) remove_offset_from_sum(&d->op->b,offset);
	}
    }
    remove_offset(&d->op->a,offset);
    remove_offset(&d->op->b,offset);
    remove_offset(&d->op->c,offset);
}


static DATA look_for_offset_in_sum(DATA d,DATA *root)
{
    if (!d.op) return data_none();
    if (d.op->dsc == &op_plus) {
	DATA tmp;

	tmp = look_for_offset_in_sum(d.op->a,root);
	if (tmp.type != dt_none) return tmp;
	tmp = look_for_offset_in_sum(d.op->b,root);
	if (tmp.type != dt_none) return tmp;
    }
    if (debug > 1) {
	printf("*** COMMON %d **************************************\n",
	  have_common_offset(root,d));
	print_expr(stdout,*root);
	print_expr(stdout,d);
	printf("******************************************************\n");
    }
    if (have_common_offset(root,d)) return d;
    return data_none();
}


static DATA look_for_offset_to_pull(DATA *d,DATA *root)
{
    DATA tmp;

    if (!d->op) return data_none();
    if (d->op->dsc == &op_access) return data_none();
    tmp = look_for_offset_to_pull(&d->op->a,root);
    if (tmp.type != dt_none) return tmp;
    tmp = look_for_offset_to_pull(&d->op->b,root);
    if (tmp.type != dt_none) return tmp;
    tmp = look_for_offset_to_pull(&d->op->c,root);
    if (tmp.type != dt_none) return tmp;
    if (d->op->dsc == &op_offset) {
	tmp = look_for_offset_in_sum(d->op->b,root);
	if (tmp.type != dt_none) {
	    tmp = data_clone(tmp);
	    remove_offset(root,tmp);
	    return tmp;
	}
    }
    return data_none();
}


static void pull_offset(DATA *d)
{
    if (!d->op || d->op->dsc != &op_offset) {
	DATA found;

	found = look_for_offset_to_pull(d,d);
	if (debug > 1) {
	    debug_expr("look_for_offset_to_pull on ...",*d);
	    debug_expr("... yields",found);
	}
	if (found.type != dt_none) {
	    DATA new;

	    new = op_binary(&op_offset,*d,found);
	    *d = new;
	}
    }
    if (!d->op) return;
    pull_offset(&d->op->a);
    pull_offset(&d->op->b);
    pull_offset(&d->op->c);
}


/* ----- Extract and combine field roots ----------------------------------- */


static int have_common_root(DATA d,FIELD_ROOT **common_root)
{
    FIELD_ROOT *root;

    if (!d.op) return 1;
    if (!have_offset(d)) return 1;
    if (d.op->dsc != &op_field_root) return 0;
    root = d.op->b.u.field_root;
    if (*common_root) return root == *common_root;
    *common_root = root;
    return 1;
}


static void pull_root(DATA *d)
{
    DATA old;

    if (!d->op || d->op->dsc != &op_field_root) return;
    old = *d;
    *d = d->op->a;
    data_destroy_1(old);
}


static void bubble_roots(DATA *d,int limit_bubbling)
{
    FIELD_ROOT *common_root;

    if (!d->op) {
	if (d->type == dt_field_root)
	    *d = op_binary(&op_field_root,data_none(),*d);
	return;
    }
    bubble_roots(&d->op->a,limit_bubbling);
    bubble_roots(&d->op->b,limit_bubbling);
    bubble_roots(&d->op->c,limit_bubbling);
    common_root = NULL;
    if (!have_common_root(d->op->a,&common_root)) return;
    if (!have_common_root(d->op->b,&common_root)) {
	if (!common_root) return;
    }
    else {
	if (!have_common_root(d->op->c,&common_root)) return;
	if (!common_root) return; /* nothing to do */
	if (!limit_bubbling) {
	    pull_root(&d->op->b);
	    pull_root(&d->op->c);
	}
    }
    /*
     * Don't bubble field root beyond access if this is a meta field. This way,
     * we avoid ||s under a common match, which would make it virtually
     * impossible for if_u32.c to extract meta field matches.
     */
    if (limit_bubbling &&
      (d->op->dsc == &op_logical_and || d->op->dsc == &op_logical_or))
	return;
    pull_root(&d->op->a);
    *d = op_binary(&op_field_root,*d,data_field_root(common_root));
}


/* ----- Remove redundant roots -------------------------------------------- */


static void trim_roots(DATA *d,const FIELD_ROOT *root)
{
    while (d->op && d->op->dsc == &op_field_root) {
	const FIELD_ROOT *this_root = d->op->b.u.field_root;
	DATA next = d->op->a;

	if (root != this_root) {
	    root = this_root;
	    break;
	}
	data_destroy_1(*d);
	*d = next;
    }
    if (d->op) {
	trim_roots(&d->op->a,root);
	trim_roots(&d->op->b,root);
	trim_roots(&d->op->c,root);
    }
}


/* ----- Propagate constant offsets to access ------------------------------ */


static int extract_sum(DATA *d)
{
    DATA old;
    int a,b;
    int sum;

    if (d->op && d->op->dsc == &op_offset) return extract_sum(&d->op->a);
    if (!d->op || d->op->dsc != &op_plus) return 0;
    sum = extract_sum(&d->op->a)+extract_sum(&d->op->b);
    a = !d->op->a.op && d->op->a.type == dt_unum;
    b = !d->op->b.op && d->op->b.type == dt_unum;
    if (!a && !b) return sum;
    sum += (a ? d->op->a.u.unum : 0)+(b ? d->op->b.u.unum : 0);
    old = *d;
    if (a && b) {
	*d = data_unum(0);
	data_destroy(old);
	return sum;
    }
    *d = a ? d->op->b : d->op->a;
    data_destroy_1(old);
    return sum;
}


static void do_push_offset(DATA *d,int base)
{
    int new_base = base;

    if (!d->op) return;
    if (d->op->dsc == &op_offset) {
	new_base += extract_sum(&d->op->b);
	if (!d->op->b.op && d->op->b.type == dt_unum) {
	    DATA old;

	    new_base += d->op->b.u.unum;
	    old = *d;
	    *d = d->op->a;
	    data_destroy_1(old);
	    do_push_offset(d,new_base);
	    return;
	}
    }
    if (d->op->dsc == &op_access) {
	d->op->b.u.unum += base;
	return;
    }
    do_push_offset(&d->op->a,new_base);
    do_push_offset(&d->op->b,base);
    do_push_offset(&d->op->c,base);
}


static void push_offset(DATA *d)
{
    do_push_offset(d,0);
}


/* ------------------------------------------------------------------------- */


void iflib_offset(DATA *d,int for_u32)
{
    use_offset(d);
    debug_expr("AFTER use_offset",*d);
    bubble_roots(d,for_u32);
    debug_expr("AFTER bubble_roots",*d);
    trim_roots(d,field_root(0));
    debug_expr("AFTER trim_roots",*d);
    if (for_u32) {
	pull_offset(d);
	debug_expr("AFTER pull_offset",*d);
    }
    push_offset(d);
    debug_expr("AFTER push_offset",*d);
}
