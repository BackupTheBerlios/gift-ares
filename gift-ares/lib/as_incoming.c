/*
 * $Id: as_incoming.c,v 1.2 2004/09/15 13:11:36 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

int as_incoming_http (ASHttpServer *server, TCPC *tcpcon,
                      ASHttpHeader *request);

int as_incoming_push (ASHttpServer *server, TCPC *tcpcon, String *buf)
{
	unsigned char *hex, *nl;
	int hlen;
	ASHash *hash;
	as_uint32 unk;

	if ((nl = strchr (buf->str, '\n')))
	    *nl = '\0';

	/* parse push reply */
	if (strncmp (buf->str, "PUSH SHA1:", 10) ||
	    !(hex = as_hex_decode (buf->str+10, &hlen)))
	{
		AS_ERR_2 ("malformed push from %s: '%s'",
			  net_ip_str(tcpcon->host), buf->str);

		return FALSE;
	}

	if (hlen < 20)
	{
		AS_ERR_2 ("truncated push from %s: '%s'",
			  net_ip_str(tcpcon->host), buf->str);
		free (hex);

		return FALSE;
	}
		
	hash = as_hash_create (hex, 20);

	free (hex);

	if (!hash)
		return FALSE;

	if (hlen >= 24)
	{
		unk = ntohl (*(long*)(hex+20));
		
		AS_DBG_2 ("%s: unknown is %08x", 
			  net_ip_str(tcpcon->host), unk);
	}

	AS_DBG_1 ("incoming push for %s", as_hash_str (hash));

	as_hash_free (hash);

	return FALSE;
}

/*****************************************************************************/
