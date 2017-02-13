/*
 * command.c - Command construction and execution
 *
 * Written 2001-2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots, Werner Almesberger
 * Copyright 2003 Werner Almesberger
 */


#include <stdio.h>
#include <stdlib.h>

#include <memutil.h>

#include "tckernel.h"
#include "tcsim.h"
#include "timer.h"
#include "command.h"


struct every {
   struct jiffval interval;
   struct jiffval until;
   struct timer_list timer;
   struct command *cmd;
};


int terminating = 0;


static void do_every(unsigned long data)
{
    struct every *dsc = (struct every *) data;
    int stop;

    stop = jiffval_cmp(now,dsc->until) > 0;
    if (!stop) cmd_run(dsc->cmd);
    if (stop || terminating) {
	cmd_free(dsc->cmd);
	free(dsc);
	return;
    }
    dsc->timer.expires = now.jiffies+dsc->interval.jiffies;
    dsc->timer.expires_ujiffies = now.ujiffies+dsc->interval.ujiffies;
    if (dsc->timer.expires_ujiffies > 999999) {
	dsc->timer.expires++;
	dsc->timer.expires_ujiffies -= 1000000;
    }
    add_hires_timer(&dsc->timer);
}


static void add_every(struct jiffval interval,struct jiffval until,
  struct command *cmd)
{
    struct every *dsc;

    dsc = alloc_t(struct every);
    dsc->cmd = cmd;
    dsc->interval = interval;
    dsc->until = until;
    dsc->timer.function = do_every;
    dsc->timer.data = (unsigned long) dsc;
    do_every((unsigned long) dsc);
}


static struct command *cmd_alloc(enum command_type type)
{
    struct command *cmd;

    cmd = alloc_t(struct command);
    cmd->type = type;
    cmd->ref = 1;
    return cmd;
}


struct command *cmd_clone(struct command *cmd)
{
    cmd->ref++;
    return cmd;
}


struct command *cmd_tc(int argc,char **argv)
{
    struct command *cmd;
    char **in,**out;

    cmd = cmd_alloc(ct_tc);
    cmd->u.tc.argc = argc;
    cmd->u.tc.argv = out = alloc(sizeof(char *)*(argc+1));
    for (in = argv; in-argv < argc; in++) *out++ = stralloc(*in);
    *out = NULL;
    return cmd;
}


struct command *cmd_send(struct net_device *dev,void *buf,int len,
  struct attributes attr)
{
    struct command *cmd;

    cmd = cmd_alloc(ct_send);
    cmd->u.send.dev = dev;
    cmd->u.send.buf = alloc(len);
    memcpy(cmd->u.send.buf,buf,len);
    cmd->u.send.len = len;
    cmd->u.send.attr = attr;
    return cmd;
}


struct command *cmd_poll(struct net_device *dev)
{
    struct command *cmd;

    cmd = cmd_alloc(ct_poll);
    cmd->u.poll.dev = dev;
    return cmd;
}


struct command *cmd_every(struct jiffval interval,struct jiffval until,
  struct command *command)
{
    struct command *cmd;

    cmd = cmd_alloc(ct_every);
    cmd->u.every.interval = interval;
    cmd->u.every.until = until;
    cmd->u.every.cmd = command;
    return cmd;
}


struct command *cmd_echo(char *msg)
{
    struct command *cmd;

    cmd = cmd_alloc(ct_echo);
    cmd->u.echo.msg = msg;
    return cmd;
}


void cmd_run(struct command *cmd)
{
    switch (cmd->type) {
	case ct_tc:
	    if (verbose > 0) {
		int i;

		print_time(now);
		printf(" T :");
		for (i = 1; i < cmd->u.tc.argc; i++)
		    printf(" %s",cmd->u.tc.argv[i]);
		printf("\n");
	    }
	    set_timer_resolution();
	    reset_tc();
	    (void) tc_main(cmd->u.tc.argc,cmd->u.tc.argv);
	    break;
	case ct_send:
	    kernel_enqueue(cmd->u.send.dev,cmd->u.send.buf,cmd->u.send.len,
	      cmd->u.send.attr);
	    kernel_poll(cmd->u.send.dev,0);
	    break;
	case ct_poll:
	    kernel_poll(cmd->u.poll.dev,1);
	    break;
	case ct_every:
	    add_every(cmd->u.every.interval,cmd->u.every.until,
	     cmd_clone(cmd->u.every.cmd));
	    break;
	case ct_echo:
	    if (verbose >= 0) {
		print_time(now);
		if (cmd->u.echo.msg) printf(" * : %s\n",cmd->u.echo.msg);
		else printf(" * :\n");
	    }
	    break;
	default:
	    abort();
    }
}


void cmd_free(struct command *cmd)
{
    char **arg;

    if (--cmd->ref) return;
    switch (cmd->type) {
	case ct_tc:
	    for (arg = cmd->u.tc.argv; *arg; arg++) free(*arg);
	    free(cmd->u.tc.argv);
	    break;
	case ct_send:
	    free(cmd->u.send.buf);
	    break;
	case ct_poll:
	    break;
	case ct_every:
	    cmd_free(cmd->u.every.cmd);
	    break;
	case ct_echo:
	    if (cmd->u.echo.msg) free(cmd->u.echo.msg);
	    break;
	default:
	    abort();
    }
    free(cmd);
}
