/*
 * q_ingress.c - Ingress qdisc
 *
 * Written 2001,2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks, Network Robots
 * Copyright 2003 Werner Almesberger
 */


#include <stddef.h>
#include <assert.h>

#include "util.h"
#include "error.h"
#include "param.h"
#include "tree.h"
#include "tc.h"
#include "filter.h"
#include "qdisc_common.h"
#include "qdisc.h"
#include "ext.h"


/* ------------------------------- Checking -------------------------------- */


static void ingress_check(QDISC *qdisc)
{
    CLASS *class;
    QDISC *inner_qdisc = NULL;

    /*
     * Pass 1: check values, find inner qdisc, and check it (assign class
     * numbers, etc.)
     */
    for (class = qdisc->classes; class; class = class->next) {
	check_childless(class);
	param_get(class->params,class->location);
        if (class->qdisc && (class->number != UNDEF_U32 ||
	  prm_class_sel_path.present))
            lwarn(class->location,
	      "suggest unnumbered class for ingress' inner qdisc");
	if (!class->qdisc) continue;
	if (inner_qdisc)
	    lerror(class->qdisc->location,"ingress has only one inner qdisc");
	check_qdisc(class->qdisc);
	inner_qdisc = class->qdisc;
    }

    /*
     * Pass 2: collect class selection paths, and invoke class selection path
     * magic.
     */
    class_selection_paths(qdisc,inner_qdisc);

    /*
     * Pass 3: merge or number class containing inner qdisc, and check numbers
     */
    merge_shared(&qdisc->classes);

    /*
     * Pass 4: check for unnumbered class
     */
    for (class = qdisc->classes; class; class = class->next)
	if (class->number == UNDEF_U32)
            lerror(class->location,
              "ingress does not auto-assign class numbers");

    sort_classes(&qdisc->classes);
    check_filters(qdisc->filters);
}


/* ------------------------ Default classification ------------------------- */


/*
 * We cheat a little. The "real" ingress simply puts some undefined value
 * into skb->tc_index. Not nice.
 */


static void ingress_default_class(DATA *d,QDISC *qdisc)
{
    append_default(d,data_decision(dr_class,require_class(qdisc,0,0)));
}


/* -------------------------------- Dump tc -------------------------------- */


static void ingress_dump_tc(QDISC *qdisc)
{
    CLASS *class;

    for (class = qdisc->classes; class; class = class->next)
	if (class->qdisc)
	    lerror(class->qdisc->location,"tc ingress does not support inner "
	      "qdisc");
    tc_pragma(qdisc->params);
    tc_qdisc_add(qdisc);
    tc_nl();
    for (class = qdisc->classes; class; class = class->next)
	tc_pragma(class->params);
    dump_filters(qdisc->filters);
}


/* ----- Dump external ----------------------------------------------------- */


static void ingress_dump_ext(FILE *file,QDISC *qdisc)
{
    const PARAM_DSC *class_sel_path[] = {
	&prm_class_sel_path,
	NULL
    };
    const CLASS *class;

    ext_dump_qdisc(file,qdisc,NULL,NULL);
    for (class = qdisc->classes; class; class = class->next) {
        param_push();
        class_param_load(class);
        ext_dump_class(file,class,class_sel_path,NULL);
        param_pop();
    }

}


/* ------------------------------ Descriptors ------------------------------ */


static const PARAM_DSC *ingress_qdisc_opt[] = {
    &prm_pragma,	/* list */
    NULL
};

static const PARAM_DSC *ingress_class_opt[] = {
    &prm_class_sel_path,
    &prm_pragma,	/* list */
    NULL
};

static PARAM_DEF ingress_qdisc = {
    .required = NULL,
    .optional = ingress_qdisc_opt,
};

static PARAM_DEF ingress_class = {
    .required = NULL,
    .optional = ingress_class_opt,
};

QDISC_DSC ingress_dsc = {
    .name = "ingress",
    .flags = QDISC_HAS_CLASSES | QDISC_HAS_FILTERS | QDISC_CHILD_QDISCS |
      QDISC_SHARED_QDISC | QDISC_CLASS_SEL_PATHS,
    .qdisc_param = &ingress_qdisc,
    .class_param = &ingress_class,
    .check = &ingress_check,
    .default_class = &ingress_default_class,
    .dump_tc = &ingress_dump_tc,
    .dump_ext = &ingress_dump_ext,
};
