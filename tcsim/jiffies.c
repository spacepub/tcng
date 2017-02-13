/*
 * jiffies.c - Time handling functions
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Werner Almesberger
 */


#include <stdio.h>
#include <math.h>
#include <linux/sched.h> /* for HZ */

#include "tcsim.h"
#include "jiffies.h"


int jiffval_cmp(struct jiffval a,struct jiffval b)
{
    if (a.jiffies < b.jiffies) return -1;
    if (a.jiffies > b.jiffies) return 1;
    if (a.ujiffies < b.ujiffies) return -1;
    if (a.ujiffies > b.ujiffies) return 1;
    return 0;
}


static void normalize_jiffval(struct jiffval *jiff)
{
    while (jiff->ujiffies > 999999) {
        jiff->ujiffies -= 1000000;
        jiff->jiffies++;
    }
}


struct jiffval dtojiffval(double value)
{
    struct jiffval jiff;

    jiff.jiffies = floor(value);
    value -= jiff.jiffies;
    jiff.ujiffies = rint(value*1e6);
    normalize_jiffval(&jiff);
    return jiff;
}


double jiffvaltod(struct jiffval jiff)
{
    return jiff.jiffies+jiff.ujiffies/1e6;
}


struct jiffval stoj(struct jiffval secs)
{
    return dtojiffval(jiffvaltod(secs)*HZ);
}


struct jiffval jtos(struct jiffval jiff)
{
    return dtojiffval(jiffvaltod(jiff)/HZ);
}


struct jiffval jv_add(struct jiffval a,struct jiffval b)
{
    a.jiffies += b.jiffies;
    a.ujiffies += b.ujiffies;
    while (a.ujiffies > 1000000) {
	a.ujiffies -= 1000000;
	a.jiffies++;
    }
    return a;
}


void print_time(struct jiffval jiff)
{
    if (!use_jiffies) jiff = jtos(jiff);
    printf("%lu.%06lu",jiff.jiffies,jiff.ujiffies);
}
