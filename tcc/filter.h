/*
 * filter.h - Filter handling
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 */


#ifndef FILTER_H
#define FILTER_H

#include "data.h"
#include "param.h"
#include "location.h"
#include "tree.h"


extern FILTER_DSC fw_dsc;
extern FILTER_DSC route_dsc;
extern FILTER_DSC rsvp_dsc;
extern FILTER_DSC tcindex_dsc;
extern FILTER_DSC if_dsc;

extern CLASS class_is_tunnel;
    /* "magic" class, which identifies tunnel selection */
extern PARAM_DEF tunnel_param;

extern CLASS class_is_drop;
    /* "magic" class, which identifies drop selection */

void add_element(ELEMENT *element,PARENT parent);
void add_filter(FILTER *filter);
ELEMENT *add_if(DATA expression,PARENT parent,FILTER *filter,LOCATION loc);
void check_filters(FILTER *list);
void dump_filters(FILTER *list);

#endif /* FILTER_H */
