/*
 * f_fw.c - Firewall mark filter
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 by Werner Almesberger
 */

#include "util.h"
#include <stddef.h>
#include <linux/if_ether.h>

#include "error.h"
#include "param.h"
#include "tree.h"
#include "tc.h"
#include "police.h"
#include "filter_common.h"
#include "filter.h"


/* ------------------------------- Checking -------------------------------- */


static void fw_check(FILTER *filter)
{
    ELEMENT *element;

    for (element = filter->elements; element; element = element->next) {
	no_tunnel(element);
	if (element->number == UNDEF_U32)
	    lerror(element->location,"fw does not auto-assign element numbers");
	if (!element->number)
	    lerror(element->location,"fw element ID must be non-zero");
	if (element->police) check_police(element->police);
    }
}


/* -------------------------------- Dump tc -------------------------------- */


static void fw_dump_tc(FILTER *filter)
{
    ELEMENT *element;

    for (element = filter->elements; element; element = element->next) {
	tc_pragma(element->params);
	tc_element_add(element,ETH_P_ALL);
	tc_add_classid(element->parent.class,0);
	if (element->police) dump_police(element->police);
	tc_nl();
    }
}


/* ------------------------------ Descriptors ------------------------------ */


static const PARAM_DSC *fw_filter_opt[] = {
    &prm_protocol,	/* unum */
    &prm_pragma,	/* list */
    NULL
};

static const PARAM_DSC *fw_element_opt[] = {
    &prm_pragma,	/* list */
    NULL
};

static PARAM_DEF fw_filter = {
    .required = NULL,
    .optional = fw_filter_opt,
};

static PARAM_DEF fw_element = {
    .required = NULL,
    .optional = fw_element_opt,
};

FILTER_DSC fw_dsc = {
    .name = "fw",
    .filter_param = &fw_filter,
    .element_param = &fw_element,
    .check = fw_check,
    .dump_tc = fw_dump_tc,
};
