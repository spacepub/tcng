/*
 * iflib_comb.c - Generation of if expressions
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Network Robots
 */


#include <stdlib.h>

#include "data.h"
#include "tree.h"
#include "param.h"
#include "op.h"
#include "filter.h"
#include "iflib.h"


/*
 * @@@ does not handle nested ifs properly
 */


/* ----- Add "class" decisions to if expression ---------------------------- */


static void complement_leaf_decisions(DATA *d,const CLASS *class)
{
    if (!d->op) {
	if (d->type == dt_decision) {
	    if (class == &class_is_drop) d->u.decision.result = dr_drop;
	    d->u.decision.class = class;
	}
	return;
    }
    complement_leaf_decisions(&d->op->a,class);
    complement_leaf_decisions(&d->op->b,class);
    complement_leaf_decisions(&d->op->c,class);
}


void complement_decisions(DATA *d,CLASS *class)
{
    *d = op_binary(&op_logical_and,*d,
      data_decision(class == &class_is_drop ? dr_drop : dr_class,class));
    complement_leaf_decisions(d,class);
}


/* ----- Iterate callback over if hierarchy -------------------------------- */


static void if_iterate_filters(const FILTER *filters,
  void (*fn)(const ELEMENT *e,void *user),void *user)
{
    const FILTER *filter;

    for (filter = filters; filter; filter = filter->next)
	if (filter->dsc == &if_dsc) {
	    const ELEMENT *element;

	    for (element = filter->elements; element; element = element->next)
		fn(element,user);
	}
}


static void if_iterate_classes(const CLASS *classes,
  void (*fn)(const ELEMENT *e,void *user),void *user)
{
    const CLASS *class;

    for (class = classes; class; class = class->next) {
	if_iterate_filters(class->filters,fn,user);
	if (class->child) if_iterate_classes(class->child,fn,user);
    }
}


void iflib_comb_iterate(const QDISC *qdisc,
  void (*fn)(const ELEMENT *e,void *user),void *user)
{
    if_iterate_filters(qdisc->filters,fn,user);
    if_iterate_classes(qdisc->classes,fn,user);
}


/* ----- Combine all ifs into single expression ---------------------------- */


static void iflib_combine_callback(const ELEMENT *e,void *user)
{
    DATA *root = user;
    DATA expr = data_clone(prm_data(e->params,&prm_if_expr));

    complement_decisions(&expr,e->parent.class);
    if (root->type == dt_none) *root = expr;
    else *root = op_binary(&op_logical_or,*root,expr);
}


DATA iflib_combine(const QDISC *qdisc)
{
    DATA d = data_none();

    iflib_comb_iterate(qdisc,iflib_combine_callback,&d);
    return d;
}
