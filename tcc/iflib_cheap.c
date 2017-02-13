/*
 * iflib_cheap.c - Detect and report "expensive" expressions
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 Network Robots
 * Copyright 2002 Werner Almesberger
 */


#include "config.h"
#include "error.h"
#include "data.h"
#include "op.h"
#include "iflib.h"


static void report_expensive(LOCATION loc,const char *msg)
{
    if (warn_exp_error) lerror(loc,msg);
    lwarn(loc,msg);
}


/* ----- Not self-contained actions trees can get very expensive ----------- */


static int do_report = 1;


static int refuse_static_after_policing(LOCATION loc,DATA d,int next_ok,
  int otherwise_ok)
{
    debugf("refuse_static_after_policing(\"%s\",next_ok %d,otherwise_ok %d)",
      !d.op ? d.type == dt_decision ? "decision" : "-" :
      d.op->dsc == &op_logical_or ? "||" :
      d.op->dsc == &op_logical_and ? "&&" :
      d.op->dsc == &op_logical_not ? "!" :
      d.op->dsc == &op_count ? "count" :
      d.op->dsc == &op_conform ? "conform" : "???",
      next_ok,otherwise_ok);
    if (!d.op) return d.type == dt_decision;
    if (d.op->dsc == &op_logical_and)
	return refuse_static_after_policing(loc,d.op->a,
	  refuse_static_after_policing(loc,d.op->b,next_ok,otherwise_ok),
	  otherwise_ok);
    if (d.op->dsc == &op_logical_or)
	return refuse_static_after_policing(loc,d.op->a,next_ok,
	  refuse_static_after_policing(loc,d.op->b,next_ok,otherwise_ok));
    if (d.op->dsc == &op_logical_not)
	return refuse_static_after_policing(loc,d.op->a,otherwise_ok,next_ok);
    if (d.op->dsc == &op_conform) {
	if (next_ok && otherwise_ok) return 1;
	if (do_report) {
	    report_expensive(loc,"policing expressions not always leading to"
	     " a decision are \"expensive\"");
	    do_report = 0;
	}
    }
    if (d.op->dsc == &op_count) {
	if (next_ok) return 1;
	if (do_report) {
	    report_expensive(loc,"policing expressions not always leading to"
	      " a decision are \"expensive\"");
	    do_report = 0;
	}
    }
    return 0;
}


void iflib_cheap_actions(LOCATION loc,DATA d)
{
    if ((!warn_expensive && !warn_exp_post_opt) || use_bit_tree) return;
    debug_expr("BEFORE iflib_cheap_actions",d);
    (void) refuse_static_after_policing(loc,d,0,0);
}


/* ----- Negation is almost always expensive ------------------------------- */


static int report_not = 1,report_ne = 1,report_gt = 1,report_ge = 1;


static void refuse_expensive_ops(LOCATION loc,DATA d)
{
    if (!d.op) return;
    if (!warn_exp_post_opt) {
	if (d.op->dsc == &op_logical_not && report_not) {
	    report_expensive(loc,"\"!\" is an \"expensive\" operation");
	    report_not = 0;
	}
	if (d.op->dsc == &op_ne && report_ne) {
	    report_expensive(loc,"\"!=\" is an \"expensive\" operation");
	    report_ne = 0;
	}
	if (d.op->dsc == &op_gt && report_gt) {
	    report_expensive(loc,"\">\" is an \"expensive\" operation");
	    report_gt = 0;
	}
	if (d.op->dsc == &op_ge && report_ge) {
	    report_expensive(loc,"\">=\" is an \"expensive\" operation");
	    report_ge = 0;
	}
    }
    refuse_expensive_ops(loc,d.op->a);
    refuse_expensive_ops(loc,d.op->b);
    refuse_expensive_ops(loc,d.op->c);
}


void iflib_cheap_ops(LOCATION loc,DATA d)
{
    if ((!warn_expensive && !warn_exp_post_opt) || use_bit_tree) return;
    debug_expr("BEFORE iflib_cheap_ops",d);
    refuse_expensive_ops(loc,d);
}
