/*
 * $Id: as_share.c,v 1.10 2004/09/18 02:13:03 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/***********************************************************************/

ASShare *as_share_new (char *path, ASHash *hash, ASMeta *meta,
                       size_t size, ASRealm realm)
{
	ASShare *share = malloc (sizeof (ASShare));
	char    *filename;

	if (!share)
		return NULL;
	
	share->path  = strdup (path);
	filename = as_get_filename (share->path);
	share->ext   = strrchr (filename, '.');
	share->size  = size;
	share->realm = realm;
	
	if (hash)
		share->hash = hash;
	else
		share->hash = as_hash_file (path);

	if (!hash)
	{
		free (share->path);
		free (share);

		return NULL;
	}

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

ASPacket *as_share_packet (ASShare *share)
{
	ASPacket *p = as_packet_create (), *tokens;

	if (!p)
		return NULL;

	tokens = share_add_tokens (share->meta);

	if (!tokens)
		return NULL;
	
	if (!tokens->used)
	{
		as_packet_free (p);
		as_packet_free (tokens);
		return NULL;
	}

	as_packet_put_le16 (p, tokens->used);
	as_packet_append (p, tokens);
	as_packet_free (tokens);
	
	as_packet_put_le32 (p, as_meta_get_int (share->meta, "bitrate"));
	as_packet_put_le32 (p, as_meta_get_int (share->meta, "frequency"));
	as_packet_put_le32 (p, as_meta_get_int (share->meta, "duration"));
	as_packet_put_8 (p, share->realm); /* realm */
	as_packet_put_le32 (p, share->size); /* filesize */
	as_packet_put_hash (p, share->hash);
	as_packet_put_strnul (p, share->ext);

	as_meta_foreach_tag (share->meta, (ASMetaForeachFunc)share_add_tag, p);

	return p;
}

/***********************************************************************/

#if 0
int main (int argc, char *argv[])
{
	printf("%s\n", as_get_filename (argv[1]));

	return 0;
}
#endif
