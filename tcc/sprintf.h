/*
 * sprintf.h - sprintf for DATA
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks
 */


#ifndef SPRINTF_H
#define SPRINTF_H


#include "data.h"


char *sprintf_data(const char *fmt,const DATA_LIST *data_list);

#endif /* SPRINTF_H */
