/*
 * $Id: as_search_man.c,v 1.1 2004/09/06 18:54:26 mkern Exp $
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
		as_hashtable_free (man->searches);
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
	as_hashtable_foreach (search->searches, search_free_itr, NULL);
	as_hashtable_free (man->searches, FALSE);
	as_hashtable_free (man->hash_searches, FALSE);
	free (man);
}

/*****************************************************************************/
