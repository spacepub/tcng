/*
 * error.c - Error reporting functions
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks, Network Robots, Werner Almesberger
 */


#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <execinfo.h>

#include <u128.h>
#include <addr.h>

#include "config.h"
#include "error.h"
#include "registry.h"


/* ----- Provide error handlers for library functions ---------------------- */


void u128_error_hook(const char *msg)
{
    error(msg);
}


void addr_error_hook(const char *msg)
{
    yyerror(msg);
}


/* ----- Debugging messages with implict indentation ----------------------- */


static int stack_zero; /* minimum stack depth */


static int backtrace_len(void)
{
    void *dummy[MAX_DEBUG_DEPTH];

    return backtrace(dummy,MAX_DEBUG_DEPTH);
}


static void vdebugf(const char *msg,va_list ap)
{
    int depth,i;

    depth = backtrace_len(); /* auto-calibrate */
    if (!stack_zero || depth < stack_zero) stack_zero = depth;
    for (i = depth-stack_zero; i; i--) putchar(' ');
    vprintf(msg,ap);
    putchar('\n');
}


void debugf(const char *msg,...)
{
    va_list ap;

    if (!debug) return;
    va_start(ap,msg);
    vdebugf(msg,ap);
    va_end(ap);
}


void debugf2(const char *msg,...)
{
    va_list ap;

    if (debug < 2) return;
    va_start(ap,msg);
    vdebugf(msg,ap);
    va_end(ap);
}


/* ----- Fine-grained warning configuration -------------------------------- */


int warn_truncate = 0; /* see config.h for description */
int warn_unused = 1;
int warn_expensive = 0;
int warn_exp_post_opt = 0;
int warn_exp_error = 0;
int warn_redefine = 0;
int warn_explicit = 1;
int warn_const_pfx = 0;


int set_warn_switch(const char *arg,int lock)
{
    static REGISTRY warn_switches;
    static int warn_switches_initialized = 0;
    static const char *warn_switch_names[] = {
	"truncate",
	"unused",
	"expensive",
	"exppostopt",
	"experror",
	"redefine",
	"explicit",
	"constpfx",
	NULL
    };
    static int *warn_switch_flags[] = {
	&warn_truncate,
	&warn_unused,
	&warn_expensive,
	&warn_exp_post_opt,
	&warn_exp_error,
	&warn_redefine,
	&warn_explicit,
	&warn_const_pfx,
	NULL
    };

    if (!warn_switches_initialized) {
	registry_open(&warn_switches,warn_switch_names,warn_switch_flags);
	warn_switches_initialized = 1;
    }
    if (registry_set_no(&warn_switches,arg)) return 0;
    if (lock && registry_lock(&warn_switches,strncmp(arg,"no",2) ? arg : arg+2))
	abort();
    return 1;
}
