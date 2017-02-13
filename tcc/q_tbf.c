/*
 * q_tbf.c - Tocken Bucket Filer qdisc
 *
 * Written 2001,2003,2004 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2003,2004 Werner Almesberger
 */


#include <stddef.h>
#include <assert.h>

#include "error.h"
#include "param.h"
#include "tree.h"
#include "tc.h"
#include "device.h" /* for guess_mtu */
#include "qdisc_common.h"
#include "qdisc.h"


/* ------------------------------- Checking -------------------------------- */


static void tbf_check(QDISC *qdisc)
{
    int guessed_mtu,mtu;

    param_get(qdisc->params,qdisc->location);
    guessed_mtu = guess_mtu(qdisc->parent.device);
    if (!prm_mtu.present)
	mtu = guessed_mtu;
    else {
	if (guessed_mtu > prm_mtu.v)
	    warnf("real MTU (guessed %d) may be larger than %d",guessed_mtu,
	      (int) prm_mtu.v);
	mtu = prm_mtu.v;
    }
    if (prm_burst.v < mtu)
	lerrorf(qdisc->location,"burst (%d) must be larger than MTU (%d)",
	  (int) prm_burst.v,mtu);
/* @@@ check that burst and limit >= rate/HZ */
    if (prm_mpu.present && prm_mpu.v < mtu)
	lerrorf(qdisc->location,"MPU (%d) smaller than MTU (%d)",
	  (int) prm_mpu.v,mtu);
    if (!prm_rate.v) lerrorf(qdisc->location,"rate must be non-zero");
    if (prm_peakrate.present && prm_rate.v > prm_peakrate.v)
	lerrorf(qdisc->location,"peakrate (%d) must be greater than rate (%d)",
	  (int) prm_peakrate.v,(int) prm_rate.v);
    if (qdisc->classes) {
	qdisc->classes->number = 0;
	  /* inner qdisc of TBF is always rooted at outer qdisc */
	assert(qdisc->classes->qdisc);
	if (qdisc->classes->next)
	    lerrorf(qdisc->classes->next->qdisc->location,
	      "TBF can have only one inner qdisc");
	check_qdisc(qdisc->classes->qdisc);
    }
}


/* ------------------------ Default classification ------------------------- */


static void tbf_default_class(DATA *d,QDISC *qdisc)
{
    append_default(d,data_decision(dr_class,require_class(qdisc,0,1)));
}


/* -------------------------------- Dump tc -------------------------------- */


static void tbf_dump_tc(QDISC *qdisc)
{
    tc_pragma(qdisc->params);
    param_get(qdisc->params,qdisc->location);
    tc_qdisc_add(qdisc);
    tc_add_size("burst",prm_burst.v);
    tc_add_size("limit",prm_limit.v);
    if (prm_mtu.present) tc_add_size("mtu",prm_mtu.v);
    tc_add_rate("rate",prm_rate.v);
    if (prm_mpu.present) tc_add_size("mpu",prm_mpu.v);
    if (prm_peakrate.present) tc_add_rate("peakrate",prm_peakrate.v);
    tc_nl();
    if (qdisc->classes) dump_qdisc(qdisc->classes->qdisc);
}


/* ------------------------------ Descriptors ------------------------------ */


static const PARAM_DSC *tbf_qdisc_req[] = {
    &prm_rate,		/* rate */
    &prm_burst,		/* size */
    &prm_limit,		/* size; users can calculate latency in tcng */
    NULL
};

static const PARAM_DSC *tbf_qdisc_opt[] = {
    &prm_mpu,		/* size */
    &prm_peakrate,	/* rate */
    &prm_mtu,		/* size (tc sometimes defaults to 2047) */
    &prm_pragma,	/* list */
    NULL
};

static PARAM_DEF tbf_qdisc = {
    .required = tbf_qdisc_req,
    .optional = tbf_qdisc_opt,
};

static PARAM_DEF tbf_class = {
    .required = NULL,
    .optional = NULL,
};

QDISC_DSC tbf_dsc = {
    .name = "tbf",
    .flags = QDISC_CHILD_QDISCS | QDISC_SHARED_QDISC,
    .qdisc_param = &tbf_qdisc,
    .class_param = &tbf_class,
    .check = &tbf_check,
    .default_class = &tbf_default_class,
    .dump_tc = &tbf_dump_tc,
    .dump_ext = &generic_dump_ext,
};
