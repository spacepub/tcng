/*
 * module.c - Load "kernel modules"
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Werner Almesberger
 */
 

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include <memutil.h>

#include "tcsim.h"


void *tc_module(char prefix,const char *name); /* extern in ulib */

void *tc_module(char prefix,const char *name)
{
    char *symbol,*path;
    void *handle,*util;

    /*
     * dlopen keeps track of what we've loaded, so it's actually safe to call
     * it whenever we need that element. Caching the new elements, like
     * iproute2/tc does, is of course more efficient.
     */
    symbol = alloc_sprintf("%s_util",name);
    handle = dlopen(NULL,RTLD_NOW); /* preloaded ? */
    if (handle) {
	util = dlsym(handle,symbol);
	if (util) {
	    free(symbol);
	    return util;
	}
    }
    path = alloc_sprintf("%c_%s.so",prefix,name);
    handle = dlopen(path,RTLD_NOW);
    if (!handle) {
	free(path);
	free(symbol);
	return NULL;
    }
    util = dlsym(handle,symbol);
    if (util) {
	free(path);
	free(symbol);
	return util;
    }
    errorf("%s does not define %s",path,symbol);
    return NULL; /* not reached */
}


void preload_tc_module(const char *path)
{
    void *handle;

    handle = dlopen(path,RTLD_NOW | RTLD_GLOBAL);
    if (!handle) errorf("%s",dlerror());
}


/*
 * Buglet: libc seems to have its own init_module, which is found if our @@@
 * module doesn't have one. Very irritating ...
 */

void kernel_module(const char *path)
{
    void *handle;
    int (*init_module)(void);
    int error;

    handle = dlopen(path,RTLD_NOW);
    if (!handle) errorf("%s",dlerror());
    init_module = dlsym(handle,"init_module");
    if (!init_module) errorf("module %s: symbol init_module not found");
    error = init_module();
    if (!error) return;
    yyerrorf("init_module: %s",strerror(error));
}
