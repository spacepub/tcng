/*
 * registry.c - Registry of named items
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 Network Robots
 */


#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "util.h"
#include "registry.h"


void registry_open(REGISTRY *reg,const char **names,int **flags)
{
    int i;

    reg->names = names;
    for (i = 0; names[i]; i++);
    if (flags) {
	reg->flags = flags;
	reg->local_pointers = 0;
    }
    else {
	reg->flags = alloc(sizeof(int *)*i);
	reg->local_pointers = 1;
    }
    reg->local_flags = alloc(sizeof(int)*i);
    for (i = 0; names[i]; i++) {
	if (flags && flags[i]) continue;
	reg->flags[i] = reg->local_flags+i;
	*reg->flags[i] = 0;
    }
    reg->locked = calloc(i,sizeof(int));
}


static int registry_find(REGISTRY *reg,const char *name)
{
    int i;

    for (i = 0; reg->names[i]; i++)
	if (!strcmp(reg->names[i],name)) return i;
    return -1;
}


int registry_set(REGISTRY *reg,const char *name)
{
    int i;

    i = registry_find(reg,name);
    if (i < 0) return -1;
    if (!reg->locked[i]) *reg->flags[i] = 1;
    return 0;
}


int registry_clear(REGISTRY *reg,const char *name)
{
    int i;

    i = registry_find(reg,name);
    if (i < 0) return -1;
    if (!reg->locked[i]) *reg->flags[i] = 0;
    return 0;
}


int registry_set_no(REGISTRY *reg,const char *name)
{
    return strncmp(name,"no",2) ? registry_set(reg,name) :
      registry_clear(reg,name+2);

}


int registry_lock(REGISTRY *reg,const char *name)
{
    int i;

    i = registry_find(reg,name);
    if (i < 0) return -1;
    reg->locked[i] = 1;
    return 0;
}


int registry_probe(REGISTRY *reg,const char *name)
{
    int i;

    i = registry_find(reg,name);
    if (i >= 0) return *reg->flags[i];
    errorf("internal error: registry_probe called for unknown item \"%s\"",
      name);
}


void registry_close(REGISTRY *reg)
{
    if (reg->local_pointers) free(reg->flags);
    free(reg->local_flags);
    free(reg->locked);
}
