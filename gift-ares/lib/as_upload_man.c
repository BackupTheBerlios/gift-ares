/*
 * $Id: as_upload_man.c,v 1.7 2004/10/30 18:28:30 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

struct queue
{
	in_addr_t host;
	time_t    time;
};

/* Internal Queue system if no external auth callback was set. Returns zero
 * for OK, -1 for queued without position, or queue position.
 */
static int upman_auth (ASUpMan *man, in_addr_t host);

/*****************************************************************************/

static as_bool upload_state_cb (ASUpload *up, ASUploadState state);
static int upload_auth_cb (ASUpload *up, int *queue_length);

/* Add or Remove progress timer as necessary. */
static void progress_timer_update (ASUpMan *man);
/* Periodic timer function which in turn calls progress callback. */
static as_bool progress_timer_func (ASUpMan *man);

/*****************************************************************************/

/* Create upload manager. */
ASUpMan *as_upman_create ()
{
	ASUpMan *man;

	if (!(man = malloc (sizeof (ASUpMan))))
		return NULL;

	if (!(man->uploads = as_hashtable_create_int ()))
	{
		free (man);
		return NULL;
	}

	man->queue   = NULL;

	man->max_active = AS_UPLOAD_MAX_ACTIVE;
	man->nuploads = man->nqueued = 0;
	man->bandwidth = 0;

	man->state_cb = NULL;
	man->auth_cb = NULL;
	man->progress_cb = NULL;
	man->progress_timer = INVALID_TIMER;

	return man;
}

static as_bool free_upload (ASHashTableEntry *entry, void *udata)
{
	as_upload_free (entry->val);
	return TRUE; /* remove */
}

/* Free upload manager and stop all uploads. */
void as_upman_free (ASUpMan *man)
{
	if (!man)
		return;

	if (man->progress_timer != INVALID_TIMER)
		timer_remove (man->progress_timer);

	as_hashtable_foreach (man->uploads, (ASHashTableForeachFunc)free_upload,
	                      NULL);

	as_hashtable_free (man->uploads, FALSE);
	list_foreach_remove (man->queue, NULL, NULL); /* free entries */
	list_free (man->queue);
	
	free (man);
}

/* Set callback triggered for every state change in one of the uploads. */
void as_upman_set_state_cb (ASUpMan *man, ASUpManStateCb state_cb)
{
	man->state_cb = state_cb;	
}

/* Set callback before every upload to authorize transfer. */
void as_upman_set_auth_cb (ASUpMan *man, ASUpManAuthCb auth_cb)
{
	man->auth_cb = auth_cb;
}

/* Set callback triggered periodically for progress updates. */
void as_upman_set_progress_cb (ASUpMan *man,
                               ASUpManProgressCb progress_cb)
{
	if (progress_cb == man->progress_cb)
		return;

	man->progress_cb = progress_cb;
	progress_timer_update (man);
}

/*****************************************************************************/

/* Create and register a new upload from http request. Takes ownership of
 * connection and request in all cases (even if no download is created).
 */
ASUpload *as_upman_start (ASUpMan *man, TCPC *c, ASHttpHeader *req)
{
	ASUpload *up;

	/* Create upload object. */
	if (!(up = as_upload_create (c, req, upload_state_cb, upload_auth_cb)))
	{
		AS_ERR_1 ("Couldn't create upload for request from %s",
		          net_ip_str (c->host));
		tcp_close (c);
		as_http_header_free (req);
		return NULL;
	}

	up->upman = man;

	/* Insert into hash table first so it is available in callback triggered
	 * by as_upload_start.
	 */
	if (!as_hashtable_insert_int (man->uploads, (as_uint32)up->host, up))
	{
		AS_ERR_1 ("Failed to insert upload to %s in hash table",
		          net_ip_str (up->host));
		as_upload_free (up);
		return NULL;
	}

	/* Try to start upload. Auth callback will decide if this succeeds */
	if (!as_upload_start (up))
	{
		/* Upload was not started. Failed/queued/404/etc */
		if (!as_hashtable_remove_int (man->uploads, (as_uint32)(up->host)))
		{
			AS_WARN_1 ("Failed to remove unstarted upload from hash table",
			           net_ip_str (up->host));
			assert (0);
		}

		as_upload_free (up);
		return NULL;
	}

	return up;
}

/*****************************************************************************/

static as_bool valid_upload_itr (ASHashTableEntry *entry, ASUpload **up)
{
	if (*up == entry->val)
		*up = NULL;

	return FALSE; /* don't delete item */
}

/* Returns TRUE if upload is currently managed by upman */
static as_bool upman_valid_upload (ASUpMan *man, ASUpload *up)
{
	/* check if upload is in the hash table */
	as_hashtable_foreach (man->uploads,
	                      (ASHashTableForeachFunc)valid_upload_itr, &up);
	if (up == NULL)
		return TRUE;

	return FALSE;
}

/* Cancel upload but do not remove it. */
as_bool as_upman_cancel (ASUpMan *man, ASUpload *up)
{
	if (!upman_valid_upload (man, up))
		return FALSE;

	/* Callback will decrement man->nuploads. */
	return as_upload_cancel (up); /* triggers callback */
}

/* Remove and free finished, failed or cancelled upload. */
as_bool as_upman_remove (ASUpMan *man, ASUpload *up)
{
	if (!upman_valid_upload (man, up))
		return FALSE;

	if (!as_hashtable_remove_int (man->uploads, (as_uint32)up->host))
	{
		AS_ERR_1 ("Couldn't remove upload to %s from hashtable",
		          net_ip_str (up->host));
		assert (0);
	}

	if (as_upload_state (up) == UPLOAD_ACTIVE)
	{
		man->nuploads--;
		assert (man->uploads >= 0);
		progress_timer_update (man);
	}

	as_upload_free (up);

	return TRUE;
}

/* Return state of upload. The advantage to as_upload_state is that this
 * makes sure the upload is actually still in the list and thus valid
 * before accessing it. If the upload is invalid UPLOAD_INVALID is
 * returned.
 */
ASUploadState as_upnman_state (ASUpMan *man, ASUpload *up)
{
	if (!upman_valid_upload (man, up))
		return UPLOAD_INVALID;

	return as_upload_state (up);
}

/*****************************************************************************/

static as_bool upload_state_cb (ASUpload *up, ASUploadState state)
{
	ASUpMan *man = up->upman;
	int ret = TRUE;

	switch (state)
	{
	case UPLOAD_FAILED:
	case UPLOAD_QUEUED:
		/* Don't forward any of these to upman callback since they mean the
		 * upload was never created.
		 */
		break;

	case UPLOAD_ACTIVE:
		/* New active upload */
		man->nuploads++;
		progress_timer_update (man);

		/* Raise callback */
		if (man->state_cb)
			ret = man->state_cb (man, up, state);
		break;

	case UPLOAD_COMPLETE:
	case UPLOAD_CANCELLED:
		/* One active upload less */
		man->nuploads--;
		assert (man->nuploads >= 0);
		progress_timer_update (man);

		/* Raise callback */
		if (man->state_cb)
			ret = man->state_cb (man, up, state);

#if 0
		/* And free download if it still exists.
		 * FIXME: Is it a good idea to automatically do this for the user
		 *        here?
		 */
		if (ret)
		{
			as_upman_remove (man, up);
			ret = FALSE;
		}
#endif
		break;

	default:
		abort ();
	}

	return ret;
}

static int upload_auth_cb (ASUpload *up, int *queue_length)
{
	ASUpMan *man = up->upman;
	int pos = 0;
	int length = 0;

	if (man->auth_cb)
	{
		pos = man->auth_cb (man, up, &length);
	}
	else
	{
		/* Use internal queue system. */
		pos = upman_auth (man, up->host);
		length = man->nqueued;
	}

	AS_DBG_3 ("Auth status for '%s': pos: %d, length: %d",
	          net_ip_str (up->host), pos, length);

	*queue_length = length;
	return pos;
}

/*****************************************************************************/

/* Add or Remove progress timer as necessary. */
static void progress_timer_update (ASUpMan *man)
{
	if (man->progress_cb && 
	    man->nuploads > 0 &&
	    man->progress_timer == INVALID_TIMER)
	{
		man->progress_timer = timer_add (AS_UPLOAD_PROGRESS_INTERVAL,
		                                 (TimerCallback)progress_timer_func,
		                                 man);
	}
	else if (man->progress_timer != INVALID_TIMER)
	{
		timer_remove_zero (&man->progress_timer);
	}
}

/* Periodic timer function which in turn calls progress callback. */
static as_bool progress_timer_func (ASUpMan *man)
{
	assert (man->progress_cb);
	assert (man->nuploads > 0);

	/* raise callback */
	man->progress_cb (man);

	return TRUE; /* reset timer */
}

/*****************************************************************************/

static void queue_remove (ASUpMan *man, List *link)
{
	free (link->data);
	man->queue = list_remove_link (man->queue, link);
	man->nqueued--;
	
	assert (man->nqueued >= 0);
}

/* Tidy up the queue, removing stale entries to avoid whoever's at the
 * head of the queue having to wait too long. A "last entry" may be
 * specified to ignore beyond, so that we can avoid timing out entries
 * if later entries haven't pinged us yet.
 */
static void tidy_queue (ASUpMan *man, struct queue *last)
{
	List *l, *next;
	time_t t = time (NULL);

	for (l = man->queue; l; l = next)
	{
		struct queue *q = l->data;
		
		next = l->next; /* to avoid referencing freed data
		                 * should we choose to remove this
		                 * entry */
 
		if (q == last)
			break;

		if (t - q->time > AS_UPLOAD_QUEUE_TIMEOUT)
		{
			/* it's stale, remove it */
			AS_DBG_2 ("Removing stale queue entry %s (%d elapsed)",
			          net_ip_str (q->host), t - q->time);
			queue_remove (man, l);
		}
	}
}

/* Returns zero for OK, -1 for not, or queue position. */
static int upman_auth (ASUpMan *man, in_addr_t host)
{
	ASUpload *up;
	List *l;
	struct queue *q;
	int i;

	if ((up = as_hashtable_lookup_int (man->uploads, (as_uint32)host)))
	{
		/* There may be non active downloads in the hash table. Specifically
		 * the download this auth request is for is already in the list.
		 */
		if (as_upload_state (up) == UPLOAD_ACTIVE)
		{
			AS_DBG_1 ("currently uploading to %s, denying",
			          net_ip_str (host));
			/* If this host is currently uploading, make it wait. */
			return -1;
		}
	}

	/* Spare slots are available even after dealing with everyone
	 * in the queue.
	 */
	if (man->nuploads + man->nqueued <= man->max_active)
	{
		AS_DBG_3 ("spare slots available (%d+%d < %d), allowing",
		          man->nuploads, man->nqueued, man->max_active);
		return 0;
	}

	/* tidy up the queue, then see if there's anyone before us */
	tidy_queue (man, NULL);

	for (l = man->queue, i = 1; l; l = l->next, i++)
	{
		q = l->data;
		if (q->host == host)
			break;
	}

	assert (list_length (man->queue) == man->nqueued);

	AS_HEAVY_DBG_1 ("queue pos is %d", i);

	if (!l)
	{
		/* not queued; insert at end */
		q = malloc (sizeof(struct queue));

		if (!q)
			return -1;

		q->host = host;
		q->time = time (NULL);
		man->queue = list_append (man->queue, q);

		man->nqueued++;
		assert (i == man->nqueued);
	}

	if (i + man->nuploads <= man->max_active)
	{
		/* sufficiently near the front of the queue: pop it */
		AS_DBG_3 ("Reserved slot available (%d+%d <= %d), allowing",
		          i, man->nuploads, man->max_active);

		queue_remove (man, l);
		return 0;
	}
	
	q->time = time (NULL);

	return i;
}

/*****************************************************************************/