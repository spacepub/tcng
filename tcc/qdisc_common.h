/*
 * qdisc_common.h - Common functions shared by qdisc handling
 *
 * Written 2001,2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots
 * Copyright 2003 Werner Almesberger
 */


#ifndef QDISC_COMMON_H
#define QDISC_COMMON_H

#include <inttypes.h>

#include "tree.h"


/* Helper functions for structure checking */

void check_childless(const CLASS *class);
void check_no_policing(const QDISC *qdisc);


/* Assignment of unique IDs */

extern uint32_t curr_id;

int recurse_class(CLASS *class,int (*fn)(CLASS *class,void *user),void *user);
int count_class_ids(CLASS *class,void *user);
int assign_class_id(CLASS *class,void *user);


/* Common RED functions */

void red_parameters(PARAM *params,uint32_t bandwidth);


/* *_default_class helper function */

CLASS *require_class(QDISC *qdisc,uint32_t number,int implicit);
void append_default(DATA *d,DATA dflt);


/* Shared qdisc for dsmark and ingress */

void merge_shared(CLASS **classes);


/* Order classes by ascending class ID */

void sort_classes(CLASS **classes);


/* Parameter management */

void class_param_load(const CLASS *class);
void qdisc_param_load(const QDISC *qdisc);


/* Class selection paths */

void class_selection_paths(QDISC *qdisc,QDISC *inner_qdisc);


/* Generic dumping at external interface */

void generic_dump_ext(FILE *file,QDISC *qdisc);

#endif /* QDISC_COMMON_H */
