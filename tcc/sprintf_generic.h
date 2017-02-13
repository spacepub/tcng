/*
 * sprintf_generic.h - Generic malloc'ing sprintf
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks
 *
 * Based on Based on ../share/memutil.c:alloc_sprintf
 */


#ifndef SPRINTF_GENERIC_H
#define SPRINTF_GENERIC_H

#include <stdarg.h>


struct sprintf_ops {
    void (*first_arg)(void *dsc);	/* move pointer to first argument */
    void (*next_arg)(void *dsc);	/* advance pointer */
    int (*got_arg)(void *dsc);		/* zero at end of list */
    int (*get_sint)(void *dsc);		/* return signed integer */
    unsigned int (*get_uint)(void *dsc);/* return unsigned integer */
    double (*get_float)(void *dsc);
    const char *(*get_string)(void *dsc);
    void (*errorf)(void *dsc,const char *msg,...);
    void (*warnf)(void *dsc,const char *msg,...);
};


char *sprintf_generic(const char *fmt,const struct sprintf_ops *ops,void *dsc);

#endif /* SPRINTF_GENERIC_H */
