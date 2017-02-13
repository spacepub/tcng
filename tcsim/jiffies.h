/*
 * jiffies.h - Time handling functions
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2002 Werner Almesberger
 */


#ifndef JIFFIES_H
#define JIFFIES_H

struct jiffval {
    unsigned long jiffies;
    unsigned long ujiffies;
};

extern struct jiffval now;


int jiffval_cmp(struct jiffval a,struct jiffval b);

struct jiffval dtojiffval(double value);
double jiffvaltod(struct jiffval jiff);
struct jiffval stoj(struct jiffval secs);
struct jiffval jtos(struct jiffval jiff);
struct jiffval jv_add(struct jiffval a,struct jiffval b);
void print_time(struct jiffval jiff);

#endif /* JIFFIES_H */
