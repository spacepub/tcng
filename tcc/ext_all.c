/*
 * ext_all.c - Dump entire configuration at external interface
 *
 * Written 2001,2002 by Werner Almesberger 
 * Copyright 2001,2002 Bivio Networks, Network Robots
 */

/*
 * This is experimental code and will change. Probably a lot.
 *
 * Known problems: @@@
 * - does not support qdiscs shared by multiples classes
 * - does not take into account that only part of the dsmark class may be used
 *   for marking
 * - does not take into account that no dsmark class may exist for marking
 *   (in fact, should look at parameters to decide whether a change is
 *   actually requested)
 * - does not report inner qdiscs changing skb->tc_index
 */


#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "param.h"
#include "tree.h"
#include "filter.h"
#include "qdisc.h"
#include "device.h"
#include "if.h"
#include "iflib.h"
#include "ext.h"
#include "ext_all.h"


int dump_all; /* dump all qdiscs and filters (experimental) */


/* -------------------- skb->tc_index to class mapping --------------------- */


static void print_class_n(FILE *file,const QDISC *qdisc,uint16_t class,
  int *first)
{
    fprintf(file,"%c%u:%u",*first ? ' ' : ',',(unsigned) qdisc->number,
      (unsigned) class);
    *first = 0;
}


static void print_class(FILE *file,const CLASS *class,int *first)
{
    print_class_n(file,class->parent.qdisc,class->number,first);
}


static void gred_path(FILE *file,const QDISC *qdisc,uint16_t index,int first)
{
    const CLASS *class;

    param_get(qdisc->params,qdisc->location);
    for (class = qdisc->classes; class; class = class->next)
	if (class->number == (index & 0xf)) {
	    print_class(file,class,&first);
	    return;
	}
    print_class_n(file,qdisc,prm_default_index.v,&first);
}


static CLASS *find_class(CLASS *classes,uint32_t number)
{
    CLASS *class;

    for (class = classes; class; class = class->next) {
	if (class->number == number) return class;
	if (class->child) {
	    CLASS *found;

	    found = find_class(class->child,number);
	    if (found) return found;
	}
    }
    return NULL;
}


static void find_path(FILE *file,const QDISC *qdisc,uint16_t index,int first);


static uint32_t filter_path(const FILTER *filters,uint16_t index,int first)
{
    const FILTER *filter;

    for (filter = filters; filter; filter = filter->next) {
	const ELEMENT *element;

	if (filter->dsc != &tcindex_dsc)
	    lerrorf(filter->location,
	      "dump_all supports only \"tcindex\", not \"%s\"",
	      filter->dsc->name);
	for (element = filter->elements; element; element = element->next)
	    if (element->police)
		lerror(element->location,
		  "dump_all does not support policing at tcindex");
    }
    /*
     * Note: filter.c ensures that filters are sorted by priority, with the
     * hightest priority (lowest number) first.
     */
    for (filter = filters; filter; filter = filter->next) {
	const ELEMENT *element;
	int shift,mask;
	unsigned value;

	param_load(filter->params,filter->dsc->filter_param,
	  filter->dsc->filter_param,filter->location);
	shift = prm_shift.present ? prm_shift.v : 0;
	mask = prm_mask.present ? prm_mask.v : 0xffff;
	value = (index & mask) >> shift;
	debugf("(index 0x%x & mask 0x%x) >> shift %d -> 0x%x",(unsigned) index,
	  mask,shift,value);
	for (element = filter->elements; element; element = element->next)
	    if ((index & mask) >> shift == element->number)
		return element->parent.class->number;
	if (!prm_fall_through.present || !prm_fall_through.v) continue;
	return value;
    }
    return UNDEF_U32;
}


static void qdisc_filter_path(FILE *file,const QDISC *qdisc,uint16_t index,
  int first)
{
    const FILTER *filters = qdisc->filters;
    const CLASS *class;
    uint32_t class_num;

    if (!filters) {
	if (qdisc->dsc->flags & QDISC_HAS_CLASSES)
	    lerrorf(qdisc->location,
	      "don't know how to select classes of \"%s\"",qdisc->dsc->name);
	return;
    }
    if (prm_present(qdisc->params,&prm_priomap))
	lerrorf(qdisc->location,
	  "dump_all does not support the \"priomap\" parameter");
    class_num = filter_path(filters,index,first);
    if (class_num == UNDEF_U32) {
	if (qdisc->dsc == &cbq_dsc) {
	    lwarnf(filters->location,
	      "no match for value 0x%x in tcindex; defaulting to CBQ root "
	      "class",(unsigned) index);
	    class_num = 0;
	}
	else lerrorf(filters->location,
	      "dump_all found no matching choice for value 0x%x in tcindex",
	      (unsigned) index);
    }
    while (1) {
	uint32_t next_class;

	class = find_class(qdisc->classes,class_num);
	if (!class) break;
	if (prm_present(class->params,&prm_priomap))
	    lerrorf(class->location,
	      "dump_all does not support the \"priomap\" parameter");
	if (!class->filters) break;
	next_class = filter_path(class->filters,index,first);
	if (next_class == UNDEF_U32) break;
	class_num = next_class;
    }
    print_class_n(file,qdisc,class_num,&first);
    if (class && class->qdisc) find_path(file,class->qdisc,index,first);
}


static void find_path(FILE *file,const QDISC *qdisc,uint16_t index,int first)
{
    if (qdisc->dsc == &gred_dsc) gred_path(file,qdisc,index,first);
    else qdisc_filter_path(file,qdisc,index,first);
}


int dump_all_decision(FILE *file,QDISC *root,uint16_t class)
{
    const CLASS *c;

    if (!dump_all) return 0;
    for (c = root->classes; c; c = c->next)
	if (c->qdisc) {
	    int one = 1;

	    fprintf(file," class");
	    print_class_n(file,root,class,&one);
	    find_path(file,c->qdisc,class,0);
	    return 1;
	}
    return 0;
}


/* ------------------- Dumps qdisc and class definitions ------------------- */


static void dump_this_qdisc(FILE *file,QDISC *qdisc);


static void dump_qdiscs_at_classes(FILE *file,const CLASS *classes)
{
    const CLASS *class;

    for (class = classes; class; class = class->next) {
	if (class->qdisc) dump_this_qdisc(file,class->qdisc);
	dump_qdiscs_at_classes(file,class->child);
    }
}


static void dump_this_qdisc(FILE *file,QDISC *qdisc)
{
    dump_qdiscs_at_classes(file,qdisc->classes);
    if (!qdisc->dsc->dump_ext)
	lerrorf(qdisc->location,"don't know how to dump_ext qdisc \"%s\"",
	  qdisc->dsc->name);
    qdisc->dsc->dump_ext(file,qdisc);
}


/* ------------------------------------------------------------------------- */


static void prepare_block(QDISC *root)
{
    if (!root->filters) return;
    dump_if_ext_prepare(root->filters);
}


static void generate_block(FILE *file,QDISC *root)
{
    dump_block(file,root);
    dump_this_qdisc(file,root);
    if (root->filters) dump_if_ext_local(root->filters,file);
}


static void dump_all_callback(FILE *file,void *user)
{
    const DEVICE *device;

    dump_pragma(file);
    for (device = devices; device; device = device->next) {
	if (device->ingress) prepare_block(device->ingress);
	if (device->egress) prepare_block(device->egress);
    }
    dump_if_ext_global(file);
    for (device = devices; device; device = device->next) {
	if (device->ingress) generate_block(file,device->ingress);
	if (device->egress) generate_block(file,device->egress);
    }
    reset_offsets();
}


static void non_root_qdisc(const QDISC *qdisc);


static void non_root_classes(const CLASS *classes)
{
    const CLASS *class;
    const FILTER *filter;

    for (class = classes; class; class = class->next) {
	for (filter = class->filters; filter; filter = filter->next) {
	    if (filter->dsc != &tcindex_dsc)
		lerrorf(filter->location,
		  "dump_all: only filter allowed at classes of non-root qdisc "
		  "is \"tcindex\"");
	}
	non_root_classes(class->child);
	if (class->qdisc) non_root_qdisc(class->qdisc);
    }
}


static void non_root_qdisc(const QDISC *qdisc)
{
    const FILTER *filter;

    for (filter = qdisc->filters; filter; filter = filter->next)
	if (filter->dsc != &tcindex_dsc)
	    lerrorf(filter->location,
	      "dump_all: only filter allowed at non-root qdisc is \"tcindex\"");
    non_root_classes(qdisc->classes);
}


static void no_root_class_filters(const CLASS *classes)
{
    const CLASS *class;

    for (class = classes; class; class = class->next) {
	if (class->filters)
	    lerrorf(class->filters->location,
	      "dump_all: classes of root qdisc may not have filters");
	no_root_class_filters(class->child);
	if (class->qdisc) non_root_qdisc(class->qdisc);
    }
}


static void no_csp_constraints(const CLASS *classes)
{
    const CLASS *class;

    for (class = classes; class; class = class->next) {
	if (class->qdisc &&
	  (class->qdisc->dsc->flags & (QDISC_HAS_CLASSES | QDISC_HAS_FILTERS)))
	    lerror(class->location,
	      "dump_all: if not using ingress or egress/dsmark, only root "
	      "qdisc may have classes");
	no_csp_constraints(class->child);
    }
}


static void check_one_qdisc(QDISC *root)
{
    if (!root) yyerror("dump_all: no root qdisc");
    if (root->filters) {
	if (root->filters->next)
	  lerror(root->filters->next->location,
	    "dump_all: only one filter allowed");
	if (root->filters->dsc != &if_dsc)
	    lerrorf(root->filters->location,
	      "dump_all: filter must be \"if\", not \"%s\"",
	      root->filters->dsc->name);
    }
    else {
	PARENT parent;

	parent.device = root->parent.device;
	parent.qdisc = root;
	parent.class = &class_is_drop;
	    /* parent.class is only used for filter->parent, and this keeps
	       add_if from de-referencing parent.class */
	parent.filter = NULL;
	parent.tunnel = NULL;
	(void) add_if(data_unum(0),parent,NULL,root->location);
    }
    no_root_class_filters(root->classes);
    if (!(root->dsc->flags & QDISC_CLASS_SEL_PATHS))
	no_csp_constraints(root->classes);
}


void dump_all_devices(const char *target)
{
    const DEVICE *device;

    for (device = devices; device; device = device->next) {
	if (device->ingress) check_one_qdisc(device->ingress);
	if (device->egress) check_one_qdisc(device->egress);
    }
    ext_build(target,NULL,NULL,dump_all_callback,NULL);
}


void dump_one_qdisc(QDISC *root,const char *target)
{
    error("not (yet ?) implemented");
}
