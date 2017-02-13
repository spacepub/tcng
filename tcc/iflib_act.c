/*
 * iflib_act.c - Re-arrange expressions containing policing and other actions
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */

/*
 * The canonical form for actions (i.e. policing and decisions) is to be at
 * the end of && chains, after all static matches. iflib_actions bubbles all
 * non-actions up the tree.
 */

/*
 * By generously combining matches from all over the place, we may generate
 * redundant or conflicting rules here. This is left to the user (i.e. if_ext)
 * to sort out.
 */


#include <stdlib.h>
#include <assert.h>

#include "config.h"
#include "location.h"
#include "error.h"
#include "data.h"
#include "op.h"
#include "iflib.h"


/*
 * Tricky case:
 *
 * (expr1 || decision) && expr2
 *
 * Here, (expr1 || decision) is _not_ self-contained, because we take expr2 if
 * expr1 is false.
 */


SELF_CONTAINED self_contained(DATA d)
{
    if (!d.op) {
	if (d.type == dt_unum) return d.u.unum ? sc_no_true : sc_no_false;
	return d.type == dt_decision ? sc_yes : sc_no;
    }
    if (d.op->dsc == &op_conform) return sc_no;
    if (d.op->dsc == &op_count) return sc_no_true;
    if (d.op->dsc == &op_logical_not)
	switch (self_contained(d.op->a)) {
	    case sc_no:
		return sc_no;
	    case sc_no_true:
		return sc_no_false;
	    case sc_no_false:
		return sc_no_true;
	    case sc_yes:
		return sc_yes;
	    default:
		abort();
	}
    if (d.op->dsc == &op_logical_and) {
	/*
	 * A B  res
	 * - -  ---
	 * Y -  Y	Y = self-contained
	 * F -  F	F = not self-contained, value is "false"
	 * T x  x	T = not self-contained, value is "true"
	 * N Y  F	N = not self-contained, value is unknown
	 * N T  N
	 * N F  F
	 * N N  N
	 */

	SELF_CONTAINED sc;

	sc = self_contained(d.op->a);
	if (sc == sc_yes || sc == sc_no_false) return sc;
	if (sc == sc_no_true) return self_contained(d.op->b);
	sc = self_contained(d.op->b);
	return sc == sc_yes || sc == sc_no_false ? sc_no_false : sc_no;
    }
    if (d.op->dsc == &op_logical_or) {
	/*
	 * A B  res
	 * - -  ---
	 * Y -  Y	Y = self-contained
	 * T -  T	F = not self-contained, value is "false"
	 * F x  x	T = not self-contained, value is "true"
	 * N Y  T	N = not self-contained, value is unknown
	 * N T  T
	 * N F  N
	 * N N  N
	 */

	SELF_CONTAINED sc;

	sc = self_contained(d.op->a);
	if (sc == sc_yes || sc == sc_no_true) return sc;
	if (sc == sc_no_false) return self_contained(d.op->b);
	sc = self_contained(d.op->b);
	return sc == sc_yes || sc == sc_no_true ? sc_no_true : sc_no;
    }
    return sc_no;
}


int action_tree(DATA d)
{
    if (!d.op) return d.type == dt_decision;
    if (d.op->dsc == &op_conform || d.op->dsc == &op_count) return 1;
    if (d.op->dsc == &op_logical_and || d.op->dsc == &op_logical_or)
	return action_tree(d.op->a) &&
	  (self_contained(d.op->a) == sc_yes || action_tree(d.op->b));
    return 0;
}


int side_effect(DATA d)
{
    if (!d.op) return d.type == dt_decision;
    if (d.op->dsc == &op_count) return 1;
    return side_effect(d.op->a) || side_effect(d.op->b) || side_effect(d.op->c);
}


/*
 * Transforms  A && M  ->  M && A
 */

/*
 * This transformation is wrong if A or M have side-effects, e.g.
 * count && match is very different from match && count.
 *
 * Unfortunately, this seems to be very hard to fix, so perhaps it's better to
 * switch over to the new algorithm instead, which doesn't have such issues.
 */

static void bubble_and(DATA *d)
{
    DATA tmp;

    assert(d->op && d->op->dsc == &op_logical_and);
    if (!action_tree(d->op->a) || action_tree(d->op->b)) return;
	/* already in canonical form */
    tmp = d->op->a;
    if (self_contained(tmp) == sc_yes) {
	data_destroy(d->op->b);
	data_destroy_1(*d);
	*d = tmp;
    }
    if (side_effect(tmp) || side_effect(d->op->b))
	dump_failed(*d,"too many side effects");
    d->op->a = d->op->b;
    d->op->b = tmp;
}


/*
 * Transforms an expression of the type  (M && P) || X  where M are static
 * matches, and P are non-self-contained policing actions, by attaching a
 * copy of X at the place of P, and replacing at each transition Mi->Ai
 * from matches to an action tree (i.e. policing or decisions), Ai with
 * (P || Ai)
 *
 * E.g.  (M1 && P1) || (M2 && P2) || D  would become
 * (M1 && ((M2 && (P1 || P2)) || (P1 || D)) || (M2 && P2) || D
 *
 * Now, || and && still remain lists, but this transformation might disturb
 * the "|| over &&" structure for matches, which would throw off later
 * stages. Therefore, we merge the || chain of X (after inserting P) back on
 * top by extracting each element E and changing it to (M && E).
 */

static void attach_prev(DATA *d,DATA attach)
{
    if (action_tree(*d)) {
	if (self_contained(attach) != sc_yes)
	    *d = op_binary(&op_logical_or,data_clone(attach),*d);
	else {
	    data_destroy(*d);
	    *d = data_clone(attach);
	}
	return;
    }
    if (!d->op) return;
    attach_prev(&d->op->a,attach);
    attach_prev(&d->op->b,attach);
    attach_prev(&d->op->c,attach);
}


static void add_precondition(DATA *d,const DATA *cond,DATA *cut)
{
    *cut = *d;
    *d = data_clone(*cond);
}


static void precondition_or(DATA *d,const DATA *cond,DATA *cut)
{
    while (d->op && d->op->dsc == &op_logical_or && !action_tree(*d)) {
	add_precondition(&d->op->a,cond,cut);
	d = &d->op->b;
    }
    add_precondition(d,cond,cut);
}


static void bubble_or(DATA *d)
{
    DATA *cut,p,x,x_cond;

    assert(d->op && d->op->dsc == &op_logical_or);
    for (cut = &d->op->a; cut->op && cut->op->dsc == &op_logical_and;
      cut = &cut->op->b)
	if (action_tree(*cut)) break;
    if (self_contained(*cut) == sc_yes) return;
	/* already in canonical form */
    p = *cut;
    x = data_clone(d->op->b);
    x_cond = data_clone(x);
    if (debug > 1) {
	debug_expr("x_cond BEFORE precondition_or",x_cond);
	debug_expr("... with precondition",d->op->a);
	debug_expr("... and cut at",*cut);
    }
    precondition_or(&x_cond,&d->op->a,cut);
    if (debug > 1) debug_expr("x_cond AFTER precondition_or",x_cond);
    attach_prev(&x_cond,p);
    if (debug > 1) {
	debug_expr("x_cond AFTER attach_prev",x_cond);
	debug_expr("... with prev being",p);
    }
    data_destroy(*d);
    *d = x_cond;
    if (self_contained(*d) == sc_yes) data_destroy(x);
    else {
	DATA *end;

	for (end = d; end->op && end->op->dsc == &op_logical_or;
	  end = &end->op->b);
	*end = op_binary(&op_logical_or,*end,x);
    }
    if (debug > 1) debug_expr("AFTER adding default",*d);
}


static void push_action(DATA *d)
{
    if (!d->op) return;
    push_action(&d->op->a);
    push_action(&d->op->b);
    push_action(&d->op->c);
    if (d->op->dsc == &op_logical_and) bubble_and(d);
    else if (d->op->dsc == &op_logical_or) bubble_or(d);
}


static void trim_self_contained(DATA *d)
{
    if (!d->op) return;
    trim_self_contained(&d->op->a);
    trim_self_contained(&d->op->b);
    trim_self_contained(&d->op->c);
    if ((d->op->dsc == &op_logical_and || d->op->dsc == &op_logical_or) &&
      self_contained(d->op->a) == sc_yes) {
      /* @@@ also use sc_no_* ? */
	DATA next = d->op->a;

	data_destroy(d->op->b);
	data_destroy_1(*d);
	*d = next;
    }
}


void iflib_actions(LOCATION loc,DATA *d)
{
    trim_self_contained(d);
    debug_expr("AFTER trim_self_contained",*d);
    iflib_cheap_actions(loc,*d);
    push_action(d);
    debug_expr("AFTER push_action",*d);
    iflib_reduce(d); /* eliminate redundancy uncovered by push_action */
    debug_expr("AFTER prune_shortcuts (3)",*d);
    iflib_normalize(d);
    debug_expr("AFTER iflib_normalize (again)",*d);
    push_action(d); /* better safe than sorry */
    debug_expr("AFTER push_action (2)",*d);
}
