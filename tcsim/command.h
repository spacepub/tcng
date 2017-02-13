/*
 * command.h - Command construction and execution
 *
 * Written 2001,2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2003 Werner Almesberger
 */


#ifndef COMMAND_H
#define COMMAND_H

#include "jiffies.h"


enum command_type { ct_tc,ct_send,ct_poll,ct_every,ct_echo };

struct command {
    enum command_type type;
    int ref; /* reference count */
    union {
	struct {
	   int argc;
	   char **argv; 
	} tc;
	struct {
	    struct net_device *dev;
	    void *buf;
	    int len;
	    struct attributes attr;
	} send;
	struct {
	    struct jiffval interval;
	    struct jiffval until;
	    struct command *cmd;
	} every;
	struct {
	    struct net_device *dev;
	} poll;
	struct {
	    char *msg;
	} echo;
    } u;
};

struct command *cmd_clone(struct command *cmd);
struct command *cmd_tc(int argc,char **argv);
struct command *cmd_send(struct net_device *dev,void *buf,int len,
  struct attributes attr);
struct command *cmd_poll(struct net_device *dev);
struct command *cmd_every(struct jiffval interval,struct jiffval until,
  struct command *command);
struct command *cmd_echo(char *msg);

void cmd_run(struct command *cmd);
void cmd_free(struct command *cmd);

#endif /* COMMAND_H */
