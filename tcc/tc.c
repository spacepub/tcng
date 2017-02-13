/*
 * tc.c - TC command generator
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/if_ether.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "data.h"
#include "param.h"
#include "tree.h"
#include "named.h"
#include "qdisc.h"
#include "tc.h"


static void tc_qdisc_op(const QDISC *qdisc,const char *name,const char *op)
{
    printf("tc qdisc %s dev %s ",op,qdisc->parent.device->name);
    if (qdisc->dsc != &ingress_dsc) {
	printf("handle %x:0 ",(int) qdisc->number);
	if (qdisc->parent.qdisc)
	    printf("parent %x:%x ",qdisc->parent.qdisc->number,
	      qdisc->parent.class ? qdisc->parent.class->number : 0);
	else printf("root ");
    }
    printf("%s",name);
}


void __tc_qdisc_add(const QDISC *qdisc,const char *name)
{
    tc_qdisc_op(qdisc,name,"add");
}


void tc_qdisc_add(const QDISC *qdisc)
{
    __tc_qdisc_add(qdisc,qdisc->dsc->name);
}


void tc_qdisc_change(const QDISC *qdisc)
{
    tc_qdisc_op(qdisc,qdisc->dsc->name,"change");
}


void tc_add_classid(const CLASS *class,int accept_invalid)
{
    printf(" classid %x:%x",(int) class->parent.qdisc->number,
      class->parent.qdisc->dsc != &cbq_dsc ? (int) class->number :
      (int) cbq_sanitize_classid_tc(class,accept_invalid));
}


void __tc_class_op(const CLASS *class,const char *name,const char *op)
{
    printf("tc class %s dev %s",op,class->parent.device->name);
    if (class->parent.class)
	printf(" parent %x:%x",(int) class->parent.qdisc->number,
	  (int) class->parent.class->number);
    printf(" classid %x:%x",(int) class->parent.qdisc->number,
      (int) class->number);
    printf(" %s",name);
}


static void tc_class_op(const CLASS *class,const char *op)
{
    __tc_class_op(class,class->parent.qdisc->dsc->name,op);
}


void tc_class_add(const CLASS *class)
{
    tc_class_op(class,"add");
}


void tc_class_change(const CLASS *class)
{
    tc_class_op(class,"change");
}


static void tc_protocol_add(const FILTER *filter,uint16_t protocol)
{
    printf(" protocol ");
    if (prm_present(filter->params,&prm_protocol))  {
	printf("%s",
	  ether_proto_print(prm_data(filter->params,&prm_protocol).u.unum));
	return;
    }
    if (!protocol) {
	printf("ip");
	return;
    }
    /* Pretty-print a few well-known protocol numbers. */
    switch (protocol) {
	case ETH_P_IP:
	    printf("ip");
	    break;
	case ETH_P_IPV6:
	    printf("ipv6");
	    break;
	case ETH_P_ALL:
	    printf("all");
	    break;
	default:
	    printf("%u",protocol);
    }
}


void __tc_filter_add2(const FILTER *filter,uint16_t protocol,int prio_offset)
{
    printf("tc filter add dev %s parent %x:%x",
      filter->parent.device->name,(int) filter->parent.qdisc->number,
      filter->parent.class ? (int) filter->parent.class->number : 0);
    tc_protocol_add(filter,protocol);
    printf(" prio %d",(int) filter->number+prio_offset);
}


void __tc_filter_add(const FILTER *filter,uint16_t protocol)
{
    __tc_filter_add2(filter,protocol,0);
}


void tc_filter_add(const FILTER *filter,uint16_t protocol)
{
    __tc_filter_add(filter,protocol);
    printf(" %s",filter->dsc->name);
}


void tc_element_add(const ELEMENT *element,uint16_t protocol)
{
    printf("tc filter add dev %s parent %x:%x",
      element->parent.filter->parent.device->name,
      (int) element->parent.filter->parent.qdisc->number,
      element->parent.filter->parent.class ?
      (int) element->parent.filter->parent.class->number : 0);
    tc_protocol_add(element->parent.filter,protocol);
    printf(" prio %d",(int) element->parent.filter->number);
    if (element->number != UNDEF_U32)
	printf(" handle %lu",(unsigned long) element->number);
    printf(" %s",element->parent.filter->dsc->name);
}


void tc_comment(char pad_char,const char *fmt,...)
{
    char buf[MAX_COMMENT];
    va_list ap;
    int pad,i;

    if (quiet) return;
    va_start(ap,fmt);
    vsnprintf(buf,sizeof(buf),fmt,ap);
    va_end(ap);
    printf("\n# ");
    pad = 79-2-strlen(buf)-2; /* width-"# "-text-surrounding_spaces */
    if (pad < 0) pad = 0;
    for (i = pad >> 1; i; i--) putchar(pad_char);
    printf(" %s ",buf);
    for (i = (pad+1) >> 1; i; i--) putchar(pad_char);
    printf("\n\n");
}


void tc_more(const char *fmt,...)
{
    va_list ap;

    va_start(ap,fmt);
    vprintf(fmt,ap);
    va_end(ap);
}


void tc_add_unum(const char *name,uint32_t value)
{
    printf(" %s %lu",name,(unsigned long) value);
}


void tc_add_hex(const char *name,uint32_t value)
{
    printf(" %s 0x%lx",name,(unsigned long) value);
}


void tc_add_fnum(const char *name,DATA data)
{
    printf(" %s %f",name,data.u.fnum);
}


void tc_add_rate(const char *name,uint32_t value)
{
    /* "bps" here is Bytes, not bits ... */
    printf(" %s %lubps",name,(unsigned long) value);
}


void tc_add_time(const char *name,uint32_t value)
{
    printf(" %s %lu",name,(unsigned long) value);
}


void tc_add_string(const char *name,DATA data)
{
    printf(" %s %s",name,data.u.string);
}


void tc_add_ipv4(const char *name,uint32_t value)
{
    printf(" %s %u.%u.%u.%u",name,value >> 24,(value >> 16) & 0xff,
      (value >> 8) & 0xff,value & 0xff);
}


void tc_nl(void)
{
    printf("\n");
}


void tc_pragma(const PARAM *params)
{
    if (prm_present(params,&prm_pragma))
	yywarn("\"pragma\" parameter ignored by \"tc\" target");
}
