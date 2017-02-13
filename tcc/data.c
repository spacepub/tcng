/*
 * data.c - Typed data containers
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots, Werner Almesberger
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <u128.h>

#include "util.h"
#include "tree.h"
#include "op.h"
#include "field.h"
#include "error.h"
#include "data.h"


/* ----- Type-specific creator functions ----------------------------------- */


DATA data_none(void)
{
    DATA d;

    d.type = dt_none;
    d.op = NULL;
    return d;
}


DATA data_unum(uint32_t in)
{
    DATA d;

    d.type = dt_unum;
    d.op = NULL;
    d.u.unum = in;
    return d;
}


DATA data_ipv4(uint32_t in)
{
    DATA d;

    d.type = dt_ipv4;
    d.op = NULL;
    d.u.unum = in;
    return d;
}


DATA data_ipv6(U128 in)
{
    DATA d;

    d.type = dt_ipv6;
    d.op = NULL;
    d.u.u128 = in;
    return d;
}


DATA data_fnum(double in)
{
    DATA d;

    d.type = dt_fnum;
    d.op = NULL;
    d.u.fnum = in;
    return d;
}


DATA data_typed_fnum(double in,DATA_TYPE type)
{
    DATA d;

    d.type = type;
    d.op = NULL;
    d.u.fnum = in;
    return d;
}


DATA data_string(const char *in)
{
    DATA d;

    d.type = dt_string;
    d.op = NULL;
    d.u.string = stralloc(in);
    return d;
}


DATA data_device(DEVICE *in)
{
    DATA d;

    d.type = dt_device;
    d.op = NULL;
    d.u.device = in;
    return d;
}


DATA data_qdisc(QDISC *in)
{
    DATA d;

    d.type = dt_qdisc;
    d.op = NULL;
    d.u.qdisc = in;
    return d;
}


DATA data_class(CLASS *in)
{
    DATA d;

    d.type = dt_class;
    d.op = NULL;
    d.u.class = in;
    return d;
}


DATA data_decision(DECISION_RESULT result,CLASS *class)
{
    DATA d;

    d.type = dt_decision;
    d.op = NULL;
    d.u.decision.result = result;
    d.u.decision.class = class;
    return d;
}


DATA data_filter(FILTER *in)
{
    DATA d;

    d.type = dt_filter;
    d.op = NULL;
    d.u.filter = in;
    return d;
}


DATA data_tunnel(TUNNEL *in)
{
    DATA d;

    d.type = dt_tunnel;
    d.op = NULL;
    d.u.tunnel = in;
    return d;
}


DATA data_police(POLICE *in)
{
    DATA d;

    d.type = dt_police;
    d.op = NULL;
    d.u.police = in;
    return d;
}


DATA data_bucket(POLICE *in)
{
    DATA d;

    d.type = dt_bucket;
    d.op = NULL;
    d.u.police = in;
    return d;
}


DATA data_list(DATA_LIST *list)
{
    DATA d;

    d.type = dt_list;
    d.op = NULL;
    d.u.list = list;
    return d;
}


DATA_LIST *data_list_element(DATA *ref)
{
    DATA_LIST *elem;

    elem = alloc_t(DATA_LIST);
    elem->ref = ref;
    elem->next = NULL;
    return elem;
}


DATA data_field(FIELD *field)
{
    DATA d;

    d.type = dt_field;
    d.op = NULL;
    d.u.field = field;
    return d;
}


DATA data_field_root(FIELD_ROOT *field_root)
{
    DATA d;

    d.type = dt_field_root;
    d.op = NULL;
    d.u.field_root = field_root;
    return d;
}


DATA data_assoc(void)
{
    DATA d;

    d.type = dt_assoc;
    d.op = NULL;
    d.u.assoc = NULL;
    return d;
}


/* ----- Type conversion and helper functions ------------------------------ */


const char *type_name(DATA_TYPE type)
{
    switch (type) {
	case dt_none:
	    return "none";
	case dt_unum:
	    return "integer";
	case dt_ipv4:
	    return "IPv4 address";
	case dt_ipv6:
	    return "IPv6 address";
	case dt_fnum:
	    return "floating-point number";
	case dt_string:
	    return "string";
	case dt_rate:
	    return "rate";
	case dt_size:
	    return "size in bytes";
	case dt_prate:
	    return "packet rate";
	case dt_psize:
	    return "size in packets";
	case dt_time:
	    return "time";
	case dt_device:
	    return "device";
	case dt_qdisc:
	    return "queuing discipline";
	case dt_class:
	    return "class";
	case dt_filter:
	    return "filter";
	case dt_tunnel:
	    return "tunnel";
	case dt_police:
	    return "policer";
	case dt_bucket:
	    return "bucket";
	case dt_list:
	    return "list";
	case dt_field:
	    return "field";
	case dt_field_root:
	    return "field_root";
	case dt_decision:
	    return "decision";
	case dt_assoc:
	    return "structure";
	default:
	    abort();
    }
}


DATA data_convert(DATA in,DATA_TYPE type)
{
    if (in.op) yyerror("constant expression required");
    if (in.type == type) return in;
    if (in.type == dt_ipv4 && type == dt_unum) /* do nothing */;
    else if (in.type == dt_unum && type == dt_ipv4) /* do nothing */;
    else if (in.type == dt_ipv6 && type == dt_unum)
	  in.u.unum = u128_to_32(in.u.u128);
    else if ((in.type == dt_unum || in.type == dt_ipv4) && type == dt_ipv6)
	  in.u.u128 = u128_from_32(in.u.unum);
    else if (in.type == dt_fnum && type == dt_unum) in.u.unum = in.u.fnum;
    else if (in.type == dt_unum && type == dt_fnum) in.u.fnum = in.u.unum;
    else yyerrorf("invalid type conversion (expected %s instead of %s)",
	  type_name(type),type_name(in.type));
    in.type = type;
    return in;
}


DATA data_add_unit(DATA in,DATA unit)
{
    if (unit.type == dt_none) return in;
    if (in.type != dt_fnum) {
	if (in.type != dt_unum)
	    yyerrorf("invalid type conversion (expected number instead of %s)",
	      type_name(in.type));
	in.u.fnum = in.u.unum;
    }
    in.type = unit.type;
    in.u.fnum *= unit.u.fnum;
    return in;
}


/* ----- Cloning ----------------------------------------------------------- */


static DATA data_clone_assoc(DATA d)
{
    DATA clone = d;
    DATA_ASSOC *entry,**next = &clone.u.assoc;

    for (entry = d.u.assoc; entry; entry = entry->next) {
	*next = alloc_t(DATA_ASSOC);
	(*next)->name = stralloc(entry->name);
	(*next)->data = data_clone(entry->data);
	(*next)->parent = NULL;
	if ((*next)->data.type == dt_assoc) {
	    DATA_ASSOC *walk;

	    for (walk = (*next)->data.u.assoc; walk; walk = walk->next)
		walk->parent = entry;
	}
	next = &(*next)->next;
    }
    *next = NULL;
    return clone;
}


DATA data_clone(DATA d)
{
    OP *new_op;

    if (!d.op) {
	if (d.type == dt_assoc) return data_clone_assoc(d);
	return d;
    }
    new_op = alloc_t(OP);
    *new_op = *d.op;
    new_op->a = data_clone(d.op->a);
    new_op->b = data_clone(d.op->b);
    new_op->c = data_clone(d.op->c);
    d.op = new_op;
    return d;
}


/* ----- Comparison -------------------------------------------------------- */


int data_equal(DATA a,DATA b)
{
    assert(!a.op && !b.op);
    if (a.type != b.type) return 0;
    switch (a.type) {
	case dt_none:
	    return 1;
	case dt_unum:
	case dt_ipv4:
	    return a.u.unum == b.u.unum;
	case dt_ipv6:
	    return u128_cmp(a.u.u128,b.u.u128) == 0;
	case dt_fnum:
	case dt_rate:
	case dt_prate:
	case dt_size:
	case dt_psize:
	case dt_time:
	    return a.u.fnum == b.u.fnum;
	case dt_string:
	    return !strcmp(a.u.string,b.u.string);
	case dt_qdisc:
	    return a.u.qdisc == b.u.qdisc;
	case dt_class:
	    return a.u.class == b.u.class;
	case dt_decision:
	    return (a.u.decision.result == b.u.decision.result) &&
	      ((a.u.decision.result != dr_class && a.u.decision.result !=
	      dr_reclassify) || a.u.decision.class == b.u.decision.class);
	case dt_filter:
	    return a.u.filter == b.u.filter;
	case dt_police:
	case dt_bucket:
	    return a.u.police == b.u.police;
	case dt_field_root:
	    return a.u.field_root == b.u.field_root;
	default:
	    errorf("data_equal can't handle type %s",type_name(a.type));
    }
    return 0; /* not reached */
}


/* ----- Destruction ------------------------------------------------------- */


static void data_destroy_assoc(DATA d)
{
    while (d.u.assoc) {
	DATA_ASSOC *next = d.u.assoc->next;

	free((void *) d.u.assoc->name);
	data_destroy(d.u.assoc->data);
	free(d.u.assoc);
	d.u.assoc = next;
    }
}


void data_destroy_1(DATA d)
{
    if (d.op) free(d.op);
    else if (d.type == dt_assoc) data_destroy_assoc(d);
}


void data_destroy(DATA d)
{
    if (d.op) {
	data_destroy(d.op->a);
	data_destroy(d.op->b);
	data_destroy(d.op->c);
    }
    data_destroy_1(d);
}


/* ----- Printing ---------------------------------------------------------- */


static void print_ipv6(FILE *file,const uint32_t *a)
{
    int i;

    for (i = 3; i >= 0; i--)
	fprintf(file,"%X:%X%s",a[i] >> 16,a[i] & 0xffff,i ? ":" : "");
}


void print_data(FILE *file,DATA d)
{
    DATA_ASSOC *entry;

    switch (d.type) {
	case dt_none:
	    fprintf(file,"(none)");
	    break;
	case dt_unum:
	    fprintf(file,"%lu",(unsigned long) d.u.unum);
	    break;
	case dt_ipv4:
	    fprintf(file,"%u.%u.%u.%u",d.u.unum >> 24,(d.u.unum >> 16) & 0xff,
	      (d.u.unum >> 8) & 0xff,d.u.unum & 0xff);
	    break;
	case dt_ipv6:
	    print_ipv6(file,d.u.u128.v);
	    break;
	case dt_fnum:
	    fprintf(file,"%#g",d.u.fnum);
	    break;
	case dt_string:
	    fprintf(file,"\"%s\"",d.u.string);
	    break;
	case dt_rate:
	    fprintf(file,"%g bps",d.u.fnum);
	    break;
	case dt_size:
	    fprintf(file,"%g B",d.u.fnum);
	    break;
	case dt_prate:
	    fprintf(file,"%g pps",d.u.fnum);
	    break;
	case dt_psize:
	    fprintf(file,"%g p",d.u.fnum);
	    break;
	case dt_time:
	    fprintf(file,"%g s",d.u.fnum);
	    break;
	case dt_device:
	    fprintf(file,"<device>");
	    break;
	case dt_qdisc:
	    fprintf(file,"<qdisc>");
	    break;
	case dt_class:
	    fprintf(file,"<class %lx:%lx>",
	      (unsigned long) d.u.class->parent.qdisc->number,
	      (unsigned long) d.u.class->number);
	    break;
	case dt_decision:
	    switch (d.u.decision.result) {
		case dr_class:
		case dr_reclassify:
		    fprintf(file,"<%s %lx:%lx>",
		      d.u.decision.result == dr_class ? "class" : "reclassify",
       		      (unsigned long) d.u.decision.class->parent.qdisc->number,
		      (unsigned long) d.u.decision.class->number);
		    break;
		case dr_continue:
		    fprintf(file,"<continue>");
		    break;
		case dr_drop:
		    fprintf(file,"<drop>");
		    break;
		default:
		    errorf("internal error: unknown decision result %d",
		      d.u.decision.result);
	    }
	    break;
	case dt_filter:
	    fprintf(file,"<filter>");
	    break;
	case dt_tunnel:
	    fprintf(file,"<tunnel>");
	    break;
	case dt_police:
	    fprintf(file,"<police %lu>",(unsigned long) d.u.police->number);
	    break;
	case dt_bucket:
	    fprintf(file,"<bucket %lu>",(unsigned long) d.u.police->number);
	    break;
	case dt_list:
	    fprintf(file,"<list>");
	    break;
	case dt_field:
	    fprintf(file,"%s",d.u.field->name);
	    break;
	case dt_field_root:
	    fprintf(file,"<field root %d>",d.u.field_root->offset_group);
	    break;
	case dt_assoc:
	    fprintf(file,"{ ");
	    for (entry = d.u.assoc; entry; entry = entry->next) {
		fprintf(file,".%s = ",entry->name);
		print_data(file,entry->data);
		fprintf(file,"; ");
	    }
	    fprintf(file,"}");
	    break;
	default:
	    errorf("internal error: unknown data type %d",d.type);
    }
}
