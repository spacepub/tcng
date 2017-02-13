/*
 * iflib_not.c - Express negation with || and &&
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Werner Almesberger
 */


#include <stdint.h>
#include <assert.h>

#include <u128.h>

#include "config.h"
#include "error.h"
#include "data.h"
#include "op.h"
#include "iflib.h"


static int print_warning = 1;
  /* print warning if negating expressions with a side-effect*/


static int warn_side_effect(DATA d)
{
    if (d.type == dt_decision || (d.op && d.op->dsc == &op_count)) {
	yywarn("negating expression with a side-effect");
	print_warning = 0;
	return 1;
    }
    if (!d.op) return 0;
    return warn_side_effect(d.op->a) || warn_side_effect(d.op->b) ||
      warn_side_effect(d.op->c);
}


/*
 * Single bit tests can be inverted by simply negating the bit being tested.
 * E.g. (X & 2) != 0  ->  (X & 2) == 2
 */


static int single_bit_test(DATA *d)
{
    DATA mask_data;
    U128 mask,val,tmp;

    /* bubble_not has already switched the operator from != to == */
    assert(d->op->dsc == &op_eq);
    assert((d->op->b.type == dt_unum || d->op->b.type == dt_ipv6) &&
      !d->op->b.op);
    if (!d->op->a.op || d->op->a.op->dsc != &op_and) return 0;
    if (!d->op->a.op->a.op &&
      (d->op->a.op->a.type == dt_unum || d->op->a.op->a.type == dt_ipv6))
	mask_data = d->op->a.op->a;
    else if (!d->op->a.op->b.op &&
	  (d->op->a.op->b.type == dt_unum || d->op->a.op->b.type == dt_ipv6))
	    mask_data = d->op->a.op->b;
	else return 0;
    mask = data_convert(mask_data,dt_ipv6).u.u128;
    if (u128_is_zero(mask)) return 0;
    val = data_convert(d->op->b,dt_ipv6).u.u128;
    if (!u128_is_zero(val) && u128_cmp(val,mask)) return 0;
    for (tmp = mask; u128_is_zero(u128_and_32(tmp,1));
      tmp = u128_shift_right(tmp,1));
    if (u128_cmp(tmp,u128_from_32(1))) return 0;
    if (u128_is_zero(val)) d->op->b = mask_data;
    else d->op->b = data_unum(0);
    return 1;
}


/*
 * Each expression has a context consisting of an alternative ("otherwise"),
 * and a continuation ("next"). We will call them N and Z below. By default,
 * there is no N, and O is the global default condition for the filter. The
 * context can be expressed as follows:
 *
 * (x || Z) && N  or, equivalently from the perspective of "x":  (x && N) || Z
 *
 * We use the following notation for the context: x [|| Z && N]
 *
 * N and Z change when we traverse || and && as follows:
 * - || changes Z for the left-hand side of the expression:
 *   (a || b ) [|| Z && N]  ->
 *   b := b [|| Z && N]
 *   a := a [|| ((b && N) || Z) && N]
 * - && changes N for the left-hand side of the expression:
 *   (a && b) [|| Z && N]  ->
 *   b := b [|| Z && N]
 *   a := a [|| Z && ((b && N) || Z) ]
 *
 * Once this is done, we can easily eliminate ! as follows:
 *
 * !a [|| Z && N]  ->  (a && Z) || N [|| Z && N]
 */


static void bubble_not(DATA *d,DATA next,DATA otherwise)
{
    if (!d->op) return;
    if (d->op->dsc == &op_ne) {
	d->op->dsc = &op_eq;
	if (single_bit_test(d)) return;
	*d = op_unary(&op_logical_not,*d);
	bubble_not(d,next,otherwise);
	return;
    }
    if (d->op->dsc == &op_logical_not) {
	DATA a = d->op->a;

	if (a.op && a.op->dsc == &op_logical_not) {
	    DATA old = *d;

	    *d = a.op->a;
	    data_destroy_1(old.op->a);
	    data_destroy_1(old);
	    bubble_not(d,next,otherwise);
	    return;
	}
	if (a.op && a.op->dsc == &op_ne) {
	    DATA old = *d;

	    a.op->dsc = &op_eq;
	    *d = a;
	    data_destroy_1(old);
	    bubble_not(d,next,otherwise);
	    return;
	}
	if (warn_expensive || warn_exp_post_opt) {
	    if (warn_exp_error)
		error("negation is an \"expensive\" operation");
	    warn("negation is an \"expensive\" operation");
	}
	if (print_warning) warn_side_effect(a);
	bubble_not(&a,otherwise,next);
	data_destroy_1(*d);
	*d = op_binary(&op_logical_or,op_binary(&op_logical_and,a,
	  data_clone(otherwise)),data_clone(next));
	return;
    }
    bubble_not(&d->op->b,next,otherwise);
    if (d->op->dsc == &op_logical_or || d->op->dsc == &op_logical_and) {
	DATA ext;

	ext = op_binary(&op_logical_or,op_binary(&op_logical_and,d->op->b,next),
	  otherwise);
	if (d->op->dsc == &op_logical_or) bubble_not(&d->op->a,next,ext);
	else bubble_not(&d->op->a,ext,otherwise);
	data_destroy_1(ext.op->a);
	data_destroy_1(ext);
	return;
    }
    bubble_not(&d->op->a,next,otherwise);
    bubble_not(&d->op->c,next,otherwise);
}


void iflib_not(DATA *d)
{
    print_warning = 1;
    debug_expr("BEFORE bubble_not",*d);
    bubble_not(d,data_unum(1),data_unum(0));
    debug_expr("AFTER bubble_not",*d);
}
