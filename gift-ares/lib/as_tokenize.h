/*
 * $Id: as_tokenize.h,v 1.2 2004/09/07 13:05:33 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_TOKENIZE_H
#define __AS_TOKENIZE_H

/*****************************************************************************/

/* Tokenize str and add it to search packet. Returns number of added tokens */
int as_tokenize_search (ASPacket *packet, unsigned char *str);

/* Tokenize str and add it to share packet. Returns number of added tokens */
int as_tokenize_share (ASPacket *packet, unsigned char *str);

/*****************************************************************************/

#endif /* __AS_TOKENIZE_H */
