/*
 * $Id: as_netinfo.c,v 1.1 2004/09/07 18:30:02 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* allocate and init network info */
ASNetInfo *as_netinfo_create ()
{
	ASNetInfo *info;

	if (!(info = malloc (sizeof (ASNetInfo))))
		return NULL;

	info->users      = 0;
	info->files      = 0;
	info->size       = 0;
	info->outside_ip = 0;

	return info;
}

/* free manager */
void as_netinfo_free (ASNetInfo *info)
{
	if (!info)
		return;

	free (info);
}

/*****************************************************************************/

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

/*****************************************************************************/

