/*
 * $Id: as_download.h,v 1.5 2004/09/14 09:36:08 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_DOWNLOAD_H
#define __AS_DOWNLOAD_H

/*****************************************************************************/

typedef enum
{
	DOWNLOAD_NEW,        /* Initial state before download is started. */
	DOWNLOAD_ACTIVE,     /* Download is transfering/looking for sources. */
	DOWNLOAD_QUEUED,     /* Download is locally queued (because
	                      * AS_DOWNLOAD_MAX_ACTIVE is reached). */
	DOWNLOAD_PAUSED,     /* Download is paused by user (or disk full). */
	DOWNLOAD_COMPLETE,   /* Download completed successfully. */
	DOWNLOAD_FAILED,     /* Download was fully transfered but hash check
	                      * failed. */
	DOWNLOAD_CANCELLED,  /* Download was cancelled. */
	DOWNLOAD_VERIFYING   /* Download is being verified after downloading. */
} ASDownloadState;

typedef struct as_download_t ASDownload;

/* Called for every state change. Return FALSE if the download was freed and
 * must no longer be accessed.
 */
typedef as_bool (*ASDownloadStateCb) (ASDownload *dl, ASDownloadState state);

struct as_download_t
{
	ASHash *hash;      /* file hash */
	char   *filename;  /* save file name */
	size_t  size;      /* file size */
	size_t  received;  /* total number of bytes already received */
	FILE   *fp;        /* file pointer */

	List *conns;       /* List of ASDownConn's with sources */

	List *chunks;      /* List of chunks sorted by chunk->start and always
	                    * kept without holes and overlap */

	timer_id maintenance_timer; /* regular timer running while download is
	                             * active to check the queued sources */

	/* download state */
	ASDownloadState state;

	ASDownloadStateCb state_cb;

	void *udata; /* arbitrary user data */
};

/*****************************************************************************/

/* Create download. */
ASDownload *as_download_create (ASDownloadStateCb state_cb);

/* Stop and free download. Incomplete file is left in place and can be
 * resumed.
 */
void as_download_free (ASDownload *dl);

/*****************************************************************************/

/* Start download using hash, filesize and save name. */
as_bool as_download_start (ASDownload *dl, ASHash *hash, size_t filesize,
                           const char *filename);

/* Restart download from incomplete _ARESTRA_ file. This will fail if the file
 * is not found/corrupt/etc.
 */
as_bool as_download_restart (ASDownload *dl, const char *filename);

/* Cancels download and removes incomplete file. */
as_bool as_download_cancel (ASDownload *dl);

/* Pause download */
as_bool as_download_pause (ASDownload *dl);

/* Queue download locally (effectively pauses it) */
as_bool as_download_queue (ASDownload *dl);

/* Resume download from paused or queued state */
as_bool as_download_resume (ASDownload *dl);

/* Returns current download state */
ASDownloadState as_download_state (ASDownload *dl);

/*****************************************************************************/

/* Add source to download (copies source). */
as_bool as_download_add_source (ASDownload *dl, ASSource *source);

/*****************************************************************************/

#endif /* __AS_DOWNLOAD_H */
