/*
 * $Id: as_meta.h,v 1.3 2004/09/06 18:55:17 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_META_H
#define __AS_META_H

/*****************************************************************************/

typedef enum
{
	REALM_ARCHIVE  = 0,
	REALM_AUDIO    = 1,
	REALM_SOFTWARE = 3,
	REALM_VIDEO    = 5,
	REALM_DOCUMENT = 6,
	REALM_IMAGE    = 7,

	REALM_ANY      = 0xFFFF /* FIXME: find network representation */
} ASRealm;

typedef enum
{
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

typedef struct
{
	char *name;					/* human readable tag name */
	char *value;				/* human readable tag value */

} ASMetaTag;

typedef struct
{
	List *tags; /* list of ASMetaTag */

} ASMeta;

/*****************************************************************************/

/* create meta data object */
ASMeta *as_meta_create ();

/* create meta data object from search result packet */
ASMeta *as_meta_parse_result (ASPacket *packet, ASRealm realm);

/* free meta data object */
void as_meta_free (ASMeta *meta);

/*****************************************************************************/

/* add gift style meta tag or update if already present */
as_bool as_meta_add_tag (ASMeta *meta, const char *name, const char *value);

/* get gift style meta tag data */
const char *as_meta_get_tag (ASMeta *meta, const char *name);

/* remove gift style meta tag */
as_bool as_meta_remove_tag (ASMeta *meta, const char *name);

/*****************************************************************************/

#if 0
ASMediaType as_meta_mediatype_from_mime (const char *mime);
#endif

/*****************************************************************************/

#endif /* __AS_META_H */
