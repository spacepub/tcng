/*
 * bytesex.c - Determine host byte order
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 */


#include <stdio.h>
#include <inttypes.h>


int main(void)
{
    uint32_t v = 0x01020304;
    unsigned char *p = (unsigned char *) &v;
    int i;

    for (i = 0; i < 4; i++)
	if (p[i] != i+1) break;
    if (i == 4) {
	printf("BIG_ENDIAN\n");
	return 0;
    }
    for (i = 0; i < 4; i++)
	if (p[i] != 4-i) break;
    if (i == 4) {
	printf("LITTLE_ENDIAN\n");
	return 0;
    }
    fprintf(stderr,"weird architecture (is this a PDP-11 ?!?)\n");
    return 1;
}
