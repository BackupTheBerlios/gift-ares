/*
 * $Id: as_packet.h,v 1.3 2004/08/26 15:57:44 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_PACKET_H
#define __AS_PACKET_H

/*****************************************************************************/

/**
 * ASPacket structure.
 */
typedef struct
{
	as_uint8 *data;     /* pointer to allocated memory */
	as_uint8 *read_ptr; /* pointer to current read position */
	size_t used;        /* used number of bytes relative to data */
	size_t allocated;   /* allocated number of bytes */
}ASPacket;

/*****************************************************************************/

/* allocates and returns a new packet */
ASPacket *as_packet_create();

/* creates packet with data from packet->read_ptr to packet->read_ptr + len
 * and moves read_ptr
 */
ASPacket *as_packet_create_copy(ASPacket* packet, size_t len);

/* frees packet */
void as_packet_free(ASPacket *packet);

/* appends everything from append->read_ptr to packet */
void as_packet_append(ASPacket *packet, ASPacket *append);

/* rewinds read_ptr to data */
void as_packet_rewind(ASPacket *packet);

/* removes everything from before read_ptr from packet */
void as_packet_truncate(ASPacket *packet);

/* returns size of entire packet */
size_t as_packet_size(ASPacket* packet);

/* returns size of remaining data */
size_t as_packet_remaining(ASPacket* packet);

/*****************************************************************************/

/* append uint8 to packet. */
as_bool as_packet_put_8 (ASPacket *packet, as_uint8 data);

/* append little/big endian uint32 to packet */
as_bool as_packet_put_le16 (ASPacket *packet, as_uint16 data);
as_bool as_packet_put_be16 (ASPacket *packet, as_uint16 data);

/* append little/big endian uint32 to packet */
as_bool as_packet_put_le32 (ASPacket *packet, as_uint32 data);
as_bool as_packet_put_be32 (ASPacket *packet, as_uint32 data);

/* append string of length len to packet */
as_bool as_packet_put_ustr (ASPacket *packet, as_uint8 *str, size_t len);

/*****************************************************************************/

/* return uint8 and move read_ptr */
as_uint8 as_packet_get_8 (ASPacket *packet);

/* return little/big endian uint16 and move read_ptr */
as_uint16 as_packet_get_le16 (ASPacket *packet);
as_uint16 as_packet_get_be16 (ASPacket *packet);

/* return little/big endian uint32 and move read_ptr */
as_uint32 as_packet_get_le32 (ASPacket *packet);
as_uint32 as_packet_get_be32 (ASPacket *packet);

/* return string of size len and move read_ptr, caller frees returned string */
as_uint8 *as_packet_get_ustr (ASPacket *packet, size_t len);

/* return string of size len with appended '\0' and move read_ptr, caller
 * frees returned string */
char *as_packet_get_str (ASPacket *packet, size_t len);

/* counts the number of bytes from read_ptr until termbyte is reached
 * returns -1 if termbyte doesn't occur in packet */
int as_packet_strlen (ASPacket *packet, as_uint8 termbyte);

/* wrapper around as_packet_strlen and as_packet_get_str */
char *as_packet_get_strnul (ASPacket *packet);

/*****************************************************************************/

/* debugging stuff */
void as_packet_dump (ASPacket *packet);

ASPacket *as_packet_slurp (void);

/*****************************************************************************/

// encrypt entire packet using cipher
void as_packet_encrypt(ASPacket *packet, ASCipher *cipher);

// decrypt entire packet using cipher
void as_packet_decrypt(ASPacket *packet, ASCipher *cipher);

/*****************************************************************************/

/* send entire packet to connected host */
as_bool as_packet_send (ASPacket *packet, TCPC *tcpcon);

/* recv 1024 bytes from connected host and append data */
as_bool as_packet_recv (ASPacket *packet, TCPC *tcpcon);

/*****************************************************************************/

void as_packet_dump(ASPacket *packet);

#endif /* __AS_PACKET_H */
