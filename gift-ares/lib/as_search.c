/*
 * $Id: as_search.c,v 1.3 2004/09/02 21:00:26 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

typedef enum {
	TAG_TITLE  = 1,
	TAG_ARTIST = 2,
	TAG_ALBUM  = 3,
	TAG_XXX    = 4, /* depends on realm */
	TAG_UNKSTR = 5, /* some unknown string, maybe "comment" */
	TAG_YEAR   = 6,
	TAG_CODEC  = 7,
	TAG_KEYWORDS = 15, /* verify */
	TAG_FILENAME = 16
} ASTagType;

typedef enum {
	REALM_AUDIO = 1,
	REALM_VIDEO = 5,
	REALM_DOCUMENT = 6,
	REALM_IMAGE = 7
} ASRealm;

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

struct search_result {
	unsigned char *user;
	as_uint16 id;
	
	int       realm;
	size_t    size;

	/* always 0x61? (could be bandwidth) */
	as_uint8  unknown;

	as_uint8  *hash;

	unsigned char *filename;
	unsigned char *ext;
};

void parse_metadata (ASPacket *packet, struct search_result *r)
{
	while (as_packet_remaining (packet))
	{
		int meta_type = as_packet_get_8 (packet);
		unsigned char *meta;
		
		/* FIXME: do something with these */
		switch (meta_type)
		{
		case TAG_TITLE:
		case TAG_ARTIST:
		case TAG_ALBUM:
		case TAG_UNKSTR:
		case TAG_YEAR:
		case TAG_CODEC:
		case TAG_KEYWORDS:
			meta = as_packet_get_strnul (packet);

			free (meta);
			break;
			
		case TAG_FILENAME:
			free (r->filename);
			r->filename = as_packet_get_strnul (packet);
			break;
			
		case TAG_XXX:
			switch (r->realm)
			{
			case REALM_AUDIO:
				/* bitrate */
				as_packet_get_le16 (packet);
				
				/* duration */
				as_packet_get_le32 (packet);
				break;
				
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


void parse_search_result (ASPacket *packet)
{
	struct search_result r;
	int reply_type = as_packet_get_8 (packet);
	memset (&r, 0, sizeof(r));

	switch (reply_type)
	{
	case 0: /* token search result */
		r.id = as_packet_get_le16 (packet);
		
		/* supernode IP/port */
		as_packet_get_be32 (packet);
		as_packet_get_le16 (packet);

		/* user's IP/port */
		as_packet_get_be32 (packet);
		as_packet_get_le16 (packet);

		r.unknown = as_packet_get_8 (packet);
		r.user = as_packet_get_strnul (packet);

		/* unknown, may be split differently */
		as_packet_get_8 (packet);
		as_packet_get_le32 (packet);

		r.realm = as_packet_get_8 (packet);
		r.size = as_packet_get_le32 (packet);
		r.hash = as_packet_get_ustr (packet, 20);
		r.ext = as_packet_get_strnul (packet);
		
		parse_metadata (packet, &r);
		break;
		
	case 1: /* hash search result */
		/* supernode IP/port */
		as_packet_get_be32 (packet);
		as_packet_get_le16 (packet);

		/* user's IP/port */
		as_packet_get_be32 (packet);
		as_packet_get_le16 (packet);

		r.unknown = as_packet_get_8 (packet);
		r.user = as_packet_get_strnul (packet);
		r.hash = as_packet_get_ustr (packet, 20);
		r.ext = as_packet_get_strnul (packet);
		break;

	default:
		return;
	}
	
	printf ("%20s %10d %d [%s]\n", r.user, r.size, r.realm, r.filename);

	free (r.user);
	free (r.filename);
	free (r.ext);
	free (r.hash);

	return;
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
