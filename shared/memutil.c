/*
 * memutil.c - Utility functions for dynamic memory allocation
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks, Network Robots
 */


#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "memutil.h"


/* ----- Malloc wrappers --------------------------------------------------- */


void *alloc(size_t size)
{
    void *new;

    new = malloc(size);
    if (new) return new;
    perror("malloc");
    exit(1);
}


char *stralloc(const char *s)
{
    char *new;

    new = strdup(s);
    if (new) return new;
    perror("strdup");
    exit(1);
}


/* ----- "Safe" sprintf ---------------------------------------------------- */


int log_8(unsigned long value)
{
    int len;

    if (!value) return 1;
    for (len = 0; value; value /= 8) len++;
    return len;
}


int log_10(unsigned long value)
{
    int len;

    if (!value) return 1;
    for (len = 0; value; value /= 10) len++;
    return len;
}


int log_16(unsigned long value)
{
    int len;

    if (!value) return 1;
    for (len = 0; value; value /= 16) len++;
    return len;
}


char *alloc_sprintf(const char *fmt,...)
{
    va_list ap;
    const char *s;
    char *buf;
    int total,real_len;

    total = 0;
    va_start(ap,fmt);
    for (s = fmt; *s; s++)
	if (*s != '%') total++;
	else {
	    int len = 0,min = 0;

	    s++;
	    if (isdigit(*s)) {
		char *end;

		min = strtoul(s,&end,10);
		s = end;
	    }
	    if (*s == '*') {
		min = va_arg(ap,int);
		s++;
	    }
	    if (*s == '.') {
		if (s[1] != '*' || s[2] != 's') abort();
		len = va_arg(ap,int);
		real_len = strlen(va_arg(ap,char *));
		if (real_len < len) len = real_len;
		s += 2;
	    }
	    else {
		int tmp;

		switch (*s) {
		    case 'c':
			(void) va_arg(ap,int); /* converted from char */
			len = 1;
			break;
		    case 's':
			len = strlen(va_arg(ap,char *));
			break;
		    case 'd':
			tmp = va_arg(ap,int);
			if (tmp < 0) {
			    tmp = -tmp;
			    len++;
			}
			len += log_10(tmp);
		        break;
		    case 'u':
			len = log_10(va_arg(ap,unsigned));
		        break;
		    case 'x':
			len = log_16(va_arg(ap,unsigned));
		        break;
		    case '%':
			len = 1;
			break;
		    default:
			abort();
		}
	    }
	    total += len < min ? min : len;
	}
    va_end(ap);
    buf = alloc(total+1);
    va_start(ap,fmt);
    real_len = vsprintf(buf,fmt,ap);
    va_end(ap);
    assert(real_len == total);
    return buf;
}
