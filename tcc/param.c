/*
 * param.c - Parameter handling
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Network Robots
 */


#include <stddef.h> /* NULL */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "location.h"
#include "param.h"


#include "param_dsc.inc"


PARAM *param_make(PARAM_DSC *dsc,DATA data)
{
    PARAM *p;

    p = alloc_t(PARAM);
    p->dsc = dsc;
    if (dsc->type != dt_none)
	data = data_convert(data,
	  dsc->type == dt_bucket && data.type == dt_police ?
	  dt_police : dsc->type);
    if (warn_truncate && (data.type == dt_size || data.type == dt_psize ||
      data.type == dt_rate || data.type == dt_time) &&
      fmod(data.u.fnum,data.type == dt_rate ? 8 : 1) != 0.0)
	yywarnf("parameter %s (%f) will be truncated",dsc->id,data.u.fnum);
    p->data = data;
    p->next = NULL;
    return p;
}


void param_add(PARAM **list,PARAM *param)
{
    while (*list) list = &(*list)->next;
    *list = param;
    param->next = NULL;
}


static void param_reset(void)
{
#include "param_reset.inc"
}


/*
 * Canonical format:
 *  size:  bytes
 *  rate:  bytes/second
 *  psize: packets
 *  unum:  "as is"
 *  fnum:  handled only if it's a priority
 */


static void param_get_1(const PARAM *param,LOCATION loc)
{
    double value;

    param->dsc->present = 1;
    switch (param->dsc->type) {
	case dt_none:
	    break;
	case dt_fnum:
	    /* @@@ aiek, hard-coded special case */
	    if (param->dsc != &prm_probability) break;
		/* can't assign fnums in general */
	    param->dsc->v = param->data.u.fnum*1e6;
	    break;
	case dt_unum:
	    param->dsc->v = param->data.u.unum;
	    break;
	case dt_string:
	    /* can't assign strings */
	    break;
	case dt_size:
	case dt_psize:
	case dt_rate:
	case dt_time:
	    value = param->data.u.fnum; /* ugly but harmless */
	    if (param->dsc->type == dt_rate) value /= 8.0;
	    if (value > 0x7fffffffUL)
		lerrorf(loc,"parameter %s (%f) is too big",param->dsc->id,
		  value);
	    param->dsc->v = value;
	    break;
	case dt_list:
	    /* can't assign lists */
	    break;
	case dt_police:
	    /* can't assign policers */
	    break;
	case dt_bucket:
	    /* can't assign buckets */
	    break;
	default:
	    abort();
    }
}


void param_get(const PARAM *params,LOCATION loc)
{
    const PARAM *param;

    param_reset();
    for (param = params; param; param = param->next)
	param_get_1(param,loc);
}


void check_required(const PARAM *params,const PARAM_DSC **required,
  LOCATION loc)
{
    if (!required) return;
    while (*required) {
	const PARAM *walk;

	for (walk = params; walk; walk = walk->next)
	    if (walk->dsc == *required) break;
	if (!walk)
	    lerrorf(loc,"required parameter \"%s\" is missing",(*required)->id);
	required++;
    }
}


void check_optional(const PARAM *param,const PARAM_DSC **required,
  const PARAM_DSC **optional,LOCATION loc)
{
    static const PARAM_DSC *null = NULL;
    const PARAM_DSC **walk = &null;

    if (required)
	for (walk = required; *walk; walk++)
	    if (*walk == param->dsc) break;
    if (optional && !*walk)
	for (walk = optional; *walk; walk++)
	    if (*walk == param->dsc) break;
    if (!*walk) lerrorf(loc,"unrecognized parameter \"%s\"",param->dsc->id);
}



void check_params(const PARAM *params,const PARAM_DSC **required,
  const PARAM_DSC **optional,LOCATION loc)
{
    check_required(params,required,loc);
    while (params) {
	check_optional(params,required,optional,loc);
	params = params->next;
    }
}


static const PARAM *lookup_const(const PARAM *params,const PARAM_DSC *dsc)
{
    while (params)
	if (params->dsc == dsc) break;
	else params = params->next;
    return params;
}


static PARAM *lookup(PARAM *params,const PARAM_DSC *dsc)
{
    while (params)
	if (params->dsc == dsc) break;
	else params = params->next;
    return params;
}


int prm_present(const PARAM *params,const PARAM_DSC *dsc)
{
    return !!lookup_const(params,dsc);
}


DATA *prm_data_ptr(PARAM *params,const PARAM_DSC *dsc)
{
    return &lookup(params,dsc)->data;
}


DATA prm_data(PARAM *params,const PARAM_DSC *dsc)
{
    return *prm_data_ptr(params,dsc);
}


uint32_t prm_unum(PARAM *params,const PARAM_DSC *dsc)
{
    return prm_data(params,dsc).u.unum;
}


/* ------------------------- New: Parameter stack -------------------------- */


static struct {
#include "param_stack.inc"
} prm_stack[MAX_PARAM_DEPTH];

static int prm_sp = 0;


void param_push(void)
{
    if (prm_sp == MAX_PARAM_DEPTH) yyerror("parameter stack overflow");
#include "param_push.inc"
    prm_sp++;
}


void param_pop(void)
{
    if (!prm_sp) yyerror("parameter stack underflow");
    prm_sp--;
#include "param_pop.inc"
}


static int in_table(const PARAM_DSC *param,const PARAM_DSC **table)
{
    const PARAM_DSC **walk;

    if (!table) return 0;
    for (walk = table; *walk; walk++)
	if (*walk == param) return 1;
    return 0;
}


static void check_recognized(const PARAM *param,const PARAM_DEF *base,
  const PARAM_DEF *curr,LOCATION loc)
{
    if (in_table(param->dsc,base->required)) return;
    if (in_table(param->dsc,base->optional)) return;
    if (in_table(param->dsc,curr->required)) return;
    if (in_table(param->dsc,curr->optional)) return;
    lerrorf(loc,"unrecognized parameter \"%s\"",param->dsc->id);
}


void param_load(const PARAM *params,const PARAM_DEF *base,
  const PARAM_DEF *curr,LOCATION loc)
{
    const PARAM *param;

    if (base == curr) param_reset();
    else {
	/*
	 * @@@ Okay, this is really ugly. We hard-code parameters that are
	 * not propagated using the normal defaulting mechanism.
	 */
	prm_pragma.present = 0;
	prm_priomap.present = 0;
    }
    for (param = params; param; param = param->next) {
        check_recognized(param,base,curr,loc);
	param_get_1(param,loc);
    }
    if (curr->required) {
	const PARAM_DSC **walk;

	for (walk = curr->required; *walk; walk++)
	    if (!(*walk)->present)
		lerrorf(loc,"required parameter \"%s\" is missing",(*walk)->id);
    }
}


void param_print(FILE *file,const PARAM_DEF *base,const PARAM_DEF *child,
  const PARAM_DEF *curr,const PARAM_DSC **special,
  void (*handle_special)(FILE *file,const PARAM_DSC *param,void *arg),
  void *special_arg)
{
    const char *last = NULL;

    while (1) {
	const PARAM_DSC **walk;
	const PARAM_DSC *best = NULL;
	int is_special = 0;

	for (walk = curr->required; walk && *walk; walk++)
	    if ((!last || strcmp(last,(*walk)->id) < 0) &&
	      (!best || strcmp(best->id,(*walk)->id) > 0))
		best = *walk;
	for (walk = curr->optional; walk && *walk; walk++)
	    if ((!last || strcmp(last,(*walk)->id) < 0) &&
	      (!best || strcmp(best->id,(*walk)->id) > 0))
		best = *walk;
	for (walk = special; walk && *walk; walk++)
	    if ((!last || strcmp(last,(*walk)->id) < 0) &&
	      (!best || strcmp(best->id,(*walk)->id) >= 0)) {
		is_special = 1;
		best = *walk;
	    }
	if (!best) break;
	last = best->id;
	if (is_special) {
	    if (handle_special) handle_special(file,best,special_arg);
	}
	else {
	    if (!best->present) continue;
	    if (curr == base && in_table(best,base->optional) && child &&
	      (in_table(best,child->required) ||
	      in_table(best,child->optional)))
		continue;
	    assert(best->type != dt_string && best->type != dt_list &&
	      best->type != dt_police && best->type != dt_bucket);
	    fprintf(file," %s %lu",best->id,(unsigned long) best->v);
	}
    }
}
