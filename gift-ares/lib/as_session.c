/*
 * $Id: as_session.c,v 1.5 2004/08/31 17:44:18 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static void session_connected (int fd, input_id input, ASSession *session);
static void session_get_packet (int fd, input_id input, ASSession *session);

static as_bool session_dispatch_packet (ASSession *session, ASPacketType type,
                                        ASPacket *packet);

static as_bool session_handshake (ASSession *session,  ASPacketType type,
                                  ASPacket *packet);

static as_bool session_set_state (ASSession *session, ASSessionState state,
                                  as_bool raise_callback);

/*****************************************************************************/

/* Create new session with specified callbacks. */
ASSession *as_session_create (ASSessionStateCb state_cb,
                              ASSessionPacketCb packet_cb)
{
	ASSession *session;

	if (!(session = malloc (sizeof (ASSession))))
		return NULL;

	session->c         = NULL;
	session->input     = INVALID_INPUT;
	session->cipher    = NULL;
	session->packet    = NULL;
	session->state     = SESSION_DISCONNECTED;
	session->state_cb  = state_cb;
	session->packet_cb = packet_cb;
	session->udata     = NULL;

	return session;
}

static void session_cleanup (ASSession *session)
{
	input_remove (session->input);
	tcp_close (session->c);
	as_cipher_free (session->cipher);
	as_packet_free (session->packet);

	session->input  = INVALID_INPUT;
	session->c      = NULL;
	session->cipher = NULL;
	session->packet = NULL;
}

/* Disconnect and free session. */
void as_session_free (ASSession *session)
{
	if (!session)
		return;

	session_cleanup (session);

	free (session);
}

/*****************************************************************************/

/* Returns current state of session */
ASSessionState as_session_state (ASSession *session)
{
	return session->state;
}

/* Connect to ip and port. Fails if already connected. */
as_bool as_session_connect (ASSession *session, in_addr_t host,
                            in_port_t port)
{
	assert (session);

	if (!(session->c = tcp_open (host, port, FALSE)))
		return FALSE;

	/* wait for connect result */
	session->input = input_add (session->c->fd, session, INPUT_WRITE,
	                            (InputCallback)session_connected, 
	                            AS_SESSION_CONNECT_TIMEOUT);

	if (session->input == INVALID_INPUT)
	{
		tcp_close (session->c);
		session->c = NULL;
		return FALSE;
	}

	session_set_state (session, SESSION_CONNECTING, TRUE);

	return TRUE;
}

/* Disconnect but does not free session. Triggers state callback. */
void as_session_disconnect (ASSession *session)
{
	assert (session);

	session_cleanup (session);

	session_set_state (session, SESSION_DISCONNECTED, TRUE);
}

/*****************************************************************************/

static void session_connected (int fd, input_id input, ASSession *session)
{
	ASPacket *packet;

	input_remove (input);
	session->input = INVALID_INPUT;

	if (net_sock_error (session->c->fd))
	{
		AS_HEAVY_DBG_2 ("Connect to %s:%d failed",
		                net_ip_str (session->c->host), session->c->port);
		session_set_state (session, SESSION_FAILED, TRUE);
		session_cleanup (session);
		return;
	}

	AS_DBG_2 ("CONNECTED %s:%d", net_ip_str (session->c->host),
	          session->c->port);
	
	/* set up packet buffer */
	if (!(session->packet = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory");
		session_set_state (session, SESSION_FAILED, TRUE);
		session_cleanup (session);
		return;
	}

	/* send syn packet */
	if (!(packet = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory");
		session_set_state (session, SESSION_FAILED, TRUE);
		session_cleanup (session);
		return;
	}
	
	as_packet_put_le16 (packet, 3);        /* length */
	as_packet_put_8 (packet, PACKET_SYN);  /* type */
	as_packet_put_8 (packet, 0x04);
	as_packet_put_8 (packet, 0x03);
	as_packet_put_8 (packet, 0x05);

	if (!as_packet_send (packet, session->c))
	{
		AS_ERR ("Send failed");
		session_set_state (session, SESSION_FAILED, TRUE);
		session_cleanup (session);
		return;
	}

	session_set_state (session, SESSION_HANDSHAKING, TRUE);

	/* wait for ack packet */
	session->input = input_add (session->c->fd, session, INPUT_READ, 
	                            (InputCallback)session_get_packet,
	                            AS_SESSION_HANDSHAKE_TIMEOUT);

	return;
}

static void session_get_packet (int fd, input_id input, ASSession *session)
{
	ASPacket *packet;
	ASPacketType packet_type;
	as_uint16 packet_len;

	if (net_sock_error (session->c->fd))
	{
		AS_HEAVY_DBG_2 ("Connection with %s:%d closed remotely",
		                net_ip_str (session->c->host), session->c->port);
		session_set_state (session, SESSION_DISCONNECTED, TRUE);
		session_cleanup (session);
		return;
	}

	if (!as_packet_recv (session->packet, session->c))	
	{
		AS_WARN_2 ("Recv failed from %s:%d", net_ip_str (session->c->host),
		           session->c->port);
		session_set_state (session, SESSION_DISCONNECTED, TRUE);
		session_cleanup (session);
		return;
	}

	/* dispatch all complete packets we have */
	while (as_packet_remaining (session->packet) >= 3)
	{
		packet_len = as_packet_get_le16 (session->packet);
		packet_type = as_packet_get_8 (session->packet);

		if (as_packet_remaining (session->packet) < packet_len)
		{
			/* rewind length and type and wait for more */
			as_packet_rewind (session->packet);
			return;
		}

		/* make copy of packet body */
		if (!(packet = as_packet_create_copy (session->packet, packet_len)))
		{
			AS_ERR ("Insufficient memory");
			session_set_state (session, SESSION_DISCONNECTED, TRUE);
			session_cleanup (session);
			return;
		}

		/* remove packet from buffer */
		as_packet_truncate (session->packet);

		/* dispatch packet */
		if (!session_dispatch_packet (session, packet_type, packet))
		{
			/* the connection has been closed/removed. Do nothing further */
			as_packet_free (packet);
			return;
		}

		/* free packet now that it has been handled */
		as_packet_free (packet);
	}

	return; /* wait for more */
}

/*****************************************************************************/

static as_bool session_dispatch_packet (ASSession *session, ASPacketType type,
                                        ASPacket *packet)
{
	if (as_session_state (session) == SESSION_HANDSHAKING)
	{
		if (type != PACKET_ACK)
		{
			AS_ERR_2 ("Handshake with %s:%d failed. Got something else than ACK.",
			          net_ip_str (session->c->host), session->c->port);
			session_set_state (session, SESSION_DISCONNECTED, TRUE);
			session_cleanup (session);
			return FALSE;
		}

		return session_handshake (session, type, packet);
	}
	else
	{
		AS_HEAVY_DBG_2 ("Received packet type 0x02x, length %d", (int)type,
		                as_packet_remaining (packet));

		if (type == PACKET_SHARE)
		{
			/* compressed packet */
			assert (0);
		}
		else
		{
			/* encrypted packet */
			if (!as_packet_decrypt (packet, session->cipher))
			{
				AS_ERR_3 ("Packet decrypt failed for type 0x%02X from %s:%d",
				          (int)type, net_ip_str (session->c->host),
				          session->c->port);
				session_set_state (session, SESSION_DISCONNECTED, TRUE);
				session_cleanup (session);
				return FALSE;
			}
		}

#if 1
		as_packet_dump (packet);
#endif

		/* raise callback for this packet */
		if (session->packet_cb)
			return session->packet_cb (session, type, packet);
		else
			return TRUE;
	}
}
  
/*****************************************************************************/

static as_bool session_handshake (ASSession *session,  ASPacketType type,
                                  ASPacket *packet)
{
	as_uint16 kind;
	as_uint16 seed_16;
	as_uint8 seed_8;

	assert (type == PACKET_ACK);

	if (as_packet_remaining (packet) < 0x15)
	{
		AS_ERR_2 ("Handshake with %s:%d failed. ACK packet too short.",
		          net_ip_str (session->c->host), session->c->port);
		session_set_state (session, SESSION_FAILED, TRUE);
		session_cleanup (session);
		return FALSE;
	}

	/* create cipher with port as handshake key */
	if (!(session->cipher = as_cipher_create (session->c->port)))
	{
		AS_ERR ("Insufficient memory");
		session_set_state (session, SESSION_FAILED, TRUE);
		session_cleanup (session);
		return FALSE;
	}

	/* decrypt packet */
	as_cipher_decrypt_handshake (session->cipher, packet->read_ptr,
	                             as_packet_remaining (packet));

#if 1
	as_packet_dump (packet);
#endif

	kind = as_packet_get_le16 (packet);

	if (kind > 0x15E)
	{
		/*
		 * When this happens Ares skips after the seed fields and reads
		 * <ip><port> pairs (of other supernodes presumably).
		 * 0x5E == 94, a load percentage? 
		 */
		AS_ERR_2 ("FIXME: Handshake with %s:%d failed. kind > 0x15E.",
		          net_ip_str (session->c->host), session->c->port);
		session_set_state (session, SESSION_FAILED, TRUE);
		session_cleanup (session);
		return FALSE;
	}

	/* skip unknown stuff */
	as_packet_get_le32 (packet);
	as_packet_get_le32 (packet);
	as_packet_get_le32 (packet);
	as_packet_get_le32 (packet);

	/* get cipher seeds */
	seed_16 = as_packet_get_le16 (packet);
	seed_8 = as_packet_get_8 (packet);

	as_cipher_set_seeds (session->cipher, seed_16, seed_8);

	AS_DBG_4 ("Handshake with %s:%d complete. seeds: 0x04X and 0x02X",
	          net_ip_str (session->c->host), session->c->port,
	          (int)seed_16, (int)seed_8);
	
	return TRUE;
}

/*****************************************************************************/

static as_bool session_set_state (ASSession *session, ASSessionState state,
                                  as_bool raise_callback)
{
	session->state = state;

	if (raise_callback && session->state_cb)
		return session->state_cb (session, session->state);

	return TRUE;
}

/*****************************************************************************/
