/*
 * util.h - Utility functions
 *
 * Written 2001,2002,2004 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks, Network Robots
 * Copyright 2004 Werner Almesberger
 */


#ifndef UTIL_H
#define UTIL_H

#include <memutil.h>


#define UNDEF_U32 (~0U)


/* ----- Helper functions for performance tuning --------------------------- */


void start_timer(void);
void print_timer(const char *label);


/* ----- Macros to simplify handling of default parameters ----------------- */

/*
 * Define list of default parameters as follows:
 *
 * #define __DEFAULT_PRM(f) f(param1) f(param2) ...
 */

#define __DP_LIST(p) &prm_##p,
#define __DP_DECL(p) prm_##p##_default,
#define __DP_ARGS(p) PARAM_DSC prm_##p##_default,
#define __DP_PASS(p) prm_##p,
#define __DP_SET(p) prm_##p##_default = prm_##p;
#define __DP_GET(p) if (!prm_##p.present) prm_##p = prm_##p##_default;
#define __DP_CHECK(p) \
  if (!prm_##p.present) lerrorf(loc,"required parameter \"%s\" is missing",#p);
#define DEFAULT_LIST __DEFAULT_PRM(__DP_LIST)
#define DEFAULT_ARGS __DEFAULT_PRM(__DP_ARGS) void *__unused
#define DEFAULT_PASS __DEFAULT_PRM(__DP_PASS) NULL
#define DEFAULT_DECL \
  PARAM_DSC __DEFAULT_PRM(__DP_DECL) __attribute__((unused)) __unused
#define DEFAULT_SET \
  do { __DEFAULT_PRM(__DP_SET) } while (0)
#define DEFAULT_GET \
  do { __DEFAULT_PRM(__DP_GET) } while (0)
#define DEFAULT_CHECK(l) \
  do { LOCATION loc = l; __DEFAULT_PRM_REQ(__DP_CHECK) } while (0)

/*
 * When recursing, we need another set of variables to save defaults to.
 */

#define __DP_DECL_SAVED(p) prm_##p##_saved,
#define __DP_PASS_SAVED(p) prm_##p##_saved,
#define __DP_SAVE(p) prm_##p##_saved = prm_##p;
#define DEFAULT_PASS_SAVED __DEFAULT_PRM(__DP_PASS_SAVED) NULL
#define DEFAULT_DECL_SAVED \
  PARAM_DSC __DEFAULT_PRM(__DP_DECL_SAVED) __attribute__((unused)) __unused
#define DEFAULT_SAVE \
  do { __DEFAULT_PRM(__DP_SAVE) } while (0)


/* ----- Helper function for qsort ----------------------------------------- */

int comp_int(const void *a,const void *b);

#endif /* UTIL_H */
