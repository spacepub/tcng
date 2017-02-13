/*
 * tcc.c - TC compiler
 *
 * Written 2001,2002,2004 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Network Robots
 * Copyright 2002,2004 Werner Almesberger
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "config.h"
#include "error.h"
#include "util.h"
#include "registry.h"
#include "data.h"
#include "var.h"
#include "tree.h"
#include "device.h"
#include "target.h"
#include "if.h"
#include "ext.h"
#include "ext_all.h"
#include "location.h"


#define STRING(s) #s


extern int yyparse(void);


DATA_LIST *pragma = NULL;
const char *default_device = DEFAULT_DEVICE;
const char *location_file = NULL;
const char *var_use_file = NULL;
int alg_mode = 0;
int remove_qdiscs = 0;
int quiet = 0;
int debug = 0;
int no_warn = 0;
int no_struct_separator = 0;
int hash_debug = 0;
REGISTRY optimization_switches;


/* ----- CPP child process ------------------------------------------------- */


static pid_t pid;


static void kill_cpp(void)
{
    if (pid) (void) kill(pid,SIGTERM);
}


static void run_cpp(int argc,char *const *argv,int use_pipe)
{
    int fds[2];

    if (use_pipe && pipe(fds) < 0) {
	perror("pipe");
	exit(1);
    }
    pid = fork();
    if (pid < 0) {
	perror("fork");
	exit(1);
    }
    if (!pid) {
	if (use_pipe && dup2(fds[1],1) < 0) {
	    perror("dup2");
	    exit(1);
	}
	if (execvp(CPP,argv) < 0) {
	    perror("execvp " CPP);
	    exit(1);
	}
	/* not reached */
    }
    if (use_pipe) {
	if (dup2(fds[0],0) < 0) {
	    perror("dup2");
	    exit(1);
	}
	if (close(fds[1]) < 0) {
	    perror("close");
	    exit(1);
	}
    }
    atexit(kill_cpp);
}


static void finish_cpp(void)
{
    int status;

    if (waitpid(pid,&status,0) < 0) {
	perror("waitpid");
	exit(1);
    }
    pid = 0; /* already dead */
    if (!WIFEXITED(status)) error("abnormal termination of cpp");
    if (WEXITSTATUS(status)) exit(WEXITSTATUS(status));
}


/* ----- Command-line parsing ---------------------------------------------- */


static void usage(const char *name)
{
    fprintf(stderr,"usage: %s [-c] [-d ...] [-E] [-f infile] "
      "[-i default_interface]\n",name);
    fprintf(stderr,"%10s [-l location_file] [-n] [-q] [-r] [-w] [-W[no]...]\n",
      "");
    fprintf(stderr,"%10s [-O[no]...] [-x elem:ext_target ...] "
      "[-t [elem:][no]target ...]\n","");
    fprintf(stderr,"%10s [-u var_use_file] [-Xphase,arg] [cpp_option ...]\n",
      "");
    fprintf(stderr,"%10s [infile]\n","");
    fprintf(stderr,"%6s %s -V\n\n","",name);
    fprintf(stderr,"  -c                    only check validity\n");
    fprintf(stderr,"  -d ...                increase debugging level\n");
    fprintf(stderr,"  -E                    run cpp only\n");
    fprintf(stderr,"  -f infile             use specified file, ignoring "
      "regular file argument\n");
    fprintf(stderr,"  -i default_interface  interface if not specified in "
      "description file\n");
    fprintf(stderr,"                        (default: "
      STRING(DEFAULT_DEVICE) ")\n");
    fprintf(stderr,"  -l location_file      write a list of source code "
      "locations of traffic\n");
    fprintf(stderr,"                        control elements (\"stderr\" for "
      "standard error)\n");
    fprintf(stderr,"  -n                    do not include default.tc\n");
    fprintf(stderr,"  -q                    quiet, produce terse output\n");
    fprintf(stderr,"  -r                    remove old qdiscs (tc only)\n");
    fprintf(stderr,"  -t [elem:][no]target  enable or disable target\n");
    fprintf(stderr,"                        elements: if\n");
    fprintf(stderr,"                        targets: all, tc, c, ext; default: "
      "tc\n");
    fprintf(stderr,"  -u variable_use_file  for each variable, write name and "
      "value\n");
    fprintf(stderr,"  -V                    print version number and exit\n");
    fprintf(stderr,"  -w                    suppress all warnings\n");
    fprintf(stderr,"  -W[no]expensive       warn if encountering \"expensive\""
      "constructs in\n");
    fprintf(stderr,"                        classifier (default: off)\n");
    fprintf(stderr,"  -W[no]exppostopt      like above, but test after "
      "optimization (default: off)\n");
    fprintf(stderr,"  -W[no]experror        turn above warnings intro hard "
     "errors (default: off)\n");
    fprintf(stderr,"  -W[no]redefine        warn if re-defining variables "
      "(default: off)\n");
    fprintf(stderr,"  -W[no]truncate        warn if truncating values "
      "(default: off)\n");
    fprintf(stderr,"  -W[no]unused          report unused variables "
      "(default: on)\n");
    fprintf(stderr,"  -W[no]explicit        warn if shared class explicitly "
      "defined (default: on)\n");
    fprintf(stderr,"  -W[no]constpfx        warn if taking prefix of IP "
      "constant (default: off)\n");
    fprintf(stderr,"  -O[no]cse             common subexpression "
      "elimination (default: on)\n");
    fprintf(stderr,"  -O[no]ne              turn != into multiple ==s "
      "(default: off)\n");
    fprintf(stderr,"  -O[no]prefix          generate prefix matches instead "
      "of bit tests (def: off)\n");
    fprintf(stderr,"  -x elem:ext_target    register external target\n");
    fprintf(stderr,"  -Xphase,arg           verbatim argument for specific "
      "build phase\n");
    fprintf(stderr,"                        phases: p=cpp, m=tcc-module, "
      "k=kmod_cc, t=tcmod_cc,\n");
    fprintf(stderr,"                                x=external (all), xelem="
      "external (element)\n");
    fprintf(stderr,"  cpp_option            -Idir, -Dname[=value], or "
      "-Uname\n");
    exit(1);
}


int main(int argc,char *const *argv)
{
    static const char *opt_switch_names[] = {
	"cse",
	"ne",
	"prefix",
	NULL
    };
    const char *tcng_topdir;
    char *include,*input = NULL;
    char opt[3] = "-?";
    char **cpp_argv;
    int check_only = 0,cpp_only = 0;
    int include_default = 1;
    int c,cpp_argc = 1;

    target_register("if","tc",1);
    target_register("if","c",0);
    target_register("if","ext",0);
    registry_open(&optimization_switches,opt_switch_names,NULL);
    registry_set(&optimization_switches,"cse");
    cpp_argv = alloc(sizeof(char *)*(argc*2+5)); /* generous allocation */
	/*
         * +1 for -std=c99 or -$
         * +1 for -I
	 * +2 for -include
         * +1 for terminating NULL
	 *
         * (and we'd actually need one less, because we count argv[0] twice)
         */
#ifdef DOLLAR
    cpp_argv[cpp_argc++] = "-$";
#else
    cpp_argv[cpp_argc++] = "-std=c99";
#endif
    tcng_topdir = getenv("TCNG_TOPDIR");
    include = alloc_sprintf("%s/" TCNG_INC_DIR,
      tcng_topdir ? tcng_topdir : TOPDIR);
    cpp_argv[cpp_argc++] = alloc_sprintf("-I%s",include); /* @@@ leak */
    /*
     * Reserved options (strong Unix tradition):
     *
     * -o file  for some output
     * -v       verbose
     */
    while ((c = getopt(argc,argv,"BNcdEf:Hhi:l:nO:qrSt:u:W:wx:D:U:I:VX:")) !=
      EOF)
	switch (c) {
	    case 'c':
		check_only = 1;
		break;
	    case 'B':
		use_bit_tree = 1;
		break;
	    case 'N':
		alg_mode++;
		break;
	    case 'H':
		hash_debug = 1;
		break;
	    case 'd':
		debug++;
		break;
	    case 'f':
		input = optarg;
		break;
	    case 'E':
		cpp_only = 1;
		break;
	    case 'h':
		usage(argv[0]);
	    case 'i':
		default_device = optarg;
		break;
	    case 'l':
		location_file = optarg;
		break;
	    case 'n':
		include_default = 0;
		break;
	    case 'O':
		if (registry_set_no(&optimization_switches,optarg))
		    usage(argv[0]);
		break;
	    case 'q':
		quiet = 1;
		break;
	    case 'r':
		remove_qdiscs = 1;
		break;
	    case 'S':
		no_struct_separator = 1;
		break;
	    case 't':
		switch_target(optarg);
		break;
	    case 'u':
		var_use_file = optarg;
		break;
	    case 'V':
		printf("tcng version %s\n",VERSION);
		return 0;
	    case 'W':
		if (!set_warn_switch(optarg,1)) usage(argv[0]);
		break;
	    case 'w':
		no_warn = 1;
		break;
	    case 'x':
		ext_target_register(optarg);
		break;
            case 'X':
		if (*optarg == 'p')  {
		    cpp_argv[cpp_argc++] = stralloc(optarg+2);
		    break;
		}
		if (*optarg == 'x') {
		    char *tmp = stralloc(optarg);
		    char *comma;

		    comma = strchr(tmp,',');
		    if (!comma) usage(argv[0]);
		    *comma = 0;
		    add_tcc_external_arg(tmp+1,comma+1);
		    free(tmp);
		    break;
		}
		if (!optarg[0] || !strchr("mkt",optarg[0]) || optarg[1] != ',')
		    usage(argv[0]);
		add_tcc_module_arg(optarg[0] == 'm',optarg);
                break;
	    case 'D':
	    case 'U':
	    case 'I':
		opt[1] = c;
		cpp_argv[cpp_argc++] = stralloc(opt);
		cpp_argv[cpp_argc++] = optarg;
		break;
	    default:
		usage(argv[0]);
	}
    ext_targets_configure();
    if (optind < argc) {
	if (!input) input = argv[optind];
	optind++;
	if (optind < argc) usage(argv[0]);
    }
    if (input) cpp_argv[cpp_argc++] = input;
    cpp_argv[0] = CPP; /* cpp 3.3.3 requires this */
    if (include_default) {
	cpp_argv[cpp_argc++] = "-include";
	cpp_argv[cpp_argc++] = alloc_sprintf("%s/default.tc",include);
	  /* @@@ leak */
    }
    cpp_argv[cpp_argc] = NULL;
    if (!cpp_only) run_cpp(cpp_argc,cpp_argv,1);
    else {
	run_cpp(cpp_argc,cpp_argv,0);
	finish_cpp();
	return 0;
    }
    free(include);
    var_begin_scope(); /* create global scope */
    (void) yyparse();
    finish_cpp();
    check_devices();
    var_end_scope(); /* report unused variables */
    if (hash_debug) dump_hash(stderr);
    if (var_use_file) write_var_use(var_use_file);
    if (!check_only) {
	if (dump_all && ext_targets) dump_all_devices(ext_targets->name);
		/* @@@ walk list of targets */
	else dump_devices();
    }
    if (location_file) write_location_map(location_file);
    return 0;
}
