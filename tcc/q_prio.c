/*
 * q_prio.c - Priority scheduler qdisc
 *
 * Written 2001-2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002,2003 Werner Almesberger
 */


#include <stddef.h>

#include "tcdefs.h"
#include "error.h"
#include "param.h"
#include "tree.h"
#include "data.h"
#include "op.h"
#include "field.h"
#include "tc.h"
#include "ext.h"
#include "filter.h"
#include "qdisc_common.h"
#include "qdisc.h"


static int default_priomap[16] = { 1,2,2,2,1,2,0,0,1,1,1,1,1,1,1,1 };
    /* from iproute2/tc/q_prio.c:prio_parse_opt */


/* ------------------------------- Checking -------------------------------- */


static void prio_check(QDISC *qdisc)
{
    CLASS *class;
    int bands,max_bands;

    curr_id = 1;
    (void) recurse_class(qdisc->classes,assign_class_id,qdisc->classes);
    param_get(qdisc->params,qdisc->location);
    if (prm_bands.present) bands = max_bands = prm_bands.v;
    else {
	bands = TCQ_PRIO_DEFAULT_BANDS;
	max_bands = TCQ_PRIO_BANDS;
    }
    if (bands < 3 && !prm_priomap.present)
	/* see iproute2/tc/q_prio.c or
	   linux/net/sched/sch_generic.c:prio2band */
	lerror(qdisc->location,"must set priomap if < 3 bands, because "
	  "default priomap uses 3rd band");
    if (prm_priomap.present) {
	DATA_LIST *walk;
	int n = 0;

	for (walk = prm_data(qdisc->params,&prm_priomap).u.list; walk;
	  walk = walk->next) {
	    if (++n > TC_PRIO_MAX)
		lerror(qdisc->location,"too many entries in priomap");
	    if (walk->ref->type != dt_class)
		lerrorf(qdisc->location,
		  "invalid type in %d%s entry of \"priomap\" (expected class, "
		  "not %s)",n,n == 1 ? "st" : n == 2 ? "nd" : "th",
		  type_name(walk->ref->type));
	}
	while (n < TC_PRIO_MAX) {
	    if (default_priomap[n] >= bands)
		lerror(qdisc->location,"\"priomap\" too short to cover "
		  "default entries exceeding allowed bands");
	    n++;
	}
    }
    for (class = qdisc->classes; class; class = class->next) {
	if (!class->number || class->number > max_bands)
	    lerrorf(class->location,
	      "prio class number must be in the range 1..%d",max_bands);
	if (class->number > bands) bands = class->number;
	check_childless(class);
	check_qdisc(class->qdisc);
    }
    if (!prm_bands.present && bands != TCQ_PRIO_DEFAULT_BANDS) {
	PARAM *next;

	next = qdisc->params;
	qdisc->params = param_make(&prm_bands,data_unum(bands));
	qdisc->params->next = next;
    }
    sort_classes(&qdisc->classes);
    check_filters(qdisc->filters);
}


/* ------------------------ Default classification ------------------------- */


/*
 * Again, this is not exactly what the real thing does. Locally generated
 * packets take skb->priority from sk->priority, and not from the TOS byte.
 * This can be fixed once we have meta fields. @@@
 */

static void prio_default_class(DATA *d,QDISC *qdisc)
{
    static int ip_tos2prio[16] = { /* from net/ipv4/route.c */
	TC_PRIO_BESTEFFORT,
	TC_PRIO_FILLER,
	TC_PRIO_BESTEFFORT,
	TC_PRIO_BESTEFFORT,
	TC_PRIO_BULK,
	TC_PRIO_BULK,
	TC_PRIO_BULK,
	TC_PRIO_BULK,
	TC_PRIO_INTERACTIVE,
	TC_PRIO_INTERACTIVE,
	TC_PRIO_INTERACTIVE,
	TC_PRIO_INTERACTIVE,
	TC_PRIO_INTERACTIVE_BULK,
	TC_PRIO_INTERACTIVE_BULK,
	TC_PRIO_INTERACTIVE_BULK,
	TC_PRIO_INTERACTIVE_BULK
    };
    DATA last = data_none();
    int i;

    param_get(qdisc->params,qdisc->location);
    for (i = 15; i >= 0; i--) {
	DATA dfl;
	int prio,band;

	prio = ip_tos2prio[i];
	band = default_priomap[prio];
	if (prm_priomap.present) {
	    DATA_LIST *walk;
	    int n = 0;

	    for (walk = prm_data(qdisc->params,&prm_priomap).u.list; walk;
	      walk = walk->next)
		if (n == i) {
		    band = walk->ref->u.class->number;
		    break;
		}
	}

	/*
	 * (raw[1] & 0x1e) == N && class X
	 */
	dfl = data_decision(dr_class,require_class(qdisc,band+1,0));
	if (i < 15)
	    dfl = op_binary(&op_logical_and,
	      op_binary(&op_eq,
		op_binary(&op_and,
		  op_ternary(&op_access,data_field_root(field_root(0)),
		    data_unum(1),data_unum(8)),
		  data_unum(0x1e)),
		data_unum(i*2)),
	      dfl);
	if (last.type == dt_none) last = dfl;
	else last = op_binary(&op_logical_or,dfl,last);
    }
    append_default(d,last);
}


/* -------------------------------- Dump tc -------------------------------- */


static void prio_dump_tc(QDISC *qdisc)
{
    CLASS *class;

    tc_pragma(qdisc->params);
    param_get(qdisc->params,qdisc->location);
    tc_qdisc_add(qdisc);
    if (prm_bands.present) tc_add_unum("bands",prm_bands.v);
    if (prm_priomap.present) {
	DATA_LIST *walk;

	tc_more(" priomap");
	for (walk = prm_data(qdisc->params,&prm_priomap).u.list; walk;
	  walk = walk->next)
	    tc_more(" %d",(int) walk->ref->u.class->number-1);
    }
    tc_nl();
    for (class = qdisc->classes; class; class = class->next) {
	tc_pragma(class->params);
	dump_qdisc(class->qdisc);
    }
    dump_filters(qdisc->filters);
}


/* ----------------------------- Dump external ----------------------------- */


static void insert_bands(FILE *file,const PARAM_DSC *param,
  const QDISC *qdisc)
{
    if (param == &prm_bands)
	fprintf(file," bands %u",
	  prm_bands.present ? (unsigned) prm_bands.v : 3);
}


static void prio_dump_ext(FILE *file,QDISC *qdisc)
{
    const PARAM_DSC *hide[] = {
	&prm_bands,
	&prm_priomap,
	NULL
    };
    const CLASS *class;

    qdisc_param_load(qdisc);
    ext_dump_qdisc(file,qdisc,hide,insert_bands);
    for (class = qdisc->classes; class; class = class->next) {
	param_push();
	class_param_load(class);
	ext_dump_class(file,class,NULL,NULL);
	param_pop();
    }
}


/* ------------------------------ Descriptors ------------------------------ */


static const PARAM_DSC *prio_qdisc_opt[] = {
    &prm_bands,		/* unum */
    &prm_pragma,	/* list */
    &prm_priomap,	/* list of classes */
    NULL
};

static const PARAM_DSC *prio_class_opt[] = {
    &prm_pragma,	/* list */
    NULL
};

static PARAM_DEF prio_qdisc = {
    .required = NULL,
    .optional = prio_qdisc_opt,
};

static PARAM_DEF prio_class = {
    .required = NULL,
    .optional = prio_class_opt,
};

QDISC_DSC prio_dsc = {
    .name = "prio",
    .flags = QDISC_HAS_CLASSES | QDISC_HAS_FILTERS | QDISC_CHILD_QDISCS,
    .qdisc_param = &prio_qdisc,
    .class_param = &prio_class,
    .check = &prio_check,
    .default_class = &prio_default_class,
    .dump_tc = &prio_dump_tc,
    .dump_ext = &prio_dump_ext,
};
