/*
 * $Id: as_meta.c,v 1.9 2004/09/18 02:13:03 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* these are only string types that have a type to themselves;
 * realm-specific tags are handled separately */
static const ASTagMapping tag_types[] = {
	{ "title",     TAG_TITLE,    TRUE  },
	{ "artist",    TAG_ARTIST,   TRUE  },
	{ "album",     TAG_ALBUM,    TRUE  },
	{ "unknown_1", TAG_UNKSTR,   FALSE },
	{ "year",      TAG_YEAR,     FALSE },
	{ "codec",     TAG_CODEC,    FALSE },
	{ "keywords",  TAG_KEYWORDS, TRUE  },
	{ "filename",  TAG_FILENAME, FALSE } /* yes, really FALSE :( */
};

#define NUM_TYPES (sizeof(tag_types) / sizeof(ASTagMapping))

/*****************************************************************************/

static as_bool meta_parse_result (ASMeta *meta, ASPacket *p, ASRealm realm);

/*****************************************************************************/

/* create meta data object */
ASMeta *as_meta_create ()
{
	ASMeta *meta;
	
	if (!(meta = malloc (sizeof (ASMeta))))
		return NULL;

	meta->tags = NULL;

	return meta;
}

/* create meta data object from search result packet */
ASMeta *as_meta_parse_result (ASPacket *packet, ASRealm realm)
{
	ASMeta *meta;
	
	if (!(meta = as_meta_create ()))
		return NULL;

	meta_parse_result (meta, packet, realm);

	return meta;
}

static int tag_free_itr (ASMetaTag *tag, void *data)
{
	free (tag->name);
	free (tag->value);
	free (tag);

	return TRUE;
}

/* free meta data object */
void as_meta_free (ASMeta *meta)
{
	if (!meta)
		return;

	/* free tags */
	list_foreach_remove (meta->tags, (ListForeachFunc)tag_free_itr, NULL);

	free (meta);
}

/*****************************************************************************/

List *meta_find_tag (ASMeta *meta, const char *name)
{
	List *link;

	for (link = meta->tags; link; link = link->next)
		if (gift_strcasecmp (((ASMetaTag *)link->data)->name, name) == 0)
			return link;

	return NULL;
}

/* add gift style meta tag or update if already present */
as_bool as_meta_add_tag (ASMeta *meta, const char *name, const char *value)
{
	List *link;
	ASMetaTag *tag;

	assert (name);
	assert (value);

	if ((link = meta_find_tag (meta, name)))
	{
		tag = (ASMetaTag *) link->data;
		free (tag->value);
	}
	else
	{
		if (!(tag = malloc (sizeof (ASMetaTag))))
			return FALSE;

		/* insert into list */
		meta->tags = list_prepend (meta->tags, tag);

		tag->name = gift_strdup (name);
	}	

	tag->value = gift_strdup (value);

	return TRUE;
}

/* get gift style meta tag data */
const char *as_meta_get_tag (ASMeta *meta, const char *name)
{
	List *link;

	if (!(link = meta_find_tag (meta, name)))
		return NULL;

	return ((ASMetaTag *) link->data)->value;
}

int as_meta_get_int (ASMeta *meta, const char *name)
{
	const char *value = as_meta_get_tag (meta, name);
	
	if (!value)
		return 0;

	return atoi (value);
}

/* remove gift style meta tag */
as_bool as_meta_remove_tag (ASMeta *meta, const char *name)
{
	List *link;

	if (!(link = meta_find_tag (meta, name)))
		return FALSE;

	tag_free_itr (link->data, NULL);
	
	meta->tags = list_remove_link (meta->tags, link);

	return TRUE;
}

/* call func for each tag */
int as_meta_foreach_tag (ASMeta *meta, ASMetaForeachFunc func, void *udata)
{
	List *link;
	int count = 0;
	
	for (link = meta->tags; link; link = link->next)
		if (func (link->data, udata))
			count++;
	
	return count;
}

/*****************************************************************************/

const ASTagMapping *as_meta_tag_name (const char *name)
{
	int i;
	
	for (i=0; i < NUM_TYPES; i++)
		if (!gift_strcasecmp (tag_types[i].name, name))
			return &tag_types[i];

	return NULL;
}

const ASTagMapping *as_meta_tag_type (ASTagType type)
{
	int i;
	
	for (i=0; i < NUM_TYPES; i++)
		if (tag_types[i].type == type)
			return &tag_types[i];

	return NULL;
}

/*****************************************************************************/

static void meta_add_string (ASMeta *meta, ASPacket *packet, const char *name)
{
	char *value;

	if ((value = as_packet_get_strnul (packet)))
	{
		as_meta_add_tag (meta, name, value);
		free (value);
	}
}

static as_bool meta_parse_result (ASMeta *meta, ASPacket *p, ASRealm realm)
{
	char buf[32];
	int i;

	/* Some packets seem to have a couple of stray bytes
	 * at the end; just hack around this for now */
	while (as_packet_remaining (p) > 2)
	{
		int meta_type = as_packet_get_8 (p);

		const ASTagMapping *map;
		
		/* turn everything into gift style meta tags */
		if ((map = as_meta_tag_type (meta_type)))
		{
			meta_add_string (meta, p, map->name);
			continue;
		}

		/* handle the realm-specific/non-string stuff */
		switch (meta_type)
		{
		case TAG_XXX:
			switch (realm)
			{
			case REALM_ARCHIVE:
				/* nothing */
				break;

			case REALM_AUDIO:
				/* bitrate */
				i = as_packet_get_le16 (p);
				sprintf (buf, "%u", i);
				as_meta_add_tag (meta, "bitrate", buf);
				
				/* duration */
				i = as_packet_get_le32 (p);
				sprintf (buf, "%u", i);
				as_meta_add_tag (meta, "duration", buf);
				break;
				
			case REALM_SOFTWARE:
			{
				as_uint8 c = as_packet_get_8 (p);
				if (c != 2 && c != 6)
				{
					AS_DBG_2 ("REALM_SOFTWARE: c=%d, offset %x",
					          c, p->read_ptr - p->data);
#ifdef DEBUG
					as_packet_dump (p);
#endif
				}
				
				/* version */
				free (as_packet_get_strnul (p));
				break;
			}
			case REALM_VIDEO:
				/* width/height */
				i = as_packet_get_le16 (p);
				sprintf (buf, "%u", i);
				as_meta_add_tag (meta, "width", buf);
				i = as_packet_get_le16 (p);
				sprintf (buf, "%u", i);
				as_meta_add_tag (meta, "height", buf);
				
				/* duration? */
				i = as_packet_get_le32 (p);
				sprintf (buf, "%u", i);
				as_meta_add_tag (meta, "video-duration?", buf);
				break;
				
			case REALM_IMAGE:
				/* width/height */
				i = as_packet_get_le16 (p);
				sprintf (buf, "%u", i);
				as_meta_add_tag (meta, "width", buf);
				i = as_packet_get_le16 (p);
				sprintf (buf, "%u", i);
				as_meta_add_tag (meta, "height", buf);
				
				/* unknown (depth?) */
				i = as_packet_get_le32 (p);
				sprintf (buf, "%u", i);
				as_meta_add_tag (meta, "bitdepth?", buf);
				break;

			case REALM_DOCUMENT:
				/* nothing */
				break;

			default:
				/* because size is implicitly encoded,
				 * we have no choice but to bail when
				 * an unknown tag type is encountered
				 */
				AS_DBG_2 ("Unknown realm %d, offset %x", realm,
				          p->read_ptr - p->data);
#ifdef DEBUG
				as_packet_dump (p);
#endif
				return FALSE;
			}
			break;

		default:
			/* see above */
			AS_DBG_2 ("Unknown tag type %d, offset %x", meta_type,
			          p->read_ptr - p->data);
#ifdef DEBUG
			as_packet_dump (p);
#endif
			return FALSE;
		}
	}

	return TRUE;
}

/*****************************************************************************/
