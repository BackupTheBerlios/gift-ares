/*
 * $Id: as_session_man.c,v 1.7 2004/09/01 15:51:36 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static as_bool sessman_maintain (ASSessMan *man);

static as_bool session_state_cb (ASSession *session, ASSessionState state);

static as_bool session_packet_cb (ASSession *session, ASPacketType type,
                                  ASPacket *packet);

/*****************************************************************************/

/* allocate and init session manager */
ASSessMan *as_sessman_create ()
{
	ASSessMan *man;

	if (!(man = malloc (sizeof (ASSessMan))))
		return NULL;

	man->connections = 0;
	man->connecting = NULL;
	man->connected = NULL;

	return man;
}

/* free manager */
void as_sessman_free (ASSessMan *man)
{
	if (!man)
		return;

	/* disconnect everything */
	as_sessman_connect (man, 0);

	free (man);
}

/*****************************************************************************/

/* Returns number of actually established sessions */
unsigned int as_sessman_established (ASSessMan *man)
{
	return list_length (man->connected);
}

/*****************************************************************************/

/* Set number of sessions that should be maintained at all times. Setting this
 * to zero will disconnect from the network. Anything non-zero will start
 * connecting.
 */
void as_sessman_connect (ASSessMan *man, unsigned int connections)
{
	man->connections = connections;

	AS_DBG_3 ("Requested: %d, connected: %d, connecting: %d",
	          man->connections, list_length (man->connected), 
	          list_length (man->connecting));

	sessman_maintain (man);
}

/*****************************************************************************/

static int sessman_disconnect_itr (ASSession *session, ASSessMan *man)
{
	as_session_disconnect (session, FALSE);
	as_session_free (session);

	return TRUE; /* remove link */
}

/* take necessary steps to maintain man->connections sessions */
static as_bool sessman_maintain (ASSessMan *man)
{
	unsigned int connected = list_length (man->connected);
	unsigned int connecting = list_length (man->connecting);
	int len;
		
	if (man->connections == 0)
	{
		/* disconnect everything */
		man->connecting = list_foreach_remove (man->connecting,
		                     (ListForeachFunc)sessman_disconnect_itr, man);
		man->connected = list_foreach_remove (man->connected,
		                     (ListForeachFunc)sessman_disconnect_itr, man);
	}
	else if (man->connections < connected)
	{
		/* We have more connections than needed. First stop all discovery. */
		man->connecting = list_foreach_remove (man->connecting,
		                     (ListForeachFunc)sessman_disconnect_itr, man);

		/* Now remove superfluous connections.
		 * TODO: Be smart about which connections we remove.
		 */
		len = connected - man->connections;
		
		while (len > 0)
		{
			as_session_disconnect (man->connected->data, FALSE);
			as_session_free (man->connected->data);

			man->connected = list_remove_link (man->connected, man->connected);
			len--;
		}
	}
	else if (man->connections > connected)
	{
		/* We need more connections. Fill up discovery queue. */
		len = AS_SESSION_PARALLEL_ATTEMPTS - connecting;

		while (len > 0)
		{
			ASSession *session;
			ASNode *node;

			/* Get next node */
			if (!(node = as_nodeman_next (AS->nodeman)))
			{
				/* FIXME: Use Ares http cache by adding download code to
				 * node manager and calling it from here.
				 */
				AS_ERR ("Ran out of nodes");
				return FALSE;	
			}

			/* Create session */
			if (!(session = as_session_create (session_state_cb,
			                                   session_packet_cb)))
			{
				AS_ERR ("Insufficient memory");
				as_nodeman_update_failed (AS->nodeman, node->host);
				return FALSE; /* hmm */
			}

			session->udata = man;

#if 1
			AS_HEAVY_DBG_3 ("Trying node %s:%d, weight: %.02f",
			                net_ip_str (node->host), node->port, node->weight);
#endif

			/* Connect to node */
			if (!(as_session_connect (session, node->host, node->port)))
			{
				as_nodeman_update_failed (AS->nodeman, node->host);
				as_session_free (session);
				continue; /* try next node */
			}

			/* Add session to connecting list */
			man->connecting = list_prepend (man->connecting, session);
			len--;
		}
	}

	AS_HEAVY_DBG_3 ("session_maintain: requested: %d, connected: %d, connecting: %d",
	                man->connections, list_length (man->connected), 
	                list_length (man->connecting));

	return TRUE;
}

/*****************************************************************************/

static as_bool session_state_cb (ASSession *session, ASSessionState state)
{
	ASSessMan *man = (ASSessMan*) session->udata;

	switch (state)
	{
	case SESSION_DISCONNECTED:
		AS_DBG_2 ("DISCONNECTED %s:%d",
		          net_ip_str (session->host), session->port);

		/* notify node manager */
		as_nodeman_update_disconnected (AS->nodeman, session->host);

		/* remove from list and free session */
		man->connected = list_remove (man->connected, session);
		as_session_free (session);

		/* keep things running */
		sessman_maintain (man);

		return FALSE;

	case SESSION_FAILED:
		AS_HEAVY_DBG_2 ("FAILED %s:%d",
		                net_ip_str (session->host), session->port);

		/* notify node manager */
		as_nodeman_update_failed (AS->nodeman, session->host);

		/* remove from list and free session */
		man->connecting = list_remove (man->connecting, session);
		as_session_free (session);

		/* keep things running */
		sessman_maintain (man);

		return FALSE;

	case SESSION_CONNECTING:
		AS_HEAVY_DBG_2 ("CONNECTING %s:%d",
		                net_ip_str (session->host), session->port);
		break;

	case SESSION_HANDSHAKING:
		AS_HEAVY_DBG_2 ("HANDSHAKING %s:%d",
		                net_ip_str (session->host), session->port);
		break;

	case SESSION_CONNECTED:
		AS_DBG_2 ("CONNECTED %s:%d",
		          net_ip_str (session->host), session->port);
		
		/* notify node manager */
		as_nodeman_update_connected (AS->nodeman, session->host);

		/* remove from connecting list... */
		man->connecting = list_remove (man->connecting, session);
		/* and add to connected list... */
		man->connected = list_prepend (man->connected, session);

		AS_DBG_3 ("Session status: requested %d, connected: %d, connecting: %d",
	              man->connections, list_length (man->connected), 
	              list_length (man->connecting));

		send_nodeinfo (session);

		return FALSE;
	}

	return TRUE;
}

#define CLIENT_NAME "aREs"

static as_bool send_nodeinfo (ASSession *session)
{
	ASPacket *packet;

	if (!(packet = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory");
		return FALSE;
	}
	
	as_packet_put_8 (packet, 0x00);
	as_packet_put_ustr (packet, "\x7c\xa7\x86\x36\x18\x54\xb7\xaa\xcc\xfd\xf4"
			    "\xbe\x0f\x20\x6a\x5a\x6d\xe8\xd3\x08\x20\x92", 22); /* no idea */
	as_packet_put_ustr (packet, "\x00\x00\x00\x04\x00\x00\xd6\x83\x00", 9); /* still no idea */
	as_packet_put_ustr (packet, "0123456789abcdef", 16); /* GUID */
	as_packet_put_le16 (packet, 0x00);
	as_packet_put_ustr (packet, CLIENT_NAME, sizeof (CLIENT_NAME));
	as_packet_put_be32 (packet, 0xc0a80080); /* our local IP */

	as_packet_encrypt (packet, session->cipher);
	as_packet_header (packet, PACKET_NODEINFO);
	as_packet_send (packet, session->c);

	return TRUE;
}

static as_bool session_packet_cb (ASSession *session, ASPacketType type,
                                  ASPacket *packet)
{
	switch (type)
	{
	case PACKET_LOCALIP:
	{
		in_addr_t ip;
		ip = as_packet_get_be32 (packet);
		AS_DBG_1 ("got local IP: %s", net_ip_str (ip)); 
		break;
	}

	case PACKET_STATS:
	{
		unsigned int users, files, size;
		users = as_packet_get_le32 (packet);
		files = as_packet_get_le32 (packet);
		size = as_packet_get_le32 (packet);
		
		printf ("got network stats: %d users, %d files, %d Gb\n",
			users, files, size);
		break;
	}
	default:
		AS_WARN_1 ("got unknown packet 0x%x:", type);
		as_packet_dump (packet);
	}

	return TRUE;
}

/*****************************************************************************/
