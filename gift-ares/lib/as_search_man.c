/*
 * $Id: as_search_man.c,v 1.3 2004/09/09 22:25:31 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* allocate and init search manager */
ASSearchMan *as_searchman_create ()
{
	ASSearchMan *man;

	if (!(man = malloc (sizeof (ASSearchMan))))
		return NULL;

	if (!(man->searches = as_hashtable_create_int ()))
	{
		free (man);
		return NULL;
	}

	if (!(man->hash_searches = as_hashtable_create_mem (TRUE)))
	{
		as_hashtable_free (man->searches, FALSE);
		free (man);
		return NULL;
	}

	man->next_search_id = INVALID_SEARCH_ID + 1;

	return man;
}

static as_bool search_free_itr (ASHashTableEntry *entry, void *udata)
{
	as_search_free ((ASSearch*) entry->val);
	return TRUE; /* remove entry */
}

/* free manager */
void as_searchman_free (ASSearchMan *man)
{
	if (!man)
		return;

	/* free all searches */
	as_hashtable_foreach (man->searches, search_free_itr, NULL);
	as_hashtable_free (man->searches, FALSE);
	as_hashtable_free (man->hash_searches, FALSE);
	free (man);
}

/*****************************************************************************/

/* handle result packet */
as_bool as_searchman_result (ASSearchMan *man, ASSession *session,
                             ASPacket *packet)
{
	ASResult *result;
	ASSearch *search;
	
	/* parse packet */
	if (!(result = as_result_parse (packet)))
		return FALSE;

	/* add supernode this came from to result source */
	result->source->parent_host = session->host;
	result->source->parent_port = session->port;

	if (result->search_id == INVALID_SEARCH_ID)
	{
		/* hash search, look search up by hash */
		if (!(search = as_searchman_lookup_hash (man, result->hash)))
		{
			char *hash_str = as_hash_encode (result->hash);
			AS_DBG_3 ("No search found for result with hash %s from %s:%d",
			          hash_str, net_ip_str (session->host), session->port);
			free (hash_str);
			as_result_free (result);
			return FALSE;
		}
	}
	else
	{
		/* normal search, look search up by id */
		if (!(search = as_searchman_lookup (man, result->search_id)))
		{
			AS_DBG_3 ("No search found for result with id %d from %s:%d",
			          result->search_id, net_ip_str (session->host),
			          session->port);
			as_result_free (result);
			return FALSE;
		}
	}

	/* add result to search if not stopped */
	if (!search->finished)
	{
		/* this also raises the result callback */
		as_search_add_result (search, result);
	}

	return TRUE;
}

/*****************************************************************************/

static as_bool send_search_itr (ASSession *session, ASSearch *search)
{
	if (search->sent >= AS_SEARCH_SEND_COUNT)
		return FALSE;

	if (!as_search_send (search, session))
		return FALSE;

	return TRUE;
}

/* create and run new query search */
ASSearch *as_searchman_search (ASSearchMan *man, ASSearchResultCb result_cb,
                               const char *query, ASRealm realm)
{
	ASSearch *search;
	int count;

	/* create search */
	if (!(search = as_search_create (man->next_search_id, result_cb, query,
	                                 realm)))
	{
		return NULL;
	}

	/* insert into search table */
	if (!as_hashtable_insert_int (man->searches, search->id, search))
	{
		AS_ERR ("Hashtable insert failed for new search");
		as_search_free (search);

		assert (0);
		return NULL;
	}

	/* we actually used this id, go to next */
	man->next_search_id++;

	/* send search to all connected supernodes */
	count = as_sessman_foreach (AS->sessman,
	                            (ASSessionForeachFunc)send_search_itr,
	                            search);

	AS_DBG_2 ("Sent new search for \"%s\" to %d supernodes",
	          search->query, count);

	return search;
}

/* create and run new hash search */
ASSearch *as_searchman_locate (ASSearchMan *man, ASSearchResultCb result_cb,
                               ASHash *hash)
{
	ASSearch *search;
	int count;
	char *hash_str;

	/* create search */
	if (!(search = as_search_create_locate (man->next_search_id, result_cb,
	                                        hash)))
	{
		return NULL;
	}

	/* insert into search table */
	if (!as_hashtable_insert_int (man->searches, search->id, search))
	{
		AS_ERR ("Hashtable insert failed for new search");
		as_search_free (search);

		assert (0);
		return NULL;
	}

	/* insert into hash search table */
	if (!as_hashtable_insert (man->hash_searches, search->hash->data,
	                          AS_HASH_SIZE, search))
	{
		AS_ERR ("Hashtable insert failed for new search");
		as_hashtable_remove_int (man->searches, search->id);
		as_search_free (search);

		assert (0);
		return NULL;
	}

	/* we actually used this id, go to next */
	man->next_search_id++;

	/* send search to all connected supernodes */
	count = as_sessman_foreach (AS->sessman,
	                            (ASSessionForeachFunc)send_search_itr,
	                            search);

	hash_str = as_hash_encode (search->hash);
	AS_DBG_2 ("Sent new hash search for \"%s\" to %d supernodes",
	          hash_str, count);
	free (hash);

	return search;
}

/* Stop search but keep previous results until search is removed. Raises
 * search callback with NULL result.
 */
as_bool as_searchman_stop (ASSearchMan *man, ASSearch *search)
{
	/* finish search and raise callback one last time */
	as_search_add_result (search, NULL);

	return TRUE;
}

/* Remove and free search and all results. Does not raise search result
 * callback.
 */
as_bool as_searchman_remove (ASSearchMan *man, ASSearch *search)
{
	ASSearch *hs;

	/* remove search from search table */
	if (!(hs = as_hashtable_remove_int (man->searches, search->id)))
	{
		AS_WARN_1 ("Couldn't remove search with id %d from hash table",
		           (int)search->id);
		/* this is a bug somewhere else */
		assert (0);
	}

	assert (hs == search);

	/* remove hash search from hash search table */
	if (search->type == SEARCH_LOCATE)
	{
		if (!(hs = as_hashtable_remove (man->hash_searches,
		                                search->hash->data, AS_HASH_SIZE)))
		{
			char *hash_str = as_hash_encode (search->hash);
			AS_WARN_2 ("Couldn't remove search with hash %s and id %d from hash table",
			           hash_str, search->id);
			free (hash_str);
			/* this is a bug somewhere else */
			assert (0);
		}

		assert (hs == search);
	}

	/* now free the search */
	as_search_free (search);

	return TRUE;
}

/*****************************************************************************/

/* get any search by id */
ASSearch *as_searchman_lookup (ASSearchMan *man, as_uint16 search_id)
{
	ASSearch *search;

	if (search_id == INVALID_SEARCH_ID)
		return NULL;

	if (!(search = as_hashtable_lookup_int (man->searches, search_id)))
		return NULL;

	return search;
}

/* get _hash_ search by hash */
ASSearch *as_searchman_lookup_hash (ASSearchMan *man, ASHash *hash)
{
	ASSearch *search;

	if (!hash)
		return NULL;

	if (!(search = as_hashtable_lookup (man->searches, hash->data,
	                                    AS_HASH_SIZE)))
	{
		return NULL;
	}

	return search;
}

/*****************************************************************************/
