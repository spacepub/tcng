/*
 * q_cbq.c - Class-Based Queuing qdisc
 *
 * Written 2001-2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002,2003 Werner Almesberger
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


#define __DEFAULT_PRM(f) f(allot) f(avpkt) f(bandwidth) f(ewma) f(maxburst) \
			 f(minburst) f(mpu) f(prio) f(weight)
#define __DEFAULT_PRM_REQ(f) f(bandwidth) f(maxburst) f(rate) \
  /* the following parameters are theoretically optional. Practically, they \
     aren't */ \
  f(avpkt) f(allot)


static void cbq_check_priomap(const QDISC *qdisc,const CLASS *class,
  DATA_LIST *priomap)
{
    LOCATION loc = class ? class->location : qdisc->location;
    DATA_LIST *walk;
    int n = 0;

    if (!priomap) return;
    for (walk = priomap; walk; walk = walk->next) {
	CLASS *ref = walk->ref->u.class;
	CLASS *scan;

	if (++n > TC_PRIO_MAX)
	    lerror(loc,"too many entries in priomap");
	if (walk->ref->type != dt_class)
		lerrorf(loc,"invalid type in %d%s entry of \"priomap\" "
		  "(expected class, not %s)",n,
		  n == 1 ? "st" : n == 2 ? "nd" : "th",
		  type_name(walk->ref->type));
	if (ref->parent.qdisc != qdisc) /* can this ever happen ? */
	    lerrorf(loc,"class %x:%x in priomap belongs to different qdisc",
	      (int) ref->parent.qdisc->number,(int) ref->number);
	if (!class) continue;
	for (scan = ref->parent.class; scan && scan != class;
	  scan = scan->parent.class);
	if (!scan)
	    lerrorf(loc,"class 0x%x in priomap is not child of class 0x%x",
	      (int) ref->number,(int) class->number);
    }
}


static void cbq_check_classes(CLASS **classes,DEFAULT_ARGS,int depth)
{
    CLASS *class;

    if (!*classes) return;
    sort_classes(classes);
    if (depth == TC_CBQ_MAXLEVEL)
	lerrorf((*classes)->location,
	  "can't nest CBQ classes deeper than %d levels",TC_CBQ_MAXLEVEL);
    for (class = *classes; class; class = class->next) {
	DEFAULT_DECL_SAVED;
	DATA_LIST *priomap = NULL;

	param_get(class->params,class->location);
	DEFAULT_GET;
	DEFAULT_CHECK(class->location);
	DEFAULT_SAVE;
	if (prm_minburst.present && prm_minburst.v > prm_maxburst.v)
	    lerrorf(class->location,"\"minburst\" (%lu) > \"maxburst\" (%lu)",
	      (unsigned long) prm_minburst.v,(unsigned long) prm_maxburst.v);
	if (prm_prio.present &&
	  (prm_prio.v == 0 || prm_prio.v > TC_CBQ_MAXPRIO))
	    lerrorf(class->location,"\"prio\" (%lu) > %d",
	      (unsigned long) prm_prio.v,TC_CBQ_MAXPRIO);
	if (!prm_rate.v) lerror(class->location,"rate must be non-zero");
	if (prm_rate.v > prm_bandwidth.v)
	lerrorf(class->location,"\"rate\" (%lu) > \"bandwidth\" (%lu)",
	  (unsigned long) prm_rate.v,(unsigned long) prm_bandwidth.v);
	if (prm_priomap.present)
	    priomap = prm_data(class->params,&prm_priomap).u.list;
	cbq_check_classes(&class->child,DEFAULT_PASS_SAVED,depth+1);
	cbq_check_priomap(class->parent.qdisc,class,priomap);
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
    root->params = param_make(&prm_rate,prm_data(qdisc->params,&prm_bandwidth));
}


static void check_root_class(QDISC *qdisc)
{
    if (!qdisc->classes || qdisc->classes->next)
	lerror(qdisc->classes ? qdisc->classes->location :
	  qdisc->classes->next->location,
	  "CBQ must have exactly one root class");
    if (qdisc->classes->number) /* hard to trigger :-) */
	lerrorf(qdisc->classes->location,
	  "CBQ root class must have number 0, not %lu\n",
	  (unsigned long) qdisc->classes->number);
    if (qdisc->filters && qdisc->classes->filters)
	lerror(qdisc->filters->location,"please specify filters either at CBQ "
	  "qdisc or at root class");
    if (qdisc->classes->filters) {
	qdisc->filters = qdisc->classes->filters;
	qdisc->classes->filters = NULL;
    }
    if (prm_data(qdisc->classes->params,&prm_rate).u.fnum / 8.0 !=
      prm_bandwidth.v)
	lerror(qdisc->classes->location,
	  "\"bandwidth\" must equal \"rate\" at CBQ root class");
    if (prm_present(qdisc->params,&prm_priomap) &&
      prm_present(qdisc->classes->params,&prm_priomap))
	lerror(qdisc->classes->location,
	  "\"priomap\" may not be present at both, CBQ qdisc and root class");
}


static void cbq_check(QDISC *qdisc)
{
    DATA_LIST *priomap = NULL;

    curr_id = 1;
    (void) recurse_class(qdisc->classes,assign_class_id,qdisc->classes);
    create_root_class(qdisc);
    param_get(qdisc->params,qdisc->location);
    check_root_class(qdisc); /* must be after param_get */
    if (prm_priomap.present)
	priomap = prm_data(qdisc->params,&prm_priomap).u.list;
    cbq_check_classes(&qdisc->classes,DEFAULT_PASS,0);
    cbq_check_priomap(qdisc,NULL,priomap);
    check_filters(qdisc->filters);
}


/* ----- Default classification -------------------------------------------- */


static void cbq_use_default(DATA *d,DATA dfl,const CLASS *class)
{
    if (!d->op) {
	if (d->type == dt_decision && d->u.decision.result == dr_class &&
	  d->u.decision.class == class)
	    *d = data_clone(dfl);
	return;
    }
    cbq_use_default(&d->op->a,dfl,class);
    cbq_use_default(&d->op->b,dfl,class);
    cbq_use_default(&d->op->c,dfl,class);
}


static void cbq_resolve_priomap(DATA *d,const DATA_LIST *priomap,QDISC *qdisc,
  CLASS *this_class)
{
    CLASS *map[TC_PRIO_MAX];
    const DATA_LIST *walk;
    DATA dfl;
    int i;

    for (i = 0; i < TC_PRIO_MAX; i++) map[i] = NULL;
    i = 0;
    for (walk = priomap; walk; walk = walk->next)
	map[i++] = walk->ref->u.class;
    for (i = 0; i < TC_PRIO_MAX; i++)
	if (!map[i]) map[i] = map[TC_PRIO_BESTEFFORT];
    dfl = data_decision(dr_class,this_class);
    for (i = TC_PRIO_MAX-1; i >= 0; i--)
	if (map[i])
	    dfl = op_binary(&op_logical_or,
	      op_binary(&op_logical_and,
		op_binary(&op_eq,
		  op_binary(&op_and,
		    op_ternary(&op_access,
		      data_field_root(field_root(0)),data_unum(1),
		      data_unum(8)),
		    data_unum(0x1e)),
		  data_unum(i*2)),
		data_decision(dr_class,map[i])),
              dfl);
    cbq_use_default(d,dfl,this_class);
    data_destroy(dfl);
}


static void cbq_push_defaults(DATA *d,PARAM *params,QDISC *qdisc,
  CLASS *classes,CLASS *this_class)
{
    CLASS *class;

    if (prm_present(params,&prm_priomap))
	cbq_resolve_priomap(d,prm_data(params,&prm_priomap).u.list,qdisc,
	  this_class);
    for (class = classes; class; class = class->next)
	cbq_push_defaults(d,class->params,qdisc,class->child,class);
}


static void cbq_default_class(DATA *d,QDISC *qdisc)
{
    /*
     * This is the only place where the root class becomes visible.
     * This may confuse external programs quite badly.
     */
    CLASS *root_class;

    root_class = require_class(qdisc,0,0);
    append_default(d,data_decision(dr_class,root_class));
    cbq_push_defaults(d,qdisc->params,qdisc,qdisc->classes,root_class);
}


/* ----- Dump tc ----------------------------------------------------------- */


/*
 * Name confusion ? Well, we call the concept "priomap", but in the case of
 * CBQ, tc and the kernel call it "defmap". So the tc-specific functions use
 * "defmap", while the output-independent ones use "priomap".
 */

static void cbq_dump_defmaps_1_tc(PARAM *params,LOCATION loc,
  const CLASS *class,uint32_t major,uint32_t minor)
{
    DATA_LIST *walk;
    uint32_t mask = 0;
    int n;

    param_get(params,loc);
    if (!prm_priomap.present) return;
    n = 0;
    for (walk = prm_data(params,&prm_priomap).u.list; walk; walk = walk->next) {
	if (walk->ref->u.class == class) mask |= 1 << n;
	n++;
    }
    if (!mask) return;
    tc_class_change(class);
    tc_more(" split %x:%x",(int) major,(int) minor);
    tc_more(" defmap %lx/%lx",(unsigned long) mask,(unsigned long) mask);
    tc_nl();
}


static void cbq_dump_defmaps_tc(const CLASS *class)
{
    const CLASS *parent;

    cbq_dump_defmaps_1_tc(class->parent.qdisc->params,
      class->parent.qdisc->location,class,class->parent.qdisc->number,0);
    for (parent = class->parent.class; parent; parent = parent->parent.class)
	cbq_dump_defmaps_1_tc(parent->params,parent->location,class,
	  class->parent.qdisc->number,parent->number);
}


static void cbq_dump_classes_tc(CLASS *classes,DEFAULT_ARGS)
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
	tc_add_rate("bandwidth",prm_bandwidth.v);
	tc_add_rate("rate",prm_rate.v);
	if (prm_allot.present) tc_add_size("allot",prm_allot.v);
	if (prm_avpkt.present) tc_add_size("avpkt",prm_avpkt.v);
	if (prm_ewma.present) tc_add_unum("ewma",prm_ewma.v);
	if (prm_maxburst.present) tc_add_psize("maxburst",prm_maxburst.v);
	if (prm_minburst.present) tc_add_psize("minburst",prm_minburst.v);
	if (prm_mpu.present) tc_add_size("mpu",prm_mpu.v);
	if (prm_prio.present) tc_add_unum("prio",prm_prio.v);
/*
 * @@@ AAAARRRRRRRRGGG!!!!
 * Hmm ... seems that I was unhappy about something here. Any suggestions what
 * this may have been ? :-)
 */
	if (prm_weight.present) tc_add_size("weight",prm_weight.v);
	if (prm_bounded.present) tc_more(" bounded");
	if (prm_isolated.present) tc_more(" isolated");
	tc_nl();
	cbq_dump_defmaps_tc(class);
	dump_qdisc(class->qdisc);
	cbq_dump_classes_tc(class->child,DEFAULT_PASS_SAVED);
	dump_filters(class->filters);
    }
}


static void cbq_dump_tc(QDISC *qdisc)
{
    DEFAULT_DECL;

    tc_pragma(qdisc->params);
    tc_qdisc_add(qdisc);
    param_get(qdisc->params,qdisc->location);
    DEFAULT_SET;
    tc_add_rate("bandwidth",prm_bandwidth.v);
    param_get(qdisc->classes->params,qdisc->classes->location);
    DEFAULT_GET;
    if (prm_avpkt.present) tc_add_size("avpkt",prm_avpkt.v);
    if (prm_ewma.present) tc_add_unum("ewma",prm_ewma.v);
    if (prm_mpu.present) tc_add_size("mpu",prm_mpu.v);
    tc_nl();
    cbq_dump_classes_tc(qdisc->classes->child,DEFAULT_PASS);
    dump_qdisc(qdisc->classes->qdisc);
    dump_filters(qdisc->filters);
}


/* ----- Sanitize classid for "tc" target ---------------------------------- */


/*
 * If selecting class 0, sch_cbq would merrily loop forever in classification.
 * Therefore, we need to force the classifier to pick an invalid class ID
 * instead. cls_u32 ignores NULL returned from bind_tcf, so this just happens
 * to have the desired effect ...
 */


uint32_t cbq_sanitize_classid_tc(const CLASS *class,int accept_invalid)
{
    static uint32_t curr_id = 0xffff;
    int i;

    if (class->number) return class->number;
    if (!accept_invalid)
	lerror(class->location,"cannot substitute class ID 0 with this "
	  "filter");
    for (i = 0; i <= 0xffff; i++) {
	if (!recurse_class(class->parent.qdisc->classes,count_class_ids,
	  &curr_id)) return curr_id;
	curr_id--;
    }
    lerror(class->location,"could not find unused class ID to substitute "
      "class 0");
}


/* ----- Dump external ----------------------------------------------------- */


static void cbq_dump_classes_ext(FILE *file,CLASS *classes)
{
    const PARAM_DSC *hide[] = {
        &prm_priomap,
        NULL
    };
    const CLASS *class;

    for (class = classes; class; class = class->next) {
	param_push();
        class_param_load(class);
	ext_dump_class(file,class,hide,NULL);
	cbq_dump_classes_ext(file,class->child);
	param_pop();
    }
}


static void cbq_dump_ext(FILE *file,QDISC *qdisc)
{
    const PARAM_DSC *hide[] = {
        &prm_priomap, /* see note below p*/
        NULL
    };
    /*
     * Note concerning prm_priomap: param_print does not understand that
     * priomap is not defaulted from qdisc to class, so it already hides
     * priomap, although for the wrong reason. Including it here anyway
     * ensures that priomap will remain hidden when param_print is fixed.
     */

    qdisc_param_load(qdisc);
    ext_dump_qdisc(file,qdisc,hide,NULL);
    cbq_dump_classes_ext(file,qdisc->classes);
}


/* ----- Descriptors ------------------------------------------------------- */


static const PARAM_DSC *cbq_qdisc_req[] = {
    &prm_bandwidth,	/* rate */
    NULL
};

static const PARAM_DSC *cbq_qdisc_opt[] = {
    &prm_pragma,	/* list */
    &prm_priomap,	/* list of classes */
    DEFAULT_LIST	/* NB: no trailing comma */
    NULL
};

static const PARAM_DSC *cbq_class_req[] = {
    &prm_rate,		/* rate */
    NULL
};

static const PARAM_DSC *cbq_class_opt[] = {
    &prm_bounded,	/* flag */
    &prm_isolated,	/* flag */
    &prm_pragma,	/* list */
    &prm_priomap,	/* list of class */
    DEFAULT_LIST	/* NB: no trailing comma */
    NULL
};

static PARAM_DEF cbq_qdisc = {
    .required = cbq_qdisc_req,
    .optional = cbq_qdisc_opt,
};

static PARAM_DEF cbq_class = {
    .required = cbq_class_req,
    .optional = cbq_class_opt,
};

QDISC_DSC cbq_dsc = {
    .name = "cbq",
    .flags = QDISC_HAS_CLASSES | QDISC_HAS_FILTERS | QDISC_CHILD_QDISCS,
    .qdisc_param = &cbq_qdisc,
    .class_param = &cbq_class,
    .check = &cbq_check,
    .default_class = &cbq_default_class,
    .dump_tc = &cbq_dump_tc,
    .dump_ext = &cbq_dump_ext,
};
