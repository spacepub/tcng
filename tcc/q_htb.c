/*
 * q_htb.c - Hierachical Token Bucket qdisc
 *
 * Written 2002 by Jacob Teplitsky
 * Copyright 2002 Bivio Networks
 */


#include <stddef.h>

#include "tcdefs.h"
#include "util.h"
#include "error.h"
#include "data.h"
#include "param.h"
#include "tree.h"
#include "field.h"
#include "op.h"
#include "tc.h"
#include "ext.h"
#include "filter.h"
#include "qdisc_common.h"
#include "qdisc.h"


/* ----- Checking ---------------------------------------------------------- */


#define __DEFAULT_PRM(f) f(burst) f(ceil) f(cburst) f(prio) f(quantum) \
  f(rate) f(tquantum)
#define __DEFAULT_PRM_REQ(f) f(rate)


static void htb_check_classes(CLASS **classes,DEFAULT_ARGS,int depth,
  int *got_default,double tquantum_default)
{
    CLASS *class;

    if (!*classes) return;
    sort_classes(classes);
    if (depth == TC_HTB_MAXLEVEL)
	lerrorf((*classes)->location,
	  "can't nest HTB classes deeper than %d levels",TC_HTB_MAXLEVEL);
    for (class = *classes; class; class = class->next) {
	DEFAULT_DECL_SAVED;
	double tquantum = tquantum_default;
	PARAM_DSC *knock_out;

	param_get(class->params,class->location);
	if (prm_quantum.present && prm_tquantum.present)
	    lerror(class->location,"duplicate parameter \"quantum\"");
	knock_out = NULL;
	if (prm_quantum.present) knock_out = &prm_tquantum;
	if (prm_tquantum.present) {
	    knock_out = &prm_quantum;
	    tquantum = prm_data(class->params,&prm_tquantum).u.fnum;
	}
	DEFAULT_GET;
	if (class->number != 0) {
	     DEFAULT_CHECK(class->location);
	}
	if (knock_out) knock_out->present = 0;
	DEFAULT_SAVE;
	if (class->number != 0) {
	    if (!prm_rate.v)
		lerror(class->location,"rate must be non-zero");
	}
	if (prm_tquantum.present && class->number) {
	    double quantum;
	    PARAM *param;

	    quantum = tquantum*(double) prm_rate.v;
	    if (quantum < 1.0)
		lerror(class->location,"quantum is less than one byte");
	    if (quantum > 0xffffffffUL)
		lerror(class->location,"quantum exceeds 4GB");
	    param = param_make(&prm_quantum,data_typed_fnum(quantum,dt_size));
	    param->next = class->params;
	    class->params = param;
	}
	if (prm_ceil.present && prm_ceil.v < prm_rate.v)
	    lerrorf(class->location,"\"ceil\" (%lu) < \"rate\" (%lu)",
	      (unsigned long) prm_ceil.v,(unsigned long) prm_rate.v);
	if (prm_prio.present && prm_prio.v > TC_HTB_MAXPRIO)
	    lerrorf(class->location,"\"prio\" (%lu) > %d",
	      (unsigned long) prm_prio.v,TC_HTB_MAXPRIO);
	if (prm_default.present) {
	    if (*got_default) {
		lerror(class->location,
		  "more than one class marked as \"default\"");
	    }
	    *got_default = 1;
	}
	
	htb_check_classes(&class->child,DEFAULT_PASS_SAVED,depth+1,
	  got_default,tquantum);
	check_qdisc(class->qdisc);
	check_filters(class->filters);
    }
}


static void create_root_class(QDISC *qdisc)
{
    CLASS *classes,*root,*class;
    int zero = 0;

    if (recurse_class(qdisc->classes,count_class_ids,&zero)) return;
    classes = qdisc->classes;
    qdisc->classes = NULL;
    root = require_class(qdisc,0,0);
    root->child = classes;
    for (class = classes; class; class = class->next)
	class->parent.class = root;
    /*
     * Root class inherits all parameters from qdisc, and gets prm_rate from
     * prm_bandwidth.
     */
}

static void check_root_class(QDISC *qdisc)
{
    if (!qdisc->classes || qdisc->classes->next)
	lerror(qdisc->classes ? qdisc->classes->location :
	  qdisc->classes->next->location,
	  "HTB must have exactly one root class");
    if (qdisc->classes->number) /* hard to trigger :-) */
	lerrorf(qdisc->classes->location,
	  "HTB root class must have number 0, not %lu\n",
	  (unsigned long) qdisc->classes->number);
    if (qdisc->filters && qdisc->classes->filters)
	lerror(qdisc->filters->location,"please specify filters either at HTB "
	  "qdisc or at root class");
    if (qdisc->classes->filters) {
	qdisc->filters = qdisc->classes->filters;
	qdisc->classes->filters = NULL;
    }
}


static void htb_check(QDISC *qdisc)
{
    int got_default = 0;

    curr_id = 1;
    (void) recurse_class(qdisc->classes,assign_class_id,qdisc->classes);
    create_root_class(qdisc);
    param_get(qdisc->params,qdisc->location);
    if (prm_quantum.present && prm_tquantum.present)
	lerror(qdisc->location,"duplicate parameter \"quantum\"");
    check_root_class(qdisc); /* must be after param_get */
    htb_check_classes(&qdisc->classes,DEFAULT_PASS,0, &got_default,
      prm_tquantum.present ? prm_data(qdisc->params,&prm_tquantum).u.fnum : 0);
#if 0
    if (!got_default) {
	 lerror(qdisc->location,
		"htb requires one class to be marked as \"default\"");
    }
#endif
    check_filters(qdisc->filters);
}


/* ----- Default classification -------------------------------------------- */


static CLASS *htb_get_default_class(CLASS *classes)
{
    CLASS *class, *rt;

    if (!classes) return (NULL);
    for (class = classes; class; class = class->next) {
	param_get(class->params,class->location);
	if (prm_default.present) {
	    return (class);
	}
	rt = htb_get_default_class(class->child);
	if (rt) {
	    return rt;
	}
    }
    return NULL;
}

static void htb_use_default(DATA *d,DATA dfl,const CLASS *class)
{
    if (!d->op) {
	if (d->type == dt_decision && d->u.decision.result == dr_class &&
	  d->u.decision.class == class)
	    *d = data_clone(dfl);
	return;
    }
    htb_use_default(&d->op->a,dfl,class);
    htb_use_default(&d->op->b,dfl,class);
    htb_use_default(&d->op->c,dfl,class);
}


static void htb_push_defaults(DATA *d,PARAM *params,QDISC *qdisc,
  CLASS *classes,CLASS *this_class)
{
    CLASS *class;

    for (class = classes; class; class = class->next)
	htb_push_defaults(d,class->params,qdisc,class->child,class);
}


static void htb_default_class(DATA *d,QDISC *qdisc)
{
    /*
     * This is the only place where the root class becomes visible.
     * This may confuse external programs quite badly.
     */
    CLASS *class;

    class = htb_get_default_class(qdisc->classes);
    if (class) {
	 append_default(d,data_decision(dr_class, class));
    }
}


/* ----- Dump tc ----------------------------------------------------------- */


static void htb_dump_classes_tc(CLASS *classes,DEFAULT_ARGS)
{
    CLASS *class;

    if (!classes) return;
    for (class = classes; class; class = class->next) {
	DEFAULT_DECL_SAVED;

	param_get(class->params,class->location);
	DEFAULT_GET;
	DEFAULT_CHECK(class->location);
	DEFAULT_SAVE;
	tc_pragma(class->params);
	tc_class_add(class);
	tc_add_rate("rate",prm_rate.v);
	if (prm_ceil.present) tc_add_rate("ceil",prm_ceil.v);
	if (prm_burst.present) tc_add_size("burst",prm_burst.v);
	if (prm_cburst.present) tc_add_size("cburst",prm_cburst.v);
	if (prm_prio.present) tc_add_unum("prio",prm_prio.v);
	if (prm_quantum.present) tc_add_size("quantum",prm_quantum.v);
	tc_nl();
	dump_qdisc(class->qdisc);
	htb_dump_classes_tc(class->child,DEFAULT_PASS_SAVED);
	dump_filters(class->filters);
    }
}


static void htb_dump_tc(QDISC *qdisc)
{
    DEFAULT_DECL;
    CLASS *class;

    tc_pragma(qdisc->params);
    tc_qdisc_add(qdisc);
    class = htb_get_default_class(qdisc->classes);
    if (class) {
	 tc_add_unum("default", class->number);
    }
    param_get(qdisc->params,qdisc->location);
//    DEFAULT_SET;
    if (prm_r2q.present) tc_add_unum("r2q",prm_r2q.v);
    DEFAULT_SET;
    param_get(qdisc->classes->params,qdisc->classes->location);
    DEFAULT_GET;
    tc_nl();
    htb_dump_classes_tc(qdisc->classes->child,DEFAULT_PASS);
    dump_qdisc(qdisc->classes->qdisc);
    dump_filters(qdisc->filters);
}


/* ----- Dump external ----------------------------------------------------- */


static void htb_dump_classes_ext(FILE *file,CLASS *classes)
{
    const PARAM_DSC *hide[] = {
	&prm_tquantum,
        NULL
    };
    const CLASS *class;

    for (class = classes; class; class = class->next) {
	param_push();
        class_param_load(class);
	ext_dump_class(file,class,hide,NULL);
	htb_dump_classes_ext(file,class->child);
	param_pop();
    }
}


static void htb_dump_ext(FILE *file,QDISC *qdisc)
{
    const PARAM_DSC *hide[] = {
	&prm_tquantum,
        NULL
    };

    qdisc_param_load(qdisc);
    ext_dump_qdisc(file,qdisc,hide,NULL);
    htb_dump_classes_ext(file,qdisc->classes);
}


/* ----- Descriptors ------------------------------------------------------- */


static const PARAM_DSC *htb_qdisc_req[] = {
    NULL
};

static const PARAM_DSC *htb_qdisc_opt[] = {
    &prm_pragma,	/* list */
    &prm_r2q,
    &prm_ceil,
    DEFAULT_LIST	/* NB: no trailing comma */
    NULL
};

static const PARAM_DSC *htb_class_req[] = {
    NULL
};

static const PARAM_DSC *htb_class_opt[] = {
    &prm_default,	/* flag */
    &prm_pragma,	/* list */
    DEFAULT_LIST	/* NB: no trailing comma */
    NULL
};

static PARAM_DEF htb_qdisc = {
    .required = htb_qdisc_req,
    .optional = htb_qdisc_opt,
};

static PARAM_DEF htb_class = {
    .required = htb_class_req,
    .optional = htb_class_opt,
};

QDISC_DSC htb_dsc = {
    .name = "htb",
    .flags = QDISC_HAS_CLASSES | QDISC_HAS_FILTERS | QDISC_CHILD_QDISCS,
    .qdisc_param = &htb_qdisc,
    .class_param = &htb_class,
    .check = &htb_check,
    .default_class = &htb_default_class,
    .dump_tc = &htb_dump_tc,
    .dump_ext = &htb_dump_ext,
};
