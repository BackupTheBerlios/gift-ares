/*
 * $Id: as_search.c,v 1.5 2004/09/05 02:54:44 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/* create a search request packet */
ASPacket *search_request (unsigned char *query, as_uint16 id)
{
	ASPacket *packet = as_packet_create();

	if (!packet)
		return NULL;

	/* no idea what this magic number is */
	as_packet_put_le16 (packet, 0xf64);
	as_packet_put_le16 (packet, id);

	/* 0x14 maybe means "everything" - this would imply we can
	 * restrict searches to just a single field too */
	if (!tokenize_string (packet, query, 0x14 | SEARCH_PACKET))
	{
		/* if we have no tokens we can abort the search
		 * immediately (unlike Ares, which sends the packet
		 * regardless...)
		*/
		
		as_packet_free (packet);
		return NULL;
	}

	return packet;
}

void parse_metadata (ASPacket *packet, ASResult *r)
{
	/* Some packets seem to have a couple of stray bytes
	 * at the end; just hack around this for now */
	while (as_packet_remaining (packet) > 2)
	{
		int meta_type = as_packet_get_8 (packet);
		
		switch (meta_type)
		{
		case TAG_TITLE:
		case TAG_ARTIST:
		case TAG_ALBUM:
		case TAG_UNKSTR:
		case TAG_YEAR:
		case TAG_CODEC:
		case TAG_KEYWORDS:
			free (r->meta[meta_type]);
			r->meta[meta_type] = as_packet_get_strnul (packet);

			break;
			
		case TAG_FILENAME:
			free (r->filename);
			r->filename = as_packet_get_strnul (packet);
			break;
			
		case TAG_XXX:
			switch (r->realm)
			{
			case REALM_ARCHIVE:
				/* nothing */
				break;

			case REALM_AUDIO:
				/* bitrate */
				as_packet_get_le16 (packet);
				
				/* duration */
				as_packet_get_le32 (packet);
				break;
				
			case REALM_SOFTWARE:
			{
				as_uint8 c = as_packet_get_8 (packet);
				if (c != 2)
				{
					printf ("c=%d, offset %x\n", c, packet->read_ptr-packet->data);
					as_packet_dump (packet);
				}
				
				/* version */
				free (as_packet_get_strnul (packet));
				break;
			}
			case REALM_VIDEO:
				/* width/height */
				as_packet_get_le16 (packet);
				as_packet_get_le16 (packet);
				
				/* duration? */
				as_packet_get_le32 (packet);
				break;
				
			case REALM_IMAGE:
				/* width/height */
				as_packet_get_le16 (packet);
				as_packet_get_le16 (packet);
				
				/* unknown (depth?) */
				as_packet_get_le32 (packet);
				break;

			case REALM_DOCUMENT:
				/* nothing */
				break;

			default:
				/* because size is implicitly encoded,
				 * we have no choice but to bail when
				 * an unknown tag type is encountered
				 */
				printf("unknown realm for 04 %d, offset %x\n", r->realm, packet->read_ptr-packet->data);
				as_packet_dump (packet);
				return;
			}
			break;

		default:
			/* see above */
			printf("unknown tag type %d, offset %x\n", meta_type, packet->read_ptr-packet->data);
			as_packet_dump (packet);
			return;
		}
	}
}

void as_result_free (ASResult *r)
{
	int i;

	free (r->user);
	free (r->filename);
	free (r->ext);
	free (r->hash);

	for (i = 0; i < RESULT_NUM_TAGS; i++)
		free (r->meta[i]);

	free (r);

	return;
}

ASResult *parse_search_result (ASPacket *packet)
{
	ASResult *r;
	int reply_type;
	int i;

	r = malloc (sizeof (ASResult));

	if (!r)
		return NULL;

	memset (r, 0, sizeof(*r));

	r->user = r->filename = r->ext = r->hash = NULL;

	for (i=0; i < RESULT_NUM_TAGS; i++)
		r->meta[i] = NULL;

	reply_type = as_packet_get_8 (packet);

	switch (reply_type)
	{
	case 0: /* token search result */
		r->id = as_packet_get_le16 (packet);
		
		/* supernode IP/port */
		as_packet_get_be32 (packet);
		as_packet_get_le16 (packet);

		/* user's IP/port */
		as_packet_get_be32 (packet);
		as_packet_get_le16 (packet);

		r->unknown = as_packet_get_8 (packet);
		r->user = as_packet_get_strnul (packet);

		/* unknown, may be split differently */
		as_packet_get_8 (packet);
		as_packet_get_le32 (packet);

		r->realm = as_packet_get_8 (packet);
		r->size = as_packet_get_le32 (packet);
		r->hash = as_packet_get_ustr (packet, 20);
		r->ext = as_packet_get_strnul (packet);
		
		parse_metadata (packet, r);
		break;
		
	case 1: /* hash search result */
		/* supernode IP/port */
		as_packet_get_be32 (packet);
		as_packet_get_le16 (packet);

		/* user's IP/port */
		as_packet_get_be32 (packet);
		as_packet_get_le16 (packet);

		r->unknown = as_packet_get_8 (packet);
		r->user = as_packet_get_strnul (packet);
		r->hash = as_packet_get_ustr (packet, 20);
		r->ext = as_packet_get_strnul (packet);
		break;

	default:
		AS_WARN_1 ("unknown search result type %d", reply_type);
		as_result_free (r);
		return NULL;
	}
	
	return r;
}

#if 0
int main (void)
{
	ASPacket *p = as_packet_slurp();

	parse_search_result (p);

	as_packet_free (p);

	return 0;
}
#endif
