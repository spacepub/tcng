/*
 * field.h - Packet field definition
 *
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */


#ifndef FIELD_H
#define FIELD_H

#include "data.h"


typedef struct _field {
    const char *name;		/* malloc'ed name; only valid in top scope */
    DATA condition;
    DATA access;
    int level;			/* scoping level */
    struct _field *outer;	/* same field at next outer scope */
    struct _field *next;	/* next field; only valid in top scope */
} FIELD;

typedef struct _field_root {
    int offset_group;
    struct _field_root *next;
} FIELD_ROOT;


FIELD_ROOT *field_root(int offset_group);
int offset_group_taken(int offset_group);

void field_begin_scope(void);
void field_end_scope(void);

FIELD *field_find(const char *name);
void field_set(const char *name,DATA access,DATA condition);
void field_redefine(FIELD *field,DATA access,DATA condition);
DATA field_expand(DATA d);
DATA field_snapshot(DATA d);

#endif /* FIELD_H */
