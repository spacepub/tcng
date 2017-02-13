/*
 * iflib_arith.c - Arithmetic conversions
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Werner Almesberger
 */


#include <stdlib.h>
#include <assert.h>

#include <u128.h>

#include "config.h"
#include "registry.h"
#include "data.h"
#include "op.h"
#include "error.h"
#include "iflib.h"


/*
 * General naming conventions in this file:
 *
 * X = general expression
 * C = constant
 * rel = relational operator
 */

/*
 * @@@ several of these optimizations should be either done in op.c, or the
 * optimizations in op.c should move here.
 */


/* ----- Helper functions -------------------------------------------------- */


static int log_2(uint32_t v)
{
    int ld = 0;

    if (!v) return -1;
    while (!(v & 1)) {
	ld++;
	v >>= 1;
    }
    return v == 1 ? ld : -1;
}


static int log_2_any(DATA d,int increment)
{
    int i,ld;

    switch (d.type) {
	case dt_unum:
	    if (increment) {
		d.u.unum++;
		if (!d.u.unum) return 32; /* roll-over */
	    }
	    return log_2(d.u.unum);
	case dt_ipv6:
	    if (increment) {
		d.u.u128 = u128_add_32(d.u.u128,1);
		if (u128_is_zero(d.u.u128)) return 128; /* roll-over */
	    }
	    for (i = 0; !d.u.u128.v[i] && i < 4; i++);
	    if (i == 4) return -1; /* 0 */
	    ld = log_2(d.u.u128.v[i]);
	    if (ld == -1) return -1;
	    ld += 32*i;
	    for (i++; i < 4; i++)
		if (d.u.u128.v[i]) return -1;
	    return ld;
	default:
	    abort();
    }
}


/* ----- *, /, and % ------------------------------------------------------- */


/*
 * Convert:
 *
 *  0 * X  ->  0
 *  X * 0  ->  0
 *  1 * X  ->  X
 *  X * 1  ->  X
 *  (2^n) * X  ->  X << n
 *  X * (2^n)  ->  X << n
 *
 *  X / 1      ->  X
 *  0 / X      ->  0
 *  X / X      ->  1
 *  X / (2^n)  ->  X >> n
 *
 *  X % 1      ->  0
 *  X % (2^n)  ->  X & (2^n)-1
 *  X % X      ->  0
 */


static int mul_opt(DATA *d,DATA x,DATA *c,uint8_t value)
{
    int ld;

    if (!value) {
	data_destroy(*d);
	*d = data_unum(0);
	return 1;
    }
    ld = log_2(value);
    if (ld == -1) return 0;
    data_destroy(*c);
    data_destroy_1(*d);
    if (!ld) {
	*d = x;
	return 1;
    }
    *d = op_binary(&op_shift_left,x,data_unum(ld));
    return 1;
}


static int div_l_opt(DATA *d,DATA x,DATA *c,uint8_t value)
{
    if (value) return 0;
    data_destroy(*d);
    *d = data_unum(0);
    return 1;
}


static int div_r_opt(DATA *d,DATA x,DATA *c,uint8_t value)
{
    int ld;

    ld = log_2(value);
    if (ld == -1) return 0;
    data_destroy(*c);
    data_destroy_1(*d);
    if (!ld) {
	*d = x;
	return 1;
    }
    *d = op_binary(&op_shift_right,x,data_unum(ld));
    return 1;
}


static int mod_opt(DATA *d,DATA x,DATA *c,uint8_t value)
{
    int ld;

    if (value == 1) {
	data_destroy(*d);
	*d = data_unum(0);
	return 1;
    }
    ld = log_2(value);
    if (ld == -1) return 0;
    d->op->dsc = &op_and;
    c->u.unum--;
    return 1;
}


static void mul_div_mod(DATA *d)
{
    if (!d->op) return;
    mul_div_mod(&d->op->a);
    mul_div_mod(&d->op->b);
    mul_div_mod(&d->op->c);
    if (d->op->dsc == &op_mult) {
	if (!d->op->a.op && d->op->a.type == dt_unum) {
	    if (mul_opt(d,d->op->b,&d->op->a,d->op->a.u.unum)) mul_div_mod(d);
	}
	else if (!d->op->b.op && d->op->b.type == dt_unum) {
	    if (mul_opt(d,d->op->a,&d->op->b,d->op->b.u.unum)) mul_div_mod(d);
	}
    }
    else if (d->op->dsc == &op_div) {
	if (!d->op->a.op && d->op->a.type == dt_unum) {
	    if (div_l_opt(d,d->op->b,&d->op->a,d->op->a.u.unum))
		mul_div_mod(d);
	}
	else if (!d->op->b.op && d->op->b.type == dt_unum) {
	    if (div_r_opt(d,d->op->a,&d->op->b,d->op->b.u.unum))
		mul_div_mod(d);
	}
	else if (expr_equal(d->op->a,d->op->b)) {
		    data_destroy(*d);
		    *d = data_unum(1);
	    }
    }
    else if (d->op->dsc == &op_mod) {
	if (!d->op->b.op && d->op->b.type == dt_unum) {
	    if (mod_opt(d,d->op->a,&d->op->b,d->op->b.u.unum)) mul_div_mod(d);
	}
	else if (expr_equal(d->op->a,d->op->b)) {
		data_destroy(*d);
		*d = data_unum(0);
	    }
    }
}


/* ----- Relational operators, common functions ---------------------------- */


static int is_relop(const OP_DSC *dsc)
{
    return dsc == &op_eq || dsc == &op_ne || dsc == &op_gt || dsc == &op_ge ||
      dsc == &op_lt || dsc == &op_le;
}


/* ----- Relational operators (move constants) ----------------------------- */


/*
 * Convert:
 *
 * C1 rel C2  ->  0  if !(C1 rel C2)
 * C1 rel C2  ->  1  if C1 rel C2
 *
 * C rel X  ->  X inv rel C  (exchange < and >, <= and >=)
 *
 * X+C1 rel C2  ->  X rel C1-C2
 * C1+X rel C2  ->  X rel C1-C2
 *
 * X-C1 rel C2  ->  X rel C1+C2
 * C1-X rel C2  ->  X rel C1-C2
 */


static void rel_plus(DATA *d,DATA *a,DATA *b)
{
    DATA *a_c,x;

    if (!a->op->a.op &&
      (a->op->a.type == dt_unum || a->op->a.type == dt_ipv6)) {
	a_c = &a->op->a;
	x = a->op->b;
    }
    else if (!a->op->b.op &&
      (a->op->b.type == dt_unum || a->op->b.type == dt_ipv6)) {
	x = a->op->a;
	a_c = &a->op->b;
    }
    else return;
    if (b->type == dt_unum && a_c->type == dt_unum) b->u.unum -= a_c->u.unum;
    else *b = data_ipv6(u128_sub(
	  data_convert(*b,dt_ipv6).u.u128,data_convert(*a_c,dt_ipv6).u.u128));
    data_destroy_1(*a_c);
    data_destroy_1(*a);
    *a = x;
}


static void rel_minus(DATA *d,DATA *a,DATA *b)
{
    DATA *a_c,x;

    if (!a->op->a.op &&
      (a->op->a.type == dt_unum || a->op->a.type == dt_ipv6)) {
	a_c = &a->op->a;
	x = a->op->b;
	if (b->type == dt_unum && a_c->type == dt_unum)
	    b->u.unum = a_c->u.unum-b->u.unum;
	else *b = data_ipv6(u128_sub(
	      data_convert(*a_c,dt_ipv6).u.u128,
	      data_convert(*b,dt_ipv6).u.u128));
    }
    else if (!a->op->b.op &&
      (a->op->b.type == dt_unum || a->op->b.type == dt_ipv6)) {
	x = a->op->a;
	a_c = &a->op->b;
	if (b->type == dt_unum && a_c->type == dt_unum)
	    b->u.unum += a_c->u.unum;
	else *b = data_ipv6(u128_add(
	      data_convert(*b,dt_ipv6).u.u128,
	      data_convert(*a_c,dt_ipv6).u.u128));
    }
    else return;
    data_destroy_1(*a_c);
    data_destroy_1(*a);
    *a = x;
}


static void rel_math(DATA *d)
{
    if (!d->op) return;
    if (is_relop(d->op->dsc)) {
	if (!d->op->a.op && !d->op->b.op &&
	  (d->op->a.type == dt_unum || d->op->a.type == dt_ipv6) &&
	  (d->op->b.type == dt_unum || d->op->b.type == dt_ipv6)) {
	    DATA tmp = op_binary(d->op->dsc,d->op->a,d->op->b);

	    data_destroy(*d);
	    *d = tmp;
	    return;
	}
	if (!d->op->a.op && (d->op->a.type == dt_unum ||
	  d->op->a.type == dt_ipv6) && d->op->b.op) {
	    DATA tmp;

	    tmp = d->op->a;
	    d->op->a = d->op->b;
	    d->op->b = tmp;
	    if (d->op->dsc == &op_gt) d->op->dsc = &op_lt;
	    else if (d->op->dsc == &op_lt) d->op->dsc = &op_gt;
	    else if (d->op->dsc == &op_ge) d->op->dsc = &op_le;
	    else if (d->op->dsc == &op_le) d->op->dsc = &op_ge;
	}
	if (d->op->a.op && d->op->a.op->dsc == &op_plus && !d->op->b.op &&
	  (d->op->b.type == dt_unum || d->op->b.type == dt_ipv6)) {
	    rel_plus(d,&d->op->a,&d->op->b);
	    return;
	}
	if (d->op->a.op && d->op->a.op->dsc == &op_minus && !d->op->b.op &&
	  (d->op->b.type == dt_unum || d->op->b.type == dt_ipv6)) {
	    rel_minus(d,&d->op->a,&d->op->b);
	    return;
	}
    }
    rel_math(&d->op->a);
    rel_math(&d->op->b);
    rel_math(&d->op->c);
}


/* ----- Relational operators (canonical mask and shift) ------------------- */


/*
 * Convert:
 *
 * X rel C  ->  X & Cg rel C  where Cg is the combined mask of X
 *
 * At each step:
 *
 * X & Cm rel C  ->  X rel C  and  Cg = Cg & Cm
 * X << Cs rel C  ->  X rel C >> Cs  and  Cg = Cg >> Cs
 * X >> Cs rel C  ->  X rel C << Cs  and  Cg = Cg << Cs
 *
 * If C << Cs exceeds 32 bits,  X rel C  becomes  0 rel 1
 * If Cg < C,  X rel C  becomes  0 rel 1
 * If Cg becomes 0,  X rel C  becomes  0 rel C
 */


static void rel_access(DATA *d)
{
    DATA *x,old;
    U128 c,mask;
    int c_shift,bits;

    if (!d->op) return;
    if (d->op->dsc == &op_logical_or || d->op->dsc == &op_logical_and ||
      d->op->dsc == &op_logical_not || d->op->dsc == &op_offset ||
      d->op->dsc == &op_precond) {
	rel_access(&d->op->a);
	rel_access(&d->op->b);
	return;
    }
    if (d->op->dsc == &op_conform || d->op->dsc == &op_count) return;
    if (!is_relop(d->op->dsc)) *d = op_binary(&op_ne,*d,data_unum(0));
    old = *d;
    if (d->op->b.op || (d->op->b.type != dt_unum && d->op->b.type != dt_ipv6))
	dump_failed(*d,"\"rel const\" expected");
    c = data_convert(d->op->b,dt_ipv6).u.u128;
    c_shift = 0;
    x = &d->op->a;
    mask = u128_not(u128_from_32(0)); /* ~0 */
    while (x->op) {
	uint32_t shift;

	if (x->op->dsc == &op_and) {
	    if (!x->op->a.op &&
	      (x->op->a.type == dt_unum || x->op->a.type == dt_ipv6)) {
		mask = u128_and(mask,data_convert(x->op->a,dt_ipv6).u.u128);
		x = &x->op->b;
		continue;
	    }
	    if (!x->op->b.op &&
	      (x->op->b.type == dt_unum || x->op->b.type == dt_ipv6)) {
		mask = u128_and(mask,data_convert(x->op->b,dt_ipv6).u.u128);
		x = &x->op->a;
		continue;
	    }
	    break;
	}
	if (x->op->b.op || x->op->b.type != dt_unum) break;
	shift = x->op->b.u.unum;
	if (x->op->dsc == &op_shift_left) {
	    mask = u128_shift_right(mask,shift);
	    c_shift -= shift;
	    if (u128_is_zero(mask)) break;
	    x = &x->op->a;
	    continue;
	}
	if (x->op->dsc == &op_shift_right) {
	    mask = u128_shift_left(mask,shift);
	    c_shift += shift;
	    if (u128_is_zero(mask)) break;
	    x = &x->op->a;
	    continue;
	}
	break;
    }
    bits = x->op && x->op->dsc == &op_access && !x->op->c.op ?
      x->op->c.u.unum : 0;
    if (c_shift > 0) {
	/* overflow */
	if ((c_shift >= 128 && !u128_is_zero(c)) ||
	  !u128_is_zero(u128_shift_right(c,128-c_shift))) {
	    warn("left-shift of value in access exceeds 128 bit range");
	    *d = op_binary(d->op->dsc,data_unum(0),data_unum(1));
	    data_destroy(old);
	    return;
	}
	if (bits == 32 && c_shift >= 32 && !u128_is_zero(c))
	    warn("left-shift of value in access exceeds 32 bit range");
	c = u128_shift_left(c,c_shift);
	if (bits == 32 && c_shift < 32 && !u128_is_32(c))
	    warn("left-shift of value in access exceeds 32 bit range");
    }
    if (c_shift < 0) {
	/* uint32_t lost = c & (c_shift > -32 ? (1 << -c_shift)-1 : ~0); */
	int lost = !u128_is_zero(
	  u128_and(c,
	    u128_sub_32(
	      u128_shift_left(
		u128_from_32(1),
	        -c_shift),1)));

	c = u128_shift_right(c,-c_shift);
	/* underflow */
	if (lost) {
	    /* X == C+eps  ->  0 */
	    if (d->op->dsc == &op_eq) {
		*d = data_unum(0);
		data_destroy(old);
		return;
	    }

	    /* X != C+eps  ->  1 */
	    if (d->op->dsc == &op_ne) {
		*d = data_unum(1);
		data_destroy(old);
		return;
	    }
	    /* X < C+eps  ->  X <= C */
	    if (d->op->dsc == &op_lt) d->op->dsc = &op_le;

	    /* X > C+eps  ->  X > C+1 */
	    if (d->op->dsc == &op_gt) c = u128_add_32(c,1);

	    /* X >= C+eps  ->  X > C */
	    if (d->op->dsc == &op_ge) d->op->dsc = &op_gt;

	    /* X <= C+eps  ->  X <= C, nothing to do */
	}
    }
    if (bits)
	mask = u128_and(mask,
	  u128_sub_32(
	    u128_shift_left(
	      u128_from_32(1),bits),1));
	/* mask &= (1 << x->op->c.u.unum)-1; */
    if (u128_cmp(mask,c) < 0) /* mask < c */
	*d = op_binary(d->op->dsc,data_unum(0),data_unum(1));
    else if (u128_is_zero(u128_or(mask,c))) /* !(mask | c) */
	*d = op_binary(d->op->dsc,data_unum(0),data_unum(0));
    else if (!u128_is_zero(u128_and(c,u128_not(mask))) /* c & ~mask */
	  && (d->op->dsc == &op_eq || d->op->dsc == &op_ne))
	*d = data_unum(d->op->dsc == &op_ne);
    else {
	DATA d_c = u128_is_32(c) ? data_unum(u128_to_32(c)) : data_ipv6(c);
	DATA d_mask = u128_is_32(mask) ? data_unum(u128_to_32(mask)) :
	  data_ipv6(mask);

	*d = op_binary(d->op->dsc,op_binary(&op_and,data_clone(*x),d_mask),d_c);
    }
    data_destroy(old);
}


/* ----- Relational operators (turn them into equality) -------------------- */


/*
 * Convert:
 *
 * X < 2^n  ->  X & ~(2^n-1) == 0
 * X <= 2^n-1  ->  X & ~(2^n-1) == 0
 * X > 2^n-1  ->  X & ~(2^n-1) != 0
 * X >= 2^n  ->  X & ~(2^n-1) != 0
 */


static void rel_to_eq(DATA *d)
{
    if (!d->op) return;
    rel_to_eq(&d->op->a);
    rel_to_eq(&d->op->b);
    rel_to_eq(&d->op->c);
    if (d->op->a.op && !d->op->b.op &&
      (d->op->b.type == dt_unum || d->op->b.type == dt_ipv6)) {
	DATA mask;
	int ld,decrement;

	if (d->op->dsc == &op_lt) {
	    ld = log_2_any(d->op->b,0);
	    if (ld == -1) return;
	    decrement = 1;
	    d->op->dsc = &op_eq;
	}
	else if (d->op->dsc == &op_le) {
	    ld = log_2_any(d->op->b,1);
	    if (ld == -1) return;
	    decrement = 0;
	    d->op->dsc = &op_eq;
	}
	else if (d->op->dsc == &op_gt) {
	    ld = log_2_any(d->op->b,1);
	    if (ld == -1) return;
	    decrement = 0;
	    d->op->dsc = &op_ne;
	}
	else if (d->op->dsc == &op_ge) {
	    ld = log_2_any(d->op->b,0);
	    if (ld == -1) return;
	    decrement = 1;
	    d->op->dsc = &op_ne;
	}
	else return;
        if (d->op->b.type == dt_unum) {
	    if (decrement) d->op->b.u.unum--;
	    mask = data_unum(~d->op->b.u.unum);
	}
	else {
	    if (decrement) d->op->b.u.u128 = u128_sub_32(d->op->b.u.u128,1);
	    mask = data_ipv6(u128_minus(d->op->b.u.u128));
	}
	d->op->a = op_binary(&op_and,d->op->a,mask);
	d->op->b = data_unum(0);
    }
}


/* ------ Eliminate expressions involving |, ^ ----------------------------- */


/*
 * Convert:
 *
 * X == X	 ->  1
 * X ^ C1 == C2  ->  X == C1 ^ C2
 * X | C1 == C2  ->  0				if (C1 & C2) != C1
 * X | C1 == C2  ->  X & ~C1 == (C2 & ~C1)	else
 *
 * ... also for C1 op X
 */


static void rel_eq_or(DATA *d)
{
    DATA x,c;
    int is_unum;

    if (!d->op) return;
    rel_eq_or(&d->op->a);
    rel_eq_or(&d->op->b);
    rel_eq_or(&d->op->c);
    if (d->op->dsc == &op_eq && expr_equal(d->op->a,d->op->b)) {
	data_destroy(*d);
	*d = data_unum(1);
	return;
    }
    if (d->op->dsc != &op_eq || d->op->b.op ||
      (d->op->b.type != dt_unum && d->op->b.type != dt_ipv6))
	return;
    if (!d->op->a.op || (d->op->a.op->dsc != &op_or &&
      d->op->a.op->dsc != &op_xor)) return;
    if (!d->op->a.op->a.op &&
      (d->op->a.op->a.type == dt_unum || d->op->a.op->a.type == dt_ipv6)) {
	c = d->op->a.op->a;
	x = d->op->a.op->b;
    }
    else if (!d->op->a.op->b.op &&
      (d->op->a.op->b.type == dt_unum || d->op->a.op->b.type == dt_ipv6)) {
	x = d->op->a.op->a;
	c = d->op->a.op->b;
    }
    else return;
    is_unum = d->op->b.type == dt_unum && c.type == dt_unum;
    if (!is_unum) {
	d->op->b = data_convert(d->op->b,dt_ipv6);
	c = data_convert(c,dt_ipv6);
    }
    if (d->op->a.op->dsc == &op_xor) {
	if (is_unum) d->op->b.u.unum ^= c.u.unum;
	else d->op->b = data_ipv6(u128_xor(d->op->b.u.u128,c.u.u128));
	data_destroy_1(d->op->a);
	data_destroy_1(c);
	d->op->a = x;
	return;
    }
    if ((is_unum && (c.u.unum & d->op->b.u.unum) != c.u.unum) ||
      (!is_unum && u128_cmp(u128_and(c.u.u128,d->op->b.u.u128),c.u.u128))) {
	data_destroy(*d);
	*d = data_unum(0);
	return;
    }
    d->op->a.op->dsc = &op_and;
    d->op->a.op->a = x;
    if (is_unum) {
	d->op->a.op->b = data_unum(~c.u.unum);
	d->op->b.u.unum &= ~c.u.unum;
    }
    else {
	d->op->a.op->b = data_ipv6(u128_not(c.u.u128));
	d->op->b.u.u128 = u128_and(d->op->b.u.u128,u128_not(c.u.u128));
    }
}


/* ----- Helper functions for lt and ne bit/prefix converters -------------- */


static int get_bit(U128 v,int n)
{
    return u128_shift_right(v,n).v[0] & 1;
}


static DATA prefix_and_twist(U128 v,int n)
{
    /* (c & (~0 << n))^(1 << n) */
    return data_ipv6(
      u128_xor(
	u128_and(
	  v,
	  u128_shift_left(
	    u128_not(u128_from_32(0)),
	    n)),
        u128_shift_left(u128_from_32(1),n)));
}


static DATA set_nth_bit(int n)
{
    if (n < 32) return data_unum(1 << n);
    return data_ipv6(u128_shift_left(u128_from_32(1),n));
}


static DATA bit_mask(int n)
{
    return data_ipv6(u128_shift_left(u128_not(u128_from_32(0)),n));
}


/* ----- General relational operators -------------------------------------- */


/*
 * The basic operation is  X <= C
 * Then, build_le can also build the reverse, i.e. X > C
 *
 * The following shortcuts are recognized:
 *
 * X <= C: 1 if C == 0xff...ff
 * X > C:  0 if C == 0xff...ff
 *
 * This is mainly done for overflow protection. Any byte vs. 128 bit issues
 * (e.g. raw > 0xff) are detected at later processing stages.
 *
 * The two remaining relational operators are implemented as follows:
 *
 * X < C: 0	    if C == 0
 *        X <= C-1  if C != 0
 *
 * X >= C: 1	    if C == 0
 *         X > C-1  if C != 0
 *
 * Note that we always assume 128 bits, so later processing stages may
 * complain about "impossible" values. (Actually, they have changed with
 * time and are less noisy now.)
 *
 * Also, we may end up with long redundant chains like  a || ... || x || 1
 * or  a && ... && x && 0
 */


static DATA build_le_bit(DATA acc,U128 c,U128 mask,int invert)
{
    DATA d,*next = &d;
    int i;

    for (i = 127; i >= 0; i--) {
	const OP_DSC *op;

	op = get_bit(c,i) == invert ? &op_logical_and : &op_logical_or;
	/*
	 * if invert:
	 *   next = (acc & (1 << i)) == (1 << i) op 0
	 * else:
	 *   next = (acc & (1 << i)) == 0 op 1
	 */
        if (!get_bit(mask,i)) {
	    /*
	     * A constant expression is much easier on later processing stages
	     * than the full test, so use this for bits that are "masked out".
	     * We could of course do better, but then it gets a bit complex.
	     *
	     * We use "acc" first to force creation of an operator structure,
	     * and overwrite the "a" operand later. This is ugly, but eval_op
	     * would happily reduce a fully constant expression to a constant,
	     * which would leave us without an anchor point for the next bit.
	     */
	    *next = op_binary(op,
	      acc,
	      data_unum(1-invert));
	    next->op->a = data_unum(1-invert);
	}
	else *next = op_binary(op,
	      op_binary(&op_eq,
		op_binary(&op_and,
		  data_clone(acc),
		  set_nth_bit(i)),
		invert ? set_nth_bit(i) : data_unum(0)),
	      data_unum(1-invert));
	next = &next->op->b;
    }
    return d;
}


/*
 * Like rel_lt_bit, but generates prefix matches, yielding a smaller number
 * of match operations, and a simpler expression structure, but each match
 * involves more bits.
 */


static DATA build_lt_prefix(DATA acc,U128 c,U128 mask,int invert)
{
    DATA d = data_none(),*next = &d;
    int i;

    for (i = 0; i < 128; i++) {
	if (get_bit(c,i) == invert) continue;
        if (!get_bit(mask,i) && !get_bit(c,i)) continue;
	  /* (X & 0) == 1 is always "false" */
	if (d.type != dt_none) {
	    *next = op_binary(&op_logical_or,*next,data_unum(0));
	    next = &next->op->b;
	}
	*next = op_binary(&op_eq,
	  op_binary(&op_and,
	    data_clone(acc),
	    bit_mask(i)),
	  prefix_and_twist(c,i));
    }
    assert(d.type != dt_none);
    return d;
}


static void build_rel(DATA *d,int invert,int const_res,int const_if_zero,
  int decrement)
{
    /*
     * When building individual bit tests:
     * decrement iff dcrement != 0
     *
     * When building prefix tests:
     * if const_res != 0:
     *   INcrement if !invert
     *   DEcrement if invert
     */
    U128 c,mask;

    if (d->op->b.op || (d->op->b.type != dt_unum && d->op->b.type != dt_ipv6))
	return; /* bad luck */
    c = data_convert(d->op->b,dt_ipv6).u.u128;
    if ((const_if_zero && u128_is_zero(c)) ||
      (!const_if_zero && !u128_cmp(c,u128_not(u128_from_32(0))))) {
	    data_destroy(*d);
	    *d = data_unum(const_res);
    }
    else {
	DATA new;

	if (d->op->a.op->dsc == &op_and && !d->op->a.op->b.op &&
	  (d->op->a.op->b.type == dt_unum || d->op->a.op->b.type == dt_ipv6))
	    mask = data_convert(d->op->a.op->b,dt_ipv6).u.u128;
	else mask = u128_not(u128_from_32(0));
	if (registry_probe(&optimization_switches,"prefix")) {
	    if (const_res) {
		if (invert) c = u128_sub_32(c,1);
		else c = u128_add_32(c,1);
	    }
	    new = build_lt_prefix(d->op->a,c,mask,invert);
	}
	else {
	    if (decrement) c = u128_sub_32(c,1);
	    new = build_le_bit(d->op->a,c,mask,invert);
	}
	data_destroy(*d);
	*d = new;
    }
}


static void rel_general(DATA *d)
{
				   /*          invert                      */
				   /*          | const_res & xc for prefix */
				   /*          | | const_if_zero           */
				   /*	       | | | decrement             */
    if (!d->op) return;            /*          | | | |			   */
    if (d->op->dsc == &op_le)	   build_rel(d,0,1,0,0);
    else if (d->op->dsc == &op_gt) build_rel(d,1,0,0,0);
    else if (d->op->dsc == &op_lt) build_rel(d,0,0,1,1);
    else if (d->op->dsc == &op_ge) build_rel(d,1,1,1,1);
    if (d->op) {
	rel_general(&d->op->a);
	rel_general(&d->op->b);
	rel_general(&d->op->c);
    }
}


/* ----- Turn != into multiple == (bit or prefix) -------------------------- */


static DATA build_ne_bit(DATA acc,DATA c)
{
    DATA d = data_none(),*next = &d;
    U128 c128,mask;
    int i;

    c128 = data_convert(c,dt_ipv6).u.u128;
    if (acc.op->dsc == &op_and && !acc.op->b.op &&
      (acc.op->b.type == dt_unum || acc.op->b.type == dt_ipv6))
	mask = data_convert(acc.op->b,dt_ipv6).u.u128;
    else mask = u128_not(u128_from_32(0));
    for (i = 0; i < 128; i++) {
	if (!get_bit(mask,i)) continue;
	    /*
	     * Would become (X & 0) == 0. Note that we do not even have to
	     * look at the value we compare with, because rel_access already
	     * removes masks that are incompatible with the value. Also, it
	     * should filter out masks which are zero, so we can be sure that
	     * this loop here will produce at least one value. (Otherwise,
	     * later stages would trip over the dt_none.)
	     */
	if (d.type != dt_none) {
	    *next = op_binary(&op_logical_or,*next,data_unum(0));
	    next = &next->op->b;
	}
	*next = op_binary(&op_eq,
	  op_binary(&op_and,
	    data_clone(acc),
	    set_nth_bit(i)),
	  get_bit(c128,i) ? data_unum(0) : set_nth_bit(i));
    }
    assert(d.type != dt_none);
    return d;
}


/* See build_ne_bit for comments. */

static DATA build_ne_prefix(DATA acc,DATA c)
{
    DATA d = data_none(),*next = &d;
    U128 c128,mask;
    int i;

    c128 = data_convert(c,dt_ipv6).u.u128;
    if (acc.op->dsc == &op_and && !acc.op->b.op &&
      (acc.op->b.type == dt_unum || acc.op->b.type == dt_ipv6))
	mask = data_convert(acc.op->b,dt_ipv6).u.u128;
    else mask = u128_not(u128_from_32(0));
    for (i = 0; i < 128; i++) {
	if (!get_bit(mask,i)) continue;
	if (d.type != dt_none) {
	    *next = op_binary(&op_logical_or,*next,data_unum(0));
	    next = &next->op->b;
	}
	*next = op_binary(&op_eq,
	  op_binary(&op_and,
	    data_clone(acc),
	    bit_mask(i)),
	  prefix_and_twist(c128,i));
    }
    assert(d.type != dt_none);
    return d;
}


static void build_ne(DATA *d)
{
    DATA new;

    if (d->op->b.op || (d->op->b.type != dt_unum && d->op->b.type != dt_ipv6))
	return; /* bad luck */
    if (registry_probe(&optimization_switches,"prefix"))
	new = build_ne_prefix(d->op->a,d->op->b);
    else new = build_ne_bit(d->op->a,d->op->b);
    data_destroy(*d);
    *d = new;
}


static void ne_to_eq(DATA *d)
{
    if (!d->op) return;
    if (d->op->dsc == &op_ne) build_ne(d);
    if (d->op) {
	ne_to_eq(&d->op->a);
	ne_to_eq(&d->op->b);
	ne_to_eq(&d->op->c);
    }
}


/* ------------------------------------------------------------------------- */


void iflib_arith(DATA *d)
{
    mul_div_mod(d);
    debug_expr("AFTER mul_div_mod",*d);
    rel_math(d);
    debug_expr("AFTER rel_math",*d);
    rel_eq_or(d);
    debug_expr("AFTER rel_eq_or",*d);
    rel_access(d);
    debug_expr("AFTER rel_access",*d);
    rel_to_eq(d);
    debug_expr("AFTER rel_to_eq",*d);
    rel_general(d);
    debug_expr("AFTER rel_general",*d);
    if (registry_probe(&optimization_switches,"ne")) {
	ne_to_eq(d);
	debug_expr("AFTER ne_to_eq",*d);
    }
}
