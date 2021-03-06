/*
 * $Id: asp_hash.h,v 1.2 2004/12/12 16:19:32 hex Exp $
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

#ifndef __ASP_HASH_H
#define __ASP_HASH_H

/*****************************************************************************/

/* Called by giFT to hash a file for this network. */
unsigned char *asp_giftcb_hash (const char *path, size_t *len);

/* Called by giFT to encode a hash in human readable form. */
unsigned char *asp_giftcb_hash_encode (unsigned char *data);

/* as_hash_decode()-alike that handles both base32 and base64. */
ASHash *asp_hash_decode (const char *encoded);

/*****************************************************************************/

#endif /* __ASP_HASH_H */
