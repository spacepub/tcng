/*
 * u128.c - 128 bit integer operations
 *
 * Written 2002 by Werner Almesberger
 * Copyright 2002 Bivio Networks
 */


#include "u128.h"


/* ----- Basic 128 bit operations ------------------------------------------ */


U128 u128_add(U128 a,U128 b)
{
    int i,carry = 0;

    for (i = 0; i < 4; i++) {
	if (carry) {
	    a.v[i]++;
	    carry = a.v[i] == 0;
	}
	a.v[i] += b.v[i];
	if (a.v[i] < b.v[i]) carry = 1;
    }
    return a;
}


U128 u128_sub(U128 a,U128 b)
{
    int i,borrow = 0;

    for (i = 0; i < 4; i++) {
	if (borrow) {
	    borrow = !a.v[i];
	    a.v[i]--;
	}
	if (a.v[i] < b.v[i]) borrow = 1;
	a.v[i] -= b.v[i];
    }
    return a;
}


U128 u128_minus(U128 a)
{
    int i,carry = 1;

    for (i = 0; i < 4; i++) {
	a.v[i] = ~a.v[i];
	if (carry) {
	    a.v[i]++;
	    carry = a.v[i] == 0;
	}
    }
    return a;
}


U128 u128_and(U128 a,U128 b)
{
    int i;

    for (i = 0; i < 4; i++)
	a.v[i] &= b.v[i];
    return a;
}


U128 u128_or(U128 a,U128 b)
{
    int i;

    for (i = 0; i < 4; i++)
	a.v[i] |= b.v[i];
    return a;
}


U128 u128_xor(U128 a,U128 b)
{
    int i;

    for (i = 0; i < 4; i++)
	a.v[i] ^= b.v[i];
    return a;
}


U128 u128_not(U128 a)
{
    int i;

    for (i = 0; i < 4; i++)
	a.v[i] = ~a.v[i];
    return a;
}


U128 u128_shift_left(U128 a,uint32_t b)
{
    int words,bits,i;

    words = b/32;
    bits = b % 32;
    for (i = 3; i >= words; i--) {
	a.v[i] = a.v[i-words] << bits;
	if (bits && i > words) a.v[i] |= a.v[i-words-1] >> (32-bits);
    }
    while (i >= 0)
	a.v[i--] = 0;
    return a;
}


U128 u128_shift_right(U128 a,uint32_t b)
{
    int words,bits,i;

    words = b/32;
    bits = b % 32;
    for (i = 0; i < 4-words; i++) {
	a.v[i] = a.v[i+words] >> bits;
	if (bits && i < 4-words-1) a.v[i] |= a.v[i+words+1] << (32-bits);
    }
    while (i < 4) a.v[i++] = 0;
    return a;
}


/* ----- 128 bit and 32 bit mixed ------------------------------------------ */


U128 u128_from_32(uint32_t a)
{
    U128 res;

    res.v[0] = a;
    res.v[1] = res.v[2] = res.v[3] = 0;
    return res;
}


uint32_t u128_to_32(U128 a)
{
    if (a.v[1] || a.v[2] || a.v[3])
	u128_error_hook("128 bit value is too large for 32 bits");
    return a.v[0];
}


int u128_is_32(U128 a)
{
    return !a.v[1] && !a.v[2] && !a.v[3];
}


U128 u128_add_32(U128 a,uint32_t b)
{
    a.v[0] += b;
    if (a.v[0] < b)
	if (!++a.v[1])
	    if (!++a.v[2]) a.v[3]++;
    return a;
}


U128 u128_sub_32(U128 a,uint32_t b)
{
    int i;

    if (a.v[0] < b)
	for (i = 1; i < 4; i++)
	    if (a.v[i]--) break;
    a.v[0] -= b;
    return a;
}


U128 u128_sub_from_32(uint32_t a,U128 b)
{
    return u128_add_32(u128_minus(b),a);
}


U128 u128_and_32(U128 a,uint32_t b)
{
    a.v[0] &= b;
    return a;
}


U128 u128_or_32(U128 a,uint32_t b)
{
    a.v[0] |= b;
    return a;
}


U128 u128_xor_32(U128 a,uint32_t b)
{
    a.v[0] ^= b;
    return a;
}


/* ----- Miscellaneous operations ------------------------------------------ */


int u128_cmp(U128 a,U128 b)
{
    int i;

    for (i = 0; i < 4; i++)
	if (a.v[i] != b.v[i]) return a.v[i] < b.v[i] ? -1 : 1;
    return 0;
}


int u128_is_zero(U128 a)
{
    int i;

    for (i = 0; i < 4; i++)
	if (a.v[i]) return 0;
    return 1;
}
