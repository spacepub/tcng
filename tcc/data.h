/*
 * data.h - Typed data containers
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots, Werner Almesberger
 */


#ifndef DATA_H
#define DATA_H


#include <stdint.h>
#include <stdio.h>

#include <u128.h>


/* including tree.h would cause circular dependency via param.h:PARAM */
struct _qdisc;
struct _class;
struct _filter;
struct _police;
/* ditto for op.h */
struct _op;
/* ditto for field.h */
struct _field;
struct _field_root;


typedef enum {
    dt_none,	/* no data */
    dt_unum,	/* unsigned integer (don't trust double to preserve a u32) */
    dt_ipv4,	/* like dt_unum, but we know this is an IPv4 address */
    dt_ipv6,	/* 128 bit integer representing an IPv6 address */
    dt_fnum,	/* floating-point number */
    dt_string,	/* string (malloc'ed) */
    dt_rate,	/* rate (bits per second) */
    dt_size,	/* size (Bytes) */
    dt_prate,	/* rate (packets per second) */
    dt_psize,	/* size (packets) */
    dt_time,	/* time (seconds) */
    dt_device,	/* device (internal use only) */
    dt_qdisc,	/* queuing discipline */
    dt_class,	/* class */
    dt_filter,	/* filter */
    dt_tunnel,	/* tunnel (internal use only) */
    dt_police,	/* policer */
    dt_bucket,	/* bucket */
    dt_list,	/* list of data elements */
    dt_field,	/* field */
    dt_field_root,
		/* field root */
    dt_decision,/* generalized policing decision */
    dt_assoc,	/* associative array (structure) */
} DATA_TYPE;


struct _data;


typedef enum {
    dr_continue,	/* continue/unspec */
    dr_class,		/* pass/ok */
    dr_drop,		/* drop/shot */
    dr_reclassify,	/* reclassify */
} DECISION_RESULT;

typedef struct _data_list {
    struct _data *ref;		/* pointer to actual data */
    struct _data_list *next;	/* next list element */
} DATA_LIST;

typedef struct _data {
    DATA_TYPE type;
    struct _op *op;
	/* if non-NULL, u is invalid and the expression is variable */
    union {
	uint32_t unum;		/* dt_unum, dt_ipv4 */
	U128 u128;		/* dt_ipv6 */
	double fnum;		/* dt_fnum, dt_p?rate, dt_p?size, dt_time */
	char *string;		/* dt_string */
	struct _device *device;	/* dt_device */
	struct _qdisc *qdisc;	/* dt_qdisc */
	struct _class *class;	/* dt_class */
	struct _filter *filter;	/* dt_filter */
	struct _tunnel *tunnel;	/* dt_tunnel */
	struct _police *police;	/* dt_police, dt_bucket */
	DATA_LIST *list;	/* dt_list */
	struct _field *field;	/* dt_field */
	struct _field_root *field_root;
				/* dt_field_root */
	struct {
	    DECISION_RESULT result;
	    const struct _class *class;
	} decision;		/* dt_decision */
	struct _data_assoc *assoc;
				/* dt_assoc */
    } u;
} DATA;

typedef struct _data_assoc {
    const char *name;		/* malloc'ed name */
    DATA data;
    const struct _data_assoc *parent; /* for reverse tree traversal */
    struct _data_assoc *next;
} DATA_ASSOC;


DATA data_none(void);
DATA data_unum(uint32_t in);
DATA data_ipv4(uint32_t in);
DATA data_ipv6(U128 in);
DATA data_fnum(double in);
DATA data_typed_fnum(double in,DATA_TYPE type);
DATA data_string(const char *in);
DATA data_device(struct _device *in);
DATA data_qdisc(struct _qdisc *in);
DATA data_class(struct _class *in);
DATA data_decision(DECISION_RESULT result,struct _class *class);
DATA data_filter(struct _filter *in);
DATA data_tunnel(struct _tunnel *in);
DATA data_police(struct _police *in);
DATA data_bucket(struct _police *in);
DATA data_list(DATA_LIST *list);
DATA_LIST *data_list_element(DATA *ref);
DATA data_field(struct _field *field);
DATA data_field_root(struct _field_root *field_root);
DATA data_assoc(void);

const char *type_name(DATA_TYPE type);

DATA data_convert(DATA in,DATA_TYPE type);
int type_is_u32(DATA_TYPE type);

/*
 * unit.type must be dt_none, dt_p?rate, dt_p?size, or dt_time
 */

DATA data_add_unit(DATA in,DATA unit);

DATA data_clone(DATA d);
int data_equal(DATA a,DATA b);

void data_destroy_1(DATA d);
void data_destroy(DATA d);

void print_data(FILE *file,DATA d);

#endif /* DATA_H */
