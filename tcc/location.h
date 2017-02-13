/*
 * location.h - Source code location of language elements (and variable use)
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots
 */

/*
 * Note: in the future, we may even try to include column information, like
 * recent versions of gcc do, so users of location.h should treat LOCATION
 * as an abstract data type.
 */


#ifndef LOCATION_H
#define LOCATION_H

#include <stdio.h>

#include "data.h"


typedef struct {
    const char *file;
    const char *tag;
    int line;
} LOCATION;


extern char *file_name;
extern int lineno;


void input_file(const char *name);
LOCATION current_location(void);
int print_location(FILE *file,LOCATION loc);
int print_current_location(FILE *file_name);

void write_location_map(const char *file_name);

const LOCATION *location_by_spec(const char *spec);

/*
 * A location spec is like the location specification used for variables, i.e
 * <type><space><identifier>. location_by_spec returns NULL if the
 * specification is in an invalid format or if no such object exists.
 */

void register_var(const char *name,const DATA_ASSOC *entry,DATA d);
void var_use_begin_scope(DATA d);
void var_use_end_scope(void);
void write_var_use(const char *file_name);

int var_any_class_ref(const struct _class *class);

#endif /* LOCATION_H */
