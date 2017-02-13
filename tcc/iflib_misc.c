/*
 * iflib_misc.c - Miscellaneous functions
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */


#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "op.h"
#include "iflib.h"


void __dump_failed(DATA d,const char *reason,const char *file,int line)
{
    putchar('\n'); /* we're probably in the middle of a line */
    fflush(stdout);
    fprintf(stderr,"can't dump subexpression (%s",file);
    if (reason) fprintf(stderr,", %s",reason);
    else fprintf(stderr,":%d",line);
    fprintf(stderr,")\n");
    if (!quiet) print_expr(stderr,d);
    exit(1);
}
