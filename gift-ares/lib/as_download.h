/*
 * $Idb: as_download.h,v 1.2 2004/09/07 15:57:57 mkern Exp $
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
	char     *uri;
} ASDownload;


ASDownload *as_download_new (ASSource *source, ASHash *hash, unsigned char *filename);

as_bool as_download_start (ASDownload *dl);

#endif
