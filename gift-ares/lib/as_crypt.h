/*
 * $Id: as_crypt.h,v 1.3 2004/08/21 12:32:22 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_CRYPT_H_
#define __AS_CRYPT_H_

#include "as_ares.h"

/*****************************************************************************/

typedef struct
{
	/* the key used to decrypt the 0x33 packet which is really the port the
	 * connection was made to */
	as_uint16 handshake_key;

	/* the seeds from the 0x33 handshake packet */
	as_uint16 session_seed_16;
	as_uint8  session_seed_8;
	
} ASCipher;

/*****************************************************************************/

/* allocate and init cipher */
ASCipher *as_cipher_create (as_uint16 handshake_key);

/* free cipher */
void as_cipher_free (ASCipher *cipher);

/* set session seeds */
void as_cipher_set_seeds (ASCipher *cipher, as_uint16 seed_16,
                          as_uint8 seed_8);

/*****************************************************************************/

/* encrypt/decrypt a block of data with session key */
void as_cipher_encrypt (ASCipher *cipher, as_uint8 packet_seed,
                        as_uint8 *data, int len);

void as_cipher_decrypt (ASCipher *cipher, as_uint8 packet_seed,
                        as_uint8 *data, int len);

/* encrypt/decrypt a block of data with handshake key */
void as_cipher_encrypt_handshake (ASCipher *cipher, as_uint8 *data, int len);
void as_cipher_decrypt_handshake (ASCipher *cipher, as_uint8 *data, int len);

/*****************************************************************************/

in_port_t as_ip2port (in_addr_t ip);

/*****************************************************************************/

#endif /* __AS_CRYPT_H_ */
