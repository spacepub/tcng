/*
 * memutil.h - Utility functions for dynamic memory allocation
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks, Network Robots
 */


#ifndef MEMUTIL_H
#define MEMUTIL_H

#include <sys/types.h>


/* ----- Malloc wrappers --------------------------------------------------- */


#define alloc_t(T) ((T *) alloc(sizeof(T)))

void *alloc(size_t size);
char *stralloc(const char *s);


/* ----- "Safe" sprintf ---------------------------------------------------- */


int log_8(unsigned long value);
int log_10(unsigned long value);
int log_16(unsigned long value);

char *alloc_sprintf(const char *fmt,...);

/* Like asprintf, but has defined behaviour on ENOMEM */

#endif /* MEMUTIL_H */
