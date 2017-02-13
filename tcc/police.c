/*
 * police.c - Policing handling
 *
 * Written 2001,2002,2004 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks, Network Robots
 * Copyright 2004 Werner Almesberger
 */


#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "util.h"
#include "error.h"
#include "param.h"
#include "tree.h"
#include "tc.h"
#include "police.h"


POLICE *policers = NULL;
static POLICE **last = &policers;


/* ----- Assignment of unique IDs ------------------------------------------ */


static uint32_t *assigned_policer_ids = NULL,*curr_policer_id;


static void gather_assigned_policer_ids(void)
{
    POLICE *pol;
    int n = 0;

    for (pol = policers; pol; pol = pol->next)
	if (pol->number != UNDEF_U32) n++;
    assigned_policer_ids = alloc(sizeof(uint32_t)*(n+1));
    n = 0;
    for (pol = policers; pol; pol = pol->next)
	if (pol->number != UNDEF_U32) assigned_policer_ids[n++] = pol->number;
    qsort(assigned_policer_ids,n,sizeof(uint32_t),&comp_int);
    assigned_policer_ids[n] = UNDEF_U32;
    curr_policer_id = assigned_policer_ids;
}


static int policer_id_is_assigned(uint32_t id)
{
    while (*curr_policer_id != UNDEF_U32 && *curr_policer_id < id)
	curr_policer_id++;
    return *curr_policer_id == id;
}


void assign_policer_ids(POLICE *list)
{
    static uint32_t ind = 1;
    POLICE *walk;

    if (!assigned_policer_ids) gather_assigned_policer_ids();
    for (walk = list; walk; walk = walk->next)
	if (walk->number == UNDEF_U32) {
	    while (policer_id_is_assigned(ind)) ind++;
	    walk->number = ind++;
	}
}


/* ----- Policing ---------------------------------------------------------- */


static const PARAM_DSC *police_rate_req[] = {
    &prm_burst,
    NULL
};

static const PARAM_DSC *police_norate_opt[] = {
    &prm_avrate,
    NULL
};

static const PARAM_DSC *police_opt[] = {
    &prm_avrate,	/* rate */
    &prm_burst,		/* size */
    &prm_mpu,		/* size */
    &prm_mtu,		/* size */
    &prm_overflow,	/* police */
    &prm_pragma,	/* list */
    &prm_peakrate,	/* rate */
    &prm_rate,		/* rate */
    NULL
};


static const char *police_action(DECISION act)
{
    switch (act) {
	case pd_ok:
	    return "pass";
	case pd_drop:
	    return "drop";
	case pd_continue:
	    return "continue";
	case pd_reclassify:
	    return "reclassify";
	default:
	    abort();
    }
}


void check_police(POLICE *police)
{
    assign_policer_ids(policers);
    param_get(police->params,police->location);
    if (!prm_rate.present)
	check_params(police->params,NULL,police_norate_opt,police->location);
    else
	check_params(police->params,police_rate_req,police_opt,
	  police->location);
    police->used = 1;
}


static void dump_police_tc(POLICE *police)
{
    tc_pragma(police->params);
    param_get(police->params,police->location);
    tc_more(" police");
    tc_more(" index %lu",(unsigned long) police->number);
    if (police->created) return;
    police->created = 1;
    if (prm_avrate.present) tc_add_rate("avrate",prm_avrate.v);
    if (prm_rate.present) {
	tc_add_rate("rate",prm_rate.v);
	tc_add_size("burst",prm_burst.v);
	if (prm_peakrate.present) {
	    tc_add_rate("peakrate",prm_peakrate.v);
	    if (!prm_mtu.present)
		lerror(police->location,
		  "\"tc\" target requires parameter \"mtu\"");
	    tc_add_size("mtu",prm_mtu.v);
	}
	if (prm_mpu.present) {
	    if (prm_mpu.v > 255)
		lwarn(police->location,
		  "some versions of \"tc\" only support \"mpu\" < 256");
	    tc_add_size("mpu",prm_mpu.v);
	}
    }
    tc_more(
#ifdef CONFIRM_EXCEED
      " conform-exceed %s/%s",
#else
      " action %s/%s",
#endif
      police_action(police->out_profile),
      police_action(police->in_profile));
}


void dump_police(POLICE *police)
{
    dump_police_tc(police);
}


PARAM_DEF police_def = {
    .required = NULL,
    .optional = police_opt,
};


/* ----- Buckets ----------------------------------------------------------- */

/*
 * Buckets are just policers, but with fewer parameters.
 */


void check_bucket(POLICE *police)
{
    assign_policer_ids(policers);
    police->used = 1;
}


static const PARAM_DSC *bucket_req[] = {
    &prm_burst,		/* size */
    &prm_rate,		/* rate */
    NULL
};

static const PARAM_DSC *bucket_opt[] = {
    &prm_mpu,		/* size */
    &prm_overflow,	/* police */
    &prm_pragma,	/* list */
    NULL
};


PARAM_DEF bucket_def = {
    .required = bucket_req,
    .optional = bucket_opt,
};


/* ----- Drop policer ------------------------------------------------------ */


POLICE *get_drop_policer(LOCATION loc)
{
    POLICE *police;

    for (police = policers; police; police = police->next)
	if (police->in_profile == pd_drop && police->out_profile == pd_drop)
	    break;
    if (!police) {
	police = alloc_t(POLICE);
	police->number = UNDEF_U32;
	police->params = NULL;
	param_add(&police->params,
	  param_make(&prm_burst,data_typed_fnum(1.0,dt_size)));
	param_add(&police->params,
	  param_make(&prm_rate,data_typed_fnum(8.0,dt_rate)));
	police->location = loc;
	police->in_profile = police->out_profile = pd_drop;
	add_police(police);
    }
    if (!police->used) {
	assign_policer_ids(policers);
	police->used = 1;
    }
    return police;
}


/* ------------------------------------------------------------------------- */


void add_police(POLICE *police)
{
    if (!police->number) error("policer index must not be zero");
    if (police->number != UNDEF_U32) {
	const POLICE *check;

	for (check = policers; check; check = check->next)
	    if (check->number == police->number)
		lerrorf(check->location,"duplicate policer index %d",
		  (int) police->number);
    }
    police->used = 0;
    police->created = 0;
    *last = police;
    last = &police->next;
    police->next = NULL;
}
