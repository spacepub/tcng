/*
 * u128.c - 128 bit integer operations
 *
 * Written 2002 by Werner Almesberger
 * Copyright 2002 Bivio Networks
 */


#ifndef U128_H
#define U128_H


#include <stdint.h>


/*
 * "U128" may seem a little ugly, but it's much safer as a name than, say
 * u128 or u128_t
 */

typedef struct {
    uint32_t v[4]; /* v[0] is the least significant word */
} U128;


/* ----- Error reporting hook ---------------------------------------------- */

extern void u128_error_hook(const char *msg);

/* ----- Basic 128 bit operations ------------------------------------------ */

U128 u128_add(U128 a,U128 b);
U128 u128_sub(U128 a,U128 b);
U128 u128_minus(U128 a);
U128 u128_and(U128 a,U128 b);
U128 u128_or(U128 a,U128 b);
U128 u128_xor(U128 a,U128 b);
U128 u128_not(U128 a);
U128 u128_shift_left(U128 a,uint32_t b);
U128 u128_shift_right(U128 a,uint32_t b);

/* Note: u128_shift_* also work for b >= 128. We use this extensively. */

/* ----- 128 bit and 32 bit mixed ------------------------------------------ */

U128 u128_from_32(uint32_t a);
uint32_t u128_to_32(U128 a);
int u128_is_32(U128 a);

U128 u128_add_32(U128 a,uint32_t b);
U128 u128_sub_32(U128 a,uint32_t b);
U128 u128_sub_from_32(uint32_t a,U128 b);
U128 u128_and_32(U128 a,uint32_t b);
U128 u128_or_32(U128 a,uint32_t b);
U128 u128_xor_32(U128 a,uint32_t b);

/* ----- Miscellaneous operations ------------------------------------------ */

int u128_cmp(U128 a,U128 b);
int u128_is_zero(U128 a);

#endif /* U128_H */
