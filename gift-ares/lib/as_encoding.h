/*
 * $Id: as_encoding.h,v 1.1 2004/09/06 12:37:24 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

char *as_base64_encode (const unsigned char *data, int src_len);
unsigned char *as_base64_decode (const char *data, int *dst_len);
