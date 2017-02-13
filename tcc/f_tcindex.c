/*
 * f_tcindex.c - Traffic Control Index filter
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Werner Almesberger
 */


#include <stdint.h>
#include <stddef.h>
#include <linux/if_ether.h>

#include "util.h"
#include "error.h"
#include "param.h"
#include "tree.h"
#include "tc.h"
#include "police.h"
#include "filter_common.h"
#include "filter.h"


/* ------------------------------- Checking -------------------------------- */


static void tcindex_check(FILTER *filter)
{
    ELEMENT *element;

    param_get(filter->params,filter->location);
    if (!prm_mask.present) prm_mask.v = 0xffff;
    if (!prm_hash.present) prm_hash.v = prm_mask.v+1;
    else {
	if (prm_hash.v < prm_mask.v+1)
	    errorf("tcindex: hash 0x%x must be greater than mask 0x%x",
	      (int) prm_hash.v,(int) prm_mask.v);
    }
    for (element = filter->elements; element; element = element->next) {
	no_tunnel(element);
	if (element->number == UNDEF_U32)
	    error("tcindex does not auto-assign element numbers");
/* @@@ check for any impossible number */
	if (element->number > prm_mask.v)
	    errorf("tcindex element number 0x%x greater than mask 0x%x",
	      (int) element->number,(int) prm_mask.v);
	if (element->police) check_police(element->police);
    }
}


/* -------------------------------- Dump tc -------------------------------- */


static void tcindex_dump_tc(FILTER *filter)
{
    ELEMENT *element;

    tc_pragma(filter->params);
    param_get(filter->params,filter->location);
    tc_filter_add(filter,ETH_P_ALL);
    if (prm_mask.present) tc_add_hex("mask",prm_mask.v);
    if (prm_hash.present) tc_add_unum("hash",prm_hash.v);
    if (prm_shift.present) tc_add_unum("shift",prm_shift.v);
    if (prm_fall_through.present)
	tc_more(" %s",prm_fall_through.v ? "fall_through" : "pass_on");
    tc_nl();
    for (element = filter->elements; element; element = element->next) {
	tc_pragma(element->params);
	tc_element_add(element,ETH_P_ALL);
	tc_add_classid(element->parent.class,0);
	if (element->police) dump_police(element->police);
	tc_nl();
    }
}


/* ------------------------------ Descriptors ------------------------------ */


static const PARAM_DSC *tcindex_filter_opt[] = {
    &prm_hash,		/* unum */
    &prm_mask,		/* unum */
    &prm_pragma,	/* list */
    &prm_protocol,	/* unum */
    &prm_shift,		/* unum */
    &prm_fall_through,	/* flag */
    NULL
};

static const PARAM_DSC *tcindex_element_opt[] = {
    &prm_pragma,	/* list */
    NULL
};

static PARAM_DEF tcindex_filter = {
    .required = NULL,
    .optional = tcindex_filter_opt,
};

static PARAM_DEF tcindex_element = {
    .required = NULL,
    .optional = tcindex_element_opt,
};

FILTER_DSC tcindex_dsc = {
    .name = "tcindex",
    .filter_param = &tcindex_filter,
    .element_param = &tcindex_element,
    .check = tcindex_check,
    .dump_tc = tcindex_dump_tc,
};
