/*
 * $Id: as_encoding.c,v 1.4 2004/09/13 23:28:52 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* caller frees returned string */
char *as_base64_encode (const unsigned char *data, int src_len)
{
	unsigned char *dst, *out;

	if(!data)
		return NULL;

	if((out = dst = malloc((src_len + 4) * 2)) == NULL)
		return NULL;

	for (; src_len > 2; src_len-=3, dst+=4, data+=3)
	{
		dst[0] = base64[data[0] >> 2];
		dst[1] = base64[((data[0] & 0x03) << 4) + (data[1] >> 4)];
		dst[2] = base64[((data[1] & 0x0f) << 2) + (data[2] >> 6)];
		dst[3] = base64[data[2] & 0x3f];
	}

	dst[0] = '\0';

	if (src_len == 1)
	{
		dst[0] = base64[data[0] >> 2];
		dst[1] = base64[((data[0] & 0x03) << 4)];
		dst[2] = '=';
		dst[3] = '=';
		dst[4] = '\0';
	}

	if (src_len == 2)
	{
		dst[0] = base64[data[0] >> 2];
		dst[1] = base64[((data[0] & 0x03) << 4) + (data[1] >> 4)];
		dst[2] = base64[((data[1] & 0x0f) << 2)];
		dst[3] = '=';
		dst[4] = '\0';
	}

	return out;
}

/* caller frees returned string */
unsigned char *as_base64_decode (const char *data, int *dst_len)
{
	unsigned char *dst, *out;
	char *p;
	int i;
	unsigned char in[4];

	if(!data)
		return NULL;

	if((out = dst = malloc (strlen (data))) == NULL)
		return NULL;

	for (i=0, *dst_len=0; *data; data++)
	{
		if((p = strchr(base64, *data)) == NULL)
			continue;

		in[i++] = (unsigned char)(p - base64);

		if (i == 4)
		{
			dst[0] = (in[0] << 2) | ((in[1] & 0x30) >> 4);
			dst[1] = ((in[1] & 0x0F) << 4) | ((in[2] & 0x3C) >> 2);
			dst[2] = ((in[2] & 0x03) << 6) | (in[3] & 0x3F);
			dst += 3;
			*dst_len += 3;
			i = 0;
		}
	}

	if (i >= 2)
	{
		dst[0] = (in[0] << 2) | ((in[1] & 0x30) >> 4);
		(*dst_len)++;
	}

	if (i == 3)
	{
		dst[1] = ((in[1] & 0x0F) << 4) | ((in[2] & 0x3C) >> 2);
		(*dst_len)++;
	}

	return out;
}

/*****************************************************************************/

static const char hex_string[] = "0123456789ABCDEFabcdef";

/* caller frees returned string */
char *as_hex_encode (const unsigned char *data, int src_len)
{
	char *out, *dst;
	int i;

	if (!data)
		return NULL;

	if (! (out = dst = malloc (src_len * 2 + 1)))
		return NULL;

	for(i=0; i<src_len; i++, dst += 2)
	{
		dst[0] = hex_string[data[i] >> 4];
		dst[1] = hex_string[data[i] & 0x0F];
	}

	dst[0] = 0;

	return out;
}

/* caller frees returned string */
unsigned char *as_hex_decode (const char *data, int *dst_len)
{
	char *dst, *h;
	int i, j;

	if (!data)
		return NULL;

	if (! (dst = malloc (strlen (data) / 2 + 1)))
		return NULL;

	for(i=0; *data && data[1]; i++, data += 2)
	{
		unsigned char byte = 0;

		for (j=0; j<2; j++)
		{
			if ((h = strchr (hex_string, data[0])) == NULL)
			{
				free (dst);
				return NULL;
			}

			byte <<= 4;
			byte |= (h - hex_string > 0x0F) ? (h - hex_string - 6) : h - hex_string;
		}

		dst[i] = byte;
	}

	if (dst_len)
		*dst_len = i;

	return dst;
}
