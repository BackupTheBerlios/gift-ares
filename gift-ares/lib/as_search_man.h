/*
 * $Id: as_search_man.h,v 1.1 2004/09/06 18:54:26 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SEARCH_MAN_H_
#define __AS_SEARCH_MAN_H_

/*****************************************************************************/

typedef struct
{
	/* We have two hash tables one for all searches keyed by search id and
	 * and extra one for hash searches keyed by file hash. Both tables point
	 * to the same result data.
	 */
	ASHashTable searches;
	ASHashTable hash_searches;

	as_uint16 next_search_id; /* our source of search ids */

} ASSearchMan;

/*****************************************************************************/

/* allocate and init search manager */
ASSearchMan *as_searchman_create ();

/* free manager */
void as_searchman_free (ASSearchMan *man);

/*****************************************************************************/

/* get search by id */
ASSearch *as_searchman_lookup (ASSearchMan *man, as_uint16 search_id);

/*****************************************************************************/

#endif /* __AS_SEARCH_MAN_H_ */
