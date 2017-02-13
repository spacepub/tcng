/*
 * f_if.c - If pseudo-filter
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Network Robots
 */


#include <stdint.h>
#include <stddef.h>

#include "error.h"
#include "param.h"
#include "tree.h"
#include "police.h"
#include "if.h"
#include "iflib.h"
#include "target.h"
#include "filter_common.h"
#include "filter.h"


/* ------------------------------- Checking -------------------------------- */


static void if_check(FILTER *filter)
{
    ELEMENT *element;
    FILTER *other;

    for (element = filter->elements; element; element = element->next) {
	iflib_cheap_ops(element->location,
	  prm_data(element->params,&prm_if_expr));
	no_tunnel(element); /* just in case ... */
	if (element->police) check_police(element->police);
    }
    for (other = filter->parent.qdisc->filters; other; other = other->next)
	if (other->dsc != &if_dsc)
	    yyerror("cannot combine if with other filters (yet)");
}


/* -------------------------------- Dump tc -------------------------------- */


static void if_dump_tc(FILTER *filter)
{
    QDISC *qdisc = filter->parent.qdisc;
    struct ext_target *target;

    if (qdisc->if_expr.type != dt_none) return;
    for (target = ext_targets; target; target = target->next)
	if (have_target("if",target->name)) {
	    dump_if_ext(filter,target->name);
	    return;
	}
    qdisc->if_expr = iflib_combine(qdisc);
    if (have_target("if","c")) dump_if_c(filter);
    else if (have_target("if","tc")) dump_if_u32(filter);
    else error("no targets available for \"if\"");
}


/* ------------------------------ Descriptors ------------------------------ */


static const PARAM_DSC *if_filter_opt[] = {
    &prm_pragma,	/* list */
    NULL
};

static PARAM_DEF if_filter = {
    .required = NULL,
    .optional = if_filter_opt,
};

FILTER_DSC if_dsc = {
    .name = "if",
    .filter_param = &if_filter,
    .element_param = NULL,
    .check = if_check,
    .dump_tc = if_dump_tc,
};
