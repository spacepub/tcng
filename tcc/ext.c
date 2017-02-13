/*
 * ext.c - Communication infrastructure for the external interface
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001,2002 Network Robots
 * Copyright 2002 Werner Almesberger
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <linux/if_ether.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "tree.h"
#include "tc.h"
#include "ext.h"


static int debug_target = 0;


static void make_argv0(char **argv,const char *name)
{
    argv[0] = alloc_sprintf("tcc-ext-%s",name);
}


static void make_unique(char **argv)
{
    static char unique[5];
    static int count = 0;

    if (count == 0x100) error("too many calls to external interface");
    sprintf(unique,"%02x%02x",getpid() & 0xff,count & 0xff);
    count++;
    argv[2] = unique;
}


void ext_config(const char *name)
{
    char *argv[] = { NULL, "config", NULL };
    FILE *file;
    char line[MAX_EXT_LINE+2];

    make_argv0(argv,name);
    file = ext_open(argv,name);
    free(argv[0]);
    ext_read(name,"on configuration query");
    while (fgets(line,MAX_EXT_LINE+2,file)) {
	char *nl;

	nl = strchr(line,'\n');
	if (nl) *nl = 0;
	if (debug) fprintf(stderr,"config %s> %s\n",name,line);
	if (!strcmp(line,"all")) dump_all = 1;
	else if (!strcmp(line,"fifos")) add_fifos = 1;
	else if (!strcmp(line,"nounspec")) generate_default_class = 1;
	else if (!strcmp(line,"nocontinue")); /* ignore */
	else if (!strcmp(line,"debug_target")) debug_target = 1;
	else if (!strcmp(line,"nocombine")) no_combine = 1;
	else errorf("unrecognized configuration item \"%s\"",line);
    }
    ext_close();
}


void ext_build(const char *name,const QDISC *qdisc,const FILTER *filter,
  void (*dump_config)(FILE *file,void *user),void *user)
{
    char *argv[] = { NULL, "build", NULL, NULL };
    FILE *file;
    int got_element;
    char line[MAX_EXT_LINE+2];

    make_argv0(argv,name);
    make_unique(argv);
    file = ext_open(argv,name);
    free(argv[0]);
    dump_config(file,user);
    ext_read(name,"when building");
    got_element = 0;
    while (fgets(line,MAX_EXT_LINE+2,file)) {
	char *nl,*element;
	int pos;

	nl = strchr(line,'\n');
	if (nl) *nl = 0;
	if (debug) fprintf(stderr,"build %s> %s\n",name,line);
	if (sscanf(line,"qdisc %as%n",&element,&pos) >= 1) {
	    if (!qdisc) errorf("did not expect a qdisc from \"%s\"",name);
	    if (got_element)
		errorf("already got an element from \"%s\"",name);
	    printf("insmod sch_%s.o\n",element);
	    __tc_qdisc_add(qdisc,element);
	    tc_more("%s\n",line+pos);
	    free(element);
	    got_element = 1;
	}
	else if (sscanf(line,"filter %as%n",&element,&pos) >= 1) {
	    if (!filter) errorf("did not expect a filter from \"%s\"",name);
	    if (got_element)
		errorf("already got an element from \"%s\"",name);
	    printf("insmod cls_%s.o\n",element);
	    __tc_filter_add(filter,ETH_P_ALL);
	    tc_more(" %s%s\n",element,line+pos);
	    free(element);
	    got_element = 1;
	}
	else errorf("unrecognized build response \"%s\"",line);
    }
    ext_close();
    if (!got_element && !dump_all && !debug_target)
	errorf("\"%s\" did not generate any element",name);
}
