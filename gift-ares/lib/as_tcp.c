/*
 * $Id: as_tcp.c,v 1.2 2004/08/21 12:32:22 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_tcp.h"
#include "as_event.h"

/*****************************************************************************/

#ifdef WIN32

#define optval_t const char *

#ifndef SD_BOTH
# define SD_BOTH 2
#endif

#endif

/*****************************************************************************/

static void socket_close (int fd)
{
	if (fd < 0)
		return;

#ifndef WIN32
	shutdown (fd, SHUT_RDWR);
	close (fd);
#else 
	shutdown (fd, SD_BOTH);
	closesocket (fd);
#endif
}

int socket_set_blocking (int fd, as_bool blocking)
{
#ifndef WIN32
	int flags;

	flags = fcntl (fd, F_GETFL);

	if (blocking)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;

	fcntl (fd, F_SETFL, flags);

	return flags;
#else
	unsigned long arg = !blocking;

	ioctlsocket (fd, FIONBIO, &arg);

	return arg;
#endif
}

/*****************************************************************************/

as_bool tcp_startup ()
{
#ifndef WIN32
	return TRUE;
#else
	WSADATA wsa;

	/* init winsock lib, we need at least winsock 2.0 */
	if (WSAStartup (MAKEWORD (2, 0), &wsa) != 0)
		return FALSE;

	return TRUE;
#endif
}

as_bool tcp_cleanup ()
{
#ifndef WIN32
	return TRUE;
#else
	if (WSACleanup () != 0)
		return FALSE; /* shouldn't happen */

	return TRUE;
#endif
}

/*****************************************************************************/

static TCPC *tcp_new (in_addr_t host, in_port_t port, int fd)
{
	TCPC *c;

	if (!(c = malloc (sizeof (TCPC))))
		return NULL;

	c->host = host;
	c->port = port;
	c->fd = fd;
	c->udata = NULL;
	
	return c;
}

static void tcp_free (TCPC *c)
{
	if (!c)
		return;

	free (c);
}

/*****************************************************************************/

TCPC *tcp_open (in_addr_t host, in_port_t port, int block)
{
	int fd;
	struct sockaddr_in addr;
	TCPC *c;

	if (host == 0 || port == 0)
		return NULL;

	if ((fd = socket (AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
		return NULL;

	memset (&addr, 0, sizeof (struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = host;
	addr.sin_port = htons (port);

	socket_set_blocking (fd, block);

	if (connect (fd, (struct sockaddr *)&addr, sizeof (struct sockaddr)) < 0)
	{
		/* failing for non-blocking is ok */
#ifndef WIN32
		if (errno != EINPROGRESS)
#else
	    if (WSAGetLastError () != WSAEWOULDBLOCK)
#endif
		{
			socket_close (fd);
			return NULL;
		}
	}
		
	if (!(c = tcp_new (host, port, fd)))
		socket_close (fd);

	return c;
}

TCPC *tcp_accept (TCPC *listening, int block)
{
	struct sockaddr_in addr;
	int len = sizeof (addr);
	int fd;
	TCPC *c;

	if (!listening)
		return NULL;

	if ((fd = accept (listening->fd, (struct sockaddr *)&addr, &len)) < 0)
		return NULL;

	socket_set_blocking (fd, block);

	if (!(c = tcp_new (addr.sin_addr.s_addr, addr.sin_port, fd)))
	{
		socket_close (fd);
		return NULL;
	}

	return c;
}

TCPC *tcp_bind (in_port_t port, int block)
{
	struct sockaddr_in addr;
	int fd, reuse;
	TCPC *c;

	if (port == 0)
		return NULL;

	if ((fd = socket (AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
		return NULL;

	memset (&addr, 0, sizeof (struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl (INADDR_ANY);
	addr.sin_port = htons (port);

	reuse = 1;
	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (optval_t)&reuse, sizeof (reuse));

	socket_set_blocking (fd, block);

	if (bind (fd, (struct sockaddr *)&addr, sizeof (struct sockaddr)) < 0)
	{
		socket_close (fd);
		return NULL;
	}

	if (listen (fd, 5) < 0)
	{
		socket_close (fd);
		return NULL;
	}

	if (!(c = tcp_new (0, port, fd)))
		socket_close (fd);

	return c;
}

void tcp_close (TCPC *c)
{
	if (!c)
		return;

	input_remove_all (c->fd);

	socket_close (c->fd);
	tcp_free (c);
}

void tcp_close_null (TCPC **c)
{
	if (!c || !(*c))
		return;

	tcp_close (*c);
	*c = NULL;
}

/*****************************************************************************/

int tcp_send (TCPC *c, unsigned char *data, size_t len)
{
	if (!c || c->fd < 0)
		return -1;

	if (len == 0)
		return 0;

	return send (c->fd, data, len, 0);
}

int tcp_recv (TCPC *c, unsigned char *buf, size_t len)
{
	if (!c || c->fd < 0)
		return -1;

	if (len == 0)
		return 0;

	return recv (c->fd, buf, len, 0);
}

int tcp_peek (TCPC *c, unsigned char *buf, size_t len)
{
	if (!c || c->fd < 0)
		return -1;

	if (len == 0)
		return 0;

	return recv (c->fd, buf, len, MSG_PEEK);
}

/*****************************************************************************/

in_addr_t net_ip (const char *ip_str)
{
	if (!ip_str)
		return 0;

	return inet_addr (ip_str);
}

char *net_ip_str (in_addr_t ip)
{
	struct in_addr addr;

	memset (&addr, 0, sizeof (addr));
	addr.s_addr = ip;

	return inet_ntoa (addr);
}

in_addr_t net_peer_ip (int fd, in_port_t *portret)
{
	struct sockaddr_in addr;
	in_addr_t ip  = 0;
	in_port_t port = 0;
	int len = sizeof (addr);

	if (getpeername (fd, (struct sockaddr *)&addr, &len) == 0)
	{
		ip = addr.sin_addr.s_addr;
		port = ntohs (addr.sin_port);
	}

	if (portret)
		*portret = port;

	return ip;
}

in_addr_t net_local_ip (int fd, in_port_t *portret)
{
	struct sockaddr_in addr;
	in_addr_t ip = 0;
	in_port_t port = 0;
	int len  = sizeof (addr);

	/* similar to getpeername, but grabs our portion of the socket */
	if (getsockname (fd, (struct sockaddr *)&addr, &len) == 0)
	{
		ip   = addr.sin_addr.s_addr;
		port = ntohs (addr.sin_port);
	}

	if (portret)
		*portret = port;

	return ip;
}

/*****************************************************************************/
