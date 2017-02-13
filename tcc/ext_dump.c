/*
 * ext_dump.c - Helper functions for dumping to the external interface
 *
 * Written 2001,2002 by Werner Almsberger
 * Copyright 2001,2002 Bivio Networks, Network Robots
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "util.h"
#include "data.h"
#include "param.h"
#include "tree.h"
#include "ext.h"


/*
 * Known restriction: we callback for prm_pragma, although prm_pragma is
 * supposed to be ignored. This may yield bad surprises for functions that
 * assume that they'll only be called for the parameters they explicitly list
 * as exceptions.
 */


static void print_pragma(FILE *file,PARAM *params)
{
    const DATA_LIST *pragma;

    if (!prm_present(params,&prm_pragma)) return;
    fprintf(file," pragma");
    for (pragma = prm_data(params,&prm_pragma).u.list; pragma;
      pragma = pragma->next)
	fprintf(file," %s",pragma->ref->u.string);
}


static const PARAM_DSC **combine_tables(const PARAM_DSC **a,const PARAM_DSC **b)
{
    const PARAM_DSC **walk,**new,**p;
    int n;

    n = 0;
    if (a)
	for (walk = a; *walk; walk++) n++;
    if (b)
	for (walk = b; *walk; walk++) n++;
    new = p = alloc(sizeof(PARAM_DSC *)*(n+1));
    while ((a && *a) || (b && *b)) {
	if (a && *a && (!b || !*b || strcmp((*a)->id,(*b)->id) < 0))
	  *p++ = *a++;
	else *p++ = *b++;
    }
    *p = NULL;
    return new;
}


/*
 * We use the indirection via class_fn to implement pseudo_prm_parent and
 * pseudo_prm_qdisc transparently.
 */


static PARAM_DSC pseudo_prm_parent ={
    .id = "parent",
};

static PARAM_DSC pseudo_prm_qdisc = {
    .id = "qdisc",
};


struct class_fn_args {
    void (*fn)(FILE *file,const PARAM_DSC *param,const CLASS *class);
    const CLASS *class;
};


static void class_fn(FILE *file,const PARAM_DSC *param,void *arg)
{
    struct class_fn_args *class_fn_args = arg;
    const CLASS *class = class_fn_args->class;

    if (param == &pseudo_prm_parent && class->parent.class) {
	fprintf(file," parent %u",(unsigned) class->parent.class->number);
	return;
    }
    if (param == &pseudo_prm_qdisc && class->qdisc) {
	fprintf(file," qdisc %u",(unsigned) class->qdisc->number);
	return;
    }
    if (class_fn_args->fn) class_fn_args->fn(file,param,class);
}


void ext_dump_class(FILE *file,const CLASS *class,const PARAM_DSC **special,
  void (*fn)(FILE *file,const PARAM_DSC *param,const CLASS *class))
{
    const PARAM_DSC *default_special[] = {
	&pseudo_prm_parent,
	&pseudo_prm_qdisc,
        &prm_pragma,
        NULL
    };
    const QDISC *qdisc = class->parent.qdisc;
    const PARAM_DSC **table;
    struct class_fn_args class_fn_args;

    fprintf(file,"class %u =",(unsigned) class->number);
    table = combine_tables(default_special,special);
    class_fn_args.fn = fn;
    class_fn_args.class = class;
    param_print(file,qdisc->dsc->qdisc_param,qdisc->dsc->class_param,
      qdisc->dsc->class_param,table,class_fn,&class_fn_args);
    free(table);
    print_pragma(file,class->params);
    fprintf(file,"\n");
}


void do_ext_dump_qdisc(FILE *file,const QDISC *qdisc,const QDISC_DSC *dsc,
  const PARAM_DSC **special,void (*fn)(FILE *file,const PARAM_DSC *param,
  const QDISC *qdisc))
{
    const PARAM_DSC *default_special[] = {
        &prm_pragma,
        NULL
    };
    const PARAM_DSC **table;

    fprintf(file,"qdisc %u = %s",(unsigned) qdisc->number,dsc->name);
    table = combine_tables(default_special,special);
    param_print(file,dsc->qdisc_param,dsc->class_param,dsc->qdisc_param,table,
      (void (*)(FILE *file,const PARAM_DSC *param,void *user)) fn,
      (void *) qdisc);
    free(table);
    print_pragma(file,qdisc->params);
    fprintf(file,"\n");
}


void ext_dump_qdisc(FILE *file,const QDISC *qdisc,const PARAM_DSC **special,
  void (*fn)(FILE *file,const PARAM_DSC *param,const QDISC *qdisc))
{
    do_ext_dump_qdisc(file,qdisc,qdisc->dsc,special,fn);
}
