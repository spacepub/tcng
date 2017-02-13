/*
 * ext_io.c - Communication infrastructure for the external interface
 *
 * Written 2001,2004 by Werner Almesberger
 * Copyright 2001 Network Robots
 * Copyright 2004 Werner Almesberger
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "config.h"
#include "util.h"
#include "location.h"
#include "error.h"
#include "ext.h"


static FILE *stream = NULL;	/* NULL if "ext" subsystem is not active */
static pid_t pid = 0;		/* 0 if there is no process */
static char *in_path,*out_path;
static int no_exit_handler = 1; /* set to 1 in child process */


/* ----- Argument handling ------------------------------------------------- */


static char **my_argv;		/* undefined if pid != 0 */

typedef struct _ext_arg {
    const char *name;
    const char *arg;
    struct _ext_arg *next;
} EXT_ARG;

static EXT_ARG *ext_args = NULL,**last_arg = &ext_args;


static void copy_argv(char **argv,const char *name)
{
    const EXT_ARG *ext_arg;
    int argc,i;

    for (argc = 0; argv[argc]; argc++);
    for (ext_arg = ext_args; ext_arg; ext_arg = ext_arg->next)
	if (!*ext_arg->name || !strcmp(ext_arg->name,name)) argc++;
    my_argv = malloc(sizeof(char *)*(argc+1));
    for (i = 0; argv[i]; i++)
	my_argv[i] = stralloc(argv[i]);
    for (ext_arg = ext_args; ext_arg; ext_arg = ext_arg->next)
	if (!*ext_arg->name || !strcmp(ext_arg->name,name))
	    my_argv[i++] = stralloc(ext_arg->arg);
    my_argv[i] = NULL;
}


static void free_argv(void)
{
    int i;

    for (i = 0; my_argv[i]; i++)
	free(my_argv[i]);
    free(my_argv);
}


void add_tcc_external_arg(const char *name,const char *arg)
{
    *last_arg = alloc_t(EXT_ARG);
    (*last_arg)->name = stralloc(name);
    (*last_arg)->arg = stralloc(arg);
    (*last_arg)->next = NULL;
    last_arg = &(*last_arg)->next;
}


/* ----- Error formatter --------------------------------------------------- */


static void expand_errors(FILE *err_file)
{
    char line[MAX_EXT_LINE+2];

    while (fgets(line,MAX_EXT_LINE+2,err_file)) {
	char *start,*pos;

	start = pos = line;
	while (1) {
	    const LOCATION *loc;
	    char *end;

	    pos = strchr(pos,'[');
	    if (!pos) break;
	    if (pos[1] != '<') {
		pos++;
		continue;
	    }
	    end = pos+2;
	    while (1) {
		end = strchr(end,'>');
		if (!end) break;
		if (end[1] == ']') break;
		end++;
	    }
	    if (!end) break;
	    /*
	     * On 64 bit, ptrdiff_t may be larger than an int. Normally, gcc
	     * doesn't complain, but in a printf argument, it does. So we need
	     * a cast.
	     */
	    if (start != pos) fprintf(stderr,"%.*s",(int) (pos-start),start);
	    *end = 0;
	    loc = location_by_spec(pos+2);
	    *end = '>';
	    if (loc) print_location(stderr,*loc);
	    else fprintf(stderr,"%.*s",(int) (end-pos)+2,pos);
	    start = pos = end+2;
	}
	fprintf(stderr,"%s",start);
    }
}

/* ----- External program -------------------------------------------------- */


static void child(char **argv,int err_fds[2])
{
    const char *path;
    int fd;

    no_exit_handler = 1;
    path = getenv("PATH");
    if (path) {
	const char *tcng_topdir;
	char *new_path;

	tcng_topdir = getenv("TCNG_TOPDIR");
	if (!tcng_topdir) tcng_topdir = TOPDIR;
	new_path = alloc(strlen(path)+strlen(tcng_topdir)+strlen(TCNG_BIN_DIR)+
	  3);
	sprintf(new_path,"%s:%s/%s",path,tcng_topdir,TCNG_BIN_DIR);
	if (setenv("PATH",new_path,1) < 0) {
	    perror("setenv");
	    exit(1);
	}
    }
    fd = open(in_path,O_RDONLY);
    if (fd < 0) {
	perror(in_path);
	exit(1);
    }
    if (dup2(fd,0) < 0) {
	perror("dup2(0)");
	exit(1);
    }
    fd = creat(out_path,0644);
    if (fd < 0) {
	perror(out_path);
	exit(1);
    }
    if (dup2(fd,1) < 0) {
	perror("dup2(1)");
	exit(1);
    }
    if (close(err_fds[0]) < 0) {
	perror("close");
	exit(1);
    }
    if (dup2(err_fds[1],2) < 0) {
	perror("dup2(2)");
	exit(1);
    }
    if (execvp(*argv,argv) < 0) {
	perror(*argv);
	exit(1);
    }
    /* not reached */
}


/* ----- Cleanup ----------------------------------------------------------- */


static void do_ext_close(int from_signal)
{
    /*
     * We have a few race conditions here, e.g. signal arriving during normal
     * exit. Can be fixed, but that's not exactly a priority item.
     */
    if (no_exit_handler) return;
    assert(stream);
    if (!from_signal) {
	if (fclose(stream) == EOF) {
	    perror("fclose");
	    _exit(1);
	}
	stream = NULL;
    }
    if (unlink(in_path) < 0) {
	perror(in_path);
	_exit(1);
    }
    if (pid) {
	if (unlink(out_path) < 0) {
	    perror(out_path);
	    _exit(1);
	}
	pid = 0;
    }
    if (!from_signal) {
	free_argv();
	free(in_path);
	free(out_path);
    }
}


static void cleanup_external(void)
{
    if (stream) do_ext_close(0);
}


static void interrupted(int sig)
{
    if (stream) do_ext_close(1);
    _exit(1);
}


/* ----- API --------------------------------------------------------------- */


FILE *ext_open(char **argv,const char *name)
{
    assert(!stream);
    in_path = alloc_sprintf("__ext_%d.in",getpid());
    out_path = alloc_sprintf("__ext_%d.out",getpid());
    stream = fopen(in_path,"w");
    if (!stream) {
	perror(in_path);
	exit(1);
    }
    if (no_exit_handler) {
	no_exit_handler = 0;
	atexit(cleanup_external);
	signal(SIGINT,interrupted);
    }
    copy_argv(argv,name);
    return stream;
}


void ext_read(const char *name,const char *action)
{
    FILE *err_file;
    int err_fds[2];
    int status;

    assert(stream);
    (void) fflush(stream);
    if (ferror(stream)) error("error writing to sub-process");
    if (!freopen(DEV_NULL,"r",stream)) { /* close old stream */
	perror("/dev/null");
	exit(1);
    }
    fflush(stdout);
    fflush(stderr);
    if (pipe(err_fds) < 0) {
	perror("pipe");
	exit(1);
    }
    err_file = fdopen(err_fds[0],"r");
    if (!err_file) {
	perror("fdopen");
	exit(1);
    }
    pid = fork();
    if (!pid) child(my_argv,err_fds);
    if (close(err_fds[1]) < 0) {
	perror("close");
	exit(1);
    }
    expand_errors(err_file);
    if (fclose(err_file) == EOF) {
	perror("fclose");
	exit(1);
    }
    if (waitpid(pid,&status,0) < 0) {
	perror("waitpid");
	exit(1);
    }
    if (!freopen(out_path,"r",stream)) {
	perror(out_path);
	exit(1);
    }
    if (WIFEXITED(status)) {
	if (!WEXITSTATUS(status)) return;
	errorf("external program \"%s\" exited with status %d %s",name,
	  WEXITSTATUS(status),action);
    }
    if (WIFSIGNALED(status))
	errorf("external program \"%s\" received signal %d %s",name,
	  WTERMSIG(status),action);
    errorf("external program \"%s\" returned unrecognized status %d %s",name,
      status,action);
}


void ext_close(void)
{
    do_ext_close(0);
}
