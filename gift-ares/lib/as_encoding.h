/*
 * $Id: as_encoding.h,v 1.3 2005/01/07 20:05:00 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_ENCODING_H
#define __AS_ENCODING_H

/*****************************************************************************/

/* caller frees returned string */
char *as_base64_encode (const unsigned char *data, int src_len);

/* caller frees returned string */
unsigned char *as_base64_decode (const char *data, int *dst_len);

/* caller frees returned string */
char *as_hex_encode (const unsigned char *data, int src_len);

/* caller frees returned string */
unsigned char *as_hex_decode (const char *data, int *dst_len);

/*****************************************************************************/

#endif /* __AS_ENCODING_H */
