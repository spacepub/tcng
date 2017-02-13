/*
 * tc.h - TC command generator
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#ifndef TC_H
#define TC_H

#include <inttypes.h>

#include "data.h"
#include "param.h"
#include "tree.h"


void __tc_qdisc_add(const QDISC *qdisc,const char *name);
void tc_qdisc_add(const QDISC *qdisc);
void __tc_class_op(const CLASS *class,const char *name,const char *op);
void tc_qdisc_change(const QDISC *qdisc);
void tc_class_add(const CLASS *class);
void tc_class_change(const CLASS *class);
void __tc_filter_add(const FILTER *filter,uint16_t protocol);
void __tc_filter_add2(const FILTER *filter,uint16_t protocol,int prio_offset);
void tc_filter_add(const FILTER *filter,uint16_t protocol);
void tc_element_add(const ELEMENT *element,uint16_t protocol);
void tc_comment(char pad_char,const char *fmt,...);
void tc_more(const char *fmt,...);
void tc_add_unum(const char *name,uint32_t value);
void tc_add_hex(const char *name,uint32_t value);
void tc_add_rate(const char *name,uint32_t value);
void tc_add_time(const char *name,uint32_t value);
void tc_add_fnum(const char *name,DATA data);
void tc_add_string(const char *name,DATA data);
void tc_add_classid(const CLASS *class,int accept_invalid);
void tc_add_ipv4(const char *name,uint32_t value);
void tc_nl(void);

#define tc_add_size tc_add_unum
#define tc_add_psize tc_add_unum

void tc_pragma(const PARAM *params);

#endif /* TC_H */
