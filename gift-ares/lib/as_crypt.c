/*
 * $Id: as_crypt.c,v 1.2 2004/08/20 13:31:07 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_crypt.h"

/*****************************************************************************/

static as_uint16 calc_packet_key (as_uint8 packet_seed, as_uint16 seed_16,
                                  as_uint8 seed_8)
{
	unsigned int ps = (unsigned int)packet_seed;
	unsigned int table_state;
	int i;

	table_state = (unsigned int) table_1[seed_8];

	for (i = 0; i < 4; i++)
	{
		table_state -= ps * 3;    /* CHECKME: what if ps * 3 > table_state? */
		table_state += table_2[ps] - table_3[ps];
		table_state += seed_16;
	}

	return (table_state & 0xFFFF);
}

/*****************************************************************************/

/* allocate and init cipher */
ASCipher *as_cipher_create (as_uint16 handshake_key)
{
	ASCipher cipher;

	if (!(cipher = malloc (sizeof (ASCipher))))
		return NULL;

	cipher->handshake_key = handshake_key;
	cipher->session_seed_16 = 0;
	cipher->session_seed_8 = 0;

	return cipher;
}

/* free cipher */
void as_cipher_free (ASCipher *cipher)
{
	if (!cipher)
		return;

	free (cipher);
}

/* set seeds and calculate session key */
void as_cipher_set_seeds (ASCipher *cipher, as_uint16 seed_16, as_uint8 seed_8)
{
	if (!cipher)
		return;

	cipher->session_seed_16 = seed_16;
	cipher->session_seed_8 = seed_8;
}

/*****************************************************************************/

/* encrypt/decrypt a block of data with session key */
void as_cipher_encrypt (ASCipher *cipher, as_uint8 packet_seed,
                        as_uint8 *data, int len)
{
	as_uint16 key;
	int i;
	
	key = calc_packet_key (packet_seed, cipher->session_seed_16,
	                       cipher->session_seed_8);

	for (i = 0; i < len; i++)
	{
		data[i] = data[i] ^ (key >> 8);
		key = (key + data[i]) * 0xCE6D + 0x58BF;
	}
}

void as_cipher_decrypt (ASCipher *cipher, as_uint8 packet_seed,
                        as_uint8 *data, int len)
{
	as_uint16 key;
	as_uint8 c;
	int i;
	
	key = calc_packet_key (packet_seed, cipher->session_seed_16,
	                       cipher->session_seed_8);

	for (i = 0; i < len; i++)
	{
		c = data[i] ^ (key >> 8);
		key = (key + data[i]) * 0xCE6D + 0x58BF;
		data[i] = c;
	}
}

/* encrypt/decrypt a block of data with handshake key */
void as_cipher_encrypt_handshake (ASCipher *cipher, as_uint8 *data, int len)
{
	as_uint16 key = session->handshake_key;
	int i;

	for (i = 0; i < len; i++)
	{
		data[i] = data[i] ^ (key >> 8);
		key = (key + data[i]) * 0x5CA0 + 0x15EC;
	}
}

void as_cipher_decrypt_handshake (ASCipher *cipher, as_uint8 *data, int len)
{
	as_uint8 c;
	as_uint16 key = session->handshake_key;
	int i;

	for (i = 0; i < len; i++)
	{
		c = data[i] ^ (key >> 8);
		key = (key + data[i]) * 0x5CA0 + 0x15EC;
		data[i] = c;
	}
}

/*****************************************************************************/

/* A slightly modified version of the algo used for search token hashing */
static as_uint16 hash_lowered_token (as_uint8 *str, int len)
{
	as_uint32 acc = 0;
	as_uint8 c;
	int b = 0;

	/* this is a very poor hash function :( */
	for (; len > 0; len--, str++)
	{
		c = tolower (*str);
		acc ^= c << (b * 8);
		b = (b + 1) & 3;
	}

	return (acc * 0x4f1bbcdc) >> 16;
}

in_port_t as_ip2port (in_addr_t ip)
{
	as_uint8 ip_str[4];
	as_uint8 tmp_str[4];
	as_uint16 ip_token;
	as_uint32 port;

	ip_str[0] = (ip >> 24) & 0xFF;
	ip_str[1] = (ip >> 16) & 0xFF;
	ip_str[2] = (ip >> 8)  & 0xFF;
	ip_str[3] = (ip)       & 0xFF;

	ip_token = hash_lowered_token (ip_str, 4);

	port = (((((as_uint16) ip_str[0]) * ip_str[0]) + ip_token) * 3);

	tmp_str[0] = port & 0xFF;
	tmp_str[1] = (port >> 8) & 0xFF;
	tmp_str[2] = 0xBE;
	tmp_str[3] = 0x04;

	port += hash_lowered_token (tmp_str, 4);
	port += ip_token + 0x12;
	port += 0x5907;                                  /* hash of "strano" */
	port -= (((as_uint16) ip_str[0] - 5) << 2) * 3;
	port += 0xCDF8;                                  /* hash of "robboso" */

	if (port < 1024)
		port += 1024;

	if (port == 36278)
		port++;

	return (in_port_t) (port & 0xFFFF);
}

/*****************************************************************************/

