/*
 * $Id: as_download.h,v 1.1 2004/09/06 17:27:55 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_DOWNLOAD_H_
#define __AS_DOWNLOAD_H_

/* FIXME */
typedef struct {
	unsigned char *filename;
	ASSource *source;
	ASHttpHeader *request;
	ASHttpClient *client;
	ASHash   *hash;
	TCPC     *c;
	as_uint32 size;
	as_uint32 retrieved;
	FILE     *f;
	timer_id  timer;
} ASDownload;

#endif
