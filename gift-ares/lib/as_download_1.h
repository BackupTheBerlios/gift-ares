/*
 * $Id: as_download_1.h,v 1.2 2004/09/09 16:55:46 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_DOWNLOAD_H
#define __AS_DOWNLOAD_H

/*****************************************************************************/

typedef struct
{


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

/*****************************************************************************/

/* Create download from hash, filesize and save name. */
ASDownload *as_download_create (ASHash *hash, size_t size,
                                const char *filename);

/* Create download from incomplete _ARESTRA_ file. This will fail if the file
 * is not found/corrupt/etc.
 */
ASDownload *as_download_recreate (const char *filename);

/* Free download and lose all data. */
void as_download_free (ASDownload *dl);

/*****************************************************************************/

/* Add source to download. */
as_bool as_download_add_source (ASDownload *dl, ASSource *source);

/*****************************************************************************/

#endif /* __AS_DOWNLOAD_H */
