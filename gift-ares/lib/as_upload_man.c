/*
 * $Id: as_upload_man.c,v 1.1 2004/10/19 23:41:48 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

typedef struct {
	ASHashTable *uploads;
	List *queue;
	
	int max;
	int nuploads;
	int nqueued;
	int bandwidth;
} ASUploadMan;

struct queue {
	in_addr_t host;
	time_t    time;
};

ASUploadMan as_upload_man_new (void)
{
	ASUploadMan *man = malloc (sizeof (ASUploadMan));

	if (!man)
		return NULL;

	man->uploads = as_hashtable_create_int ();
	man->queue   = NULL;

	man->max = AS_MAX_UPLOADS;
	man->nuploads = man->nqueued = 0;
	man->bandwidth = 0;

	return man;
}

as_bool free_upload (ASHashTableEntry *entry, void *udata)
{
	as_upload_free (entry->val);

	return TRUE; /* remove */
}

ASUploadMan as_upload_man_free (ASUploadMan *man)
{
	as_hashtable_foreach (man->uploads, (ASHashTableForeachFunc)free_upload,
                           NULL);

	as_hashtable_free (man->uploads, FALSE);
	list_foreach_remove (man->queue, NULL, NULL); /* free entries */
	list_free (man->queue);
	
	free (man);
}

/* returns zero for OK, -1 for not, or queue position */
int as_upload_man_auth (ASUploadMan *man, in_addr_t host)
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
		q = malloc (sizeof struct queue);

		if (!q)
			return -1;

		q->host = host;
		q->time = time ();
		man->queue = list_append (man->queue, q);

		return ++man->nqueued;
	}

	if (!l->prev)
	{
		/* first in queue: pop it */
		free (l->data);
		man->queue = list_remove_link (l);
		man->nqueued--;
		
		return 0;
	}
	
	q->time = time ();

	return i;
}

