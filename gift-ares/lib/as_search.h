/*
 * $Id: as_search.h,v 1.1 2004/09/02 11:21:28 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SEARCH_H_
#define __AS_SEARCH_H_

/* create a search request packet */
ASPacket *search_request (unsigned char *query, as_uint16 id);

void parse_search_result (ASPacket *packet);

#endif

