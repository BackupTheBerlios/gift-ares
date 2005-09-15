/*
 * $Id: as_packet.h,v 1.20 2005/09/15 21:13:53 mkern Exp $
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
typedef struct as_packet_t
{
	as_uint8 *data;     /* pointer to allocated memory */
	as_uint8 *read_ptr; /* pointer to current read position */
	size_t used;        /* used number of bytes relative to data */
	size_t allocated;   /* allocated number of bytes */
} ASPacket;

typedef enum
{
	/* unencrypted */
	PACKET_NODE_REQ    = 70, /* 0x46, request nodes from index node */
	PACKET_SYN         = 90, /* 0x5A, first packet sent by connecting party */

	/* otherwise specially handled */
	PACKET_COMPRESSED  = 50, /* 0x32, zlib compression encapsulation layer */
	PACKET_ACK         = 51, /* 0x33, first packet sent back by supernode */
	PACKET_ACK2        = 56, /* 0x38, new ACK packet in Ares 2962 */
	PACKET_PUSH2       = 7,  /* 0x07, push request */

	/* encrypted */
	PACKET_HANDSHAKE   = 0,  /* 0x00, info about our node */
	PACKET_STATS       = 1,  /* 0x01, network stats 1 */
	PACKET_STATS2      = 30, /* 0x1E, network stats 2 */
	PACKET_LOCALIP     = 37, /* 0x25, local (external) IP */
	PACKET_NICKNAME    = 5,  /* 0x05, your nickname */
	PACKET_SHARE       = 28, /* 0x1C, used to send shares to supernode */
	PACKET_SEARCH      = 9,  /* 0x09, token search */
	PACKET_LOCATE      = 80, /* 0x50, hash search */
	PACKET_LOCALT_STOP = 81, /* 0x51, seems to be sent when download is
	                          *       cancelled to stop supernode from sending
							  *       more locate results */
	PACKET_RESULT      = 18, /* 0x12, search result */
	PACKET_NODELIST    = 54, /* 0x36, list of index nodes? */
	PACKET_NODELIST2   = 55, /* 0x37, replaces PACKET_NODELIST in Ares 2962 */
	PACKET_PUSH        = 8,  /* 0x08, push request */
	PACKET_SUPERINFO   = 58, /* 0x3a, info about this supernode? */
} ASPacketType;

typedef enum
{
	PACKET_PLAIN    = 0x00, /* Packet is neither compressed nor encrypted */
	PACKET_ENCRYPT  = 0x01, /* Packet it encrypted */
	PACKET_COMPRESS = 0x02  /* Packet is compressed */
} ASPacketFlag;

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
as_bool as_packet_append(ASPacket *packet, ASPacket *append);

/* appends c count number of times to packet */
as_bool as_packet_pad (ASPacket *packet, as_uint8 c, size_t count);

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

/* append big endian in_addr_t */
as_bool as_packet_put_ip (ASPacket *packet, in_addr_t ip);

/* append string of length len to packet */
as_bool as_packet_put_ustr (ASPacket *packet, const as_uint8 *str, size_t len);

/* append nul-terminated string to packet */
as_bool as_packet_put_strnul (ASPacket *packet, const as_uint8 *str);

/* append 20 byte sha1 hash */
as_bool as_packet_put_hash (ASPacket *packet, ASHash *hash);

/*****************************************************************************/

/* return uint8 and move read_ptr */
as_uint8 as_packet_get_8 (ASPacket *packet);

/* return little/big endian uint16 and move read_ptr */
as_uint16 as_packet_get_le16 (ASPacket *packet);
as_uint16 as_packet_get_be16 (ASPacket *packet);

/* return little/big endian uint32 and move read_ptr */
as_uint32 as_packet_get_le32 (ASPacket *packet);
as_uint32 as_packet_get_be32 (ASPacket *packet);

/* return big endian in_addr_t and move read_ptr */
in_addr_t as_packet_get_ip (ASPacket *packet);

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

/* get 20 byte sha1 hash */
ASHash *as_packet_get_hash (ASPacket *packet);

/*****************************************************************************/

/* debugging stuff */
void as_packet_dump (ASPacket *packet);

ASPacket *as_packet_slurp (void);

/*****************************************************************************/

/* Encrypt entire packet using cipher. This will add the two bytes of seed
 * to the beginning of the packet.
 */
as_bool as_packet_encrypt (ASPacket *packet, ASCipher *cipher);

/* Decrypt entire packet using cipher. This will remove the two bytes of seed
 * at the beginning of the packet.
 */
as_bool as_packet_decrypt (ASPacket *packet, ASCipher *cipher);

/* Compress packet with zlib */
as_bool as_packet_compress (ASPacket *packet);

/* Prepend the three-byte header (type and length). */
as_bool as_packet_header (ASPacket *packet, ASPacketType type);

/*****************************************************************************/

/* send entire packet to connected host */
as_bool as_packet_send (ASPacket *packet, TCPC *tcpcon);

/* recv 1024 bytes from connected host and append data */
as_bool as_packet_recv (ASPacket *packet, TCPC *tcpcon);

/*****************************************************************************/

void as_packet_dump(ASPacket *packet);

#endif /* __AS_PACKET_H */
