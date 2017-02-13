/*
 * q_dsmark.c - Differentiated Services Marking qdisc
 *
 * Written 2001-2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks, Network Robots
 * Copyright 2002,2003 Werner Almesberger
 */


#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include "util.h"
#include "location.h"
#include "error.h"
#include "param.h"
#include "tree.h"
#include "field.h"
#include "op.h"
#include "tc.h"
#include "filter.h"
#include "qdisc_common.h"
#include "qdisc.h"
#include "ext.h"


#define __DEFAULT_PRM(f) f(mask) f(value)


/* ----- Checking ---------------------------------------------------------- */


static void dsmark_check(QDISC *qdisc)
{
    DEFAULT_DECL;
    QDISC *inner_qdisc = NULL;
    CLASS *class;
    int have_indices;
    uint32_t indices;

    param_get(qdisc->params,qdisc->location);
    DEFAULT_SET;
    have_indices = prm_indices.present;
    indices = prm_indices.v;
    if (have_indices) {
	int tmp;

	for (tmp = prm_indices.v; tmp != 1; tmp >>= 1)
	    if (tmp & 1)
		lerror(qdisc->location,"indices must be a power of two");
    }

    /*
     * Pass 1: check values, find inner qdisc, and check it (assign class
     * numbers, etc.)
     */
    for (class = qdisc->classes; class; class = class->next) {
	check_childless(class);
	param_get(class->params,class->location);

	/* The following test must be before DEFAULT_GET ! */
	if (class->qdisc && (class->number != UNDEF_U32 ||
	  prm_mask.present || prm_value.present || prm_class_sel_path.present))
	    lwarnf(class->location,
	      "suggest unnumbered and parameter-less class for inner qdisc "
	      "of %s",qdisc->dsc->name);
	DEFAULT_GET;
	if (prm_mask.present && prm_mask.v > 0xff)
	    lerrorf(class->location,"mask 0x%x above limit 0xff",prm_mask.v);
	if (!class->qdisc) continue;
	if (inner_qdisc)
	    lerrorf(class->qdisc->location,"%s has only one inner qdisc",
	      qdisc->dsc->name);
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
     * Pass 4: check class number range
     */
    for (class = qdisc->classes; class; class = class->next) {
	if (class->number == UNDEF_U32) 
	    lerrorf(class->location,"%s does not auto-assign class numbers",
	      qdisc->dsc->name);
	param_get(class->params,class->location);
	DEFAULT_GET;
	if (((prm_mask.present && prm_mask.v != 0xff) ||
	  (prm_value.present && prm_value.v)) && have_indices &&
	  class->number >= indices)
	    lerrorf(class->location,
	      "%s class number (%lu) must be < indices (%lu) if using "
	      "parameters",qdisc->dsc->name,(unsigned long) class->number,
	      (unsigned long) indices);
    }
    sort_classes(&qdisc->classes);
    check_filters(qdisc->filters);
}


static void egress_check(QDISC *qdisc)
{
    if (qdisc->parent.qdisc)
	lwarn(qdisc->location,"egress qdisc should be at top level");
    dsmark_check(qdisc);
}


/* ----- Default classification -------------------------------------------- */


/*
 * @@@ Supporting dsmark's behaviour in the absence of a default index is
 * kind of difficult, because there are 64k possible skb->tc_index values
 * that need to be mapped to "classes". So it's better to wait with this
 * until we can absorb dsmark into some more abstract data forwarding
 * model.
 *
 * Update: at least if set_tc_index is present, things don't look quite so
 * bad.
 */

static void dsmark_default_class(DATA *d,QDISC *qdisc)
{
    DATA dfl;
    int i;

    param_get(qdisc->params,qdisc->location);
    /*
     * egress automatically provides class selection path compatible default
     * index.
     */
    if (qdisc->dsc == &egress_dsc && !prm_default_index.present) {
	prm_default_index.present = 1;
	prm_default_index.v = 0;
    }
    if (prm_default_index.present) {
	append_default(d,
	  data_decision(dr_class,require_class(qdisc,prm_default_index.v,0)));
	return;
    }
    if (!prm_set_tc_index.present || !prm_set_tc_index.v)
	lerror(qdisc->location,"dsmark_default_class: need "
	  "\"default_index\" or \"set_tc_index\"");
    dfl = data_decision(dr_class,require_class(qdisc,255,0));
    for (i = 254; i >= 0; i--)
	dfl = op_binary(&op_logical_or,
	  op_binary(&op_logical_and,
	    op_binary(&op_eq,
	      op_ternary(&op_access,data_field_root(field_root(0)),
		data_unum(1),data_unum(8)),
	      data_unum(i)),
	    data_decision(dr_class,require_class(qdisc,i,0))),
          dfl);
    append_default(d,dfl);
}


/* ----- Helper function for setting "indices" ----------------------------- */


static uint32_t get_indices(const QDISC *qdisc)
{
    const CLASS *class;
    uint32_t max_class = 0,indices;

    if (prm_indices.present) return prm_indices.v;
    for (class = qdisc->classes; class; class = class->next)
	if (class->number > max_class) max_class = class->number;
    for (indices = 1; indices <= max_class; indices *= 2);
    return indices;
}


/* ----- Dump tc ----------------------------------------------------------- */


static void dsmark_dump_tc(QDISC *qdisc)
{
    DEFAULT_DECL;
    CLASS *class;

    tc_pragma(qdisc->params);
    param_get(qdisc->params,qdisc->location);
    DEFAULT_SET;
    __tc_qdisc_add(qdisc,"dsmark"); /* also egress becomes dsmark */
    tc_more(" indices %d",(int) get_indices(qdisc));
    if (prm_default_index.present)
	tc_add_unum("default_index",prm_default_index.v);
    else if (qdisc->dsc == &egress_dsc) tc_add_unum("default_index",0);
    if (prm_set_tc_index.present) tc_more(" set_tc_index");
    tc_nl();
    for (class = qdisc->classes; class; class = class->next) {
	tc_pragma(class->params);
	param_get(class->params,class->location);
	DEFAULT_GET;
	if (prm_mask.present || prm_value.present) {
	    __tc_class_op(class,"dsmark","change");
	      /* also egress becomes dsmark */
	    if (prm_mask.present) tc_add_hex("mask",prm_mask.v);
	    if (prm_value.present) tc_add_hex("value",prm_value.v);
	    tc_nl();
	}
	dump_qdisc(class->qdisc);
    }
    dump_filters(qdisc->filters);
}


/* ----- Dump external ----------------------------------------------------- */


static void print_indices(FILE *file,const PARAM_DSC *param,const QDISC *qdisc)
{
    if (param == &prm_indices)
	fprintf(file," indices %d",(int) get_indices(qdisc));
}


static void dsmark_dump_ext(FILE *file,QDISC *qdisc)
{
    const PARAM_DSC *indices[] = {
	&prm_indices,
	NULL
    };
    const PARAM_DSC *class_sel_path[] = {
	&prm_class_sel_path,
	NULL
    };
    const CLASS *class;

    qdisc_param_load(qdisc);
    do_ext_dump_qdisc(file,qdisc,&dsmark_dsc,indices,print_indices);
    for (class = qdisc->classes; class; class = class->next) {
        param_push();
        class_param_load(class);
        ext_dump_class(file,class,class_sel_path,NULL);
        param_pop();
    }
}


/* ----- Descriptors ------------------------------------------------------- */


static const PARAM_DSC *dsmark_qdisc_opt[] = {
    &prm_default_index,
    &prm_indices,
    &prm_pragma,	/* list */
    &prm_set_tc_index,
    DEFAULT_LIST	/* NB: no trailing comma */
    NULL
};

static const PARAM_DSC *dsmark_class_opt[] = {
    &prm_class_sel_path,
    &prm_pragma,	/* list */
    DEFAULT_LIST	/* NB: no trailing comma */
    NULL
};

static PARAM_DEF dsmark_qdisc = {
    .required = NULL,
    .optional = dsmark_qdisc_opt,
};

static PARAM_DEF dsmark_class = {
    .required = NULL,
    .optional = dsmark_class_opt,
};

QDISC_DSC dsmark_dsc = {
    .name = "dsmark",
    .flags = QDISC_HAS_CLASSES | QDISC_HAS_FILTERS | QDISC_CHILD_QDISCS |
      QDISC_SHARED_QDISC | QDISC_CLASS_SEL_PATHS,
    .qdisc_param = &dsmark_qdisc,
    .class_param = &dsmark_class,
    .check = &dsmark_check,
    .default_class = &dsmark_default_class,
    .dump_tc = &dsmark_dump_tc,
    .dump_ext = &dsmark_dump_ext,
};

QDISC_DSC egress_dsc = {
    .name = "egress",
    .flags = QDISC_HAS_CLASSES | QDISC_HAS_FILTERS | QDISC_CHILD_QDISCS |
      QDISC_SHARED_QDISC | QDISC_CLASS_SEL_PATHS,
    .qdisc_param = &dsmark_qdisc,
    .class_param = &dsmark_class,
    .check = &egress_check,
    .default_class = &dsmark_default_class,
    .dump_tc = &dsmark_dump_tc,
    .dump_ext = &dsmark_dump_ext,
};
