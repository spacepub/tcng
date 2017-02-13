/*
 * f_unspec.c - Minimum classifier; always returns unspec
 *
 * Written 2001,2004 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2004 Werner Almesberger
 */


#include <stdio.h>
#include <netinet/in.h> /* for utils.h */
#include <linux/types.h> /* for utils.h */

#include "utils.h"
#include "tc_util.h"


static int unspec_parse_opt(struct filter_util *fu,char *handle,int argc,
  char **argv,struct nlmsghdr *n)
{
	if (argc) {
		fprintf(stderr,"Usage: unspec\n");
		return -1;
	}
	return 0;
}


static int unspec_print_opt(struct filter_util *fu,FILE *f,struct rtattr *opt,
  __u32 handle)
{
	return 0;
}


struct filter_util unspec_util = {
	NULL,
	"unspec",
	unspec_parse_opt,
	unspec_print_opt,
};
