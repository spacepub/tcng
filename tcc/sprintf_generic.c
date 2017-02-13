/*
 * sprintf_generic.c - Generic malloc'ing sprintf
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks
 *
 * Based on ../share/memutil.c:alloc_sprintf
 */


#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "util.h"
#include "sprintf_generic.h"


char *sprintf_generic(const char *fmt,const struct sprintf_ops *ops,void *dsc)
{
    const char *s;
    char *buf,*t;
    int total;

    total = 0;
    ops->first_arg(dsc);
    for (s = fmt; *s; s++) {
	char dummy[100]; /* should be big enough for even nasty doubles */
	int len = 0,min = 0;

	if (*s != '%') {
	    total++;
	    continue;
	}
	s++;
	if (*s == '-' || isdigit(*s)) {
	    char *end;

	    if (*s == '-') s++;
	    min = strtoul(s,&end,10);
	    s = end;
	}
	if (*s == '*') {
	    min = ops->get_sint(dsc);
	    ops->next_arg(dsc);
	    if (min < 0) min = -min;
	    s++;
	}
	if (*s == '.') {
	    int real_len;

	    s++;
	    if (isdigit(*s)) {
		char *end;

		len = strtoul(s,&end,10);
		s = end;
	    }
	    else {
		if (*s++ != '*') ops->errorf(dsc,"invalid precision");
		len = ops->get_uint(dsc);
		ops->next_arg(dsc);
	    }
	    switch (*s) {
		case 's':
		    real_len = strlen(ops->get_string(dsc));
		    ops->next_arg(dsc);
		    if (real_len < len) len = real_len;
		    break;
		case 'f':
		    min = len = sprintf(dummy,"%*.*f",min,len,
		      ops->get_float(dsc));
		    ops->next_arg(dsc);
		    break;
		default:
		    if (*s != 's')
			ops->errorf(dsc,
			  "precision (%%.) only allowed for %%s or %%f");
	    }
	}
	else {
	    int tmp;

	    switch (*s) {
		case 'c':
		    len = 1;
		    ops->next_arg(dsc);
		    break;
		case 's':
		    len = strlen(ops->get_string(dsc));
		    ops->next_arg(dsc);
		    break;
		case 'd':
		    tmp = ops->get_sint(dsc);
		    ops->next_arg(dsc);
		    /* warn ? */
		    if (tmp < 0) {
			tmp = -tmp;
			len++;
		    }
		    len += log_10(tmp);
		    break;
		case 'u':
		    len = log_10(ops->get_uint(dsc));
		    ops->next_arg(dsc);
		    break;
		case 'o':
		    len = log_8(ops->get_uint(dsc));
		    ops->next_arg(dsc);
		    break;
		case 'x':
		case 'X':
		    len = log_16(ops->get_uint(dsc));
		    ops->next_arg(dsc);
		    break;
		case 'f':
		    min = len = sprintf(dummy,"%*f",min,ops->get_float(dsc));
		    ops->next_arg(dsc);
		    break;
		case '%':
		    min = len = 1;
		    break;
		default:
		    ops->errorf(dsc,"unrecognized format character \"%c\"",*s);
	    }
	}
	total += len < min ? min : len;
    }
    if (ops->got_arg(dsc)) {
	ops->next_arg(dsc);
	ops->warnf(dsc,"ignoring extra sprintf argument%s",
	  ops->got_arg(dsc) ? "s" : "");
    }
    buf = t = malloc(total+1);
    if (!buf) {
	perror("malloc");
	exit(1);
    }
    ops->first_arg(dsc);
    for (s = fmt; *s; s++)
	if (*s != '%') *t++ = *s;
	else {
	    int min = 0;
	    char *zero = "";
	    char fmt[10]; /* using only 8 */

	    s++;
	    if (*s == '-' || isdigit(*s)) {
		char *end;

		if (*s == '0' || (s[0] == '-' && s[1] == '0')) zero = "0";
		min = strtol(s,&end,10);
		s = end;
	    }
	    if (*s == '*') {
		min = ops->get_sint(dsc);
		ops->next_arg(dsc);
		s++;
	    }
	    if (*s == '.') {
		int len;

		s++;
		if (isdigit(*s)) {
		    char *end;

		    len = strtoul(s,&end,10);
		    s = end;
		}
		else {
		    assert(*s == '*');
		    len = ops->get_uint(dsc);
		    ops->next_arg(dsc);
		    s++;
		}
		sprintf(fmt,"%%%s*.*%c",zero,*s);
		if (*s == 's') t += sprintf(t,fmt,min,len,ops->get_string(dsc));
		else t += sprintf(t,fmt,min,len,ops->get_float(dsc));
		ops->next_arg(dsc);
	    }
	    else {
		switch (*s) {
		    case 'c':
			sprintf(fmt,"%%%s*c",zero);
			t += sprintf(t,fmt,min,ops->get_sint(dsc));
			ops->next_arg(dsc);
		        break;
		    case 's':
			sprintf(fmt,"%%%s*s",zero);
			t += sprintf(t,fmt,min,ops->get_string(dsc));
			ops->next_arg(dsc);
			break;
		    case 'd':
			sprintf(fmt,"%%%s*ld",zero);
			t += sprintf(t,fmt,min,(long) ops->get_sint(dsc));
			ops->next_arg(dsc);
		        break;
		    case 'u':
			sprintf(fmt,"%%%s*lu",zero);
			t += sprintf(t,fmt,min,(unsigned long)
			  ops->get_uint(dsc));
			ops->next_arg(dsc);
		        break;
		    case 'o':
			sprintf(fmt,"%%%s*lo",zero);
			t += sprintf(t,fmt,min,(unsigned long)
			  ops->get_uint(dsc));
			ops->next_arg(dsc);
		        break;
		    case 'x':
			sprintf(fmt,"%%%s*lx",zero);
			t += sprintf(t,fmt,min,(unsigned long)
			  ops->get_uint(dsc));
			ops->next_arg(dsc);
		        break;
		    case 'X':
			sprintf(fmt,"%%%s*lX",zero);
			t += sprintf(t,fmt,min,(unsigned long)
			  ops->get_uint(dsc));
			ops->next_arg(dsc);
		        break;
		    case 'f':
			sprintf(fmt,"%%%s*lf",zero);
			t += sprintf(t,fmt,min,
			  ops->get_float(dsc));
			ops->next_arg(dsc);
		        break;
		    case '%':
			*t++ = '%';
		        break;
		    default:
			abort();
		}
	    }
	}
    assert(t-buf == total);
    *t = 0;
    return buf;
}
