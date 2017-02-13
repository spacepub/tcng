/*
 * q_red.c - Random Early Detection qdisc
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */


#include <stddef.h>

#include "param.h"
#include "tree.h"
#include "tc.h"
#include "qdisc_common.h"
#include "qdisc.h"


/* ------------------------------- Checking -------------------------------- */


static void red_check(QDISC *qdisc)
{
/* @@@ warn if rounding ? avoid conversion ? */
/* @@@ missing checks:
 * max <= limit
 * min <= max
 * pretty much everything: > 0 or >= 0
 */
}


/* -------------------------------- Dump tc -------------------------------- */


static void red_dump_tc(QDISC *qdisc)
{
    tc_pragma(qdisc->params);
    param_get(qdisc->params,qdisc->location);
    tc_qdisc_add(qdisc);
    red_parameters(qdisc->params,prm_bandwidth.v);
    if (prm_ecn.present) tc_more(" ecn");
    tc_nl();
}


/* ------------------------------ Descriptors ------------------------------ */


static const PARAM_DSC *red_qdisc_req[] = {
    &prm_limit,		/* size */
    &prm_min,		/* size */
    &prm_max,		/* size */
    &prm_burst,		/* size (tc uses number of packets) */
    &prm_avpkt,		/* size */
    &prm_bandwidth,	/* rate (tc defaults to 10 Mbps) */
    NULL
};

static const PARAM_DSC *red_qdisc_opt[] = {
    &prm_ecn,		/* default: off */
    &prm_pragma,	/* list */
    &prm_probability,	/* default: 0.02 */
    NULL
};

static PARAM_DEF red_qdisc = {
    .required = red_qdisc_req,
    .optional = red_qdisc_opt,
};

QDISC_DSC red_dsc = {
    .name = "red",
    .flags = 0,
    .qdisc_param = &red_qdisc,
    .check = &red_check,
    .dump_tc = &red_dump_tc,
    .dump_ext = &generic_dump_ext,
};
