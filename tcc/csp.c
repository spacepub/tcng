/*
 * csp.c - Class Selection Paths
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001,2002 Bivio Networks, Network Robots
 */


#include <stddef.h>
#include <stdlib.h>

#include "util.h"
#include "location.h"
#include "error.h"
#include "param.h"
#include "tree.h"
#include "filter.h"
#include "qdisc.h"
#include "qdisc_common.h"


/*
 * Known bugs and problems (of class selection paths):
 * - implementation is somewhat inefficient
 * - struct sel_ref and struct sel_path should be one structure
 * - does not detect cases where no default choice is needed (such is life ...)
 * - some error messages may appear a little obscure
 */

/*
 * Due to a bug in the dsmark API, we can only support 15 out of 16 possible
 * bits.
 */

#define CSP_INDEX_BITS 15


typedef struct _csp_sel_path_elem {
    CLASS *class;
    struct _csp_sel_path_elem *next;
} CSP_SEL_PATH_ELEM;

typedef struct _csp_sel_path {
    CSP_SEL_PATH_ELEM *path;
    int equals;                 /* equal paths, >= 1 */
    int index;                  /* nth such path, >= 0 */
    struct _csp_sel_path *next;
} CSP_SEL_PATH;

typedef struct _csp_sel_ref {
    uint32_t *number;
    const DATA_LIST *list;
    struct _csp_sel_ref *next;
} CSP_SEL_REF;

typedef struct _csp_sel_class {
    CLASS *class;
    int number;
    struct _csp_sel_class *next;
} CSP_SEL_CLASS;

typedef struct _csp_sel_qdisc {
    QDISC *qdisc;
    CSP_SEL_CLASS *classes;
    int num_classes;
    int shift,bits;
    int curr_class;
    struct _csp_sel_qdisc *next;
} CSP_SEL_QDISC;

typedef struct _csp_ctx {
    CSP_SEL_PATH *paths;
    CSP_SEL_REF *refs;
    CSP_SEL_QDISC *qdiscs;
} CSP_CTX;


typedef struct _csp_sel_prev {
    CLASS *class;
    const struct _csp_sel_prev *prev;
} CSP_SEL_PREV;


/* ----- Initialize the context -------------------------------------------- */


static void begin_selection_paths(CSP_CTX *ctx)
{
    debugf("begin_selection_paths");
    ctx->paths = NULL;
    ctx->refs = NULL;
    ctx->qdiscs = NULL;
}


/* ----- Add class selection path and assign class numbers ----------------- */


static void register_sel_class(CSP_CTX *ctx,QDISC *qdisc,CLASS *class)
{
    CSP_SEL_QDISC *q;
    CSP_SEL_CLASS *cl;

    for (q = ctx->qdiscs; q; q = q->next)
	if (q->qdisc == qdisc) break;
    if (!q) {
	q = alloc_t(CSP_SEL_QDISC);
	q->qdisc = qdisc;
	q->classes = NULL;
	q->num_classes = 0;
	q->next = ctx->qdiscs;
	ctx->qdiscs = q;
    }
    for (cl = q->classes; cl; cl = cl->next)
	if (cl->class == class) return;
    cl = alloc_t(CSP_SEL_CLASS);
    cl->class = class;
    if (qdisc->dsc == &gred_dsc) cl->number = class->number;
    else cl->number = ++q->num_classes;
    cl->next = q->classes;
    q->classes = cl;
}


static void register_sel_path(CSP_CTX *ctx,const CSP_SEL_PREV *last,
  LOCATION loc)
{
    CSP_SEL_PATH_ELEM *path = NULL;
    CSP_SEL_PATH *walk,*new;
    const CSP_SEL_PREV *prev;
    int n;

    for (prev = last; prev; prev = prev->prev) {
	QDISC *qdisc = prev->class->parent.qdisc;
	CSP_SEL_PATH_ELEM *new;

	if (qdisc->filters)
	    lerror(qdisc->location,
	      "qdisc must not have filters when using class selection path");
	register_sel_class(ctx,qdisc,prev->class);
	new = alloc_t(CSP_SEL_PATH_ELEM);
	new->class = prev->class;
	new->next = path;
	path = new;
    }
    n = 0;
    for (walk = ctx->paths; walk; walk = walk->next) {
	const CSP_SEL_PATH_ELEM *a,*b;

	a = walk->path;
	b = path;
	while (a && b && a->class == b->class) {
	    a = a->next;
	    b = b->next;
	}
	if (!a && !b) {
	    n++;
	    walk->equals++;
	}
    }
//    if (walk) lerror(loc,"duplicate class selection path");
    new = alloc_t(CSP_SEL_PATH);
    new->path = path;
    new->equals = n+1;
    new->index = n;
    new->next = ctx->paths;
    ctx->paths = new;
}


static void find_last_class(LOCATION loc,CSP_CTX *ctx,
  const DATA_LIST *list,CLASS *classes,const CLASS **last_class,int left,
  const CSP_SEL_PREV *prev)
{
    CLASS *class;

    for (class = classes; class; class = class->next) {
	CSP_SEL_PREV this;
	const DATA_LIST *scan;

	this.class = class;
	this.prev = prev;
	for (scan = list; scan; scan = scan->next)
	    if (scan->ref->u.class == class) break;
	if (!scan) {
	    if (class->qdisc)
		find_last_class(loc,ctx,list,class->qdisc->classes,last_class,
		  left,&this);
	}
	else {
	    if (left == 1) {
		if (*last_class && *last_class != class)
		    lerror(loc,"class selection path is not unique");
		*last_class = class;
		register_sel_path(ctx,&this,loc);
		continue;
	    }
	    if (!class->qdisc)
		lerror(loc,"unfinished class selection path reaches leaf");
	    find_last_class(loc,ctx,list,class->qdisc->classes,last_class,
	      left-1,&this);
	}
	if (class->child)
	    find_last_class(loc,ctx,list,class->child,last_class,left,prev);
    }
}


static void register_selection_path(CSP_CTX *ctx,uint32_t *number,
  const DATA_LIST *list,QDISC *inner_qdisc,LOCATION loc)
{
    const DATA_LIST *walk;
    const CLASS *last_class = NULL;
    CSP_SEL_REF *ref;
    int classes = 0;

    for (walk = list; walk; walk = walk->next) {
	const DATA_LIST *scan;
	CLASS *class;

	if (walk->ref->type != dt_class)
	    lerrorf(loc,"invalid type in class selection path "
	      "(expected class, not %s)",type_name(walk->ref->type));
	class = walk->ref->u.class;
	for (scan = list; scan != walk; scan = scan->next)
	    if (scan->ref->u.class == class) {
		lwarn(loc,"duplicate class in class selection path");
		break;
	    }
	if (scan == walk) classes++;
    }
    if (list && !inner_qdisc)
	lerror(loc,"without an inner qdisc, there is scarcely anything to "
	  "select");
    if (!list) register_sel_path(ctx,NULL,loc);
    else {
	find_last_class(loc,ctx,list,inner_qdisc->classes,&last_class,classes,
	  NULL);
	if (!last_class) lerror(loc,"cannot resolve class selection path");
    }
    ref = alloc_t(CSP_SEL_REF);
    ref->number = number;
    ref->list = list;
    ref->next = ctx->refs;
    ctx->refs = ref;
}


/* ----- Generate numbers and filter --------------------------------------- */


static int selection_bits(CSP_CTX *ctx,const QDISC *qdisc);


static int max_bits(CSP_CTX *ctx,const CLASS *classes)
{
    const CLASS *class;
    int max = 0;

    for (class = classes; class; class = class->next) {
	int bits;

	if (class->qdisc) {
	    bits = selection_bits(ctx,class->qdisc);
	    if (bits > max) max = bits;
	}
	if (class->child) {
	    bits = max_bits(ctx,class->child);
	    if (bits > max) max = bits;
	}
    }
    return max;
}


/*
 * Bit assignment rules: we need enough bits for all classes that are selected
 * in a qdisc. Furthermore, one value (0) is reserved for not selecting any
 * class, whether this case may occur or not.
 */


static int selection_bits(CSP_CTX *ctx,const QDISC *qdisc)
{
    CSP_SEL_QDISC *q;

    for (q = ctx->qdiscs; q && q->qdisc != qdisc; q = q->next);
    if (!q) return 0; /* qdisc not used anywhere */
    if (qdisc->dsc == &gred_dsc) q->bits = 4;
    else for (q->bits = 0; 1 << q->bits <= q->num_classes; q->bits++);
    q->shift = max_bits(ctx,qdisc->classes);
    return q->shift+q->bits;
}


static FILTER *sel_add_tcindex(QDISC *qdisc,int shift,int mask_bits)
{
    FILTER **f;
    int n = 0;

    for (f = &qdisc->filters; *f; f = &(*f)->next) n++;
    *f = alloc_t(FILTER);
    (*f)->parent.device = qdisc->parent.device;
    (*f)->parent.qdisc = qdisc;
    (*f)->parent.class = NULL;
    (*f)->parent.filter = NULL;
    (*f)->parent.tunnel = NULL;
    (*f)->number = n+1;
    debugf("sel_add_tcindex: qdisc %u shift %d mask_bits %d (filter %d)",
      (unsigned) qdisc->number,shift,mask_bits,n+1);
    (*f)->location = qdisc->location;
    (*f)->dsc = &tcindex_dsc;
    (*f)->elements = NULL;
    (*f)->params = param_make(&prm_shift,data_unum(shift));
    (*f)->params->next =
      param_make(&prm_mask,data_unum(((1 << mask_bits)-1) << shift));
    (*f)->next = NULL;
    return *f;
}


static void sel_add_element(FILTER *f,CLASS *class,uint32_t n)
{
    ELEMENT *e;
    PARENT parent;

    debugf("sel_add_element: filter %u class %u:%u element %u",
      (unsigned) f->number,(unsigned) class->parent.qdisc->number,
      (unsigned) class->number,(unsigned) n);
    e = alloc_t(ELEMENT);
    parent = f->parent;
    parent.class = class;
    parent.filter = f;
    add_element(e,parent);
    e->number = n;
    e->location = f->location;
    e->police = NULL;
    e->next = NULL;
    e->params = NULL;
}


static void sel_add_hier_filters(CSP_CTX *ctx)
{
    CSP_SEL_QDISC *q;

    for (q = ctx->qdiscs; q; q = q->next) {
	FILTER *f;
	CSP_SEL_CLASS *cl;

	if (q->qdisc->dsc == &gred_dsc) continue;
	f = sel_add_tcindex(q->qdisc,q->shift,q->bits);
	for (cl = q->classes; cl; cl = cl->next)
	    sel_add_element(f,cl->class,cl->number);
    }
}


static void sel_set_class_numbers(CSP_CTX *ctx,int top_shift)
{
    CSP_SEL_REF *ref = ctx->refs;
    CSP_SEL_PATH *path = ctx->paths;

    while (ref) {
	CSP_SEL_PATH_ELEM *elem;
	uint32_t number;

	number = path->index << top_shift;
	for (elem = path->path; elem; elem = elem->next) {
	    const CSP_SEL_QDISC *q;
	    const CSP_SEL_CLASS *cl;

	    for (q = ctx->qdiscs; q->qdisc != elem->class->parent.qdisc;
	      q = q->next);
	    for (cl = q->classes; cl->class != elem->class; cl = cl->next);
	    number |= cl->number << q->shift;
	}
	*ref->number = number;
	debugf("sel_set_class_numbers(%u)",(unsigned) number);
	ref = ref->next;
	path = path->next;
    }
}


static void set_number_refs(CSP_CTX *ctx)
{
    CSP_SEL_REF *ref;
    int n = 0;

    for (ref = ctx->refs; ref; ref = ref->next)
	*ref->number = ++n;
    if (n >= 1 << CSP_INDEX_BITS)
	errorf("number of class selection paths exceeds %u",
	  1 << CSP_INDEX_BITS);
}


static void sel_add_seq_filters(CSP_CTX *ctx)
{
    const CSP_SEL_QDISC *q;
    CSP_SEL_PATH *path = ctx->paths;
    int n = 0;

    for (q = ctx->qdiscs; q; q = q->next)
	if (q->qdisc->dsc != &gred_dsc)
	    (void) sel_add_tcindex(q->qdisc,0,CSP_INDEX_BITS);
    for (path = ctx->paths; path; path = path->next) {
	CSP_SEL_PATH_ELEM *elem;

	n++;
	for (elem = path->path; elem; elem = elem->next) {
	    const QDISC *qdisc = elem->class->parent.qdisc;
	    const CSP_SEL_CLASS *cl;

	    if (qdisc->dsc == &gred_dsc) continue;
	    for (q = ctx->qdiscs; q->qdisc != qdisc; q = q->next);
	    for (cl = q->classes; cl->class != elem->class; cl = cl->next);
	    sel_add_element(q->qdisc->filters,cl->class,n);
	}
    }
}


static void sel_destroy(CSP_CTX *ctx)
{
    while (ctx->paths) {
	CSP_SEL_PATH *next = ctx->paths->next;

	while (ctx->paths->path) {
	    CSP_SEL_PATH_ELEM *next = ctx->paths->path->next;

	    free(ctx->paths->path);
	    ctx->paths->path = next;
	}
	free(ctx->paths);
	ctx->paths = next;
    }
    while (ctx->refs) {
	CSP_SEL_REF *next = ctx->refs->next;

	free(ctx->refs);
	ctx->refs = next;
    }
    while (ctx->qdiscs) {
	CSP_SEL_QDISC *next = ctx->qdiscs->next;

	while (ctx->qdiscs->classes) {
	    CSP_SEL_CLASS *next = ctx->qdiscs->classes->next;

	    free(ctx->qdiscs->classes);
	    ctx->qdiscs->classes = next;
	}
	free(ctx->qdiscs);
	ctx->qdiscs = next;
    }
}


static void end_selection_paths(CSP_CTX *ctx,QDISC *inner_qdisc)
{
    const CSP_SEL_PATH *path;
    int max_equals,top_bits,bits;

    debugf("end_selection_paths (%p)",ctx->paths);
    if (!ctx->paths) return;
    max_equals = 0;
    for (path = ctx->paths; path; path = path->next)
	if (path->equals > max_equals) max_equals = path->equals;
    for (top_bits = 0; (1 << top_bits) < max_equals; top_bits++);
    bits = selection_bits(ctx,inner_qdisc);
    if (top_bits+bits <= CSP_INDEX_BITS) {
	sel_add_hier_filters(ctx);
	sel_set_class_numbers(ctx,bits);
    }
    else {
	lwarnf(inner_qdisc->location,
	  "number of index bits required for path exceeds %d",
	  CSP_INDEX_BITS);
	set_number_refs(ctx);
	sel_add_seq_filters(ctx);
    }
    sel_destroy(ctx);
}


/* ------------------------------------------------------------------------- */


void class_selection_paths(QDISC *qdisc,QDISC *inner_qdisc)
{
    CSP_CTX ctx;
    CLASS *class;

    begin_selection_paths(&ctx);
    for (class = qdisc->classes; class; class = class->next) {
	param_get(class->params,class->location);
	if (prm_class_sel_path.present) {
	    if (class->number != UNDEF_U32)
		lerror(class->location,"can't use class selection path and "
		  "class number on same class");
	    register_selection_path(&ctx,&class->number,
	      prm_data(class->params,&prm_class_sel_path).u.list,inner_qdisc,
	      class->location);
	}
    }
    end_selection_paths(&ctx,inner_qdisc);
}
