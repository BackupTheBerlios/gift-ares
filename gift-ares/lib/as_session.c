/*
 * $Id: as_session.c,v 1.13 2004/09/02 16:31:48 mkern Exp $
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

static as_bool session_error (ASSession *session);

/*****************************************************************************/

/* Create new session with specified callbacks. */
ASSession *as_session_create (ASSessionStateCb state_cb,
                              ASSessionPacketCb packet_cb)
{
	ASSession *session;

	if (!(session = malloc (sizeof (ASSession))))
		return NULL;

	session->host      = INADDR_NONE;
	session->port      = 0;
	session->c         = NULL;
	session->input     = INVALID_INPUT;
	session->cipher    = NULL;
	session->packet    = NULL;
	session->state     = SESSION_DISCONNECTED;
	session->state_cb  = state_cb;
	session->packet_cb = packet_cb;
	session->udata     = NULL;
	session->search_id = 0;

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
	assert (session->c == NULL);

	session->host = host;
	session->port = port;

	if (!(session->c = tcp_open (session->host, session->port, FALSE)))
	{
		AS_ERR_2 ("tcp_open failed for %s:%d", net_ip_str (host), port);
		return FALSE;
	}

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

/* Disconnect but does not free session. Triggers state callback if
 * specified.
 */
void as_session_disconnect (ASSession *session, as_bool raise_callback)
{
	assert (session);

	session_cleanup (session);

	session_set_state (session, SESSION_DISCONNECTED, raise_callback);
}

/*****************************************************************************/

/* Send packet to supernode. flag specifies if the packet should be encrypted
 * or compressed. The body packet will be modified.
 */
as_bool as_session_send (ASSession *session, ASPacketType type,
                         ASPacket *body, ASPacketFlag flag)
{
	/* encrypt or compress packet body */
	switch (flag)
	{
	case PACKET_PLAIN:
		break;

	case PACKET_ENCRYPTED:
		if (!as_packet_encrypt (body, session->cipher))
		{
			AS_ERR ("Encrypt failed");
			return FALSE;
		}
		break;

	case PACKET_COMPRESSED:
		/* TODO */
		AS_ERR ("Packet compression not implemented");
		assert (0);
		return FALSE;
		break;
	}

	/* added packet header  */
	if (!as_packet_header (body, type))
	{
		AS_ERR ("Insufficient memory");
		return FALSE;
	}

	/* send it off */
	if (!as_packet_send (body, session->c))
	{
		AS_ERR ("Send failed");
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static void session_connected (int fd, input_id input, ASSession *session)
{
	ASPacket *packet;

	input_remove (input);
	session->input = INVALID_INPUT;

	if (net_sock_error (fd))
	{
		AS_HEAVY_DBG_2 ("Connect to %s:%d failed",
		                net_ip_str (session->host), session->port);
		session_error (session);
		return;
	}

	AS_DBG_2 ("Connected to %s:%d", net_ip_str (session->host),
	          session->port);
	
	/* set up packet buffer */
	if (!(session->packet = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory");
		session_error (session);
		return;
	}

	/* send syn packet */
	if (!(packet = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory");
		session_error (session);
		return;
	}

	/* packet body */
	as_packet_put_8 (packet, 0x04);
	as_packet_put_8 (packet, 0x03);
	as_packet_put_8 (packet, 0x05);

	if (!as_session_send (session, PACKET_SYN, packet, PACKET_PLAIN))
	{
		AS_ERR ("Send failed");
		as_packet_free (packet);
		session_error (session);
		return;
	}

	as_packet_free (packet);

	if (!session_set_state (session, SESSION_HANDSHAKING, TRUE))
		return; /* session was freed */

	/* wait for supernode handshake packet */
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

	if (net_sock_error (fd))
	{
		AS_HEAVY_DBG_2 ("Connection with %s:%d closed remotely",
		                net_ip_str (session->host), session->port);
		session_error (session);
		return;
	}

	if (!as_packet_recv (session->packet, session->c))	
	{
		AS_WARN_2 ("Recv failed from %s:%d", net_ip_str (session->host),
		           session->port);
		session_error (session);
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
			session_error (session);
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
			          net_ip_str (session->host), session->port);
			session_error (session);
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
				          (int)type, net_ip_str (session->host),
				          session->port);
				session_error (session);
				return FALSE;
			}
		}

		/* raise callback for this packet */
		if (session->packet_cb)
			return session->packet_cb (session, type, packet);
		else
			return TRUE;
	}
}
  
/*****************************************************************************/

static as_bool session_send_handshake (ASSession *session,
                                       as_uint8 supernode_guid[16])
{
	ASPacket *packet;
	as_uint8 *nonce;

	/* Create our part of the handshake */
	if (!(packet = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory");
		return FALSE;
	}

	/* hardcoded zero byte */
	as_packet_put_8 (packet, 0x00);

	/* 22 byte nonce created from supernode guid */
	if (!(nonce = as_cipher_nonce (session->cipher, supernode_guid)))
	{
		AS_ERR ("Handshake nonce creation failed");
		return FALSE;
	}
	as_packet_put_ustr (packet, nonce, 22);
	free (nonce);

	/* unknown, 2 bytes, (always?) zero */
	as_packet_put_le16 (packet, 0x0000);
	/* unknown, 1 byte, (always?) zero */
	as_packet_put_8 (packet, 0x00);
	/* unknown, 1 byte, (always?) 0x04 */
	as_packet_put_8 (packet, 0x04);
	/* unknown, 1 byte, (always?) zero */
	as_packet_put_8 (packet, 0x00);
	/* unknown, 1 byte, (always?) zero */
	as_packet_put_8 (packet, 0x00);

	/* our listening port. FIXME: faked for now */
	as_packet_put_le16 (packet, 0xDEAD);

	/* unknown string, (always?) zero string */
	as_packet_put_ustr (packet, "", 1);

	/* client GUID, 16 bytes */
	as_packet_put_ustr (packet, "0123456789abcdef", 16);

	/* hardcoded zero byte */
	as_packet_put_8 (packet, 0x00);
	/* hardcoded zero byte */
	as_packet_put_8 (packet, 0x00);

	/* client name, zero terminated */
	as_packet_put_ustr (packet, AS_CLIENT_NAME, sizeof (AS_CLIENT_NAME));
	
	/* local ip */
	as_packet_put_ip (packet, net_local_ip (session->c->fd, NULL));

	if (!as_session_send (session, PACKET_HANDSHAKE, packet,
	                      PACKET_ENCRYPTED))
	{
		AS_ERR ("Send failed");
		as_packet_free (packet);
		return FALSE;
	}

	as_packet_free (packet);

	return TRUE;
}

static as_bool session_handshake (ASSession *session,  ASPacketType type,
                                  ASPacket *packet)
{
	as_uint16 children;
	as_uint16 seed_16;
	as_uint8 seed_8;
	as_uint8 *supernode_guid;

	assert (type == PACKET_ACK);

	if (as_packet_remaining (packet) < 0x15)
	{
		AS_ERR_2 ("Handshake with %s:%d failed. ACK packet too short.",
		          net_ip_str (session->host), session->port);
		session_error (session);
		return FALSE;
	}

	/* create cipher with port as handshake key */
	if (!(session->cipher = as_cipher_create (session->c->port)))
	{
		AS_ERR ("Insufficient memory");
		session_error (session);
		return FALSE;
	}

	/* decrypt packet */
	as_cipher_decrypt_handshake (session->cipher, packet->read_ptr,
	                             as_packet_remaining (packet));

#if 0
	as_packet_dump (packet);
#endif

	/* we think this is the child count of the supernode */
	children = as_packet_get_le16 (packet);

	/* Get supernode GUID used in our reply below. */
	supernode_guid = as_packet_get_ustr (packet, 16);

	/* get cipher seeds */
	seed_16 = as_packet_get_le16 (packet);
	seed_8 = as_packet_get_8 (packet);

	/* Add supplied nodes to our cache. */
	while (as_packet_remaining (packet) >= 6)
	{
		in_addr_t host = as_packet_get_ip (packet);
		in_port_t port = (in_port_t) as_packet_get_le16 (packet);

		/* FIXME: The session manager should really do this. Accessing the
		 * node manager from here is ugly.
		 */
		as_nodeman_update_reported (AS->nodeman, host, port);
	}

	if (children > 350)
	{
		/* Ares disconnects if there are more than 350 children. Do the
		 * same.
		 */
		AS_DBG_3 ("Handshake with %s:%d aborted. Supernode has %d (>350) children.",
		          net_ip_str (session->host), session->port, (int)children);
		session_error (session);
		free (supernode_guid);
		return FALSE;
	}

	/* Set up cipher. */
	as_cipher_set_seeds (session->cipher, seed_16, seed_8);

	/* Send our part of the handshake. */
	if (!session_send_handshake (session, supernode_guid))
	{
		AS_ERR_2 ("Handshake send failed to %s:%d",
		          net_ip_str (session->host), session->port);
		session_error (session);
		free (supernode_guid);
		return FALSE;
	}

	free (supernode_guid);

	/* Handshake is complete now. */
	AS_DBG_4 ("Handshake with %s:%d complete. seeds: 0x%04X and 0x%02X",
		      net_ip_str (session->host), session->port,
			  (int)seed_16, (int)seed_8);

	if (!session_set_state (session, SESSION_CONNECTED, TRUE))
		return FALSE; /* session was freed */

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

static as_bool session_error (ASSession *session)
{
	as_bool ret;

	session_cleanup (session);

	if (session->state == SESSION_HANDSHAKING ||
	    session->state == SESSION_CONNECTING)
		ret = session_set_state (session, SESSION_FAILED, TRUE);
	else
		ret = session_set_state (session, SESSION_DISCONNECTED, TRUE);

	return ret;
}

/*****************************************************************************/
