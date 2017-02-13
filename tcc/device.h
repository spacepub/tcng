/*
 * device.h - Device handling
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 */


#ifndef DEVICE_H
#define DEVICE_H

#include "tree.h"
#include "param.h"


extern DEVICE *devices;
extern PARAM_DEF device_def;


int guess_mtu(DEVICE *device);

void add_device(DEVICE *device);
void check_devices(void);
void dump_devices(void);

#endif /* DEVICE_H */
