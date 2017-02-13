/*
 * tree.h - All qdisc/class/filter data structures
 *
 * Written 2001-2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots
 * Copyright 2003 Werner Almesberger
 */


#ifndef TREE_H
#define TREE_H

#include <stdint.h>
#include <stdio.h>

#include "location.h"
#include "param.h"


struct _device;
struct _qdisc;
struct _class;
struct _filter;
struct _tunnel;


typedef struct {
    struct _device *device;
    struct _qdisc *qdisc;
    struct _class *class;
    struct _filter *filter;
    struct _tunnel *tunnel;
} PARENT;

typedef enum {
    pd_invalid = 0,
    pd_ok = 1,
    pd_drop = 2,
    pd_continue = 4,
    pd_reclassify = 8,
} DECISION;

typedef struct _police {
    uint32_t number;
    LOCATION location;
    PARAM *params;
    DECISION in_profile;
    DECISION out_profile;
    int used; /* 0 if policer is not referenced */
    int created; /* if non-zero, we can just reference it */
    struct _police *next;
} POLICE;

typedef struct _element {
    PARENT parent;
    uint32_t number;
    LOCATION location;
    PARAM *params;
    POLICE *police;
    struct _element *next;
} ELEMENT;

typedef struct {
    const char *name;
    PARAM_DEF *filter_param;
    PARAM_DEF *element_param;
    void (*check)(struct _filter *filter);
    void (*dump_tc)(struct _filter *filter);
} FILTER_DSC;

typedef struct _filter {
    PARENT parent;
    uint32_t number;
    LOCATION location;
    const FILTER_DSC *dsc;
    PARAM *params;
    ELEMENT *elements;
    struct _filter *next;
} FILTER;

typedef struct _tunnel {
    PARENT parent;
    uint32_t number;
    LOCATION location;
    PARAM *params;
} TUNNEL;

typedef struct _class {
    PARENT parent;
    uint32_t number;
    LOCATION location;
    PARAM *params;
    struct _qdisc *qdisc;
    FILTER *filters;
    struct _class *child;
    struct _class *next;
    int implicit; /* @@@ temporary: flag implicitly generated class */
} CLASS;

#define QDISC_HAS_CLASSES	1	/* qdisc has classes (see below) */
#define QDISC_HAS_FILTERS	2	/* classes have filters */
#define QDISC_CHILD_QDISCS	8	/* qdisc's classes have child qdiscs */
#define QDISC_SHARED_QDISC	16	/* all classes share one qdisc */
#define QDISC_CLASS_SEL_PATHS	32	/* qdisc can generate class sel paths */

/*
 * Note that QDISC_CHILD_QDISCS does not imply QDISC_HAS_CLASSES. E.g. TBF can
 * have one child qdisc, but no classes. Nevertheless, tcc adds a dummy class
 * for internal use and also for the external interface.
 */

typedef struct {
    const char *name;
    int flags;
    PARAM_DEF *qdisc_param;
    PARAM_DEF *class_param; /* undefined if classless */
    void (*check)(struct _qdisc *qdisc); /* assign IDs and check parameters */
    void (*default_class)(DATA *d,struct _qdisc *qdisc);
	/* undefined if classless */
    void (*dump_tc)(struct _qdisc *qdisc);
    void (*dump_ext)(FILE *file,struct _qdisc *qdisc);
} QDISC_DSC;

typedef struct _qdisc {
    PARENT parent;
    QDISC_DSC *dsc;
    uint32_t number;
    LOCATION location;
    PARAM *params;
    CLASS *classes;
    FILTER *filters;
    DATA if_expr; /* dt_none if we don't have if */
} QDISC;

typedef struct _device {
    const char *name;
    LOCATION location;
    PARAM *params;
    QDISC *egress;
    QDISC *ingress;
    struct _device *next;
} DEVICE;


extern DATA_LIST *pragma; /* global pragma */

#endif /* TREE_H */
