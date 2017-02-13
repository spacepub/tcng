/*
 * filter.c - Filter handling
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Network Robots
 */


#include <stdint.h>
#include <stddef.h>

#include "util.h"
#include "error.h"
#include "param.h"
#include "op.h"
#include "tree.h"
#include "police.h"
#include "filter_common.h"
#include "filter.h"


/* ----------------------- Assignment of unique IDs ------------------------ */


static void assign_filter_ids(FILTER *list)
{
    uint32_t prio = 1;

    while (list) {
	if (list->number == UNDEF_U32) list->number = prio++;
	else {
	    if (list->number < prio)
		errorf("assigned priority (%u) below minimum available "
		  "priority (%u)",list->number,prio);
	    prio = list->number+1;
	}
	if (list->number & ~0xffff)
	    errorf("invalid filter ID 0x%x",list->number);
	list = list->next;
    }
}


/* -------------------------------- Tunnels -------------------------------- */


CLASS class_is_tunnel;


void no_tunnel(ELEMENT *element)
{
    if (element->parent.tunnel)
	errorf("filter \"%s\" does not support tunnels",
	  element->parent.filter->dsc->name);
}


/* --------------------------------- Drop ---------------------------------- */


CLASS class_is_drop;


/* ------------------------------------------------------------------------- */


void add_element(ELEMENT *element,PARENT parent)
{
    ELEMENT **anchor;

    if (!parent.filter) yyerror("filter element without filter");
    element->parent = parent;
    for (anchor = &parent.filter->elements; *anchor; anchor = &(*anchor)->next);
    *anchor = element;
}


void add_filter(FILTER *filter)
{
    FILTER **anchor;
    QDISC *qdisc = filter->parent.qdisc;

    if (!qdisc) yyerror("filter without qdisc");
    if (!(qdisc->dsc->flags & QDISC_HAS_FILTERS))
	yyerrorf("qdisc \"%s\" does not have filters",qdisc->dsc->name);
    for (anchor = filter->parent.class ? &filter->parent.class->filters :
      &qdisc->filters; *anchor; anchor = &(*anchor)->next);
    *anchor = filter;
}


/*
 * Add a dummy filter element for "if" to the current qdisc, unless one
 * already exists. Then append a filter element for this if expression.
 * Note: there is currently no way to attach an if expression to a class @@@
 * instead of a qdisc.
 */


static void unum_only(DATA d,LOCATION loc)
{
    if (d.type == dt_none) return;
    if (d.op && (d.op->dsc == &op_conform || d.op->dsc == &op_count)) return;
    if (d.type != dt_unum && d.type != dt_ipv6 && d.type != dt_field &&
      d.type != dt_field_root && d.type != dt_decision)
	lerror(loc,"only integer expressions allowed in if");
    if (!d.op) return;
    unum_only(d.op->a,loc);
    unum_only(d.op->b,loc);
    unum_only(d.op->c,loc);
}


static void if_check_policers(DATA d)
{
    /*
     * If is wicked - it may also use policers in the expression !
     * @@@ in fact, it should propagate element->police into the expression
     */
    if (d.type == dt_police) check_police(d.u.police);
    else if (d.type == dt_bucket) check_bucket(d.u.police);
    if (!d.op) return;
    if_check_policers(d.op->a);
    if_check_policers(d.op->b);
    if_check_policers(d.op->c);
}


static void hack_type(DATA *d)
{
    if (d->type == dt_ipv4) d->type = dt_unum;
    if (d->op) {
	hack_type(&d->op->a);
	hack_type(&d->op->b);
	hack_type(&d->op->c);
    }
}


ELEMENT *add_if(DATA expression,PARENT parent,FILTER *filter,LOCATION loc)
{
    ELEMENT **element;

    hack_type(&expression);
    unum_only(expression,loc);
    if_check_policers(expression);
    if (!filter) {
	FILTER **anchor;

#if 0
	for (anchor = parent.class->parent.class ?
	  &parent.class->parent.class->filters :
	  &parent.class->parent.qdisc->filters; *anchor;
	  anchor= &(*anchor)->next)
#endif
	for (anchor = &parent.qdisc->filters; *anchor;
	  anchor= &(*anchor)->next)
	    if ((*anchor)->dsc == &if_dsc) break;
	if (*anchor) filter = *anchor;
	else {
	    filter = alloc_t(FILTER);
	    filter->parent = parent;
	    if (parent.class == &class_is_drop) filter->parent.class = NULL;
		/* okay for now, because "if" always goes to qdisc anyway @@@ */
	    else filter->parent.class = parent.class->parent.class;
		/* current parent.class is local class; need to get outer one */
	    filter->number = UNDEF_U32;
	    filter->location = filter->parent.qdisc->location;
	    filter->dsc = &if_dsc;
	    filter->params = NULL;
	    filter->elements = NULL;
	    filter->next = NULL;
	    *anchor = filter;
	}
    }
    for (element = &filter->elements; *element; element = &(*element)->next);
    *element = alloc_t(ELEMENT);
    (*element)->parent = parent;
    (*element)->parent.filter = filter;
    (*element)->number = UNDEF_U32;
    (*element)->location = loc;
    (*element)->params = param_make(&prm_if_expr,expression);
    (*element)->police = NULL; /* @@@ sure ? */
    (*element)->next = NULL;
    return *element;
}


void check_filters(FILTER *list)
{
    assign_filter_ids(list);
    while (list) {
	list->dsc->check(list);
	list = list->next;
    }
}


void dump_filters(FILTER *list)
{
    while (list) {
	list->dsc->dump_tc(list);
	list = list->next;
    }
}
