/*
 * $Id: as_search.h,v 1.2 2004/09/05 02:54:44 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SEARCH_H_
#define __AS_SEARCH_H_

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
	REALM_ARCHIVE = 0,
	REALM_AUDIO = 1,
	REALM_SOFTWARE = 3,
	REALM_VIDEO = 5,
	REALM_DOCUMENT = 6,
	REALM_IMAGE = 7
} ASRealm;

#define RESULT_NUM_TAGS  16 /* 0..15 */

typedef struct search_result {
	unsigned char *user;
	as_uint16 id;
	
	int       realm;
	size_t    size;

	/* always 0x61? (could be bandwidth) */
	as_uint8  unknown;

	as_uint8  *hash;

	unsigned char *filename;
	unsigned char *ext;
	unsigned char *meta[RESULT_NUM_TAGS];
} ASResult;

typedef void (*ASResultCallback)(ASResult *);

/* create a search request packet */
ASPacket *search_request (unsigned char *query, as_uint16 id);

/* create a search result */
ASResult *parse_search_result (ASPacket *packet);

/* free search result */
void as_result_free (ASResult *result);
#endif

