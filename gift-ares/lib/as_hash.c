/*
 * $Id: as_hash.c,v 1.6 2004/09/10 17:58:53 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* create hash from raw data */
ASHash *as_hash_create (const as_uint8 *src, unsigned int src_len)
{
	ASHash *hash;

	if (!(hash = malloc (sizeof (ASHash))))
		return NULL;

	if (!src || src_len == 0)
	{
		memset (hash->data, 0, AS_HASH_SIZE);
	}
	else
	{
		if (src_len != AS_HASH_SIZE)
		{
			/* we currently only know about one hash (size) */
			assert (src_len == AS_HASH_SIZE);
			free (hash);
			return NULL;
		}

		memcpy (hash->data, src, src_len);
	}

	return hash;
}

/* create copy of hash */
ASHash *as_hash_copy (ASHash *hash)
{
	return as_hash_create (hash->data, AS_HASH_SIZE);
}

/* free hash */
void as_hash_free (ASHash *hash)
{
	if (!hash)
		return;

	free (hash);
}

/*****************************************************************************/

/* create hash from base64 encoded string */
ASHash *as_hash_decode (const char *encoded)
{
	ASHash *hash;
	unsigned char *bin;
	int len;

	if (!(bin = as_base64_decode (encoded, &len)))
		return NULL;

	if (len != AS_HASH_SIZE)
	{
		free (bin);
		return NULL;
	}

	hash = as_hash_create (bin, len);

	free (bin);

	return hash;
}

/* return base64 encoded string of hash. caller frees result. */
char *as_hash_encode (ASHash *hash)
{
	char *str;

	if (!(str = as_base64_encode (hash->data, AS_HASH_SIZE)))
		return NULL;

	return str;
}

/* Return static string of hash for _debugging_ purposes. Do not use for
 * anything critical because threading may corrupt buffer. */
char *as_hash_str (ASHash *hash)
{
	char *encoded;
	static char buf[AS_HASH_BASE64_SIZE + 16];
	
	/* This implementation is far from efficient */
	if (!(encoded = as_hash_encode (hash)))
		return NULL;

	gift_strncpy (buf, encoded, sizeof (buf) - 1);
	free (encoded);

	return buf;
}

/*****************************************************************************/

