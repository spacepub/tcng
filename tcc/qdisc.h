/*
 * qdisc.h - Qdisc handling
 *
 * Written 2001-2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks, Network Robots
 * Copyright 2002,2003 Werner Almesberger
 */


#ifndef QDISC_H
#define QDISC_H

#include <stdint.h>

#include "tree.h"


extern QDISC_DSC ingress_dsc;
extern QDISC_DSC cbq_dsc;
extern QDISC_DSC dsmark_dsc;
extern QDISC_DSC egress_dsc;
extern QDISC_DSC fifo_dsc;
extern QDISC_DSC gred_dsc;
extern QDISC_DSC htb_dsc;
extern QDISC_DSC prio_dsc;
extern QDISC_DSC red_dsc;
extern QDISC_DSC sfq_dsc;
extern QDISC_DSC tbf_dsc;


void assign_qdisc_ids(QDISC *root);
CLASS *make_class(QDISC *qdisc,CLASS *parent_class,int implicit);
CLASS *get_any_class(QDISC *qdisc);
void add_default_fifos(QDISC *qdisc);
void check_qdisc(QDISC *qdisc);
void dump_qdisc(QDISC *qdisc);

uint32_t cbq_sanitize_classid_tc(const CLASS *class,int accept_invalid);

#endif /* QDISC_H */
