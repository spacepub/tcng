/*
 * var.h - Variable handling
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots
 */


#ifndef VAR_H
#define VAR_H

#include <stdio.h>

#include "data.h"
#include "location.h"


typedef enum {
    vu_new,		/* just created */
    vu_unused,		/* initialized but never read */
    vu_used,		/* also read */
    vu_forward		/* forward-declaration, will be set later */
} VAR_USAGE;

typedef struct _var {
    DATA data;
    DATA *data_ref;		/* for forward-declared variables */
    VAR_USAGE usage;
    LOCATION location;
    LOCATION found;		/* new location if redefining */
    struct _var_hash *hash;	/* hash entry */
    struct _var_scope *scope;	/* my scope */
    struct _var *next_in_scope;	/* next in same scope */
    struct _var *next_with_name;/* next with same name (in outer scope) */
} VAR;

typedef struct _var_scope {
    VAR *vars;			/* variables in scope; may be NULL */
    VAR **last;			/* last variable in this scope */
    struct _var_scope *next;	/* next (outer) scope */
} VAR_SCOPE;

typedef struct _var_hash {
    const char *name;		/* variable name */
    VAR *vars;			/* pointer to variable */
    struct _var_hash *next;	/* next entry with same hash value */
} VAR_HASH;


VAR *var_find(const char *name);

void var_begin_scope(void);
void var_end_scope(void);

void var_outer_scope(void);
void var_inner_scope(void);

/*
 * var_outer_scope is like var_end_scope, except that it doesn't remove
 * variables.
 */

void var_begin_access(VAR *var);
void var_follow(const char *name);

void var_push_access(void);
void var_pop_access(void);

void var_set(DATA data);
DATA *var_forward(void); /* also allocates data */
int var_initialized(const VAR *var);
DATA var_get(void);

void dump_hash(FILE *file);

#endif /* VAR_H */
