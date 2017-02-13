/*
 * util.c - Utility functions
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks, Network Robots
 */


#include <stdio.h>
#include <sys/time.h>

#include "util.h"


/* ----- Helper functions for performance tuning --------------------------- */


static struct timeval timer;


void start_timer(void)
{
    gettimeofday(&timer,NULL);
}


void print_timer(const char *label)
{
    struct timeval now;

    gettimeofday(&now,NULL);
    now.tv_usec -= timer.tv_usec;
    now.tv_sec -= timer.tv_sec;
    if (now.tv_usec < 0) {
	now.tv_usec += 1000000;
	now.tv_sec--;
    }
    fprintf(stderr,"%s: %d.%06d\n",label,(int) now.tv_sec,(int) now.tv_usec);
}


/* ----- Helper function for qsort ----------------------------------------- */


int comp_int(const void *a,const void *b)
{
    /*
     * These numbers are small (16 bit), so we don't need to worry about
     * overflows.
     */
    return *(const int *) a-*(const int *) b;
}
