/*
 * $Id: as_incoming.c,v 1.4 2004/09/19 17:53:43 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

int as_incoming_http (ASHttpServer *server, TCPC *tcpcon,
                      ASHttpHeader *request)
{
	ASHash *hash;
	ASShare *share;

	if (strncmp (request->uri, "sha1:", 5) ||
	    !(hash = as_hash_decode (request->uri + 5)))
	{
		AS_DBG_2 ("Malformed uri '%s' from %s",
			  request->uri, net_ip_str (tcpcon->host));
		return FALSE;
	}

	share = as_shareman_lookup (AS->shareman, hash);

	if (!share)
	{
		AS_DBG_2 ("Unknown share request '%s' from %s",
			  as_hash_str (hash), net_ip_str (tcpcon->host));
		as_hash_free (hash);
		return FALSE;
	}

	AS_DBG_2 ("Upload request: '%s' from %s",
		  share->path, net_ip_str (tcpcon->host));

	as_hash_free (hash);

	return FALSE;
}

/*****************************************************************************/

int as_incoming_push (ASHttpServer *server, TCPC *tcpcon, String *buf)
{
	unsigned char *hex, *nl;
	int hlen;
	ASHash *hash;
	as_uint32 unk;
	ASDownload *dl;

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
		
		AS_HEAVY_DBG_2 ("%s: unknown is %08x",
		                net_ip_str(tcpcon->host), unk);
	}

	/* Find download with this hash */
	if (!(dl = as_downman_lookup_hash (AS->downman, hash)))
	{
		/* This can happen if the download is already finished */
		AS_HEAVY_DBG_2 ("Couldn't find download for pushed hash '%s' from %s",
		                as_hash_str (hash), net_ip_str(tcpcon->host));
		as_hash_free (hash);
		return FALSE;
	}

	/* Add connection to download */
	if (!as_download_take_push (dl, tcpcon))
	{
		AS_HEAVY_DBG_3 ("Couldn't add pushed hash '%s' from %s to download '%s'",
		                as_hash_str (hash), net_ip_str(tcpcon->host),
		                dl->filename);
		as_hash_free (hash);
		return FALSE;
	}

	AS_HEAVY_DBG_3 ("Added incoming push from %s for '%s' to download '%s'",
	                net_ip_str(tcpcon->host), as_hash_str (hash),
	                dl->filename);

	as_hash_free (hash);
	return TRUE; /* Don't close connection */
}

/*****************************************************************************/
