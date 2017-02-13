/*
 * tcm_f.c - Common code for tcc-generated classifiers, user-space part
 *
 * Written 2001,2004 by Werner Almesberger
 * Copyright 2001,2004 Werner Almesberger
 */


#include <stdio.h>
#include <netinet/in.h> /* for utils.h */
#include <linux/types.h> /* for utils.h */

#include "utils.h"
#include "tc_util.h"


#define _C(a,b) a##b
#define C(a,b) _C(a,b)

#define _S(s) #s
#define S(s) _S(s)


#include MDI_FILE


static int C(NAME,_parse_opt)(struct filter_util *fu,char *handle,int argc,
  char **argv,struct nlmsghdr *n)
{
	if (argc) {
		fprintf(stderr,"Usage: " S(NAME) "\n");
		return -1;
	}
	return 0;
}


static int C(NAME,_print_opt)(struct filter_util *fu,FILE *f,
  struct rtattr *opt,__u32 handle)
{
	return 0;
}


struct filter_util C(NAME,_util) = {
	NULL,
	S(NAME),
	C(NAME,_parse_opt),
	C(NAME,_print_opt),
};
