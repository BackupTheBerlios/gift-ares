/*
 * $Id: as_packet.c,v 1.10 2004/09/01 15:51:36 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static as_bool packet_resize (ASPacket *packet, size_t len);
static as_bool packet_write(ASPacket *packet, void *data, size_t size);
static as_bool packet_read(ASPacket *packet, void *data, size_t size);

/*****************************************************************************/

ASPacket *as_packet_create()
{
	ASPacket *packet;

	packet = malloc(sizeof(ASPacket));
	packet->data = packet->read_ptr = NULL;
	packet->used = packet->allocated = 0;

	return packet;
}

ASPacket *as_packet_create_copy(ASPacket* packet, size_t len)
{
	ASPacket *new_packet;

	new_packet = as_packet_create();

	if(len > as_packet_remaining(packet))
		len = as_packet_remaining(packet);

	packet_write(new_packet, packet->read_ptr, len);
	packet->read_ptr += len;
	
	return new_packet;
}

void as_packet_free(ASPacket *packet)
{
	if(!packet)
		return;

	if(packet->data)
		free(packet->data);
	free(packet);
}

void as_packet_append(ASPacket *packet, ASPacket *append)
{
	packet_write(packet, append->read_ptr, as_packet_remaining(append));
}

void as_packet_rewind(ASPacket *packet)
{
	packet->read_ptr = packet->data;
}

void as_packet_truncate(ASPacket *packet)
{
	size_t remaining = as_packet_remaining(packet);
	size_t i;
	as_uint8 *src = packet->read_ptr;
	as_uint8 *dst = packet->data;

	for(i = 0; i < remaining; ++i, ++src, ++dst)
		*dst = *src;

	packet->read_ptr = packet->data;
	packet->used = remaining;
}

size_t as_packet_size(ASPacket* packet)
{
	return packet->used;
}

size_t as_packet_remaining(ASPacket* packet)
{
	assert (packet->read_ptr >= packet->data);
	assert (packet->used >= (size_t) (packet->read_ptr - packet->data));

	return packet->used - (packet->read_ptr - packet->data);
}

/*****************************************************************************/

as_bool as_packet_put_8 (ASPacket *packet, as_uint8 data)
{
	if (!packet_resize (packet, packet->used + sizeof (as_uint8)))
		return FALSE;

	packet->data[packet->used] = data;
	packet->used++;

	return TRUE;
}

as_bool as_packet_put_le16 (ASPacket *packet, as_uint16 data)
{
	if (!packet_resize (packet, packet->used + sizeof (as_uint16)))
		return FALSE;

	packet->data[packet->used++] = data & 0xFF;
	packet->data[packet->used++] = data >> 8;
	return TRUE;
}

as_bool as_packet_put_be16 (ASPacket *packet, as_uint16 data)
{
	if (!packet_resize (packet, packet->used + sizeof (as_uint16)))
		return FALSE;

	packet->data[packet->used++] = data >> 8;
	packet->data[packet->used++] = data & 0xFF;
	return TRUE;
}

as_bool as_packet_put_le32 (ASPacket *packet, as_uint32 data)
{
	if (!packet_resize (packet, packet->used + sizeof (as_uint32)))
		return FALSE;

	packet->data[packet->used++] = data & 0xFF;
	packet->data[packet->used++] = (data >> 8) & 0xFF;
	packet->data[packet->used++] = (data >> 16) & 0xFF;
	packet->data[packet->used++] = (data >> 24) & 0xFF;
	return TRUE;
}

as_bool as_packet_put_be32 (ASPacket *packet, as_uint32 data)
{
	if (!packet_resize (packet, packet->used + sizeof (as_uint32)))
		return FALSE;

	packet->data[packet->used++] = (data >> 24) & 0xFF;
	packet->data[packet->used++] = (data >> 16) & 0xFF;
	packet->data[packet->used++] = (data >> 8) & 0xFF;
	packet->data[packet->used++] = data & 0xFF;
	return TRUE;
}

as_bool as_packet_put_ustr (ASPacket *packet, as_uint8 *str, size_t len)
{
	return packet_write(packet, str, len);
}

/*****************************************************************************/

as_uint8 as_packet_get_8 (ASPacket *packet)
{
	if(as_packet_remaining(packet) < sizeof (as_uint8))
		return 0; /* hmm */

	packet->read_ptr++;
	return packet->read_ptr[-1];
}

as_uint16 as_packet_get_le16 (ASPacket *packet)
{
	as_uint16 ret;

	if(as_packet_remaining(packet) < sizeof (as_uint16))
		return 0;

	ret = *packet->read_ptr++;
	ret |= ((as_uint16) *packet->read_ptr++) << 8;
	return ret;
}

as_uint16 as_packet_get_be16 (ASPacket *packet)
{
	as_uint16 ret;

	if(as_packet_remaining(packet) < sizeof (as_uint16))
		return 0;

	ret = ((as_uint16) *packet->read_ptr++) << 8;
	ret |= *packet->read_ptr++;
	return ret;
}

as_uint32 as_packet_get_le32 (ASPacket *packet)
{
	as_uint32 ret;

	if(as_packet_remaining(packet) < sizeof (as_uint32))
		return 0;

	ret = *packet->read_ptr++;
	ret |= ((as_uint32) *packet->read_ptr++) << 8;
	ret |= ((as_uint32) *packet->read_ptr++) << 16;
	ret |= ((as_uint32) *packet->read_ptr++) << 24;
	return ret;
}

as_uint32 as_packet_get_be32 (ASPacket *packet)
{
	as_uint32 ret;

	if(as_packet_remaining(packet) < sizeof (as_uint32))
		return 0;

	ret = ((as_uint32) *packet->read_ptr++) << 24;
	ret |= ((as_uint32) *packet->read_ptr++) << 16;
	ret |= ((as_uint32) *packet->read_ptr++) << 8;
	ret |= *packet->read_ptr++;
	return ret;
}

as_uint8 *as_packet_get_ustr (ASPacket *packet, size_t len)
{
	as_uint8 *ret;
	
	if (!(ret = malloc (len)) || !packet_read (packet, ret, len))
	{
		free (ret);
		return NULL;
	}
	return ret;
}

char *as_packet_get_str (ASPacket *packet, size_t len)
{
	char *ret;

	if (!(ret = malloc (len+1)) || !packet_read (packet, ret, len))
	{
		free (ret);
		return NULL;
	}

	ret[len] = 0;
	return ret;
}

int as_packet_strlen (ASPacket *packet, as_uint8 termbyte)
{
	as_uint8 *p = packet->read_ptr;
	int remaining = as_packet_remaining (packet);
	int i = 0;

	for(i=0; i < remaining; i++, p++)
		if(*p == termbyte)
			return i;

	return -1;
}

char *as_packet_get_strnul (ASPacket *packet)
{
	int len = as_packet_strlen (packet, '\0');

	if (len < 0)
		return NULL;
	
	return as_packet_get_ustr (packet, len+1);
}

/*****************************************************************************/

/* Encrypt entire packet using cipher. This will add the two bytes of seed
 * to the beginning of the packet.
 */
as_bool as_packet_encrypt(ASPacket *packet, ASCipher *cipher)
{
	as_uint8 seed_a = 0x00; /* always use zero for out packet seeds */
	as_uint8 seed_b = 0x00;

	/* encrypt packet with choosen seeds */
	as_cipher_encrypt (cipher, seed_a, packet->data, as_packet_size (packet));

	/* make enough room for seeds */
	if (!packet_resize (packet, as_packet_size (packet) + 2))
		return FALSE;

	/* move data towards end by two bytes */
	memmove (packet->data + 2, packet->data, as_packet_size (packet));
	packet->used += 2;

	/* add seeds at front */
	packet->data[0] = seed_a;
	packet->data[1] = seed_b;

	return TRUE;
}

/* Encrypt entire packet using cipher. This will add the two bytes of seed
 * to the beginning of the packet.
 */
as_bool as_packet_header(ASPacket *packet, ASPacketType type)
{
	int len;

	/* make enough room for seeds */
	if (!packet_resize (packet, as_packet_size (packet) + 3))
		return FALSE;

	/* move data towards end by two bytes */
	memmove (packet->data + 3, packet->data, as_packet_size (packet));

	len = packet->used;
	packet->used += 3;

	/* add seeds at front */
	packet->data[0] = len & 255;
	packet->data[1] = len >> 8;
	packet->data[2] = type;

	return TRUE;
}

/* Decrypt entire packet using cipher. This will remove the two bytes of seed
 * at the beginning of the packet.
 */
as_bool as_packet_decrypt(ASPacket *packet, ASCipher *cipher)
{
	as_uint8 seed_a, seed_b;

	if (as_packet_remaining (packet) < 3)
		return FALSE;

	/* read packet seeds an remove them from packet */
	seed_a = as_packet_get_8 (packet);
	seed_b = as_packet_get_8 (packet);
	as_packet_truncate (packet);

	/* decrypt packet using first seed */
	as_cipher_decrypt (cipher, seed_a, packet->read_ptr,
	                   as_packet_remaining (packet));

	return TRUE;
}

/*****************************************************************************/

as_bool as_packet_send (ASPacket *packet, TCPC *tcpcon)
{
	int sent;

	if ((sent = tcp_send (tcpcon, packet->data, packet->used)) < 0)
		return FALSE;

	if (sent < (int) packet->used)
	{
		/* FIXME: short send */
		return FALSE;
	}

	return TRUE;
}

as_bool as_packet_recv (ASPacket *packet, TCPC *tcpcon)
{
	int ret;

	packet_resize(packet, packet->used + 1024);

	ret = tcp_recv(tcpcon, packet->data + packet->used, packet->allocated - packet->used);

	if(ret <= 0)
		return FALSE;

	packet->used += ret;

	return TRUE;
}

/*****************************************************************************/

void as_packet_dump (ASPacket *packet)
{
	as_uint8 *data=packet->data;
	int len = packet->used;
	int i;
	int i2;
	int i2_end;

	printf ("packet len=%d\n", len);
	for (i2 = 0; i2 < len; i2 = i2 + 16)
	{
		i2_end = (i2 + 16 > len) ? len: i2 + 16;
		for (i = i2; i < i2_end; i++)
			if (isprint(data[i]))
				fprintf(stdout, "%c", data[i]);
			else
			fprintf(stdout, ".");
		for ( i = i2_end ; i < i2 + 16; i++)
			fprintf(stdout, " ");
		fprintf(stdout, " | ");
		for (i = i2; i < i2_end; i++)
			fprintf(stdout, "%02x ", data[i]);
		fprintf(stdout, "\n");
	}
}

ASPacket *as_packet_slurp (void)
{
	int c;
	ASPacket *p = as_packet_create();

	while ((c = getchar()) != EOF)
		as_packet_put_8 (p, (as_uint8) c);

	return p;
}

/*****************************************************************************/

/* make sure packet->mem is large enough to hold len bytes */
static as_bool packet_resize (ASPacket *packet, size_t len)
{
	as_uint8 *new_mem;
	size_t new_alloc, read_offset;

	if (!packet)
		return FALSE;

	/* there is always space for no bytes */
	if (len == 0)
		return TRUE;

	/* the buffer we have allocated is already large enough */
	if (packet->allocated >= len)
		return TRUE;

	/* determine an appropriate resize length */
	new_alloc = packet->allocated;
	while (new_alloc < len)
		new_alloc += 512;

	read_offset = packet->read_ptr - packet->data;

	/* gracefully fail if we are unable to resize the buffer */
	if (!(new_mem = realloc (packet->data, new_alloc)))
		return FALSE;

	/* modify the packet structure to reflect this resize */
	packet->data = new_mem;
	packet->read_ptr = packet->data + read_offset;
	packet->allocated = new_alloc;

	return TRUE;
}

static as_bool packet_write(ASPacket *packet, void *data, size_t size)
{
	if (!packet_resize (packet, packet->used + size))
		return FALSE;

	memcpy(packet->data + packet->used, data, size);
	packet->used += size;

	return TRUE;
}

static as_bool packet_read(ASPacket *packet, void *data, size_t size)
{
	if(as_packet_remaining(packet) < size)
		return FALSE;

	memcpy(data, packet->read_ptr, size);
	packet->read_ptr += size;

	return TRUE;
}


