/* @@@ CHECK:
   - range of user-assigned ids (also filter)
   - their uniqueness
  (done for qdiscs and filter, classes missing)
*/

/*
 * qdisc.c - Qdisc handling
 *
 * Written 2001-2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots
 * Copyright 2003 Werner Almesberger
 */


#include <stddef.h>
#include <stdlib.h>

#include "config.h" /* for ext_all hack @@@ */
#include "util.h"
#include "error.h"
#include "param.h"
#include "tree.h"
#include "op.h"
#include "device.h"
#include "tc.h"
#include "ext.h"
#include "filter.h"
#include "qdisc_common.h"
#include "qdisc.h"
#include "target.h" /* for ext_all hack @@@ */
#include "ext_all.h" /* for ext_all hack @@@ */


/* ----- Helper functions for structure checking --------------------------- */


void check_childless(const CLASS *class)
{
    if (class->child)
	lerrorf(class->child->location,"%s does not have nested classes",
	  class->parent.qdisc->dsc->name);
    if (class->filters)
	lerrorf(class->filters->location,"%s classes don't have local filters",
	  class->parent.qdisc->dsc->name);
}


static void check_no_policing_filters(const FILTER *filter)
{
    const ELEMENT *element;

    if (!filter) return;
    for (element = filter->elements; element; element = element->next)
	if (element->police)
	    yyerrorf("qdisc \"%s\" does not support policing",
	      filter->parent.qdisc->dsc->name);
}


static void check_no_policing_classes(const CLASS *classes)
{
    const CLASS *class;

    for (class = classes; class; class = class->next) {
	check_no_policing_filters(class->filters);
	check_no_policing_classes(class->child);
    }
}


void check_no_policing(const QDISC *qdisc)
{
    check_no_policing_filters(qdisc->filters);
    check_no_policing_classes(qdisc->classes);
}


/* ----- Assignment of unique IDs ------------------------------------------ */


struct recurse_tmp {
    int (*fn)(QDISC *qdisc,void *user);
    void *user;
};

uint32_t curr_id;


int recurse_class(CLASS *class,int (*fn)(CLASS *class,void *user),void *user)
{
    int count = 0;

    while (class) {
	if (fn(class,user)) count++;
	if (class->child) count += recurse_class(class->child,fn,user);
	class = class->next;
    }
    return count;
}


static int recurse_qdisc(QDISC *qdisc,int (*fn)(QDISC *qdisc,void *user),
  void *user);


static int class_to_qdisc(CLASS *class,void *user)
{
    struct recurse_tmp *tmp = user;

    if (!class->qdisc) return 0;
    return recurse_qdisc(class->qdisc,tmp->fn,tmp->user);
}


static int recurse_qdisc(QDISC *qdisc,int (*fn)(QDISC *qdisc,void *user),
  void *user)
{
    struct recurse_tmp tmp;

    if (!qdisc) return 0;
    tmp.fn = fn;
    tmp.user = user;
    return fn(qdisc,user)+recurse_class(qdisc->classes,class_to_qdisc,&tmp);
}


int count_class_ids(CLASS *class,void *user)
{
    return class->number == *(uint32_t *) user;
}


int assign_class_id(CLASS *class,void *user)
{
    CLASS *root = user;

    if (class->number != UNDEF_U32) {
	if (recurse_class(root,count_class_ids,&class->number) > 1)
	    lerrorf(class->location,"duplicate class ID 0x%x",class->number);
    }
    else {
	while (recurse_class(root,count_class_ids,&curr_id)) curr_id++;
	class->number = curr_id;
    }
    if (class->number & ~0xffff)
	lerrorf(class->location,"invalid class disc ID 0x%x",class->number);
    return 0;
}


static int count_qdisc_ids(QDISC *qdisc,void *user)
{
    return qdisc->number == *(uint32_t *) user;
}


static int assign_qdisc_id(QDISC *qdisc,void *user)
{
    QDISC *root = user;

    if (qdisc->number != UNDEF_U32) {
	if (recurse_qdisc(root,count_qdisc_ids,&qdisc->number) > 1)
	    lerrorf(qdisc->location, "duplicate qdisc ID 0x%x",qdisc->number);
    }
    else {
	while (recurse_qdisc(root,count_qdisc_ids,&curr_id)) curr_id++;
	qdisc->number = curr_id;
    }
    if (!qdisc->number || (qdisc->number & ~0xffff))
	lerrorf(qdisc->location, "invalid qdisc ID 0x%x",qdisc->number);
    return 0;
}


void assign_qdisc_ids(QDISC *root)
{
    curr_id = 1;
    (void) recurse_qdisc(root,assign_qdisc_id,root);
}


/* ----- Add FIFO qdiscs where default qdisc is used ----------------------- */


static QDISC *make_fifo(CLASS *parent)
{
    QDISC *qdisc;

    qdisc = alloc_t(QDISC);
    qdisc->parent = parent->parent;
    qdisc->parent.class = parent;
    qdisc->dsc = &fifo_dsc;
    qdisc->number = UNDEF_U32;
    qdisc->location = parent->location;
    qdisc->params = NULL;
    qdisc->classes = NULL;
    qdisc->filters = NULL;
    qdisc->if_expr = data_none();
    return qdisc;
}


static CLASS *make_pseudo_class(QDISC *parent)
{
    CLASS *class;

    class = alloc_t(CLASS);
    class->parent.device = parent->parent.device;
    class->parent.qdisc = parent;
    class->parent.class = NULL;
    class->parent.filter = NULL;
    class->parent.tunnel = NULL;
    class->number = UNDEF_U32;
    class->location = parent->location;
    class->params = NULL;
    class->qdisc = NULL;
    class->filters = NULL;
    class->child = NULL;
    class->next = NULL;
    class->implicit = 1;
    return class;
}


static int adf_classes_scan(CLASS *class)
{
    int qdiscs = 0;

    while (class) {
	if (class->qdisc) {
	    qdiscs++;
	    add_default_fifos(class->qdisc);
	}
	qdiscs += adf_classes_scan(class->child);
	class = class->next;
    }
    return qdiscs;
}


static void adf_classes_add_everywhere(CLASS *class)
{
    while (class) {
	if (!class->qdisc) class->qdisc = make_fifo(class);
	adf_classes_add_everywhere(class->child);
	class = class->next;
    }
}


static void adf_classes_add_to_unnumbered(QDISC *qdisc)
{
    CLASS **class;

    for (class = &qdisc->classes; *class &&
      ((*class)->number != UNDEF_U32 ||
      prm_present((*class)->params,&prm_class_sel_path));
      class = &(*class)->next);
    if (!*class) *class = make_pseudo_class(qdisc);
    (*class)->qdisc = make_fifo(*class);
}


CLASS *get_any_class(QDISC *qdisc)
{
    if (!qdisc->classes) qdisc->classes = make_pseudo_class(qdisc);
    return qdisc->classes;
}


void add_default_fifos(QDISC *qdisc)
{
    if (!(qdisc->dsc->flags & QDISC_CHILD_QDISCS)) return;
    (void) get_any_class(qdisc);
    if (!qdisc->classes) qdisc->classes = make_pseudo_class(qdisc);
    if (qdisc->dsc->flags & QDISC_SHARED_QDISC) {
	int qdiscs;

	qdiscs = adf_classes_scan(qdisc->classes);
	/* if qdiscs > 1, this will be caught in the qdisc's check function */
	if (!qdiscs) adf_classes_add_to_unnumbered(qdisc);
    }
    else {
	(void) adf_classes_scan(qdisc->classes);
	adf_classes_add_everywhere(qdisc->classes);
    }
}


/* ----- Common RED functions ---------------------------------------------- */


void red_parameters(PARAM *params,uint32_t bandwidth)
{
    tc_add_size("limit",prm_limit.v);
    tc_add_size("min",prm_min.v);
    tc_add_size("max",prm_max.v);
#if 0 /* @@@ more correct if avpkt is no integer (e.g. if calaculated)  */
    tc_more(" burst %d",(int) (prm_data(qdisc->params,&prm_burst).u.fnum/
      prm_data(qdisc->params,&prm_avpkt).u.fnum));
#endif
    tc_add_unum("burst",prm_burst.v/prm_avpkt.v);
    tc_add_size("avpkt",prm_avpkt.v);
    tc_add_rate("bandwidth",bandwidth);
    if (prm_probability.present)
	tc_add_fnum("probability",prm_data(params,&prm_probability));
}


/* ----- *_default_class helper function ----------------------------------- */


CLASS *require_class(QDISC *qdisc,uint32_t number,int implicit)
{
    CLASS *class;

    for (class = qdisc->classes; class; class = class->next)
	if (class->number == number) break;
    if (!class) {
	class = make_class(qdisc,NULL,implicit);
	class->number = number;
    }
    return class;
}


void append_default(DATA *d,DATA dflt)
{
    *d = op_binary(&op_logical_or,*d,dflt);
}


/* ----- Shared qdisc for dsmark and ingress ------------------------------- */


void merge_shared(CLASS **classes)
{
    CLASS **default_zero = NULL,*explicit_zero = NULL;
    CLASS **class,*tmp;
    const FILTER *f;
    const ELEMENT *e;

    if (warn_explicit)
	for (class = classes; *class; class = &(*class)->next)
	    if ((*class)->qdisc && !(*class)->implicit)
		lwarn((*class)->location,"class for shared qdisc should be "
		  "implicit");
    /*
     * big exception: if this is the only class, auto-assign class number,
     * for consistency with "dsmark { fifo; }" and such.
     */
    if (*classes && !(*classes)->next && (*classes)->number == UNDEF_U32) {
	(*classes)->number = 0;
	return;
    }
    for (class = classes; *class; class = &(*class)->next) {
	if ((*class)->qdisc && (*class)->number == UNDEF_U32)
	    default_zero = class;
	if (!(*class)->number) explicit_zero = *class;
    }
    if (!default_zero) return;
    if (!explicit_zero) {
	(*default_zero)->number = 0;
	return;
    }
    explicit_zero->qdisc = (*default_zero)->qdisc;
    explicit_zero->qdisc->parent.class = explicit_zero;
    tmp = *default_zero;
    *default_zero = tmp->next;
    /*
     * The no longer used class may still be referenced by a variable, and its
     * variable scope still exists, and will be included in variable use
     * output. Therefore, we can't just "free" it. We refuse all direct
     * references to it, though.
     */
    if (var_any_class_ref(tmp))
	lerror(tmp->location,"class has been merged and cannot be "
	  "referenced by a variable");
    for (f = tmp->parent.qdisc->filters; f; f = f->next)
	if (f->dsc == &if_dsc)
	    for (e = f->elements; e; e = e->next)
		if (e->parent.class == tmp)
		    lerror(tmp->location,
		      "class has been merged and cannot be selected");
}


/* ----- Sort classes ------------------------------------------------------ */


/*
 * Let's hope there aren't too many of them, so bubble sort will do nicely.
 */


void sort_classes(CLASS **classes)
{
    CLASS **a,**b,**next_b;

    for (a = classes; *a; a = &(*a)->next)
	for (b = &(*a)->next; *b; b = next_b) {
	    next_b = &(*b)->next;
	    if ((*a)->number > (*b)->number) {
		CLASS *tmp;

		tmp = *b;
		*b = *next_b;
		*next_b = *a;
		*a = tmp;
	    }
	    else if ((*a)->number == (*b)->number)
		    lerrorf((*a)->location,"duplicate class number %u",
		      (unsigned) (*a)->number);
		/* since we already compare all classes with each other, we
		   might as well do the duplication test for those qdiscs
		   that don't do it elsewhere, e.g. dsmark */
	}
}


/* ----- Parameter management ---------------------------------------------- */


void class_param_load(const CLASS *class)
{
    const QDISC *qdisc = class->parent.qdisc;

    param_load(class->params,qdisc->dsc->qdisc_param,qdisc->dsc->class_param,
      class->location);
}


void qdisc_param_load(const QDISC *qdisc)
{
    param_load(qdisc->params,qdisc->dsc->qdisc_param,qdisc->dsc->qdisc_param,
      qdisc->location);
}


/* ----- Generic dumping at external interface ----------------------------- */


void generic_dump_ext(FILE *file,QDISC *qdisc)
{
    const CLASS *class;

    qdisc_param_load(qdisc);
    ext_dump_qdisc(file,qdisc,NULL,NULL);
    for (class = qdisc->classes; class; class = class->next) {
	param_push();
	class_param_load(class);
	ext_dump_class(file,class,NULL,NULL);
	param_pop();
    }
}


/* ------------------------------------------------------------------------- */


static void add_class(CLASS *class)
{
    CLASS **anchor;

    if (!class->parent.qdisc) yyerror("class without qdisc");
    for (anchor = class->parent.class ? &class->parent.class->child :
      &class->parent.qdisc->classes; *anchor; anchor = &(*anchor)->next);
    *anchor = class;
}


CLASS *make_class(QDISC *qdisc,CLASS *parent_class,int implicit)
{
    CLASS *class;

    if (!(qdisc->dsc->flags & QDISC_HAS_CLASSES) &&
      (!implicit || !(qdisc->dsc->flags & QDISC_CHILD_QDISCS)))
	yyerrorf("qdisc \"%s\" has no classes",qdisc->dsc->name);
    class = alloc_t(CLASS);
    class->parent.device = qdisc->parent.device;
    class->parent.qdisc = qdisc;
    class->parent.class = parent_class;
    class->parent.filter = NULL; /* n/a */
    class->parent.tunnel = NULL; /* n/a */
    add_class(class);
    class->number = UNDEF_U32;
    class->location = qdisc->location;
    class->params = NULL;
    class->qdisc = NULL;
    class->filters = NULL;
    class->child = NULL;
    class->next = NULL;
    class->implicit = implicit;
    return class;
}


void check_qdisc(QDISC *qdisc)
{
    if (qdisc) qdisc->dsc->check(qdisc);
}


void dump_qdisc(QDISC *qdisc)
{
#if 0
    if (ext_targets && dump_all) { /* compatibility hack - REMOVE SOON @@@ */
	dump_all_qdisc(qdisc,ext_targets->name);
	return;
    }
#endif
    if (qdisc) qdisc->dsc->dump_tc(qdisc);
}
