/*
 * $Id: asp_hash.c,v 1.1 2004/12/04 01:31:17 mkern Exp $
 *
 * Copyright (C) 2003 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "asp_plugin.h"

/*****************************************************************************/

/* Called by giFT to hash a file for this network. */
unsigned char *asp_giftcb_hash (const char *path, size_t *len)
{
	ASHash *hash;
	unsigned char *data;

	if (!(hash = as_hash_file (path)))
	{
		AS_ERR_1 ("Failed to hash file '%s'.", path);
		return NULL;
	}

	/* Make copy of hash data for giFT. */
	if (!(data = malloc (sizeof (AS_HASH_SIZE))))
	{
		as_hash_free (hash);
		return NULL;
	}

	memcpy (data, hash->data, AS_HASH_SIZE);
	as_hash_free (hash);

	if (len)
		*len = AS_HASH_SIZE;

	/* giFT will free this. */
	return data;
}

/* Called by giFT to encode a hash in human readable form. */
unsigned char *asp_giftcb_hash_encode (unsigned char *data)
{
	ASHash *hash;
	unsigned char *encoded;

	if (!(hash = as_hash_create (data, AS_HASH_SIZE)))
		return NULL;

	encoded = as_hash_encode (hash);
	as_hash_free (hash);

	return encoded;
}

/*****************************************************************************/
