/*
 * echoh_main.c - Print qdisc/class hierarchy; main function on stderr
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 Network Robots
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tccext.h"
#include "echoh.h"


static void usage(const char *name)
{
    fprintf(stderr,"usage: %s mode ...\n",name);
    exit(1);
}


int main(int argc,char **argv)
{
    TCCEXT_CONTEXT *ctx;

    if (argc < 2) usage(*argv);
    if (!strcmp(argv[1],"config")) {
	int i;

	printf("debug_target\n");
	for (i = 2; i < argc; i++) printf("%s\n",argv[i]);
	return 0;
    }
    if (!strcmp(argv[1],"check")) return 0;
    if (strcmp(argv[1],"build")) {
	fprintf(stderr,"%s: unrecognized mode %s",*argv,argv[1]);
	return 1;
    }
    if (argc < 3) usage(*argv);
    ctx = tccext_parse(stdin,NULL);
    echoh_buckets(stderr,ctx->buckets);
    echoh_blocks(stderr,ctx->blocks);
    return 0;
}
