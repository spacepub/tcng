/*
 * q_sfq.c - Statistical Fair Queuing qdisc
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */


#include <stddef.h>

#include "error.h"
#include "param.h"
#include "tree.h"
#include "tc.h"
#include "device.h" /* for guess_mtu */
#include "qdisc_common.h"
#include "qdisc.h"


/* ------------------------------- Checking -------------------------------- */


static void sfq_check(QDISC *qdisc)
{
    int guessed_mtu;

    param_get(qdisc->params,qdisc->location);
    if (!prm_quantum.present) return;
    guessed_mtu = guess_mtu(qdisc->parent.device);
    if (guessed_mtu > prm_quantum.v)
	warnf("real MTU (guessed %d) may be larger than quantum %d",
	  guessed_mtu,(int) prm_quantum.v);
}


/* -------------------------------- Dump tc -------------------------------- */


static void sfq_dump_tc(QDISC *qdisc)
{
    tc_pragma(qdisc->params);
    param_get(qdisc->params,qdisc->location);
    tc_qdisc_add(qdisc);
    if (prm_perturb.present) tc_add_time("perturb",prm_perturb.v);
    if (prm_quantum.present) tc_add_size("quantum",prm_quantum.v);
    tc_nl();
}


/* ------------------------------ Descriptors ------------------------------ */


static const PARAM_DSC *sfq_qdisc_opt[] = {
    &prm_perturb,	/* default: 0 */
    &prm_pragma,	/* list */
    &prm_quantum,	/* default: MTU */
    NULL
};

static PARAM_DEF sfq_qdisc = {
    .required = NULL,
    .optional = sfq_qdisc_opt,
};

QDISC_DSC sfq_dsc = {
    .name = "sfq",
    .flags = 0,
    .qdisc_param = &sfq_qdisc,
    .check = &sfq_check,
    .dump_tc = &sfq_dump_tc,
    .dump_ext = &generic_dump_ext,
};
