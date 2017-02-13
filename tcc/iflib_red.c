/*
 * iflib_red.c - Normalize and reduce (optimize) an if expression
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */


#include <stdlib.h>

#include "config.h"
#include "util.h"
#include "registry.h"
#include "error.h"
#include "data.h"
#include "tree.h"
#include "op.h"
#include "iflib.h"


/*
 * @@@ does not handle nested ifs properly
 */


#define BUNDLE_THRESHOLD 2 /* bundle if at least that many common entries */
			   /* MUST be > 1 */


/* --------------------- Normalize boolean expression ---------------------- */


static void bubble_or(DATA *d)
{
    DATA new;

    while (1) {
	SELF_CONTAINED sc;

	if (d->type == dt_none) return;
	if (d->op) {
	    bubble_or(&d->op->a);
	    bubble_or(&d->op->b);
	    bubble_or(&d->op->c);
	}
	if (!d->op || d->op->dsc != &op_logical_and) return;

	sc = self_contained(d->op->a);
	if (sc == sc_yes || sc == sc_no_false) {
	    new = d->op->a;
	    data_destroy(d->op->b);
	    data_destroy_1(*d);
	    *d = new;
	    continue;
	}

	/*
	 * (a || b) && c -> (a && c) || (b && c)
	 *
	 * Exception: if b has a side-effect, this transformation is only
	 * valid if c is self-contained or if a is self-contained or always
	 * true.
	 */
	if (d->op->a.op && d->op->a.op->dsc == &op_logical_or) {
	    sc = self_contained(d->op->a.op->a);
	    if (!side_effect(d->op->a.op->b) ||
	      self_contained(d->op->b) == sc_yes ||
	      sc == sc_yes || sc == sc_no_true) {
		new = op_binary(&op_logical_or,
		  op_binary(&op_logical_and,data_clone(d->op->a.op->a),
		    data_clone(d->op->b)),
		  op_binary(&op_logical_and,data_clone(d->op->a.op->b),
		    data_clone(d->op->b)));
		data_destroy(*d);
		*d = new;
		continue;
	    }
	}

	/* a && (b || c) -> (a && b) || (a && c) */
	if (d->op->b.op && d->op->b.op->dsc == &op_logical_or) {
	    if (action_tree(d->op->b) &&
	      self_contained(d->op->b) == sc_yes)
		break;
		  /* Do NOT normalize actions */
	    new = op_binary(&op_logical_or,
	      op_binary(&op_logical_and,data_clone(d->op->a),
		data_clone(d->op->b.op->a)),
	      op_binary(&op_logical_and,data_clone(d->op->a),
		data_clone(d->op->b.op->b)));
	    data_destroy(*d);
	    *d = new;
	    continue;
	}
	break;
    }
}


static void linearize(DATA *d)
{
    if (d->type == dt_none) return;

    /* (a || b) || c -> a || (b || c)   (idem for &&) */
    while (d->op &&
      (d->op->dsc == &op_logical_or || d->op->dsc == &op_logical_and) &&
      d->op->a.op && d->op->a.op->dsc == d->op->dsc) {
	DATA tmp;

	tmp = d->op->a;
	d->op->a = tmp.op->b;
	tmp.op->b = *d;
	*d = tmp;
    }
    if (d->op) {
	linearize(&d->op->a);
	linearize(&d->op->b);
	linearize(&d->op->c);
    }
}


void iflib_normalize(DATA *d)
{
    bubble_or(d);
    debug_expr("AFTER bubble_or",*d);
    linearize(d);
    debug_expr("AFTER linearize (2)",*d);
}


/* --------------- Eliminate redundant common subexpressions --------------- */


int expr_equal(DATA a,DATA b)
{
    if (a.op) {
	if (!b.op) return 0;
	if (a.op->dsc != b.op->dsc) return 0;
	return expr_equal(a.op->a,b.op->a) && expr_equal(a.op->b,b.op->b) &&
	  expr_equal(a.op->c,b.op->c);
    }
    if (b.op) return 0;
    return data_equal(a,b);
}


static void common_subexpressions(DATA *d)
{
    while (d->op && (d->op->dsc == &op_or || d->op->dsc == &op_and ||
      d->op->dsc == &op_logical_or || d->op->dsc == &op_logical_and)) {
	DATA *walk;
	int again = 0;

	for (walk = &d->op->b; walk->op && walk->op->dsc == d->op->dsc;
	  walk = &walk->op->b) {
	    DATA next;

	    /* a && ... && a && ... -> a && ... && ... */
	    if (expr_equal(d->op->a,walk->op->a)) {
		next = walk->op->b;
		data_destroy(walk->op->a);
		data_destroy_1(*walk);
		*walk = next;
		again = 1;
		break;
	    }

	    /* a && ... && a -> a && ... */
	    if (expr_equal(d->op->a,walk->op->b)) {
		next = walk->op->a;
		data_destroy(walk->op->b);
		data_destroy_1(*walk);
		*walk = next;
		break;
	    }
	}
	if (!again) break;
    }
    if (d->op) {
	common_subexpressions(&d->op->a);
	common_subexpressions(&d->op->b);
	common_subexpressions(&d->op->c);
    }
}


/* ------------------ Bundle common && prefix in || chain ------------------ */

/*
 * (a && b) || (a && c) || d -> (a && (b || c) || d
 *
 * Note: (a && x) expressions must be consecutive, e.g. we can't bundle
 * (a && b) || c || (a && d)  to  (a && (b || d) || c, because shortcut
 * evaluation would be different if a = c = d = 1, b = 0
 *
 * Also, "a" must be a sequence of &&s right at the beginning of
 * the expression, e.g. we can't bundle (a && b && c) || (b && a && c)
 *
 * Note: none of this is really binding. In fact, we could relax the
 * reordering constraints quite a bit it we take into account that they
 * are caused by a) accepting states (i.e. decision), and b) field
 * accessibility. E.g. in the example above, we know that "a" and "b"
 * can each be evaluated without precondition, so they're in fact
 * commutative.
 */

/*
 * First, convert the ||-then-&& structure into a matrix, for easier handling
 */

struct or_row {
    DATA *and;
    int ands;
    DATA **cells;
};

struct matrix {
    int ors;
    struct or_row *rows;
};

static void bundle_make_matrix(DATA *d,struct matrix *m)
{
    DATA *or;
    int i;

    m->ors = 0;
    for (or = d; 1; or = &or->op->b) {
	m->ors++;
	if (!or->op || or->op->dsc != &op_logical_or) break;
    }
    m->rows = alloc(sizeof(struct or_row)*m->ors);
    or = d;
    for (i = 0; i < m->ors-1; i++) {
	m->rows[i].and = &or->op->a;
	or = &or->op->b;
    }
    m->rows[i].and = or;
    for (i = 0; i < m->ors; i++) {
	struct or_row *r = m->rows+i;
	DATA *and;
	int j;

	r->ands = 0;
	for (and = r->and; 1; and = &and->op->b) {
	    r->ands++;
	    if (!and->op || and->op->dsc != &op_logical_and) break;
	}
	r->cells = alloc(sizeof(DATA *)*r->ands);
	and = r->and;
	for (j = 0; j < r->ands-1; j++) {
	    r->cells[j] = &and->op->a;
	    and = &and->op->b;
	}
	r->cells[j] = and;
    }
}


static char bundle_dump_char(DATA d)
{
    if (d.op) {
	if (d.op->dsc == &op_eq) return '=';
	if (d.op->dsc == &op_logical_or) return '|';
	if (d.op->dsc == &op_conform) return 'P';
	if (d.op->dsc == &op_count) return 'P';
	return '?';
    }
    if (d.type == dt_decision) return 'D';
    return 'd';
}


static void bundle_dump_matrix(FILE *file,struct matrix m)
{
    int or;

    if (!debug) return;
    fprintf(file,"----- current matrix -----\n");
    for (or = 0; or < m.ors; or++) {
	int and;

	for (and = 0; and < m.rows[or].ands; and++)
	    putc(bundle_dump_char(*m.rows[or].cells[and]),file);
	putc('\n',file);
    }
    fprintf(file,"----------\n");
}


static void bundle_free_matrix(struct matrix *m)
{
    int i;

    for (i = 0; i < m->ors; i++)
	free(m->rows[i].cells);
    free(m->rows);
}


/*
 * Find best optimization. Actually, this isn't optimal yet. We should also
 * take into account how we could bundle adjacent areas. But this is probably
 * overkill.
 */


static void bundle_find_best(struct matrix *m,int *best_or,int *best_ors,
  int *best_ands)
{
    int first_or;

    *best_ors = *best_ands = 0;
    for (first_or = 0; first_or < m->ors; first_or++) {
	int ors;

	for (ors = 2; first_or+ors <= m->ors; ors++) {
	    int min_ands = -1;
	    int i,j,ands;

	    for (i = first_or; i < first_or+ors; i++)
		if (m->rows[i].ands < min_ands || min_ands == -1)
		    min_ands = m->rows[i].ands;
	    if (ors*min_ands < *best_ors**best_ands) continue;
	    for (ands = 1; ands <= min_ands; ands++) {
		for (i = first_or+1; i < first_or+ors; i++)
		    for (j = 0; j < ands; j++)
			if (!expr_equal(*m->rows[first_or].cells[j],
			  *m->rows[i].cells[j]))
			    goto out;
		if (ors*ands > *best_ors**best_ands) {
		    *best_or = first_or;
		    *best_ors = ors;
		    *best_ands = ands;
		}
	    }
out:
	    ; /* semicolon required for "new" C syntax */
	}
    }
}


/*
 * Build a new tree
 *
 *          |-| ands=2
 * 	    a b c
 * first -> d e f-- ors=2
 *          d e g--
 *          h
 *          i j k
 *
 * becomes
 * (a && b && c) || (d && e && (f || g)) || h || (i && j && k)
 *
 * Note: we data_clone a lot here. This keeps the whole design reasonably
 * simple, but it's bad for performance.
 */


static DATA bundle_ands(struct or_row *r,int from,int ands,DATA add)
{
    DATA tmp = add;
    int j,last = ands == -1 ? r->ands-1 : from+ands-1;

    if (add.type == dt_none) {
	if (last < from) return data_none();
	tmp = data_clone(*r->cells[last]);
    }
    else last++;
    for (j = last-1; j >= from; j--)
	tmp = op_binary(&op_logical_and,data_clone(*r->cells[j]),tmp);
    return tmp;
}


static DATA bundle_ors(struct matrix *m,int from,int ors,int from_ands,
  int ands,DATA add)
{
    DATA tmp = add;
    int i,last = ors == -1 ? m->ors-1 : from+ors-1;

    if (add.type == dt_none) {
	if (last < from) return data_none();
	tmp = bundle_ands(m->rows+last,from_ands,ands,data_none());
    }
    else last++;
    for (i = last-1; i >= from; i--) {
	if (tmp.type == dt_none) break; /* a || ab -> a */
	tmp = op_binary(&op_logical_or,bundle_ands(m->rows+i,from_ands,ands,
	  data_none()),tmp);
    }
    return tmp;
}


static DATA bundle_rebuild(struct matrix *m,int from,int ors,int ands)
{
    DATA d,new;

    new = bundle_ors(m,from,ors,ands,-1,data_none());
    new = bundle_ands(m->rows+from,0,ands,new);
    d = from+ors == m->ors ? new :
      op_binary(&op_logical_or,new,bundle_ors(m,from+ors,-1,0,-1,data_none()));
    if (from) d = bundle_ors(m,0,from,0,-1,d);
    return d;
}


/*
 * Skip all ors
 */


static void bundle(DATA *d)
{
    while (1) {
	struct matrix m;
	int from,ors,ands;
	DATA old = *d;

	debug_expr("BEFORE bundle iteration",*d);
	if (!d->op) return;
	if (d->op->dsc != &op_logical_or) break;
	bundle_make_matrix(d,&m);
	bundle_dump_matrix(stdout,m);
	bundle_find_best(&m,&from,&ors,&ands);
	if (ors*ands < BUNDLE_THRESHOLD) {
	    bundle_free_matrix(&m);
	    break;
	}
	debugf("BEFORE bundle_rebuild (from %d, ors %d, ands %d)\n",from,ors,
	  ands);
	*d = bundle_rebuild(&m,from,ors,ands);
	debug_expr("AFTER bundle_rebuild",*d);
	data_destroy(old);
	bundle_free_matrix(&m);
    }
    bundle(&d->op->a);
    bundle(&d->op->b);
    bundle(&d->op->c);
}


/* ---------- Remove dead branches caused by shortcut evaluation ----------- */


void prune_shortcuts(DATA *d)
{
    DATA old = *d;

    if (!d->op) return;
    prune_shortcuts(&d->op->a);
    prune_shortcuts(&d->op->b);
    prune_shortcuts(&d->op->c);

    /* <class X> || ... -> <class X>  and <class X> && ... -> <class X> */
    if ((d->op->dsc == &op_logical_or || d->op->dsc == &op_logical_and) &&
      d->op->a.type == dt_decision) {
	*d = d->op->a;
	data_destroy(old.op->b);
	data_destroy_1(old);
	return;
    }

    if (d->op->dsc == &op_logical_and) {
	if (!d->op->a.op && d->op->a.type == dt_unum) {
	    /* 0 && X -> 0 */
	    if (!d->op->a.u.unum) {
		*d = data_unum(0);
		data_destroy(old);
		return;
	    }
	    /* 1 && X -> X */
	    if (d->op->a.u.unum) {
		*d = old.op->b;
		data_destroy_1(old);
		return;
	    }
	}
	if (!d->op->b.op && d->op->b.type == dt_unum) {
	    /* X && 0 -> 0, unless X has side-effect */
	    if (!d->op->b.u.unum && !side_effect(d->op->a)) {
		*d = data_unum(0);
		data_destroy(old);
		return;
	    }
	    /* X && 1 -> X */
	    if (d->op->b.u.unum) {
		*d = old.op->a;
		data_destroy_1(old);
		return;
	    }
	}
    }

    if (d->op->dsc == &op_logical_or) {
	if (!d->op->a.op && d->op->a.type == dt_unum) {
	    /* 1 || X -> 1 */
	    if (d->op->a.u.unum) {
		*d = data_unum(1);
		data_destroy(old);
		return;
	    }
	    /* 0 || X -> X */
	    if (!d->op->a.u.unum) {
		*d = old.op->b;
		data_destroy_1(old);
		return;
	    }
	}
	if (!d->op->b.op && d->op->b.type == dt_unum) {
	    /* X || 1 -> 1, unless X has side-effect */
	    if (d->op->b.u.unum && !side_effect(d->op->a)) {
		*d = data_unum(1);
		data_destroy(old);
		return;
	    }
	    /* X || 0 -> X */
	    if (!d->op->b.u.unum) {
		*d = old.op->a;
		data_destroy_1(old);
		return;
	    }
	}
    }
}


/* --------------------- Optimize the "if" expression ---------------------- */


void iflib_reduce(DATA *d)
{
    debug_expr("BEFORE iflib_reduce",*d);
    /*
     * There's a bit white magic going on here.
     *
     * The first round of "prune_shortcuts" eliminates constructs like
     * 1 && class ...  which turns them into action trees, which can then be
     * left alone by bubble_or.
     *
     * The first "linearize" turns expressions of the type
     *  classification && action  which have become
     * (classification && action) && class X  into
     * classification (action && class X)
     *
     * This subtle change means that the action part can be self-contained
     * (that is, if "action" contains an || to filter out "false"), and
     * "bubble_or" doesn't have to break it into pieces to extract the ||s. 
     *
     * This in turn will keep iflib_act.c:push_action from needing to
     * re-arrange the expression even further, in order to bring all actions
     * together again.
     *
     * Since "bubble_or" breaks apart linear structures, and further processing
     * steps expect to find the expression in a linear structure, we have to
     * call "linearize" again after "bubble_or" (both in "iflib_normalize").
     */
    linearize(d);
    debug_expr("AFTER linearize (1)",*d);
    prune_shortcuts(d);
    debug_expr("AFTER prune_shortcuts (1)",*d);
    if (alg_mode || !registry_probe(&optimization_switches,"cse")) {
	iflib_normalize(d);
	return;
    }
    bundle(d);
    debug_expr("AFTER bundle",*d);
    iflib_normalize(d);
    common_subexpressions(d);
    debug_expr("AFTER common_subexpressions",*d);
    bundle(d);
    debug_expr("AFTER bundle",*d);
    prune_shortcuts(d);
    debug_expr("AFTER prune_shortcuts (2)",*d);
}
