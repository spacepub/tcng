/*
 * usvc.c - User-space services (mainly netlink) replacement for tcsim
 *
 * Written 2001-2003 by Werner Almesberger
 * Copyright 2001 EPFL-ICA, Network Robots
 * Copyright 2002 Bivio Networks, Werner Almesberger
 * Copyright 2003 Werner Almesberger
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <netinet/in.h>

#include <linux/types.h>
#include <linux/rtnetlink.h>
#include <linux/netlink.h>

#include <memutil.h>

#include "libnetlink.h"
#include "tcsim.h"


/*
 * Everything here comes straight from iproute2/lib/libnetlink.c
 */


int rtnl_open(struct rtnl_handle *rth, unsigned subscriptions)
{
    memset(&rth->local,0,sizeof(rth->local));
    rth->seq = 0;
    return 1; /* stdin ;-) */
}


void rtnl_close(struct rtnl_handle *rth)
{
    /* do nothing */
}


int librtnl_sendto(int s,const void *msg,int len,unsigned int flags,
  const struct sockaddr *to,int tolen); /* extern in ulib */

int librtnl_sendto(int s,const void *msg,int len,unsigned int flags,
  const struct sockaddr *to,int tolen)
{
    (void) pseudo_netlink_to_kernel(msg,len);
    return len;
}


int librtnl_sendmsg(int s,const struct msghdr *msg,unsigned int flags);
    /* extern in ulib */

int librtnl_sendmsg(int s,const struct msghdr *msg,unsigned int flags)
{
    int i,len = 0;
    unsigned char *buf;
    int ret;

    for (i = 0; i < msg->msg_iovlen; i++) len += msg->msg_iov[i].iov_len;
    buf = alloc(len);
    len = 0;
    for (i = 0; i < msg->msg_iovlen; i++) {
	memcpy(buf+len,msg->msg_iov[i].iov_base,msg->msg_iov[i].iov_len);
	len += msg->msg_iov[i].iov_len;
    }
    ret = librtnl_sendto(s,buf,len,0,NULL,0);
    free(buf);
    return ret;
}


int librtnl_recvmsg(int s,const struct msghdr *msg,unsigned int flags);
    /* extern in ulib */

int librtnl_recvmsg(int s,const struct msghdr *msg,unsigned int flags)
{
    int ret;

    ret = pseudo_netlink_from_kernel(msg->msg_iov[0].iov_base,
      msg->msg_iov[0].iov_len);
    if (ret > 0) msg->msg_iov[0].iov_len = ret;
    return ret;
}


int ll_init_map(struct rtnl_handle *rth);
    /* extern in ulib */

int ll_init_map(struct rtnl_handle *rth)
{
    return 0;
}
