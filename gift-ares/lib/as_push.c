/*
 * $Id: as_push.c,v 1.2 2004/09/22 04:04:12 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

typedef struct {
	TCPC     *c;
	ASSource *source;
	ASHash   *hash;
} ASPush;

static void push_connected (int fd, input_id input, ASPush *push);

/*****************************************************************************/

void as_push_free (ASPush *push)
{
	tcp_close_null (&push->c);
	free (push);
}

/* TODO: callback on success/failure */
as_bool as_push_send (ASSource *source, ASHash *hash)
{
	ASPush *push;
	TCPC *c;

	if (!AS->netinfo->port)
		return FALSE;

	c = tcp_open (source->shost, source->sport, FALSE);

	if (!c)
		return FALSE;

	push = malloc (sizeof (ASPush));
	push->source = source;
	push->hash = hash;
	push->c = c;

	input_add (c->fd, (void *)push, INPUT_WRITE,
		   (InputCallback)push_connected, AS_SESSION_CONNECT_TIMEOUT);

	return TRUE;
}

static void push_connected (int fd, input_id input, ASPush *push)
{
	ASPacket *p;

	if (net_sock_error (fd))
	{
		AS_WARN_1 ("error sending push request to %s", net_ip_str (push->source->shost));
		
		as_push_free (push);
		return;
	}

	p = as_packet_create ();

	as_packet_put_ip (p, push->source->host);
	as_packet_put_le16 (p, AS->netinfo->port);
	as_packet_put_hash (p, push->hash);
	as_packet_put_ustr (p, "12345678", 8);
	as_packet_put_8 (p, 0x61);

	as_encrypt_push (p->data, p->used, push->source->shost, push->source->sport);

	as_packet_header (p, PACKET_PUSH2);

	if (!as_packet_send (p, push->c))
	{
		/* do something */
	}
	else
		AS_DBG_1 ("sent push request to %s", net_ip_str (push->source->shost));

	as_packet_free (p);

	as_push_free (push);
}
