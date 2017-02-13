/*
 * if_c.c - Processing of the "if" command, generating a C function
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Werner Almesberger, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots, Werner Almesberger
 */

/*
 * In GCC we trust. There's absolutely zero optimization happening in this
 * file. It's up to the C compiler to figure out what to do. Since optimizing
 * arithmetic expressions is about the oldest issue in compiler design, most
 * compilers are really good at this.
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <linux/if_ether.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "data.h"
#include "op.h"
#include "field.h"
#include "tree.h"
#include "param.h"
#include "tc.h"
#include "iflib.h"
#include "if.h"

#include "tccmeta.h"


static int results = 0;
static int buckets = 0;
static uint32_t bucket[MAX_C_BUCKETS];
static unsigned long class[MAX_C_RESULTS];


/* ------------------------ Dump bucket definitions ------------------------ */


static int __lookup_bucket(uint32_t number)
{
    int n;

    for (n = 0; n < buckets; n++)
	if (bucket[n] == number) return n;
    abort();
}


static int lookup_bucket(DATA d)
{
    assert(d.type == dt_police || d.type == dt_bucket); /* @@@ */
    return __lookup_bucket(d.u.police->number);
}


static uint32_t depth_sum(const POLICE *p)
{
    const POLICE *overflow = NULL;

    if (!p) return 0;
    if (prm_present(p->params,&prm_overflow))
	overflow = prm_data(p->params,&prm_overflow).u.police;
    return prm_data(p->params,&prm_burst).u.fnum+depth_sum(overflow);
}


static void add_bucket(FILE *file,const POLICE *p)
{
    int overflow = -1;
    uint32_t total_depth,max_time;
    int scale;

    param_get(p->params,p->location);
    if (!prm_rate.present && prm_peakrate.present)
        error("if_c only supports single-rate token bucket policer");
    if (buckets >= MAX_C_BUCKETS)
	errorf("too many buckets in if_c (max %d)",MAX_C_BUCKETS);
    bucket[buckets++] = p->number;
    if (prm_present(p->params,&prm_overflow))
	overflow = __lookup_bucket(prm_data(p->params,
	  &prm_overflow).u.police->number);
    total_depth = depth_sum(p);
    max_time = total_depth/(double) prm_rate.v*1000000.0;
    if (debug)
	fprintf(stderr,"add_bucket(depth, %lu, total_dep %lu max_time %lu\n",
	  (unsigned long) prm_burst.v,(unsigned long) total_depth,
	  (unsigned long) max_time);
    for (scale = 0; (max_time >> scale) > 255; scale++);
    fprintf(file,"  INIT_BUCKET(%lu,%lu,%lu,%lu,%d,%lu,%lu,%d), \\\n",
      (unsigned long) prm_rate.v,
      prm_mpu.present ? (unsigned long) prm_mpu.v : 0,
      (unsigned long) prm_burst.v,(unsigned long) prm_burst.v,overflow,
      (unsigned long) max_time,(unsigned long) total_depth,scale);
}


static void collect_buckets(FILE *file,DATA d)
{
    if (!d.op && (d.type == dt_police || d.type == dt_bucket)) { /* @@@ */
	const POLICE *p = d.u.police;

	param_get(p->params,p->location);
	if (prm_present(p->params,&prm_overflow))
	    add_bucket(file,prm_data(p->params,&prm_overflow).u.police);
	add_bucket(file,d.u.police);
	return;
    }
    if (d.op) {
	collect_buckets(file,d.op->a);
	collect_buckets(file,d.op->b);
	collect_buckets(file,d.op->c);
    }
}



static void dump_buckets(FILE *file,DATA d)
{
    fprintf(file,"#define BUCKETS \\\n");
    collect_buckets(file,d);
    fprintf(file,"\n");
}


/* --------------------------- Dump C expression --------------------------- */


static void dump_c_data(FILE *file,DATA d)
{
    switch (d.type) {
	case dt_unum:
	case dt_ipv4:
	    fprintf(file,"0x%lx",(unsigned long) d.u.unum);
	    break;
	case dt_decision:
	    switch (d.u.decision.result) {
		case dr_class:
		case dr_reclassify:
    		    if (results >= MAX_C_RESULTS)
			errorf("too many results in if_c (max %d)",
			  MAX_C_RESULTS);
		    class[results] = (d.u.decision.class->parent.qdisc->number
		       << 16) | d.u.decision.class->number;
		    fprintf(file,"({ RESULT(%s,%d); 0; })",
		      d.u.decision.result == dr_class ? "TC_POLICE_OK" :
		      "TC_POLICE_RECLASSIFY",results);
		    results++;
		    break;
		case dr_drop:
		    fprintf(file,"({ RESULT(TC_POLICE_SHOT,0); 0; })");
		    break;
		case dr_continue:
		    fprintf(file,"({ RESULT(TC_POLICE_UNSPEC,0); 0; })");
		    break;
		default:
		    dump_failed(d,"unsupported decision");
	    }
	    break;
	default:
	    dump_failed(d,"bad type");
    }
}


static void dump_c_expr(FILE *file,DATA d);


static void dump_math_op(FILE *file,const OP *op)
{
    if (op->b.type == dt_none) {
	fprintf(file," %s",op->dsc->name);
	dump_c_expr(file,op->a);
    }
    else {
	fputc('(',file);
	dump_c_expr(file,op->a);
	fprintf(file," %s ",op->dsc->name);
	dump_c_expr(file,op->b);
	fputc(')',file);
	
    }
}


static void dump_c_op(FILE *file,DATA d)
{
    if (d.op->dsc == &op_plus || d.op->dsc == &op_minus || 
      d.op->dsc == &op_mult || d.op->dsc == &op_div || d.op->dsc == &op_mod ||
      d.op->dsc == &op_not || d.op->dsc == &op_and || d.op->dsc == &op_or ||
      d.op->dsc == &op_xor || d.op->dsc == &op_shift_left ||
      d.op->dsc == &op_shift_right || d.op->dsc == &op_eq ||
      d.op->dsc == &op_ne || d.op->dsc == &op_gt || d.op->dsc == &op_le ||
      d.op->dsc == &op_ge || d.op->dsc == &op_lt ||
      d.op->dsc == &op_logical_not || d.op->dsc == &op_logical_or ||
      d.op->dsc == &op_logical_and) {
	dump_math_op(file,d.op);
	return;
    }
    if (d.op->dsc == &op_conform) {
	fprintf(file,"police_conform(skb,%d)",lookup_bucket(d.op->a));
	return;
    }
    if (d.op->dsc == &op_count) {
	fprintf(file,"(police_count(skb,%d), 1)",lookup_bucket(d.op->a));
	return;
    }
    if (d.op->dsc == &op_access) {
	if (d.op->a.op || d.op->a.type != dt_field_root)
	    dump_failed(d,"invalid access base");
	if (d.op->a.u.field_root->offset_group) {
	    if (d.op->a.u.field_root->offset_group != META_FIELD_ROOT)
		errorf("\"c\" only supports field roots 0 and %d",
		  META_FIELD_ROOT);
	    if (d.op->b.op || d.op->b.type != dt_unum ||
	      d.op->b.u.unum != META_PROTOCOL_OFFSET)
		error("invalid offset for meta_protocol");
	    if (d.op->c.op || d.op->c.type != dt_unum ||
	      d.op->c.u.unum != META_PROTOCOL_SIZE*8)
		error("invalid size for meta_protocol access");
	    fprintf(file,"ACCESS_META_PROTOCOL");
	    return;
	}
	fprintf(file,"ACCESS%d(",d.op->c.u.unum);
	dump_c_expr(file,d.op->b);
	fprintf(file,")");
	return;
    }
    dump_failed(d,"bad operator");
/* @@@ missing: mask */
}


static void dump_c_expr(FILE *file,DATA d)
{
    if (!d.op) dump_c_data(file,d);
    else dump_c_op(file,d);
}


/* ------------------------------------------------------------------------- */


static char tcc_module_args[MAX_MODULE_ARGS] = "";


void add_tcc_module_arg(int local,const char *arg)
{
    if (strlen(tcc_module_args)+strlen(arg)+3 >= MAX_MODULE_ARGS)
	error(TCC_MODULE_CMD "argument string too long");
    strcat(tcc_module_args," '");
    if (local) strcat(tcc_module_args,arg+2);
    else {
	strcat(tcc_module_args,"-X");
	strcat(tcc_module_args,arg);
    }
    strcat(tcc_module_args,"'");
}


void dump_if_c(const FILTER *filter)
{
    DATA d = filter->parent.qdisc->if_expr;
    char *name,*file_name,*cmd;
    const char *tcng_topdir;
    FILE *file;
    int i;

    if (prm_present(filter->params,&prm_pragma))
	lwarn(filter->location,"\"pragma\" on \"if\" ignored by \"C\" target");
    name = alloc_sprintf("_c%02x%02x%01x%01x",
      (int) (filter->parent.qdisc->number & 0xff),
      (int) (filter->number & 0xff),(int) (getpid() & 0xf),
      (int) (time(NULL) & 0xf));
    file_name = alloc_sprintf("%s.mdi",name);
    file = fopen(file_name,"w");
    if (!file) {
	perror(file_name);
	exit(1);
    }
    fprintf(file,"#define NAME %s\n\n",name);
    dump_buckets(file,d);
    fprintf(file,"#define EXPRESSION \\\n");
    dump_c_expr(file,d);
    fprintf(file,"\n\n#define ELEMENTS %d\n#define ELEMENT(f) \\\n",results);
    for (i = 0; i < results; i++)
	fprintf(file,"  f(%d,0x%lx)%s\n",i,class[i],
	  i == results-1 ? "" : " \\");
    if (ferror(file)) errorf("error writing to %s",file_name);
    if (fclose(file) == EOF) {
	perror(file_name);
	exit(1);
    }
    tcng_topdir = getenv("TCNG_TOPDIR");
    cmd = alloc_sprintf("%s/" TCNG_BIN_DIR "/" TCC_MODULE_CMD "%s cls %s %s",
      tcng_topdir ? tcng_topdir : DATA_DIR,tcc_module_args,file_name,name);
    fflush(stdout);
    fflush(stderr);
    if (system(cmd)) errorf("system(%s) failed",cmd);
    printf("insmod cls_%s.o\n",name);
    __tc_filter_add(filter,ETH_P_ALL);
    tc_more(" %s\n",name);
    free(name);
    free(file_name);
    free(cmd);
}
