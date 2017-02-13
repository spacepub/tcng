/*
 * f_rsvp.c - RSVP filter (with tunnels)
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Werner Almesberger
 */


#include <stdint.h>
#include <stddef.h>

#include "util.h"
#include "error.h"
#include "param.h"
#include "tree.h"
#include "tc.h"
#include "police.h"
#include "filter_common.h"
#include "filter.h"


/* -------------------------------- Tunnels -------------------------------- */


static void assign_tunnel_ids(ELEMENT *elements)
{
    ELEMENT *element;
    uint32_t id = 1;

    for (element = elements; element; element = element->next) {
	if (!element->parent.tunnel) continue;
	if (element->parent.tunnel->number != UNDEF_U32) {
	    id = element->parent.tunnel->number;
	    if (!id || id > 255)
		errorf("tunnel ID must be in the range 1..255, not %lu",
		  (unsigned long) id);
	}
	while (1) {
	    ELEMENT *walk;

	    for (walk = elements; walk; walk = walk->next)
		if (walk->parent.tunnel &&
		  walk->parent.tunnel != element->parent.tunnel &&
		  walk->parent.tunnel->number == id)
		    break;
	    if (!walk) break;
	    if (element->parent.tunnel->number != UNDEF_U32)
		errorf("duplicate tunnel ID %lu",id);
	    if (++id == 256) id = 1;
	}
	element->parent.tunnel->number = id;
    }
}


static const PARAM_DSC *tunnel_req[] = {
    &prm_skip,		/* size */
    NULL
};

static const PARAM_DSC *tunnel_opt[] = {
    &prm_pragma,	/* list */
    NULL
};

PARAM_DEF tunnel_param = {
    .required = tunnel_req,
    .optional = tunnel_opt,
};


/* --------------------------------- RSVP ---------------------------------- */


#define __DEFAULT_PRM(f) f(dst) f(src) f(sport) f(dport) f(ipproto) f(ah) f(esp)
#define __DEFAULT_PRM_REQ(f) f(dst)


enum protocol_style { ps_none,ps_ah,ps_esp,ps_ipproto };


static enum protocol_style get_protocol(enum protocol_style proto)
{
    int protocols = 0;

    if (proto != dt_none) return proto;
    if (prm_ah.present) {
	protocols++;
	proto = ps_ah;
    }
    if (prm_esp.present) {
	protocols++;
	proto = ps_esp;
    }
    if (prm_ipproto.present) {
	protocols++;
	proto = ps_ipproto;
    }
    if (protocols > 1)
	error("exactly one of \"ah\", \"esp\", or \"ipproto\" must be "
	  "specified");
    return proto;
}


/* ------------------------------- Checking -------------------------------- */


static void rsvp_check(FILTER *filter)
{
    DEFAULT_DECL;
    ELEMENT *element;

    param_get(filter->params,filter->location);
    DEFAULT_SET;
    (void) get_protocol(ps_none);
    if (!filter->elements) warnf("RSVP filter without elements is useless");
    assign_tunnel_ids(filter->elements);
    for (element = filter->elements; element; element = element->next) {
	enum protocol_style protocol_style;

	param_get(element->params,element->location);
	protocol_style = get_protocol(ps_none);
	DEFAULT_GET;
	DEFAULT_CHECK(element->location);
	protocol_style = get_protocol(protocol_style);
	if (element->number != UNDEF_U32)
	    error("rsvp has no user-settable element numbers");
	if ((protocol_style == ps_ah || protocol_style == ps_esp) &&
	  (prm_sport.present || prm_dport.present))
	    error("specify either ports or \"ah\"/\"esp\"");
	if (protocol_style == ps_none)
	    error("exactly one of \"ah\", \"esp\", or \"ipproto\" must be "
	      "specified");
        if (element->police) check_police(element->police);
    }
}


/* -------------------------------- Dump tc -------------------------------- */


static void rsvp_dump_tc(FILTER *filter)
{
    DEFAULT_DECL;
    ELEMENT *element;

    tc_pragma(filter->params);
    param_get(filter->params,filter->location);
    DEFAULT_SET;
    tc_filter_add(filter,0);
    tc_nl();
    for (element = filter->elements; element; element = element->next) {
	enum protocol_style protocol_style;

	tc_pragma(filter->params);
	param_get(element->params,element->location);
	protocol_style = get_protocol(ps_none);
	DEFAULT_GET;
	protocol_style = get_protocol(protocol_style);
	tc_element_add(element,0);
	if (protocol_style == ps_ipproto) tc_add_unum("ipproto",prm_ipproto.v);
	tc_add_ipv4("session",prm_dst.v);
	if (prm_dport.present) tc_more("/%d",(int) prm_dport.v);
	if (protocol_style == ps_ah) tc_add_hex("spi/ah",prm_ah.v);
	if (protocol_style == ps_esp) tc_add_hex("spi/esp",prm_esp.v);
	if (prm_src.present) tc_add_ipv4("sender",prm_src.v);
	if (prm_sport.present) {
	    if (!prm_src.present) tc_more(" sender any");
	    tc_more("/%d",(int) prm_sport.v);
	}
	if (element->parent.class != &class_is_tunnel) {
	    tc_add_classid(element->parent.class,0);
	    if (element->parent.tunnel)
		tc_add_unum("tunnelid",element->parent.tunnel->number);
	}
	else {
	    if (element->parent.tunnel->parent.tunnel)
		tc_add_unum("tunnelid",
		  element->parent.tunnel->parent.tunnel->number);
	    tc_pragma(element->parent.tunnel->params);
	    param_get(element->parent.tunnel->params,
	      element->parent.tunnel->location);
	    tc_add_unum("tunnel",element->parent.tunnel->number);
	    tc_add_unum("skip",prm_skip.v);
	}
	if (element->police) dump_police(element->police);
	tc_nl();
    }
}


/* ------------------------------ Descriptors ------------------------------ */


static const PARAM_DSC *rsvp_filter_opt[] = {
    &prm_pragma,	/* list */
    &prm_protocol,	/* unum */
    DEFAULT_LIST	/* NB: no trailing comma */
    NULL
};

static const PARAM_DSC *rsvp_element_opt[] = {
    &prm_pragma,	/* list */
    DEFAULT_LIST	/* NB: no trailing comma */
    NULL
};

static PARAM_DEF rsvp_filter = {
    .required = NULL,
    .optional = rsvp_filter_opt,
};

static PARAM_DEF rsvp_element = {
    .required = NULL,
    .optional = rsvp_element_opt,
};

FILTER_DSC rsvp_dsc = {
    .name = "rsvp",
    .filter_param = &rsvp_filter,
    .element_param = &rsvp_element,
    .check = rsvp_check,
    .dump_tc = rsvp_dump_tc,
};
