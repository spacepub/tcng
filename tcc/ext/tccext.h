/*
 * tccext.h - Parse information received from tcc's external interface
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001,2002 Bivio Networks, Network Robots.
 * Copyright 2002 Werner Almesberger
 * Distributed under the LGPL.
 */


#ifndef TCCEXT_H
#define TCCEXT_H


#include <stdint.h>
#include <stdio.h>


#define __TE_SIZEOF(user_size,default_size) \
  ({ if ((user_size) && (user_size) < (default_size)) abort(); \
    (user_size) ? (user_size) : (default_size); })

#define TE_SIZEOF(ctx,field,dflt) __TE_SIZEOF(ctx->sizes.field,sizeof(dflt))


/* ----- Pragmas ----------------------------------------------------------- */


typedef struct _tccext_pragma {
    const char *pragma;			/* pragma value */
    struct _tccext_pragma *next;	/* next pragma; NULL if last */
} TCCEXT_PRAGMA;


/* ----- Fields and offsets ------------------------------------------------ */


struct _tccext_offset;

typedef struct _tccext_field {
    struct _tccext_offset *offset_group;/* base offset group; NULL for 0/raw */
    int offset;				/* bit offset within offset group */
    int length;				/* field length in bits */
} TCCEXT_FIELD;

typedef struct _tccext_offset {
    struct _tccext_offset *base;	/* base offset group; NULL for 0/raw */
    TCCEXT_FIELD field;			/* field containing increment */
    int shift_left;			/* increment scaling (to yield bits) */
    int group_number;			/* group number (semi-private) */
    struct _tccext_offset *next;	/* next offset group; NULL if last
					   (semi-private) */
} TCCEXT_OFFSET;


/*
 * Detect meta field group either by comparing pointer with
 * &tccext_meta_field_group, or by testing that offset->group_number equals
 * META_FIELD_ROOT (from tccmeta.h)
 */

extern TCCEXT_OFFSET tccext_meta_field_group;


/* ----- Buckets ----------------------------------------------------------- */


typedef struct _tccext_bucket {
    uint32_t rate;			/* bucket rate, in bytes per second */
    int mpu;				/* minimum policed unit, in bytes */
    uint32_t depth;			/* bucket depth, in bytes */
    uint32_t initial_tokens;		/* inital "credit", in bytes */
    TCCEXT_PRAGMA *pragmas;		/* bucket pragmas; NULL if none */
    const char *location;		/* location spec */
    int index;				/* bucket index (semi-private) */
    struct _tccext_bucket *overflow;	/* bucket to send excess tokens to */
    struct _tccext_bucket *next;	/* next bucket; NULL if last
					   (semi-private) */
} TCCEXT_BUCKET;


/* ----- Actions ----------------------------------------------------------- */


typedef enum { tat_conform,tat_count,tat_class,tat_drop,tat_unspec }
  TCCEXT_ACTION_TYPE;

typedef struct _tccext_class_list {
    struct _tccext_class *class;	/* class referenced by list entry */
    struct _tccext_class_list *next;	/* next list entry; NULL if last */
} TCCEXT_CLASS_LIST;

typedef struct _tccext_action {
    TCCEXT_ACTION_TYPE type;
    union {
	struct {
	    TCCEXT_BUCKET *bucket;
	    struct _tccext_action *yes_action;
	    struct _tccext_action *no_action;
	} conform;
	struct {
	    TCCEXT_BUCKET *bucket;
	    struct _tccext_action *action;
	} count;
	TCCEXT_CLASS_LIST *class_list;
    } u;
    int index;				/* action index (semi-private) */
    struct _tccext_action *next;	/* next action; NULL if last
					   (semi-private) */
} TCCEXT_ACTION;


/* ----- Bit-based classification ------------------------------------------ */

/*
 * WARNING: this definition of TCCEXT_BIT is experimental and will change in
 *          the future !
 */

typedef struct _tccext_bit {
    TCCEXT_ACTION *action;		/* take action instead of matching */
    TCCEXT_FIELD field;			/* field (bit) to test; undefined if
					   "actions" is non-NULL */
    struct _tccext_bit *edge[2];	/* next bits, depending on value */
    int index;				/* bit number (semi-private) */
    struct _tccext_bit *next;		/* next bit (private) */
} TCCEXT_BIT;


/* ----- Rule-based classification ----------------------------------------- */


typedef struct _tccext_match {
    TCCEXT_FIELD field;			/* field to match with */
    uint8_t *data;			/* data, right-aligned to byte
					   boundary (note that the allocation
					   is rounded to the next full word,
					   but this does not affect the
					   alignment) */
    struct _tccext_match *next;		/* next match in rule; NULL if last */
    struct _tccext_context *context;	/* back pointer; PRIVATE */
} TCCEXT_MATCH;

typedef struct _tccext_rule {
    TCCEXT_MATCH *matches;		/* ordered list of matches */
    TCCEXT_ACTION *actions;		/* actions if match; NULL indicates a
					   "barrier" */
    struct _tccext_rule *next;		/* next rule; NULL if last */
} TCCEXT_RULE;


/* ----- Qdiscs and classes ------------------------------------------------ */


typedef struct _tccext_parameter {
    const char *name;			/* parameter name */
    uint32_t value;			/* parameter value */
    struct _tccext_parameter *next;	/* next parameter; NULL if last */
} TCCEXT_PARAMETER;

typedef struct _tccext_class {
    struct _tccext_qdisc *parent_qdisc;	/* parent queuing discipline */
    int index;				/* class number */
    TCCEXT_PARAMETER *parameters;	/* class parameters; NULL if none */
    TCCEXT_PRAGMA *pragmas;		/* class pragmas; NULL if none */
    const char *location;		/* location spec */
    struct _tccext_qdisc *qdisc;	/* child queuing discipline (or NULL) */
    struct _tccext_class *classes;	/* child classes; NULL if none */
    struct _tccext_class *next;		/* next class; NULL if none */
} TCCEXT_CLASS;

typedef struct _tccext_qdisc {
    const char *type;			/* qdisc type (prio, red, etc.);
					   NULL if qdisc is anonymous
					   (i.e. implicitly generated) */
    int index;				/* qdisc index */
    TCCEXT_PARAMETER *parameters;	/* class parameters; NULL if none */
    TCCEXT_CLASS *classes;		/* classes of this qdisc */
    TCCEXT_PRAGMA *pragmas;		/* qdisc pragmas; NULL if none */
    const char *location;		/* location spec */
    struct _tccext_qdisc *next;		/* next qdisc; NULL if last */
} TCCEXT_QDISC;


/* ----- Blocks ------------------------------------------------------------ */

/*
 * A traffic control block is the set of qdiscs associated with the ingress
 * or egress side of an interface. All other elements (buckets, actions,
 * etc.) may be shared across blocks.
 */

typedef enum {
    tbr_ingress,			/* block is on ingress side */
    tbr_egress,				/* block is on egress side */
} TCCEXT_BLOCK_ROLE;

typedef struct _tccext_block {
    const char *name;			/* interface name */
    TCCEXT_BLOCK_ROLE role;		/* role of block */
    TCCEXT_QDISC *qdiscs;		/* ordered list of qdiscs (first qdisc
					   in list is the top-level qdisc);
					   NULL if no qdiscs */
    TCCEXT_ACTION *actions;		/* list of actions; NULL if none */
    TCCEXT_RULE *rules;			/* ordered list of rules; NULL if
					   none */
    TCCEXT_BIT *fsm;			/* finite state machine with single-bit
					   decisions; NULL if none */
    TCCEXT_BIT *bits;			/* list of bits (private) */
    TCCEXT_PRAGMA *pragmas;		/* interface pragmas; NULL if none */
    const char *location;		/* location spec */
    struct _tccext_block *next;		/* next block; NULL if last */
} TCCEXT_BLOCK;

/* Note: only one of "rules" and "fsm" may be non-NULL ! */


/* ----- Context ----------------------------------------------------------- */


typedef struct {
    int offset;				/* user offset size; 0 for default */
    int bucket;				/* user bucket size; 0 for default */
    int class_list;			/* user class list size; 0 default */
    int action;				/* user action size; 0 for default */
    int actions;			/* user actions size; 0 for default */
    int match;				/* user match size; 0 for default */
    int rule;				/* user rule size; 0 for default */
    int bit;				/* user bit size; 0 for default */
    int class;				/* user class size; 0 for default */
    int qdisc;				/* user qdisc size; 0 for default */
    int block;				/* user block size; 0 for default */
    int context;			/* user context size; 0 for default */
    int pragma;				/* user pragma size; 0 for default */
    int parameter;			/* user parameter size; 0 for default */
} TCCEXT_SIZES;

typedef struct _tccext_context {
    TCCEXT_PRAGMA *pragmas;		/* list of global pragmas; none: NULL */
    TCCEXT_BUCKET *buckets;		/* list of buckets; NULL if none */
    TCCEXT_OFFSET *offset_groups;	/* offset groups (semi-private) */
    TCCEXT_BLOCK *blocks;		/* list of blocks; NULL if none */
    TCCEXT_SIZES sizes;
} TCCEXT_CONTEXT;


/* ----- Function prototypes ----------------------------------------------- */


TCCEXT_CONTEXT *tccext_parse(FILE *file,TCCEXT_SIZES *sizes);
    /* pass NULL in sizes to use defaults */

void tccext_destroy(TCCEXT_CONTEXT *ctx);
    /* Delete structure returned by tccext_parse. tccext_destroy does not
       de-allocate memory objects attached to user fields ! */

#endif
