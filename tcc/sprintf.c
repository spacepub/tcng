/*
 * sprintf.c - sprintf for DATA
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks
 */


#include <stdarg.h>
#include <assert.h>

#include "error.h"
#include "data.h"
#include "sprintf_generic.h"
#include "sprintf.h"


struct dsc {
    const DATA_LIST *curr;
    const DATA_LIST *list;
};


static void first_arg(void *dsc)
{
    struct dsc *d = dsc;

    d->curr = d->list;
}


static void next_arg(void *dsc)
{
    struct dsc *d = dsc;

    assert(d->curr);
    d->curr = d->curr->next;
}


static int got_arg(void *dsc)
{
    struct dsc *d = dsc;

    return !!d->curr;
}


static unsigned int get_unum(void *dsc)
{
    struct dsc *d = dsc;

    if (!d->curr) yyerror("not enough arguments for format string");
    return data_convert(*d->curr->ref,dt_unum).u.unum;
}


static int get_unum_int(void *dsc)
{
    return (int) get_unum(dsc);
}


static double get_float(void *dsc)
{
    struct dsc *d = dsc;

    if (!d->curr) yyerror("not enough arguments for format string");
    return data_convert(*d->curr->ref,dt_fnum).u.fnum;
}


static const char *get_string(void *dsc)
{
    struct dsc *d = dsc;

    if (!d->curr) yyerror("not enough arguments for format string");
    return data_convert(*d->curr->ref,dt_string).u.string;
}


static void my_errorf(void *dsc,const char *msg,...)
{
    va_list ap;

    va_start(ap,msg);
    vyyerrorf(msg,ap);
    va_end(ap);
}


static void my_warnf(void *dsc,const char *msg,...)
{
    va_list ap;

    va_start(ap,msg);
    vyywarnf(msg,ap);
    va_end(ap);
}


static struct sprintf_ops ops = {
    .first_arg = first_arg,
    .next_arg = next_arg,
    .got_arg = got_arg,
    .get_sint = get_unum_int,
    .get_uint = get_unum,
    .get_float = get_float,
    .get_string = get_string,
    .errorf = my_errorf,
    .warnf = my_warnf
};


char *sprintf_data(const char *fmt,const DATA_LIST *data_list)
{
    struct dsc dsc;

    dsc.list = data_list;
    return sprintf_generic(fmt,&ops,&dsc);
}
