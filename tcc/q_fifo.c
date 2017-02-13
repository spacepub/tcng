/*
 * q_fifo.c - First In, First Out qdisc
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Werner Almesberger
 */


#include <stddef.h>

#include "error.h"
#include "param.h"
#include "tree.h"
#include "tc.h"
#include "qdisc_common.h"
#include "qdisc.h"


/* ------------------------------- Checking -------------------------------- */


static void fifo_check(QDISC *qdisc)
{
    param_get(qdisc->params,qdisc->location);
    if (prm_limit.present && prm_plimit.present)
	lerror(qdisc->location,"duplicate parameter \"limit\"");
}


/* -------------------------------- Dump tc -------------------------------- */


static void fifo_dump_tc(QDISC *qdisc)
{
    tc_pragma(qdisc->params);
    param_get(qdisc->params,qdisc->location);
    if (prm_limit.present) {
	__tc_qdisc_add(qdisc,"bfifo");
	tc_add_size("limit",prm_limit.v);
    }
    else {
	__tc_qdisc_add(qdisc,"pfifo");
	if (prm_plimit.present) tc_add_psize("limit",prm_plimit.v);
    }
    tc_nl();
}


/* ------------------------------ Descriptors ------------------------------ */


static const PARAM_DSC *fifo_qdisc_opt[] = {
    &prm_limit,		/* size */
    &prm_plimit,	/* psize */
    &prm_pragma,	/* list */
    NULL
};

static PARAM_DEF fifo_qdisc = {
    .required = NULL,
    .optional = fifo_qdisc_opt,
};

QDISC_DSC fifo_dsc = {
    .name = "fifo",
    .flags = 0,
    .qdisc_param = &fifo_qdisc,
    .check = &fifo_check,
    .dump_tc = &fifo_dump_tc,
    .dump_ext = &generic_dump_ext,
};
