/*
 * $Id: as_upload_man.c,v 1.2 2004/10/24 03:45:59 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

struct queue {
	in_addr_t host;
	time_t    time;
};

ASUploadMan *as_upman_create (void)
{
	ASUploadMan *man = malloc (sizeof (ASUploadMan));

	if (!man)
		return NULL;

	man->uploads = as_hashtable_create_int ();
	man->queue   = NULL;

	man->max = AS_UPLOAD_MAX_ACTIVE;
	man->nuploads = man->nqueued = 0;
	man->bandwidth = 0;

	return man;
}

static as_bool free_upload (ASHashTableEntry *entry, void *udata)
{
	as_upload_free (entry->val);

	return TRUE; /* remove */
}

void as_upman_free (ASUploadMan *man)
{
	as_hashtable_foreach (man->uploads, (ASHashTableForeachFunc)free_upload,
                           NULL);

	as_hashtable_free (man->uploads, FALSE);
	list_foreach_remove (man->queue, NULL, NULL); /* free entries */
	list_free (man->queue);
	
	free (man);
}

static void queue_remove (ASUploadMan *man, List *link)
{
	free (link->data);
	man->queue = list_remove_link (man->queue, link);
	man->nqueued--;
	
	assert (man->nqueued);
} 

/* Tidy up the queue, removing stale entries to avoid whoever's at the
 * head of the queue having to wait too long. A "last entry" may be
 * specified to ignore beyond, so that we can avoid timing out entries
 * if later entries haven't pinged us yet.
 */
static void tidy_queue (ASUploadMan *man, struct queue *last)
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

		if (q->time + AS_UPLOAD_QUEUE_TIMEOUT < t)
			continue;

		/* it's stale, remove it */
		queue_remove (man, l);
	}
}

/* returns zero for OK, -1 for not, or queue position */
int as_upman_auth (ASUploadMan *man, in_addr_t host)
{
	List *l;
	struct queue *q;
	int i;

	if (as_hashtable_lookup_int (man->uploads, (int)host))
	{
		/* if this host is currently uploading, make it wait */
		return -1;
	}

	if (man->nuploads < man->max)
		return 0;

	for (l = man->queue, i = 0; l; l = l->next, i++)
	{
		q = l->data;
		if (q->host == host)
			break;
	}

	if (!l)
	{
		/* not queued; insert at end */
		q = malloc (sizeof(struct queue));

		if (!q)
			return -1;

		q->host = host;
		q->time = time (NULL);
		man->queue = list_append (man->queue, q);

		return ++man->nqueued;
	}

	/* tidy up the queue, then see if there's anyone before us */
	tidy_queue (man, q);

	if (!l->prev)
	{
		/* first in queue: pop it */
		queue_remove (man, l);
		return 0;
	}
	
	q->time = time (NULL);

	return i;
}

static as_bool upload_callback (ASUpload *up, ASUploadState state)
{
	ASUploadMan *man = AS->upman;

	switch (state)
	{
	case UPLOAD_COMPLETED:
	case UPLOAD_CANCELLED:
	{
		void *ret;
		ret = as_hashtable_remove_int (man->uploads, (int)(up->c->host));
		assert (ret);
		man->nuploads--;

		assert (man->uploads >= 0);
		break;
	}
	default:
		break;
	}

	return TRUE;
}

ASUpload *as_upman_start (ASUploadMan *man, TCPC *c, ASShare *share, 
			  ASHttpHeader *req)
{
	ASUpload *up = as_upload_new (c, share, req,
				      (ASUploadStateCb)upload_callback);

	if (!up)
		return NULL;

	as_hashtable_insert_int (man->uploads, (int)(c->host), up);

	man->nuploads++;

	return up;
}
