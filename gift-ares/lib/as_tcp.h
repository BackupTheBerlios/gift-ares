/*
 * $Id: as_tcp.h,v 1.1 2004/08/20 11:55:33 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_TCP_H
#define __AS_TCP_H

#include "as_ares.h"

/*****************************************************************************/

typedef struct
{
	in_addr_t         host;     /* host we are connected to */
	in_port_t         port;     /* port we are connected to */

	int               fd;       /* socket */
	void             *udata;    /* user data */
} TCPC;

/*****************************************************************************/

as_bool tcp_startup ();

as_bool tcp_cleanup ();

/*****************************************************************************/

TCPC *tcp_open (in_addr_t host, in_port_t port, int block);

TCPC *tcp_accept (TCPC *listening, int block);

TCPC *tcp_bind (in_port_t port, int block);

void tcp_close (TCPC *c);

void tcp_close_null (TCPC **c);

/*****************************************************************************/

int tcp_send (TCPC *c, unsigned char *data, size_t len);

int tcp_recv (TCPC *c, unsigned char *buf, size_t len);

int tcp_peek (TCPC *c, unsigned char *buf, size_t len);

/*****************************************************************************/

in_addr_t net_ip (const char *ip_str);

char *net_ip_str (in_addr_t ip);

in_addr_t net_peer_ip (int fd, in_port_t *portret);

in_addr_t net_local_ip (int fd, in_port_t *portret);

/*****************************************************************************/

#endif /* __AS_TCP_H */
