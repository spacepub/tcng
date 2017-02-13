/*
 * config.h - Configuration definitions and variables
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots, Werner Almesberger
 */


#ifndef CONFIG_H
#define CONFIG_H

#include "registry.h"


#define DEFAULT_DEVICE	"eth0"
#define CPP		"/lib/cpp"
#define DEV_NULL	"/dev/null"  /* dummy file system object */
#define MAX_COMMENT	1024	/* maximum comment line(s) in tc output */
#define TCC_MODULE_CMD	"tcc-module"
#define TCNG_BIN_DIR	"bin"
#define TCNG_INC_DIR	"include"
#define MAX_DEBUG_DEPTH	100	/* stack depth for debugf; okay to guess */
#define MAX_PARAM_DEPTH	100	/* parameter stack depth */
#define MAX_MODULE_ARGS	1024	/* maximum length of tcc-module arguments */
#define MAX_C_BUCKETS	1024	/* buckets used in if_c */
#define MAX_C_RESULTS	4096	/* exit points in if_c expression */
#define MAX_U32_KEYS	1024	/* number of u32 keys per match */
#define MAX_EXT_LINE	10000	/* maximum line length at ext. interface */
#define RESERVED_OFFSET_GROUPS 10
				/* reserved for internal/future use */
#define AUTO_OFFSET_GROUP_BASE 100 /* auto-assign offset groups from here */

extern const char *default_device;
extern const char *location_file; /* write location map to this file */
extern const char *var_use_file; /* write variable use info to this file */
extern int remove_qdiscs; /* issue tc commands to remove old qdiscs */
extern int quiet; /* produce terse output */
extern int debug; /* debugging message and sanity check level (0 = none) */
extern int generate_default_class; /* if_ext generates default classes */
extern int use_bit_tree; /* use experimental bit tree algorithm */
extern int alg_mode; /* algorithm mode: 0 = default; > 0 = experimental */
extern int dump_all; /* dump all qdiscs and filters (experimental) */
extern int add_fifos; /* add FIFOs default qdisc is used */
extern int no_combine; /* don't combine ifs into single expression */
extern int no_warn; /* suppress all warnings */
extern int no_struct_separator; /* join struct entries without . */

extern int hash_debug; /* at end, write hash statistics to stderr */

extern int warn_truncate; /* warn if truncating parameter values */
extern int warn_unused; /* report unused variables */
extern int warn_expensive; /* warn about expensive operations */
extern int warn_exp_post_opt; /* like above, but test after optimization */
extern int warn_exp_error; /* turn above warnings intro hard errors */
extern int warn_redefine; /* warn when re-defining variables */
extern int warn_explicit; /* warn if shared class is explictly defined */
extern int warn_const_pfx; /* warn if taking prefix of constant IP address */

extern REGISTRY optimization_switches;

#endif /* CONFIG_H */
