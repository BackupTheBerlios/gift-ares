/*
 * $Id: as_session.c,v 1.1 2004/08/26 16:05:33 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

static void session_connected (int fd, input_id input, ASSession *sess);
static void session_handshake (int fd, input_id input, ASSession *sess);
static void session_get_packet (int fd, input_id input, ASSession *sess);


ASSession *as_session_new (in_addr_t host, in_port_t port)
{
	ASSession *sess;
	
	sess = malloc (sizeof(*sess));

	if (!sess)
		return NULL;

	sess->c = tcp_open (host, port, FALSE);

	if (!sess->c)
	{
		free (sess);
		return NULL;
	}

	sess->packet = as_packet_create ();

	sess->timer = input_add (sess->c->fd, sess, INPUT_WRITE, 
				 (InputCallback)session_connected, 30*SECONDS);

	sess->state = STATE_DISCONNECTED;

	return sess;
}


static void session_connected (int fd, input_id input, ASSession *sess)
{
	ASPacket *p;

	input_remove (input);

	if (net_sock_error (sess->c->fd))
	{
		printf ("error connecting\n");
		return;
	}
	
	if (!(p = as_packet_create ()))
		return;
	
	as_packet_put_le16(p, 3);
	as_packet_put_8(p, 0x5a);
	as_packet_put_8(p, 4);
	as_packet_put_8(p, 3);
	as_packet_put_8(p, 5);
	if (!as_packet_send (p, sess->c))
		return;

	printf ("connected\n");




	sess->timer = input_add (sess->c->fd, sess, INPUT_READ, 
				 (InputCallback)session_handshake, 30*SECONDS);
	
}

static void session_handshake (int fd, input_id input, ASSession *sess)
{
	ASPacket *p;

	input_remove (input);

	if (net_sock_error (sess->c->fd))
	{
		printf ("error handshaking\n");
		return;
	}
	
	if (!(p = as_packet_create ()))
		return;
	
	as_packet_put_le16(p, 3);
	as_packet_put_8(p, 0x5a);
	as_packet_put_8(p, 4);
	as_packet_put_8(p, 3);
	as_packet_put_8(p, 5);
	if (!as_packet_send (p, sess->c))
		return;

	printf ("sent handshake\n");
	sess->timer = input_add (sess->c->fd, sess, INPUT_WRITE, 
				 (InputCallback)session_get_packet, 30*SECONDS);
	
}

static void session_get_packet(int fd, input_id input, ASSession *sess)
{
        ASPacket *packet;

        input_remove (input);

        if(net_sock_error (sess->c->fd))
        {
//                FST_HEAVY_DBG_2 ("socket error for %s:%d",
//                                                 session->node->host, session->node->port);
//                fst_session_disconnect (session);
                return;
        }

        if (! (packet = as_packet_create ()))
        {
//                fst_session_disconnect (session);
                return;
        }

        if (!as_packet_recv (packet, sess->c))
        {
                as_packet_free (packet);
//                fst_session_disconnect (session);
                return;
        }

	as_packet_append (sess->packet, packet);
	as_packet_free (packet);

        while (as_packet_remaining (sess->packet))
        {
		int len, type;

		if (as_packet_remaining (sess->packet) < 3)
		{
			as_packet_rewind (sess->packet);
			/* get more data */
			input_add (sess->c->fd, (void*) sess,INPUT_READ,
				   (InputCallback) session_get_packet, 0);
			return;
		}

		len = as_packet_get_le16 (sess->packet);

		if (as_packet_remaining (sess->packet) < len)
		{
			as_packet_rewind (sess->packet);
			/* get more data */
			input_add (sess->c->fd, (void*) sess,INPUT_READ,
				   (InputCallback) session_get_packet, 0);
			return;
		}
		
		type = as_packet_get_8 (sess->packet);

		packet = as_packet_create_copy (sess->packet, len);

		as_packet_truncate (sess->packet);

		printf ("packet type 0x%x, len %d\n", type, len);

		as_packet_dump (packet);
	}
}
  
