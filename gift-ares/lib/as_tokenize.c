/*
 * $Id: as_tokenize.c,v 1.3 2004/08/26 15:57:44 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "as_ares.h"
#include "as_tokenize.h"

#define DELIM " -.,!\"0123456789:()[]?\r\n\t"

#define MAX_TOKENS 128

#define SEARCH_PACKET 0x100

struct token_list {
	int len;
	int type; /* which tag type this came from */
	as_uint16 list[MAX_TOKENS];
};

static as_uint16 hash_token (unsigned char *str)
{
	as_uint32 acc = 0;
	unsigned char c;
	int b = 0;

	/* this is a very poor hash function :( */
	while ((c = *str++)) {
		acc ^= c << (b*8);
		b = (b + 1) & 3;
	}
	
	return (acc * 0x4f1bbcdc) >> 16;
}

static int add_token (ASPacket *packet, unsigned char *str, int len,
		struct token_list *tl)
{
	unsigned char buf[32], *ptr = buf;
	as_uint16 token;
	int i;

	/* we don't want ludicrously-sized tokens anyway */
	if (len > 30)
		return FALSE;

	for (i=len; i--;)
	{
		*ptr++ = tolower (*str++);
	}
	*ptr++ = '\0';

	token = hash_token (buf);

	/* check for dups */
	for (i=0; i<tl->len; i++)
		if (tl->list[i] == token)
			break;

	if (i == tl->len && i < MAX_TOKENS)
	{
		/* not found: add it */
#if 0
		printf ("token '%s' %02x\n", buf, token);
#endif
		tl->list[tl->len++] = token;

		/* yes, there are two different packet formats, one
		 * for shares and one for searches... */
		if (tl->type & SEARCH_PACKET) {
			as_packet_put_8 (packet, (as_uint8) tl->type);
			as_packet_put_8 (packet, (as_uint8) len);
			as_packet_put_le16 (packet, token);
		} else {
			as_packet_put_8 (packet, (as_uint8) tl->type);
			as_packet_put_le16 (packet, token);
			as_packet_put_8 (packet, (as_uint8) len);
		}			

		as_packet_put_ustr (packet, buf, len);

		return TRUE;
	}

	return FALSE;
}

/* returns number of tokens added */
int tokenize_string (ASPacket *packet, unsigned char *str, int type)
{
	unsigned char *ptr=str;
	size_t len;
	int count = 0;

	struct token_list tl;
	tl.len = 0;
	tl.type = type;

	while (*ptr) {
		len = strcspn (ptr, DELIM);

		/* single characters are not tokenized */
		if (len > 1)
			count += add_token (packet, ptr, len, &tl);

		ptr += len;
		if (!*ptr++)
			break;
	}

	return count;
}

#if 0
int main(int argc, char *argv[])
{
	ASPacket *p;

#if 1
	p = as_packet_create();

	tokenize_string (p, argv[1], 1);
	as_packet_dump (p);
	as_packet_free (p);
#else
	unsigned char ip[5];
	sscanf(argv[1], "%d.%d.%d.%d", ip+3,ip+2,ip+1,ip);
	ip[4]=0;
	printf("%d\n", hash_token (ip));
#endif

	return 0;
}
#endif