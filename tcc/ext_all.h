/*
 * ext_all.h - Dump entire configuration at external interface
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */


#ifndef EXT_ALL_H
#define EXT_ALL_H

#include <inttypes.h>
#include <stdio.h>

#include "tree.h"


int dump_all_decision(FILE *file,QDISC *root,uint16_t class);
//void dump_all_qdisc(QDISC *root,const char *target);
void dump_all_devices(const char *target);
void dump_one_qdisc(QDISC *root,const char *target);

#endif /* EXT_ALL_H */
