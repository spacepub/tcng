/*
 * device.c - Device handling
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2002 Bivio Networks
 */


#include <string.h>

#include "config.h"
#include "error.h"
#include "tc.h"
#include "tree.h"
#include "qdisc.h"
#include "device.h"


int add_fifos = 0; /* add FIFOs default qdisc is used */

DEVICE *devices = NULL;


static const PARAM_DSC *device_opt[] = {
    &prm_pragma,	/* list */
    NULL
};

PARAM_DEF device_def = {
    .required = NULL,
    .optional = device_opt,
};


int guess_mtu(DEVICE *device)
{
    return 1510; /* @@@ */
}


void add_device(DEVICE *device)
{
    DEVICE **walk;

    for (walk = &devices; *walk; walk = &(*walk)->next)
	if (!strcmp((*walk)->name,device->name))
	    yyerror("duplicate device name");
    *walk = device;
}


void check_devices(void)
{
    DEVICE *device;

    for (device = devices; device; device = device->next) {
	if (device->ingress) {
	    device->ingress->number = 0xffff; /* ingress magic number */
	    assign_qdisc_ids(device->ingress);
	    check_qdisc(device->ingress);
	}
	if (device->egress) {
	    if (add_fifos) add_default_fifos(device->egress);
	    assign_qdisc_ids(device->egress);
	    check_qdisc(device->egress);
	}
    }
}


void dump_devices(void)
{
    DEVICE *device;

    for (device = devices; device; device = device->next) {
	if (remove_qdiscs) {
	    tc_more("tc qdisc del dev %s root",device->name);
	    tc_nl();
	    tc_more("tc qdisc del dev %s ingress",device->name);
	    tc_nl();
	}
	if (!device->egress && !device->ingress) continue;
	tc_comment('=',"Device %s",device->name);
	if (device->ingress) dump_qdisc(device->ingress);
	if (device->egress) dump_qdisc(device->egress);
    }
}
