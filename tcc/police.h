/*
 * police.h - Policing handling
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2002 Network Robots
 */


#ifndef POLICE_H
#define POLICE_H

#include "data.h"
#include "param.h"
#include "tree.h"
#include "location.h"


extern POLICE *policers;

extern PARAM_DEF police_def;
extern PARAM_DEF bucket_def;


void assign_policer_ids(POLICE *list);

void check_police(POLICE *police);
void check_bucket(POLICE *police);
void dump_police(POLICE *police);
POLICE *get_drop_policer(LOCATION loc);
void add_police(POLICE *police);

#endif /* POLICE_H */
