/*
 * $Id: as_search_result.c,v 1.4 2004/09/07 15:57:57 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static as_bool result_parse (ASResult *r, ASPacket *packet);

/*****************************************************************************/

/* create empty search result */
ASResult *as_result_create ()
{
	ASResult *result;

	if (!(result = malloc (sizeof (ASResult))))
		return NULL;

	result->search_id = INVALID_SEARCH_ID;
	
	if (!(result->source = as_source_create ()))
	{
		free (result);
		return NULL;
	}

	result->meta     = NULL;
	result->realm    = REALM_ANY;
	result->hash     = NULL;
	result->filesize = 0;
	result->filename = NULL;
	result->fileext  = NULL;
	result->unknown  = 0;

	return result;
}


/* create search result from packet */
ASResult *as_result_parse (ASPacket *packet)
{
	ASResult *result;

	if (!(result = as_result_create ()))
		return NULL;

	if (!result_parse (result, packet))
	{
		as_result_free (result);
		return NULL;
	}

	return result;
}

/* free search result */
void as_result_free (ASResult *result)
{
	if (!result)
		return;

	as_source_free (result->source);
	as_meta_free (result->meta);
	as_hash_free (result->hash);

	free (result->filename);
	free (result->fileext);

	free (result);
}

/*****************************************************************************/

static as_bool result_parse (ASResult *r, ASPacket *packet)
{
	int reply_type;

	/* get data from packet */
	reply_type = as_packet_get_8 (packet);

	switch (reply_type)
	{
	case 0: /* token search result */
		r->search_id = as_packet_get_le16 (packet);
		
		/* supernode IP/port */
		r->source->shost = as_packet_get_ip (packet);
		r->source->sport = as_packet_get_le16 (packet);

		/* user's IP/port */
		r->source->host = as_packet_get_ip (packet);
		r->source->port = as_packet_get_le16 (packet);

		/* bandwidth? */
		r->unknown = as_packet_get_8 (packet);

		/* username */
		r->source->username = as_packet_get_strnul (packet);

		/* unknown, may be split differently */
		as_packet_get_8 (packet);
		as_packet_get_le32 (packet);

		r->realm = (ASRealm) as_packet_get_8 (packet);
		r->filesize = as_packet_get_le32 (packet);
		r->hash = as_packet_get_hash (packet);
		r->fileext = as_packet_get_strnul (packet);

		/* parse meta data */
		r->meta = as_meta_parse_result (packet, r->realm);

		/* get filename from meta data as special case */
		if ((r->filename = as_meta_get_tag (r->meta, "filename")))
		{
			r->filename = strdup (r->filename);
			as_meta_remove_tag (r->meta, "filename");
		}

		break;
		
	case 1: /* hash search result */
		/* supernode IP/port */
		r->source->shost = as_packet_get_ip (packet);
		r->source->sport = as_packet_get_le16 (packet);

		/* user's IP/port */
		r->source->host = as_packet_get_ip (packet);
		r->source->port = as_packet_get_le16 (packet);

		/* bandwidth? */
		r->unknown = as_packet_get_8 (packet);

		r->source->username = as_packet_get_strnul (packet);
		r->hash = as_packet_get_hash (packet);
		r->fileext = as_packet_get_strnul (packet);
		break;

	default:
		AS_WARN_1 ("Unknown search result type %d", reply_type);
		return FALSE;
	}

	/* no hash is bad */
	if (!r->hash)
		return FALSE;
	
	return TRUE;
}

/*****************************************************************************/

#if 0
int main (void)
{
	ASPacket *p = as_packet_slurp();
	ASResult *r = as_result_parse (p);

	as_packet_free (p);
	as_result_free (r);

	return 0;
}
#endif
