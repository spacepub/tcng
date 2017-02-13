/*
 * echoh.h - Print qdisc/class hierarchy
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 Network Robots
 * Distributed under the LGPL.
 */

#ifndef ECHOH_H
#define ECHOH_H

#include <stdio.h>

#include "tccext.h"


void echoh_blocks(FILE *file,TCCEXT_BLOCK *b);
void echoh_buckets(FILE *file,TCCEXT_BUCKET *p);

#endif /* ECHOH_H */
