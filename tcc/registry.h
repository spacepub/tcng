/*
 * registry.h - Registry of named items
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 Network Robots
 */


#ifndef REGISTRY_H
#define REGISTRY_H


typedef struct {
    const char **names;
    int **flags;
    int local_pointers; /* non-zero if "flags" is malloc'ed */
    int *local_flags; /* storage for flags not provided by user */
    int *locked;
} REGISTRY;


void registry_open(REGISTRY *reg,const char **names,int **flags);

int registry_set(REGISTRY *reg,const char *name);
int registry_clear(REGISTRY *reg,const char *name);
int registry_set_no(REGISTRY *reg,const char *name);

int registry_lock(REGISTRY *reg,const char *name);

int registry_probe(REGISTRY *reg,const char *name);
void registry_close(REGISTRY *reg);

#endif /* REGISTRY */
