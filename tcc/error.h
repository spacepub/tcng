/*
 * error.h - Error reporting functions
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 */


#ifndef ERROR_H
#define ERROR_H

#include <stdarg.h>

#include "location.h"
#include "registry.h"


/*
 * Reporting functions with yy in their name also print the source location
 * of * the problem. Those without only print the message (e.g. because we
 * don't discover the problem until reaching EOF.)
 */

/*
 * The functions are defined in tcng.l
 */

void vyywarnf(const char *fmt,va_list ap);
void yywarnf(const char *fmt,...);
void yywarn(const char *s);

void lwarnf(LOCATION loc,const char *fmt,...);
void lwarn(LOCATION loc,const char *s);

void warnf(const char *fmt,...);
void warn(const char *s);

void __attribute__((noreturn)) vyyerrorf(const char *fmt,va_list ap);
void __attribute__((noreturn)) yyerrorf(const char *fmt,...);
void __attribute__((noreturn)) yyerror(const char *s);

void __attribute__((noreturn)) vlerrorf(LOCATION loc,const char *fmt,
  va_list ap);
void __attribute__((noreturn)) lerrorf(LOCATION loc,const char *fmt,...);
void __attribute__((noreturn)) lerror(LOCATION loc,const char *s);

void __attribute__((noreturn)) verrorf(const char *fmt,va_list ap);
void __attribute__((noreturn)) errorf(const char *fmt,...);
void __attribute__((noreturn)) error(const char *s);


/*
 * Print a debugging message to standard output, indented by the length of a
 * stack trace. Also provides the terminating \n.
 *
 * debugf doesn't print anything if "debug" is zero. debugf2 is like debugf,
 * but only prints anything if debug > 1
 */

void debugf(const char *msg,...);
void debugf2(const char *msg,...);


extern REGISTRY warn_switches;

int set_warn_switch(const char *arg,int lock);

#endif /* ERROR_H */
