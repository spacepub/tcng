/*
 * if.h - Processing of the "if" command
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */


#ifndef IF_H
#define IF_H

#include <stdio.h> /* for do_dump_if_ext */

#include "tree.h"


void reset_offsets(void);

void dump_if_u32(const FILTER *filter);
void dump_if_c(const FILTER *filter);

/* ext_if material  @@@ move all this elsewhere */

void dump_pragma(FILE *file);
void dump_block(FILE *file,const QDISC *q);

void dump_if_ext_prepare(const FILTER *filter);
void dump_if_ext_global(FILE *file);
void dump_if_ext_local(const FILTER *filter,FILE *file);
void do_dump_if_ext(const FILTER *filter,FILE *file);

void dump_if_ext(const FILTER *filter,const char *target);
void add_tcc_module_arg(int local,const char *arg); /* @@@ move elsewhere ? */

#endif /* IF_H */
