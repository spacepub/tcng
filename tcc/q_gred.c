/*
 * q_gred.c - Generalized RED qdisc
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks, Network Robots
 * Copyright 2002 Werner Almesberger
 */


#include <stddef.h>
#include <stdlib.h>

#include "tcdefs.h"
#include "util.h"
#include "error.h"
#include "param.h"
#include "tree.h"
#include "tc.h"
#include "ext.h"
#include "qdisc_common.h"
#include "qdisc.h"


#define __DEFAULT_PRM(f) f(avpkt) f(burst) f(limit) f(max) f(min)
#define __DEFAULT_PRM_REQ(f) __DEFAULT_PRM(f)


/* ------------------------------- Checking -------------------------------- */


static void gred_check(QDISC *qdisc)
{
    DEFAULT_DECL;
    CLASS *class;
    int grio,dp,got_default = 0;

    param_get(qdisc->params,qdisc->location);
    DEFAULT_SET;
    grio = prm_grio.present;
    dp = 0;
    for (class = qdisc->classes; class; class = class->next) {
	check_childless(class);
	if (class->qdisc)
	    lerror(class->qdisc->location,"gred has no inner qdiscs");
	if (class->number == UNDEF_U32) {
	    const CLASS *walk;

	    for (walk = qdisc->classes; walk; walk = walk->next)
		if (walk->number == dp)
		    lerror(walk->location,
		      "can't auto-assign gred class IDs in this case");
	    class->number = dp;
	}
	else {
	    const CLASS *walk;

	    if (class->number >= TCQ_GRED_MAX_DPs)
		lerrorf(class->location,
		  "gred class number must be in the range 0..%d",
		  TCQ_GRED_MAX_DPs-1);
	    for (walk = qdisc->classes; walk; walk = walk->next)
		if (walk != class && walk->number == class->number)
		    lerrorf(class->location,"duplicate class ID 0x%x",
		      class->number);
	    dp = class->number;
	}
	dp++;
	param_get(class->params,class->location);
	DEFAULT_GET;
	DEFAULT_CHECK(class->location);
/* @@@ perform value checks listed for red */
	if (prm_default.present) {
	    if (!class->number)
		lerror(class->location,"can't use DP 0 as default");
	    if (got_default)
		lerror(class->location,
		  "more than one class marked as \"default\"");
	    got_default = 1;
	}
	if (grio && !prm_prio.present)
	    lerror(class->location,"\"prio\" is required when using grio");
	if (!grio && prm_prio.present)
	    lerror(class->location,"\"prio\" is not allowed if not using grio");
    }
    for (class = qdisc->classes; class; class = class->next) {
	param_get(class->params,class->location);
	if (prm_prio.present && (prm_prio.v == 0 || prm_prio.v > dp))
	    lerrorf(class->location,
	      "\"prio\" (%d) larger than number of DPs (%d)",
	      (int) prm_prio.v,dp);
    }
    sort_classes(&qdisc->classes);
    if (!got_default)
	lerror(qdisc->location,
	  "gred requires one class to be marked as \"default\"");
}


/* -------------------------------- Dump tc -------------------------------- */


static void gred_dump_tc(QDISC *qdisc)
{
    DEFAULT_DECL;
    CLASS *class;
    uint32_t bandwidth;
    uint32_t default_dp = 0; /* initialize for gcc */

    tc_pragma(qdisc->params);
    param_get(qdisc->params,qdisc->location);
    DEFAULT_SET;
    tc_qdisc_add(qdisc);
    tc_more(" setup");
    bandwidth = prm_bandwidth.v;
    if (prm_grio.present) tc_more(" grio");
    for (class = qdisc->classes; class; class = class->next) {
	tc_pragma(class->params);
	param_get(class->params,class->location);
	if (prm_default.present) default_dp = class->number;
	if (!class->next) tc_add_unum("DPs",class->number+1);
    }
    tc_add_unum("default",default_dp);
    tc_nl();
    for (class = qdisc->classes; class; class = class->next) {
	param_get(class->params,class->location);
	DEFAULT_GET;
	tc_qdisc_change(qdisc);
	tc_add_unum("DP",class->number);
	if (prm_prio.present) tc_add_unum("prio",prm_prio.v);
	red_parameters(class->params,bandwidth);
	tc_nl();
    }
}


/* ----------------------------- Dump external ----------------------------- */


static void insert_default(FILE *file,const PARAM_DSC *param,
  const QDISC *qdisc)
{
    const CLASS *class;
    
    if (param != &prm_default) return;
    for (class = qdisc->classes; class; class = class->next)
	if (prm_present(class->params,&prm_default)) {
	    fprintf(file," default %d",(int) class->number);
	    return;
	}
    abort();
}


static void gred_dump_ext(FILE *file,QDISC *qdisc)
{
    const PARAM_DSC *gred_default[] = {
	&prm_default,
	NULL
    };
    const CLASS *class;

    qdisc_param_load(qdisc);
    ext_dump_qdisc(file,qdisc,gred_default,insert_default);
    for (class = qdisc->classes; class; class = class->next) {
	param_push();
	class_param_load(class);
	ext_dump_class(file,class,gred_default,NULL);
	param_pop();
    }
}


/* ------------------------------ Descriptors ------------------------------ */


static const PARAM_DSC *gred_qdisc_req[] = {
    &prm_bandwidth,	/* rate */
    NULL
};

static const PARAM_DSC *gred_qdisc_opt[] = {
    &prm_grio,		/* flag */
    &prm_pragma,	/* list */
    DEFAULT_LIST	/* NB: no trailing comma */
    NULL
};

static const PARAM_DSC *gred_class_opt[] = {
    &prm_default,	/* flag */
    &prm_pragma,	/* list */
    &prm_probability,	/* default: 0.02 */
    &prm_prio,		/* unum */
    DEFAULT_LIST	/* NB: no trailing comma */
    NULL
};

static PARAM_DEF gred_qdisc = {
    .required = gred_qdisc_req,
    .optional = gred_qdisc_opt,
};

static PARAM_DEF gred_class = {
    .required = NULL,
    .optional = gred_class_opt,
};

QDISC_DSC gred_dsc = {
    .name = "gred",
    .flags = QDISC_HAS_CLASSES,
    .qdisc_param = &gred_qdisc,
    .class_param = &gred_class,
    .check = &gred_check,
    .dump_tc = &gred_dump_tc,
    .dump_ext = &gred_dump_ext,
};
