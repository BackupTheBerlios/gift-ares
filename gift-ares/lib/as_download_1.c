/*
 * $Id: as_download_1.c,v 1.2 2004/09/10 18:04:46 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* filename prefix for incomplete files */
#define INCOMPLETE_PREFIX "_ARESTRA_"

/*****************************************************************************/

static as_bool conn_state_cb (ASDownConn *conn, ASDownConnState state);

static as_bool conn_data_cb (ASDownConn *conn, as_uint8 *data,
                             unsigned int len);

/*****************************************************************************/

/* Create download. */
ASDownload *as_download_create (ASDownloadStateCb state_cb)
{
	ASDownload *dl;

	if (!(dl = malloc (sizeof (ASDownload))))
		return NULL;

	dl->hash     = NULL;
	dl->filename = NULL;
	dl->size     = 0;
	dl->received = 0;
	dl->fp       = NULL;

	dl->conns  = NULL;
	dl->chunks = NULL;

	dl->state    = DOWNLOAD_NEW;
	dl->state_cb = state_cb;
	
	dl->udata = NULL;

	return dl;
}

/* Stop and free download. */
void as_download_free (ASDownload *dl)
{
	Link *l;

	if (!dl)
		return;
	
	/* TODO: save state */

	as_hash_free (dl->hash);
	free (dl->filename);
	fclose (dl->fp);

	for (l = dl->conns; l; l = l->next)
		as_downconn_free (dl->conns);
	list_free (dl->conns);

	for (l = dl->chunks; l; l = l->next)
		as_downchunk_free (dl->chunks);
	list_free (dl->chunks);

	free (dl);
}

/*****************************************************************************/

/* Start download using hash, filesize and save name. */
as_bool as_download_start (ASDownload *dl, ASHash *hash, size_t filesize,
                           const char *filename)
{
	ASDownChunk *chunk;

	if (dl->state != DOWNLOAD_NEW)
	{
		assert (dl->state == DOWNLOAD_NEW);
		return FALSE;
	}

	if (!hash || !filename || filesize == 0)
	{
		assert (hash);
		assert (filename);
		assert (filesize > 0);
		return FALSE;
	}

	/* open file */
	dl->filename = stringf_dup ("%s%s", INCOMPLETE_PREFIX, filename);

	if (!(fp = fopen (dl->filename, "wb")))
	{
		AS_ERR_1 ("Unable to open download file \"%s\" for writing",
		          dl->filename);
		free (dl->filename);
		dl->filename = NULL;
		return FALSE;
	}

	/* create one initial chunk for entire file */
	dl->size = filesize;

	if (!(chunk = as_downchunk_create (0, dl->size)))
	{
		AS_ERR ("Insufficient memory");
		free (dl->filename);
		dl->filename = NULL;
		dl->size = 0;
		return FALSE;
	}

	dl->chunks = list_prepend (dl->chunks, chunk);

	/* copy hash */
	dl->hash = as_hash_copy (hash);

	/* TODO: reevaluate chunking to kick things off */

	return TRUE;
}

/* Restart download from incomplete _ARESTRA_ file. This will fail if the file
 * is not found/corrupt/etc.
 */
as_bool as_download_restart (ASDownload *dl, const char *filename)
{
	/* TODO */
	assert (0);
	return FALSE;
}

/* Pause download */
as_bool as_download_pause (ASDownload *dl)
{
	if (dl->state == DOWNLOAD_PAUSED)
		return TRUE;

	/* TODO */

	return TRUE;
}

/* Resume paused download */
as_bool as_download_resume (ASDownload *dl)
{
	if (dl->state != DOWNLOAD_PAUSED)
		return TRUE;

	/* TODO */

	return TRUE;
}

/* Returns current download state */
ASDownloadState as_download_state (ASDownload *dl)
{
	return dl->state
}

/*****************************************************************************/

/* Add source to download (copies source). */
as_bool as_download_add_source (ASDownload *dl, ASSource *source)
{	
	Link *l;
	ASDownConn *conn;

	/* Sources can be added in any state but we need to make sure there are
	 * no duplicates
	 */
	for (l = dl->conns; l; l = l->next)
	{
		conn = l->data;
		if (as_source_equal (conn->source, source))
		{
			AS_DBG_1 ("Source \"%s\" already added.", as_source_str (source));
			return FALSE;
		}
	}

	/* create new connection and add it */
	if (!(conn = as_downconn_create (source, conn_state_cb, conn_data_cb)))
		return FALSE;

	dl->conns = list_prepend (dl->conns, conn);

	/* TODO: reevaluate chunking */

	return TRUE;
}

/*****************************************************************************/

/* Called for every state change. Return FALSE if the connection was freed and
 * must no longer be accessed.
 */
static as_bool conn_state_cb (ASDownConn *conn, ASDownConnState state)
{

}

/* Called for every piece of data downloaded. Return FALSE if the connection
 * was freed and must no longer be accessed.
 */
static as_bool conn_data_cb (ASDownConn *conn, as_uint8 *data,
                             unsigned int len)
{

}

/*****************************************************************************/

