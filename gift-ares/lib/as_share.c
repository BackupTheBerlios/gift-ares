/*
 * $Id: as_share.c,v 1.4 2004/09/16 02:49:21 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/***********************************************************************/

/* [insert obligatory rant here] */
#ifdef WIN32
#define DIRSEP "/\\"
#else
#define DIRSEP "/"
#endif

static char *get_filename (char *path)
{
	char *name = path;
	size_t n;

	/* gah, strrcspn() doesn't exist, so we have to cobble
	 * something else together */
	while (name[n = strcspn (name, DIRSEP)])
		name += n + 1;

	return name;
}

ASShare *share_new (char *path, ASHash *hash, ASMeta *meta,
		    size_t size, ASRealm realm)
{
	ASShare *share = malloc (sizeof (ASShare));
	char    *filename;

	if (!share)
		return NULL;
	
	filename = get_filename (path);
	share->path  = strdup (path);
	share->ext   = strrchr (filename, '.');
	share->size  = size;
	share->realm = realm;
	
	if (hash)
		share->hash = hash;
	else
		share->hash = as_hash_file (path);
	
	if (!meta)
		meta = as_meta_create ();

	as_meta_add_tag (meta, "filename", filename);

	share->meta = meta;

	return share;
}

void as_share_free (ASShare *share)
{
	if (!share)
		return;
	
	free (share->path);
	as_meta_free (share->meta);
	as_hash_free (share->hash);

	free (share);
}

/***********************************************************************/

static as_bool share_tokenize_tag (ASMetaTag *tag, ASPacket *p)
{
	const ASTagMapping *map;

	map = as_meta_tag_name (tag->name);
	
	if (!map || !map->tokenize)
		return FALSE;

	return !!as_tokenize (p, tag->value, map->type);
}

static ASPacket *share_add_tokens (ASMeta *meta)
{
	ASPacket *p = as_packet_create ();

	if (!p)
		return NULL;

	as_meta_foreach_tag (meta, (ASMetaForeachFunc)share_tokenize_tag, p);

	return p;
}

static as_bool share_add_tag (ASMetaTag *tag, ASPacket *p)
{
	const ASTagMapping *map;

	map = as_meta_tag_name (tag->name);
	
	if (!map)
		return FALSE;

	as_packet_put_8 (p, map->type);
	as_packet_put_strnul (p, tag->value);

	return TRUE;
}

ASPacket *share_packet (ASShare *share)
{
	ASPacket *p = as_packet_create (), *tokens;

	if (!p)
		return NULL;

	tokens = share_add_tokens (share->meta);

	if (!tokens)
		return NULL;
	
	as_packet_put_le16 (p, tokens->used);
	as_packet_append (p, tokens);
	as_packet_free (tokens);
	
	as_packet_put_le32 (p, 0); /* bitrate */
	as_packet_put_le32 (p, 0); /* duration */
	as_packet_put_8 (p, share->realm); /* realm */
	as_packet_put_le32 (p, share->size); /* filesize */
	as_packet_put_hash (p, share->hash);
	as_packet_put_strnul (p, share->ext);

	as_meta_foreach_tag (share->meta, (ASMetaForeachFunc)share_add_tag, p);

	as_packet_put_8 (p, 0); /* unnecessary terminator */

	return p;
}

/***********************************************************************/

#if 0
int main (int argc, char *argv[])
{
	printf("%s\n", get_filename (argv[1]));

	return 0;
}
#endif
