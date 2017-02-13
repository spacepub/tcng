/*
 * tcsim.c - TC simulator
 *
 * Written 2001,2002,2004 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks
 * Copyright 2002,2004 Werner Almesberger
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <u128.h>
#include <addr.h>
#include <memutil.h>

#include "tcsim.h"


#define CPP "/lib/cpp"


extern int yyparse(void);


int snap_len = 1 << 20; /* dump up to snap_len bytes in tracing output */
int use_jiffies = 0; /* print time in jiffies, not seconds */
int use_generation = 0; /* print generation numbers instead of skb addresses */
int printk_threshold = 6; /* KERN_INFO */
int debug = 0;
int verbose = 0;
int preserve = 0; /* preserve attributes across links */
int check_only = 0; /* only check syntax, don't execute commands */


/* ----- Provide error handlers for library functions ---------------------- */


void u128_error_hook(const char *msg)
{
    yyerror(msg);
}


void addr_error_hook(const char *msg)
{
    yyerror(msg);
}


/* ----- tcc subprocess ---------------------------------------------------- */


static char *tcc_cmd = TCC_CMD;
static char **tcc_argv;
static int tcc_argc = 3;


static void add_tcc_arg(int local,const char *arg)
{
    if (local) arg += 2;
    else tcc_argv[tcc_argc++] = "-X";
    tcc_argv[tcc_argc++] = stralloc(arg);
}


char *run_tcc(const char *dev,const char *in)
{
    pid_t writer_pid,tcc_pid;
    int pipe1[2],pipe2[2];
    char buf[2048];
    char *out;
    int got,len;
    int status;

    fflush(stdout);
    if (debug)
	fprintf(stderr,"--- TCC input ----------\n%s\n----------\n",in);
    if (pipe(pipe1) < 0) {
	perror("pipe");
	exit(1);
    }
    if (pipe(pipe2) < 0) {
	perror("pipe");
	exit(1);
    }
    writer_pid = fork();
    if (writer_pid < 0) {
	perror("fork");
	exit(1);
    }
    if (!writer_pid) {
	const char *here;

	for (here = in; *here; here += len) {
	    len = write(pipe1[1],here,strlen(here));
	    if (len < 0) {
		perror("write");
		_exit(1);
	    }
	    if (!len) {
		fprintf(stderr,"can't write\n");
		_exit(1);
	    }
	}
	_exit(0);
    }
    if (close(pipe1[1]) < 0) {
	perror("close");
	exit(1);
    }
    tcc_pid = fork();
    if (tcc_pid < 0) {
	perror("fork");
	exit(1);
    }
    if (!tcc_pid) {
	if (dup2(pipe1[0],0) < 0) {
	    perror("dup2");
	    _exit(1);
	}
	if (dup2(pipe2[1],1) < 0) {
	    perror("dup2");
	    _exit(1);
	}
	tcc_argv[0] = tcc_cmd;
	tcc_argv[1] = "-i";
	tcc_argv[2] = (char *) dev;
	tcc_argv[tcc_argc] = NULL;
	if (execvp(tcc_cmd,tcc_argv) < 0) {
	    perror(tcc_cmd);
	    _exit(1);
	}
	/* not reached */
    }
    if (close(pipe2[1]) < 0) {
	perror("close");
	exit(1);
    }
    out = NULL;
    len = 0;
    while (1) {
	got = read(pipe2[0],buf,sizeof(buf));
	if (got < 0) {
	    perror("read");
	    exit(1);
	}
	if (!got) break;
	out = realloc(out,len+got+1);
	if (!out) {
	    perror("realloc");
	    exit(1);
	}
	memcpy(out+len,buf,got);
	len += got;
	out[len] = 0;
    }
    (void) waitpid(writer_pid,&status,0);
    if (!WIFEXITED(status)) errorf("abnormal termination of writer");
    if (WEXITSTATUS(status)) exit(WEXITSTATUS(status));
    (void) waitpid(tcc_pid,&status,0);
    if (!WIFEXITED(status)) errorf("abnormal termination of tcc");
    if (WEXITSTATUS(status)) exit(WEXITSTATUS(status));
    if (!out) errorf("tcc returned no data");
    if (debug) {
	fprintf(stderr,"--- TCC output ----------\n%s----------\n",out);
	fflush(stderr);
    }
    return out;
}


/* ----- cpp subprocess ---------------------------------------------------- */


static pid_t cpp_pid;


static void kill_cpp(void)
{
    if (cpp_pid) (void) kill(cpp_pid,SIGTERM);
}


static void run_cpp(int argc,char *const *argv)
{
    int fds[2];

    if (pipe(fds) < 0) {
	perror("pipe");
	exit(1);
    }
    cpp_pid = fork();
    if (cpp_pid < 0) {
	perror("fork");
	exit(1);
    }
    if (!cpp_pid) {
	if (dup2(fds[1],1) < 0) {
	    perror("dup2");
	    exit(1);
	}
	if (execvp(CPP,argv) < 0) {
	    perror("execvp " CPP);
	    exit(1);
	}
	/* not reached */
    }
    if (dup2(fds[0],0) < 0) {
	perror("dup2");
	exit(1);
    }
    if (close(fds[1]) < 0) {
	perror("close");
	exit(1);
    }
    atexit(kill_cpp);
}


static void finish_cpp(void)
{
    int status;

    if (waitpid(cpp_pid,&status,0) < 0) {
	perror("waitpid");
	exit(1);
    }
    cpp_pid = 0; /* already dead */
    if (!WIFEXITED(status)) errorf("abnormal termination of cpp");
    if (WEXITSTATUS(status)) exit(WEXITSTATUS(status));
}


/* ----- Command line processing ------------------------------------------- */


static void usage(const char *name)
{
    fprintf(stderr,"usage: %s [-c] [-d [-d]] [-g] [-j] [-k number] [-n] [-p] "
      "[-q]\n",name);
    fprintf(stderr,"%12s [-s snap_len] [-v ...] [-Xphase,arg] "
      "[cpp_option ...] [file]\n","");
    fprintf(stderr,"%6s %s -V\n\n","",name);
    fprintf(stderr,"  -c           only check syntax, don't execute "
      "commands\n");
    fprintf(stderr,"  -d           print all kernel messages (printk)\n");
    fprintf(stderr,"  -d -d        print tcsim debugging messages\n");
    fprintf(stderr,"  -g           print generation numbers, not skb "
      "addresses\n");
    fprintf(stderr,"  -j           print time in jiffies, not seconds\n");
    fprintf(stderr,"  -n           do not include default.tcsim\n");
    fprintf(stderr,"  -p           preserve attributes across links\n");
    fprintf(stderr,"  -q           quiet - don't even trace E or D events\n");
    fprintf(stderr,"  -k threshold set kernel logging threshold (default: 6, "
      "7 with -d)\n");
    fprintf(stderr,"  -s snap_len  limit packet content dumped to snap_len "
      "bytes\n");
    fprintf(stderr,"  -v           enable function result tracing\n");
    fprintf(stderr,"  -v -v        also trace function invocation\n");
    fprintf(stderr,"  -v -v -v     also trace internal element state\n");
    fprintf(stderr,"  -V           print version number and exit\n");
    fprintf(stderr,"  -Xphase,arg  verbatim argument for specific build "
      "phase\n");
    fprintf(stderr,"               phases: c=tcc, P=cpp (tcsim), p=cpp (tcc), "
      "m=tcc-module,\n");
    fprintf(stderr,"                       k=kmod_cc, t=tcmod_cc, x=external "
      "(all),\n");
    fprintf(stderr,"                       xelem=external (element)\n");
    fprintf(stderr,"  cpp_option   -Idir, -Dname[=value], or -Uname\n");
    exit(1);
}


int main(int argc,char *const *argv)
{
    const char *tcng_topdir;
    char *include;
    char opt[3] = "-?";
    char *end;
    char **cpp_argv;
    int c,cpp_argc = 1;
    int set_printk_threshold = -1;
    int include_default = 1;

    cpp_argv = alloc(sizeof(char *)*(argc*2+5));
	/*
	 * +1 for -std=c99 or -$
	 * +1 for -I
	 * +2 for -include
	 * +1 for terminating NULL
	 */
    tcc_argv = alloc(sizeof(char *)*(argc*2+2));
	/*
	 * -2 for tcsim's argv[0]
	 * +1 for tcc's argv[0]
	 * +2 for -i dev
	 * +1 for terminating NULL
	 */
#ifdef DOLLAR
    cpp_argv[cpp_argc++] = "-$";
#else
    cpp_argv[cpp_argc++] = "-std=c99";
#endif
    tcng_topdir = getenv("TCNG_TOPDIR");
    include = alloc_sprintf("%s/lib/tcng/include",
      tcng_topdir ? tcng_topdir : TOPDIR);
    if (tcng_topdir) tcc_cmd = alloc_sprintf("%s/bin/tcng",tcng_topdir);
    cpp_argv[cpp_argc++] = alloc_sprintf("-I%s",include); /* @@@ leak */
    while ((c = getopt(argc,argv,"cdgjhk:npqs:vD:U:I:VX:")) != EOF)
	switch (c) {
	    case 'c':
		check_only = 1;
		break;
	    case 'd':
		if (printk_threshold == 7) debug = 1;
		printk_threshold = 7; /* KERN_DEBUG, printk all messages */
		break;
	    case 'g':
		use_generation = 1;
		break;
	    case 'h':
		usage(argv[0]);
	    case 'j':
		use_jiffies = 1;
		break;
	    case 'k':
		set_printk_threshold = strtoul(optarg,&end,0);
		if (*end) usage(argv[0]);
		break;
	    case 'n':
		include_default = 0;
		break;
	    case 'p':
		preserve = 1;
		break;
	    case 'q':
		if (verbose) usage(argv[0]);
		verbose = -1;
		break;
	    case 's':
		snap_len = strtoul(optarg,&end,0);
		if (*end) usage(argv[0]);
		break;
	    case 'v':
		if (verbose < 0) usage(argv[0]);
		verbose++;
		break;
	    case ':':
		usage(argv[0]);
	    case 'V':
		printf("tcsim version %s (kernel %s, iproute2 %s)\n",VERSION,
		  KFULLVERSION,IVERSION);
		return 0;
	    case 'X':
		if (*optarg == 'P')  {
		    cpp_argv[cpp_argc++] = stralloc(optarg+2);
		    break;
		}
		if (optarg[0] == 'x') {
		    if (!strchr(optarg,',')) usage(argv[0]);
		}
		else {
		    if (!optarg[0] || !strchr("cmkpt",optarg[0]) ||
		      optarg[1] != ',')
			usage(argv[0]);
		}
		add_tcc_arg(optarg[0] == 'c',optarg);
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
    if (argc > optind+1) usage(argv[0]);
    while (optind < argc) cpp_argv[cpp_argc++] = argv[optind++];
    if (set_printk_threshold != -1) printk_threshold = set_printk_threshold;
    if (kernel_init()) errorf("oops, trouble");
    if (verbose) setup_tracing();
    cpp_argv[0] = CPP; /* cpp 3.3.3 requires this */
    if (include_default) {
	cpp_argv[cpp_argc++] = "-include";
	cpp_argv[cpp_argc++] = alloc_sprintf("%s/default.tcsim",include);;
	  /* leak @@@ */
    }
    cpp_argv[cpp_argc] = NULL;
    run_cpp(cpp_argc,cpp_argv);
    (void) yyparse();
    finish_cpp();
    return 0;
}
