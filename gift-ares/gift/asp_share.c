/*
 * $Id: asp_share.c,v 1.3 2004/12/04 15:30:46 mkern Exp $
 *
 * Copyright (C) 2003 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "asp_plugin.h"

/*****************************************************************************/

void meta_from_gift (ds_data_t *key, ds_data_t *value, ASMeta *meta)
{
	as_bool ret;

	if (!STRCASECMP (key->data, "bitrate") && value->data)
	{
		char *val = strdup (value->data);
		int len = strlen (val);
		
		if (len > 3)
			val[len - 3] = '\0';
		
		ret = as_meta_add_tag (meta, key->data, val);
		
		free (val);
	}
	else
		ret = as_meta_add_tag (meta, key->data, value->data);

	assert (ret);
}

/* FIXME? */
static List *sharelist = NULL;
static timer_id share_timer = INVALID_TIMER;

static BOOL submit_shares (List **list)
{
	assert (list_verify_integrity (*list, TRUE));

	if (!as_shareman_add_and_submit (AS->shareman, *list))
	{
		AS_ERR_1 ("Failed to submit %d shares from list cache.",
		          list_length (*list));
	}

	list_free (*list);
	*list = NULL;

	share_timer = INVALID_TIMER;

	return FALSE; /* Remove timer. */
}

/*****************************************************************************/

/* Called by giFT so we can add custom data to shares. */
void *asp_giftcb_share_new (Protocol *p, Share *share)
{
	return NULL;
}

/* Called be giFT for us to free custom data. */
void asp_giftcb_share_free (Protocol *p, Share *share, void *data)
{
	assert (!share_get_udata (share, PROTO->name));
}

/* Called by giFT when share is added. */
BOOL asp_giftcb_share_add (Protocol *p, Share *share, void *data)
{
	ASShare *ashare;
	ASMeta *meta;
	ASRealm realm;
	Hash *hash;
	ASHash *ashash;

	/* Get realm from file extension like ares does. */
	realm = as_meta_realm_from_filename (share->path);
	
	/* Ares never shares unknown files. */
	if (realm == REALM_UNKNOWN)
		return FALSE;

	/* Convert meta data from giFT. */
	if (!(meta = as_meta_create ()))
		return FALSE;

	share_foreach_meta (share, (DatasetForeachFn)meta_from_gift, meta);

	/* Get hash from giFT's share object */
	if (!(hash = share_get_hash (share, "SHA1")) ||
		!(ashash = as_hash_create (hash->data, AS_HASH_SIZE)))
	{
		AS_ERR_1 ("Couldn't get hash from share '%s'.", share->path);
		as_meta_free (meta);
		return FALSE;
	}

	/* Create ares share object. */
	if (!(ashare = as_share_create (share->path, ashash, meta, share->size, realm)))
	{
		AS_ERR_1 ("Couldn't create ares share object for '%s'.", share->path);
		as_meta_free (meta);
		as_hash_free (ashash);
		return FALSE;
	}

	/* Associate ares share with giFT and vice versa.
	 * FIXME: submit_share will free share objects with duplicate hashes later
	 *        without notice.
	 */
	assert (!share_get_udata (share, PROTO->name));
	share_set_udata (share, PROTO->name, ashare);
	ashare->udata = share;

	/* Add share to list and commit to ares library using a timer. */
	sharelist = list_prepend (sharelist, ashare);

	if (share_timer != INVALID_TIMER)
		timer_reset (share_timer);
	else
		share_timer = timer_add (15 * SECONDS, (TimerCallback) submit_shares,
		                         &sharelist);

	return TRUE;
}

/* Called by giFT when share is removed. */
BOOL asp_giftcb_share_remove (Protocol *p, Share *share, void *data)
{
	Hash *hash;
	ASHash *ashash;
	ASShare *ashare;

	/* FIXME: There is no need for this to hold. */
	assert (share_timer == INVALID_TIMER);

	/* Get hash from giFT's share object */
	if (!(hash = share_get_hash (share, "SHA1")) ||
		!(ashash = as_hash_create (hash->data, AS_HASH_SIZE)))
	{
		AS_ERR_1 ("Couldn't get hash from share '%s'.", share->path);
		return FALSE;
	}

	/* Lookup ares share. */
	if (!(ashare = as_shareman_lookup (AS->shareman, ashash)))
	{
		AS_ERR_1 ("Share '%s' not found for removal.", share->path);
		as_hash_free (ashash);
		return FALSE;
	}

	/* FIXME: giFT allows multiple shares with same hash. */
	if (ashare->udata != share)
	{
		AS_ERR ("FIXME: Hash collision on share removal.");
		as_hash_free (ashash);
		return FALSE;
	}

	/* Remove share. */
	if (!as_shareman_remove (AS->shareman, ashash))
	{
		AS_ERR_1 ("Failed to remove share '%s'.", share->path);
		as_hash_free (ashash);
		return FALSE;
	}

	as_hash_free (ashash);

	/* Disassociate from giFT share. */
	share_set_udata (share, PROTO->name, NULL);

	return TRUE;
}

/* Called by giFT when it starts/ends syncing shares. */
void asp_giftcb_share_sync (Protocol *p, int begin)
{

}

/* Called by giFT when user hides shares. */
void asp_giftcb_share_hide (Protocol *p)
{

}

/* Called by giFT when user shows shares. */
void asp_giftcb_share_show (Protocol *p)
{

}

/*****************************************************************************/
