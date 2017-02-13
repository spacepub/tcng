/*
 * q_discard.c - Minimum queuing discipline; discards all packets
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


static int discard_parse_opt(struct qdisc_util *qu,int argc,char **argv,
  struct nlmsghdr *n)
{
	if (argc) {
		fprintf(stderr,"Usage: discard\n");
		return -1;
	}
	return 0;
}


static int discard_print_opt(struct qdisc_util *qu,FILE *f,struct rtattr *opt)
{
    return 0;
}


static int discard_print_xstats(struct qdisc_util *qu,FILE *f,
  struct rtattr *xstats)
{
	return 0;
}


struct qdisc_util discard_util = {
	NULL,
	"discard",
	discard_parse_opt,
	discard_print_opt,
        discard_print_xstats,
};
