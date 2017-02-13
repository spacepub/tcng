/*
 * f_route.c - Route tag filter
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Werner Almesberger
 */


#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "util.h"
#include "error.h"
#include "param.h"
#include "tree.h"
#include "tc.h"
#include "police.h"
#include "filter_common.h"
#include "filter.h"


#define __DEFAULT_PRM(f) f(from) f(fromif) f(order) f(to)


/* ------------------------------- Checking -------------------------------- */


static void route_check(FILTER *filter)
{
    ELEMENT *element;

    for (element = filter->elements; element; element = element->next) {
	no_tunnel(element);
	if (element->number != UNDEF_U32)
	    error("route has no user-settable element numbers");
	if (element->police) check_police(element->police);
    }
}


/* -------------------------------- Dump tc -------------------------------- */


static void route_dump_tc(FILTER *filter)
{
    DEFAULT_DECL;
    ELEMENT *element;

    tc_pragma(filter->params);
    param_get(filter->params,filter->location);
    DEFAULT_SET;
    tc_filter_add(filter,0);
    tc_nl();
    for (element = filter->elements; element; element = element->next) {
	tc_pragma(element->params);
	param_get(element->params,filter->location);
	DEFAULT_GET;
	tc_element_add(element,0);
	if (prm_from.present) tc_add_unum("from",prm_from.v);
	if (prm_fromif.present)
	    tc_add_string("fromif",prm_data(element->params,&prm_fromif));
	if (prm_to.present) tc_add_unum("to",prm_to.v);
	if (prm_order.present) tc_add_unum("order",prm_order.v);
	tc_add_classid(element->parent.class,0);
	if (element->police) dump_police(element->police);
	tc_nl();
    }
}


/* ------------------------------ Descriptors ------------------------------ */


static const PARAM_DSC *route_filter_opt[] = {
    &prm_pragma,	/* list */
    &prm_protocol,	/* unum */
    DEFAULT_LIST	/* NB: no trailing comma */
    NULL
};

static const PARAM_DSC *route_element_opt[] = {
    &prm_pragma,	/* list */
    DEFAULT_LIST	/* NB: no trailing comma */
    NULL
};

static PARAM_DEF route_filter = {
    .required = NULL,
    .optional = route_filter_opt,
};

static PARAM_DEF route_element = {
    .required = NULL,
    .optional = route_element_opt,
};

FILTER_DSC route_dsc = {
    .name = "route",
    .filter_param = &route_filter,
    .element_param = &route_element,
    .check = route_check,
    .dump_tc = route_dump_tc,
};
