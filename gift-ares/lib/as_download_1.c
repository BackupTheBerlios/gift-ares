/*
 * $Id: as_download_1.c,v 1.5 2004/09/13 13:40:04 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

#ifdef WIN32
#include <io.h> /* _chsize and friends */
#endif

/*****************************************************************************/

/* Filename prefix for incomplete files */
#define INCOMPLETE_PREFIX "_ARESTRA_"

/* If defined failed downloads will not be deleted */
#define KEEP_FAILED

/*****************************************************************************/

static as_bool conn_state_cb (ASDownConn *conn, ASDownConnState state);

static as_bool conn_data_cb (ASDownConn *conn, as_uint8 *data,
                             unsigned int len);

static void stop_all_connections (ASDownload *dl);

static void download_maintain (ASDownload *dl);

static as_bool download_failed (ASDownload *dl);
static as_bool download_complete (ASDownload *dl);
static as_bool download_finished (ASDownload *dl);

/*****************************************************************************/

static as_bool download_set_state (ASDownload *dl, ASDownloadState state,
                                   as_bool raise_callback)
{
	dl->state = state;

	if (raise_callback && dl->state_cb)
		return dl->state_cb (dl, dl->state);

	return TRUE;
}

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
	List *l;

	if (!dl)
		return;
	
	/* TODO: save state */

	as_hash_free (dl->hash);
	free (dl->filename);
	if (dl->fp)
		fclose (dl->fp);

	for (l = dl->conns; l; l = l->next)
		as_downconn_free (l->data);
	list_free (dl->conns);

	for (l = dl->chunks; l; l = l->next)
		as_downchunk_free (l->data);
	list_free (dl->chunks);

	free (dl);
}

/*****************************************************************************/

/* Start download using hash, filesize and save name. */
as_bool as_download_start (ASDownload *dl, ASHash *hash, size_t filesize,
                           const char *filename)
{
	ASDownChunk *chunk;
	struct stat st;

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

	/* create incomplete file name */
	dl->filename = stringf_dup ("%s%s", INCOMPLETE_PREFIX, filename);

	/* make sure the file does not already exist */
	if (stat (dl->filename, &st) != -1)
	{
		AS_ERR_1 ("Download file \"%s\" already exists.", dl->filename);
		free (dl->filename);
		dl->filename = NULL;
		return FALSE;
	}

	/* open file */
	if (!(dl->fp = fopen (dl->filename, "wb")))
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
		AS_ERR_1 ("Couldn't create initial chunk (0,%u)", dl->size);
		free (dl->filename);
		dl->filename = NULL;
		dl->size = 0;
		return FALSE;
	}

	dl->chunks = list_prepend (dl->chunks, chunk);

	/* copy hash */
	dl->hash = as_hash_copy (hash);

	/* raise callback */
	if (!download_set_state (dl, DOWNLOAD_ACTIVE, TRUE))
		return FALSE;

	/* start things off */
	download_maintain (dl);

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

/* Cancels download and removes incomplete file. */
as_bool as_download_cancel (ASDownload *dl)
{
	if (dl->state != DOWNLOAD_ACTIVE && dl->state != DOWNLOAD_QUEUED &&
	    dl->state != DOWNLOAD_PAUSED)
	{
		return FALSE;
	}

	AS_DBG_1 ("Cancelling download \"%s\"", dl->filename);

	/* stop all connections */
	stop_all_connections (dl);

	/* close fd */
	if (dl->fp)
	{
		fclose (dl->fp);
		dl->fp = NULL;
	}

	/* delete incomplete file. */
	if (unlink (dl->filename) < 0)
		AS_ERR_1 ("Failed to unlink incomplete file \"%s\"", dl->filename);

	/* raise callback */
	if (!download_set_state (dl, DOWNLOAD_CANCELLED, TRUE))
		return FALSE;

	return TRUE;
}

/* Pause download */
as_bool as_download_pause (ASDownload *dl)
{
	if (dl->state == DOWNLOAD_PAUSED)
		return TRUE;

	if (dl->state != DOWNLOAD_ACTIVE && dl->state != DOWNLOAD_QUEUED)
		return FALSE;

	AS_DBG_1 ("Pausing download \"%s\"", dl->filename);

	/* Stop all active chunk downloads. */
	stop_all_connections (dl);

	if (!download_set_state (dl, DOWNLOAD_PAUSED, TRUE))
		return FALSE;

	return TRUE;
}

/* Queue download locally (effectively pauses it) */
as_bool as_download_queue (ASDownload *dl)
{
	if (dl->state == DOWNLOAD_QUEUED)
		return TRUE;

	if (dl->state != DOWNLOAD_ACTIVE && dl->state != DOWNLOAD_PAUSED)
		return FALSE;

	AS_DBG_1 ("Queuing download \"%s\"", dl->filename);

	/* Stop all active chunk downloads. */
	stop_all_connections (dl);

	if (!download_set_state (dl, DOWNLOAD_QUEUED, TRUE))
		return FALSE;

	return TRUE;
}

/* Resume download from paused or queued state */
as_bool as_download_resume (ASDownload *dl)
{
	if (dl->state == DOWNLOAD_ACTIVE)
		return TRUE;

	if (dl->state != DOWNLOAD_PAUSED && dl->state != DOWNLOAD_QUEUED)
		return FALSE;

	/* Activate chunk downloads */
	if (!download_set_state (dl, DOWNLOAD_ACTIVE, TRUE))
		return FALSE;

	download_maintain (dl);

	return TRUE;
}

/* Returns current download state */
ASDownloadState as_download_state (ASDownload *dl)
{
	return dl->state;
}

/*****************************************************************************/

/* Add source to download (copies source). */
as_bool as_download_add_source (ASDownload *dl, ASSource *source)
{	
	List *l;
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

	/* point udata1 to download, we will use udata2 for the chunk */
	conn->udata1 = dl;

	dl->conns = list_prepend (dl->conns, conn);

	/* check if the source should be used now */
	if (dl->state == DOWNLOAD_ACTIVE)
		download_maintain (dl);

	return TRUE;
}

/*****************************************************************************/

static as_bool disassociate_conn (ASDownConn *conn)
{
	ASDownload *dl = conn->udata1;
	ASDownChunk *chunk = conn->udata2;

	/* disassociate chunk */
	conn->udata2 = NULL;
	chunk->udata = NULL;

	/* remove connection if it failed too often */
	if (conn->fail_count >= AS_DOWNLOAD_SOURCE_MAX_FAIL)
	{
		AS_DBG_3 ("Removing source %s:%d after it failed %d times",
		          net_ip_str (conn->source->host), conn->source->port,
		          conn->fail_count);

		/* remove connection */
		as_downconn_free (conn);
		dl->conns = list_remove (dl->conns, conn);
		return FALSE;
	}

	return TRUE;
}

/* Called for every state change. Return FALSE if the connection was freed and
 * must no longer be accessed.
 */
static as_bool conn_state_cb (ASDownConn *conn, ASDownConnState state)
{
	ASDownload *dl = conn->udata1;
	ASDownChunk *chunk = conn->udata2;

	assert (chunk);
	assert (dl);

	switch (state)
	{
	case DOWNCONN_UNUSED:
		break;

	case DOWNCONN_CONNECTING:
		/* Chunk used by connection may not be in chunk list at this point.
		 * See recalc_chunks. 
		 */
		AS_HEAVY_DBG_4 ("DOWNCONN_CONNECTING: %s:%d for chunk (%u,%u).",
		                net_ip_str (conn->source->host), conn->source->port,
		                chunk->start, chunk->size);
		break;

	case DOWNCONN_TRANSFERRING:
		AS_HEAVY_DBG_4 ("DOWNCONN_TRANSFERRING: chunk (%u,%u) from %s:%d.", chunk->start,
		                chunk->size, net_ip_str (conn->source->host),
		                conn->source->port);
		break;

	case DOWNCONN_COMPLETE:
		/* This happens if remote closes connection or number of requested
		 * bytes are read.
		 */
		assert (chunk->received <= chunk->size);

		AS_HEAVY_DBG_4 ("DOWNCONN_COMPLETE: Chunk (%u,%u), conn %s:%d.",
		                chunk->start, chunk->size,
		                net_ip_str (conn->source->host), conn->source->port);

		disassociate_conn (conn);
		download_maintain (dl);
		break;

	case DOWNCONN_FAILED:
		AS_HEAVY_DBG_4 ("DOWNCONN_FAILED: Chunk (%u,%u), conn %s:%dd.",
		                chunk->start, chunk->size,
		                net_ip_str (conn->source->host), conn->source->port);

		disassociate_conn (conn);
		download_maintain (dl);
		break;

	case DOWNCONN_QUEUED:

		AS_HEAVY_DBG_4 ("DOWNCONN_QUEUED: %s:%d, pos: %d, length: %d.",
		                net_ip_str (conn->source->host), conn->source->port,
		                conn->queue_pos, conn->queue_len);

		disassociate_conn (conn);
		download_maintain (dl);
		break;
	}

	return TRUE;
}

/* Called for every piece of data downloaded. Return FALSE if the connection
 * was freed and must no longer be accessed.
 */
static as_bool conn_data_cb (ASDownConn *conn, as_uint8 *data,
                             unsigned int len)
{
	ASDownload *dl = conn->udata1;
	ASDownChunk *chunk = conn->udata2;
	size_t write_len;

	assert (len > 0);
	assert (chunk);
	assert (dl);
	assert (dl->fp);
	assert (chunk->size - chunk->received <= conn->chunk_size);

	/* seek to correct place in file */
	if (fseek (dl->fp, chunk->start + chunk->received, SEEK_SET) != 0)
	{
		AS_ERR_1 ("Seek failed for download \"%s\". Pausing.", dl->filename);
		as_download_pause (dl);	
		return FALSE;
	}

	/* make sure we are not writing past the chunk end */
	write_len = len;

	if (write_len > chunk->size - chunk->received)
	{
		write_len = chunk->size - chunk->received;
		AS_HEAVY_DBG_1 ("Got more data than needed for chunk, truncated to %u.",
		                len);
	}

	/* write the data */
	if (fwrite (data, 1, write_len, dl->fp) != write_len)
	{
		AS_ERR_1 ("Write failed for download \"%s\". Pausing.", dl->filename);
		as_download_pause (dl);	
		return FALSE;
	}

	chunk->received += write_len;
	assert (chunk->received <= chunk->size);

	/* chunk complete? */
	if (chunk->received == chunk->size)
	{
		AS_HEAVY_DBG_4 ("Chunk (%u,%u) from %s:%d complete.", chunk->start,
		                chunk->size, net_ip_str (conn->source->host),
		                conn->source->port);

		/* Cancel connection if we cannot keep it open because there we got
		 * or requested more data than the chunk needs. Otherwise just leave
		 * and let DOWNCONN_COMPLETE reuse the connection.
		 */
		if (len != write_len ||
		    conn->chunk_start + conn->chunk_size > chunk->start + chunk->size)
		{
			AS_HEAVY_DBG_2 ("Cancelling connection to %s:%d because chunk is"
			                "complete before transfer end.",
			                net_ip_str (conn->source->host),
			                conn->source->port);

			as_downconn_cancel (conn);

			/* disassociate chunk */
			conn->udata2 = NULL;
			chunk->udata = NULL;

			/* clean up / start new connections / etc */
			download_maintain (dl);

			/* don't return TRUE after the connection might have been reused */
			return FALSE;
		}
	}

	return TRUE;
}

/*****************************************************************************/

/* Cancels connections and disassociates them from chunks */
static void stop_all_connections (ASDownload *dl)
{
	List *conn_l;
	ASDownChunk *chunk;
	ASDownConn *conn;

	/* loop through connections so we can also close those which are
	 * persistent but not currently associated with a chunk.
	 */
	for (conn_l = dl->conns; conn_l; conn_l = conn_l->next)
	{
		conn = conn_l->data;

		/* cancel connection */
		as_downconn_cancel (conn);

		/* if there is a chunk disassociate it */
		if ((chunk = conn->udata2))
		{
			conn->udata2 = NULL;
			chunk->udata = NULL;
		}
	}
}

/* Verify chunk list is consistent */
static as_bool verify_chunks (ASDownload *dl)
{
	List *link;
	ASDownChunk *chunk, *next_chunk;

	link = dl->chunks; 

	/* there must always be at least one chunk */
	if (!link)
	{
		AS_ERR ("Chunk list empty.");
		return FALSE;
	}

	while (link)
	{
		chunk = link->data;

		/* chunk cannot have received more than its size  */
		if (chunk->received > chunk->size)
		{
			AS_ERR_2 ("Chunk received more than its size."
			          "size: %u, received: %u",
			          chunk->size, chunk->received);
			return FALSE;
		}

		/* complete chunks must be without connection */
		if (chunk->received == chunk->size && chunk->udata != NULL)
		{
			AS_ERR ("Complete chunk still associated with connection");
			return FALSE;
		}

		/* next chunk must begin at end of this chunk or it must be the end of
		 * the file.
		 */
		if (link->next)
		{
			next_chunk = link->next->data;

			if (chunk->start + chunk->size != next_chunk->start)
			{
				AS_ERR_2 ("Start of next chunk is %u, should be %u.",
				          next_chunk->start, chunk->start + chunk->size);
				return FALSE;
			}
		}
		else
		{
			if (chunk->start + chunk->size != dl->size)
			{
				AS_ERR_2 ("Last chunk ends at %u but file size is %u",
				          chunk->start + chunk->size, dl->size);
				return FALSE;
			}
		}

		link = link->next;
	}

	return TRUE;
}

/* Merge complete chunks. */
static as_bool merge_chunks (ASDownload *dl)
{
	List *link, *last_link;
	ASDownChunk *chunk, *last_chunk;

	last_link = dl->chunks; 
	link = last_link->next;

	while (link)
	{
		chunk = link->data;
		last_chunk = last_link->data;

		if (chunk->received == chunk->size &&
		    last_chunk->received == last_chunk->size)
		{	
			assert (chunk->udata == NULL);
			assert (last_chunk->udata == NULL);

			AS_HEAVY_DBG_5 ("Merging chunks (%u,%u) and (%u,%u) of \"%s\"",
			                last_chunk->start, last_chunk->size,
			                chunk->start, chunk->size, dl->filename);

			/* increase last chunk by size of chunk */
			last_chunk->size += chunk->size;
			last_chunk->received = last_chunk->size;

			/* free chunk and remove link */
			as_downchunk_free (chunk);
			last_link = list_remove_link (last_link, link);

			/* adjust link so loop can continue */
			link = last_link;
		}

		last_link = link;
		link = link->next;
	}

	return TRUE;
}

/* Assing new connections to unused chunks. Split large chunks if there are
 * unused connections.
 */
static as_bool recalc_chunks (ASDownload *dl)
{
	List *chunk_l;
	List *conn_l, *tmp_l;
	ASDownChunk *chunk, *new_chunk;
	ASDownConn *conn;
	time_t now = time (NULL);
	size_t remaining, new_start, new_size;

	if (!(conn_l = dl->conns))
		return TRUE; /* nothing to do */

	/* First assign connections to unused chunks */

	for (chunk_l = dl->chunks; chunk_l; chunk_l = chunk_l->next)
	{
		chunk = chunk_l->data;

		/* skip active and complete chunks */
		if (chunk->udata || chunk->received == chunk->size)
			continue;
		
		/* find a connection for this unused chunk */
		while (conn_l)
		{
			conn = conn_l->data;

			if (!conn->udata2 &&
			    (conn->state != DOWNCONN_QUEUED || conn->queue_next_try <= now))
			{
				/* associate chunk with connection */
				chunk->udata = conn;
				conn->udata2 = chunk;

				/* use the connection we found with this chunk */
				if (!as_downconn_start (conn, dl->hash,
				                        chunk->start + chunk->received,
				                        chunk->size - chunk->received))
				{
					/* download start failed, remove connection. */
					AS_DBG_2 ("Failed to start download from %s:%d, removing source.",
					          net_ip_str (conn->source->host),
					          conn->source->port);

					as_downconn_free (conn);
					chunk->udata = NULL;

					tmp_l = conn_l->next;
					dl->conns = list_remove_link (dl->conns, conn_l);
					conn_l = tmp_l;

					/* try next connection */
					continue;
				}

				AS_HEAVY_DBG_5 ("Started unused chunk (%u,%u) of \"%s\" with %s:%d",
			                    chunk->start, chunk->size, dl->filename,
				                net_ip_str (conn->source->host),
				                conn->source->port);

				/* move on to next chunk */
				break;
			}

			conn_l = conn_l->next;
		}

		if (!conn_l)
			break; /* no more suitable connections */
	}

	/* Now loop through remaining connections and create new chunks for them
	 * by splitting large ones.
	 */
	while (conn_l)
	{
		conn = conn_l->data;

		/* skip active and queued connections */
		if (conn->udata2 ||
		    (conn->state == DOWNCONN_QUEUED && conn->queue_next_try > now))
		{
			conn_l = conn_l->next;
			continue;
		}

		/* Find chunk with largest remaining size */
		remaining = 0;
		tmp_l = NULL;
		for (chunk_l = dl->chunks; chunk_l; chunk_l = chunk_l->next)
		{
			chunk = chunk_l->data;
			if (chunk->size - chunk->received > remaining)
			{
				remaining = chunk->size - chunk->received;
				tmp_l = chunk_l;
			}
		}

		chunk_l = tmp_l;

		if (!chunk_l || remaining <= AS_DOWNLOAD_MIN_CHUNK_SIZE * 2)
		{
			/* No more chunks to break up */
			break;
		}

		/* break up this chunk in middle of remaining size */
		chunk = chunk_l->data;
		new_size = (chunk->size - chunk->received) / 2;
		new_start = chunk->start + chunk->size - new_size;

		if (!(new_chunk = as_downchunk_create (new_start, new_size)))
		{
			/* Nothing we can do but bail out. Overall state should still be
			 * consistent and other connections and chunks can go on.
			 */
			AS_ERR_2 ("Couldn't create chunk (%u,%u)", new_start, new_size);
			return FALSE;						
		}

		/* associate new chunk with connection */
		new_chunk->udata = conn;
		conn->udata2 = new_chunk;

		/* Start new connection */
		if (!as_downconn_start (conn, dl->hash,
		                        new_chunk->start + new_chunk->received,
		                        new_chunk->size - new_chunk->received))
		{
			/* download start failed, remove connection. */
			AS_DBG_2 ("Failed to start download from %s:%d, removing source.",
			          net_ip_str (conn->source->host),
			          conn->source->port);

			as_downchunk_free (new_chunk);
			as_downconn_free (conn);

			tmp_l = conn_l->next;
			dl->conns = list_remove_link (dl->conns, conn_l);
			conn_l = tmp_l;

			/* try next connection */
			continue;
		}

		AS_HEAVY_DBG_5 ("Started new chunk (%u,%u) of \"%s\" with %s:%d",
	                    new_chunk->start, new_chunk->size, dl->filename,
		                net_ip_str (conn->source->host),
		                conn->source->port);

		/* Shorten old chunk. */
		chunk->size -= new_size;

		AS_HEAVY_DBG_4 ("Reduced old chunk from (%u,%u) to (%u,%u)",
	                    chunk->start, chunk->size + new_size,
	                    chunk->start, chunk->size);

		/* Insert new chunk after old one */
		tmp_l = list_prepend (NULL, new_chunk);
		tmp_l->next = chunk_l->next;
		tmp_l->prev = chunk_l;
		chunk_l->next = tmp_l;
		if (tmp_l->next)
			tmp_l->next->prev = tmp_l;

		/* go on with next connection */
		conn_l = conn_l->next;
	}

	return TRUE;
}

/*
 * The heart of the download system. It is called whenever there are new
 * sources, finished chunks, etc.
 * Merges complete chunks and tries to assign sources to inactive ones. If 
 * there are more sources than chunks the chunks are split up. If the minimum
 * chunk size is reached faster sources are prefered.
 */
static void download_maintain (ASDownload *dl)
{
	if (dl->state != DOWNLOAD_ACTIVE)
	{
		/* Must not happen. */
		assert (dl->state == DOWNLOAD_ACTIVE);
		return;
	}

	/* Verify integrity of chunk list. */
	if (!verify_chunks (dl))
	{
		AS_ERR_1 ("Corrupted chunk list detected for \"%s\"", dl->filename);
		
		/* Fail download */
		download_failed (dl);

		assert (0);
		return;
	}

	/* Merge complete chunks. */
	if (!merge_chunks (dl))
	{
		AS_ERR_1 ("Merging chunks failed for \"%s\"", dl->filename);
		
		/* Fail download */
		download_failed (dl);

		assert (0);
		return;
	}

#if 1
	{
	List *l;
	AS_HEAVY_DBG ("Chunk state after merging:");
	for (l = dl->chunks; l; l = l->next)
	{
		ASDownChunk *chunk = l->data;
		ASDownConn *conn = chunk->udata;

		AS_HEAVY_DBG_5 ("Chunk: (%7u, %7u, %7u) conn %15s %s",
		                chunk->start, chunk->size, chunk->received,
						conn ? net_ip_str (conn->source->host) : "-",
		                (chunk->received == chunk->size) ? " complete" : "");
	}
	}
#endif

	/* Is the download complete? */
	if (((ASDownChunk *)dl->chunks->data)->received == dl->size)
	{
		/* Download complete */
		download_finished (dl);
		return;
	}

	/* Download not complete. Start more chunk downloads. */
	if (!recalc_chunks (dl))
	{
		/* This should be harmless. */
		AS_WARN_1 ("Recalculating chunks failed for \"%s\"", dl->filename);
	}

	/* Check if we need more sources */
	if (dl->conns == NULL)
	{
		/* TODO: start source search  */
		AS_ERR_1 ("FIXME: No more sources for \"%s\". Make me find more.",
		          dl->filename);
	}
}

/*****************************************************************************/

static as_bool download_failed (ASDownload *dl)
{
	AS_DBG_1 ("Failed download \"%s\"", dl->filename);

	/* Stop all chunk downloads if there are still any */
	stop_all_connections (dl);

	/* close fd */
	if (dl->fp)
	{
		fclose (dl->fp);
		dl->fp = NULL;
	}

	/* delete incomplete file. */
#ifndef KEEP_FAILED
	if (unlink (dl->filename) < 0)
		AS_ERR_1 ("Failed to unlink incomplete file \"%s\"", dl->filename);
#else
	AS_WARN_1 ("Keeping failed download \"%s\" for debugging.", dl->filename);
#endif

	/* raise callback */
	if (!download_set_state (dl, DOWNLOAD_FAILED, TRUE))
		return FALSE;

	return TRUE;
}

static as_bool download_complete (ASDownload *dl)
{
	AS_DBG_1 ("Completed download \"%s\"", dl->filename);

	/* close fd */
	if (dl->fp)
	{
		fclose (dl->fp);
		dl->fp = NULL;
	}

	/* rename complete file to not include the prefix. */
	if (strncmp (dl->filename, INCOMPLETE_PREFIX, strlen (INCOMPLETE_PREFIX)) == 0)
	{
		char *completed_name = dl->filename + strlen (INCOMPLETE_PREFIX);
		char *new_name = strdup (completed_name);
		int i = 0;
		struct stat st;

		/* find a free filename and rename to that */
		while (1)
		{
			/* rename file if name is available */
			if (stat (new_name, &st) < 0)
			{
				if (rename (dl->filename, new_name) >= 0)
				{
					AS_DBG_2 ("Moved complete file \"%s\" to \"%s\"",
					          dl->filename, new_name);

					/* update download filename */
					free (dl->filename);
					dl->filename = new_name;
					break;
				}
			}

			free (new_name);

			if (++i == 100)
			{
				AS_ERR_2 ("Renaming of \"%s\" still failed after %d tries",
				          dl->filename, i);
				break;
			}

			new_name = stringf_dup ("%s.%d", completed_name, i);
		}
	}
	else
	{
		AS_WARN_1 ("Complete file \"%s\" has no prefix. No renaming performed.",
		           dl->filename);
	}

	/* raise callback */
	if (!download_set_state (dl, DOWNLOAD_COMPLETE, TRUE))
		return FALSE;

	return TRUE;
}

/* Checks finished transfer. */
static as_bool download_finished (ASDownload *dl)
{
	ASHash *hash;
#ifdef WIN32
	int fd;
#endif

	AS_DBG_1 ("Verifying download \"%s\"", dl->filename);

	/* Do some sanity checks */
	assert (dl->chunks->next == NULL);
	assert (((ASDownChunk *)dl->chunks->data)->udata == NULL);
	assert (((ASDownChunk *)dl->chunks->data)->size == dl->size);
	assert (dl->fp != NULL);

	/* Close file pointer */
	fclose (dl->fp);
	dl->fp = NULL;

	/* raise callback */
	if (!download_set_state (dl, DOWNLOAD_VERIFYING, TRUE))
		return FALSE;

	/* Truncate incomplete file to correct size removing the state data at
	 * the end.
	 */
#ifndef WIN32
	if (truncate (dl->filename, dl->size) < 0)
#else
	if ((fd = _open (dl->filename, _O_BINARY | _O_WRONLY)) < 0 ||
	    _chsize (fd, dl->size) != 0 ||
	    _close (fd) != 0)
#endif
	{
		AS_ERR_1 ("Failed to truncate complete download \"%s\"",
		          dl->filename);
		/* File is probably still useful so continue. */
	}

	/* Hash file and compare hashes.
	 * TODO: Make non-blocking.
	 */
	if (!(hash = as_hash_file (dl->filename)))
	{
		AS_ERR_1 ("Couldn't hash \"%s\" for verification", dl->filename);
		return download_failed (dl);
	}

	if (!as_hash_equal (dl->hash, hash))
	{
		AS_ERR_1 ("Downloaded file \"%s\" corrupted!", dl->filename);
		as_hash_free (hash);
		return download_failed (dl);
	}

	as_hash_free (hash);

	/* Download is OK */
	return download_complete (dl);
}

/*****************************************************************************/

