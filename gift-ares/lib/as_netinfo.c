/*
 * $Id: as_netinfo.c,v 1.9 2004/11/05 01:07:57 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static as_bool make_guid (as_uint8 *guid)
{
	ASSHA1State sha1;
	as_uint8 hash[20];
	time_t t = time (NULL);

	as_sha1_init (&sha1);

	as_sha1_update (&sha1, &t, sizeof (t));
	/* FIXME: add more stuff here */

	as_sha1_final (&sha1, hash);

	memcpy (guid, hash, 16);

	return TRUE;
}

/* allocate and init network info */
ASNetInfo *as_netinfo_create ()
{
	ASNetInfo *info;

	if (!(info = malloc (sizeof (ASNetInfo))))
		return NULL;

	info->conn_want  = 0;
	info->conn_have  = 0;
	info->users      = 0;
	info->files      = 0;
	info->size       = 0;
	info->stats_cb   = NULL;

	info->outside_ip = 0;
	info->port       = 0;

	info->nick       = NULL;
	make_guid (info->guid);

	return info;
}

/* free manager */
void as_netinfo_free (ASNetInfo *info)
{
	if (!info)
		return;

	free (info->nick);

	free (info);
}

/*****************************************************************************/

/* set callback for stats changes */
void as_netinfo_set_stats_cb (ASNetInfo *info, ASNetInfoStatsCb stats_cb)
{
	info->stats_cb = stats_cb;
}

/*****************************************************************************/

/* handle connect state change */
void as_netinfo_handle_connect (ASNetInfo *info, unsigned int conn_want,
                                unsigned int conn_have)
{
	if (info->conn_want != conn_want || info->conn_have != conn_have)
	{
		info->conn_want = conn_want;
		info->conn_have = conn_have;

		/* Raise callback */
		if (info->stats_cb)
			info->stats_cb (info);
	}
}

/* handle stats packet */
as_bool as_netinfo_handle_stats (ASNetInfo *info, ASSession *session,
                                 ASPacket *packet)
{
	unsigned int users, files, size;

	users = as_packet_get_le32 (packet);
	files = as_packet_get_le32 (packet);
	size  = as_packet_get_le32 (packet);
		
	if (users == 0 || files == 0 || size == 0)
	{
		AS_WARN_4 ("Ignoring bad looking network stats from %s: %d users, %d files, %d GB",
		           net_ip_str (session->host), users, files, size);
		return FALSE;
	}

	AS_HEAVY_DBG_4 ("Got network stats from %s: %d users, %d files, %d GB",
	                net_ip_str (session->host), users, files, size);

	info->users = users;
	info->files = files;
	info->size  = size;

	/* Raise callback */
	if (info->stats_cb)
		info->stats_cb (info);

	return TRUE;
}

/* handle outside ip packet */
as_bool as_netinfo_handle_ip (ASNetInfo *info, ASSession *session,
                              ASPacket *packet)
{
	in_addr_t ip;
	
	if ((ip = as_packet_get_ip (packet)) == 0)
		return FALSE;

	AS_DBG_1 ("Reported outside ip: %s", net_ip_str (ip));

	if (info->outside_ip != 0 && ip != info->outside_ip)
	{
		AS_WARN_1 ("Reported outside ip differs from previously reported %s",
		           net_ip_str (info->outside_ip));
	}
	
	info->outside_ip = ip;

	return TRUE;
}

/* handle nickname packet */
as_bool as_netinfo_handle_nick (ASNetInfo *info, ASSession *session,
                                ASPacket *packet)
{
	unsigned char *nick;
	
	nick = as_packet_get_strnul (packet);

	if (!nick)
		return FALSE;

	AS_DBG_1 ("Got nickname: '%s'", nick);

	if (info->nick && strcmp (nick, info->nick))
	{
		AS_WARN_1 ("Reported nick differs from previously reported nick '%s'",
		           info->nick);
	}
	
	free (info->nick);
	info->nick = nick;

	return TRUE;
}

/*****************************************************************************/

