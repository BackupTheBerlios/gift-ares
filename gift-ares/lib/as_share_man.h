/*
 * $Id: as_share_man.h,v 1.5 2004/10/24 01:18:56 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SHARE_MAN_H_
#define __AS_SHARE_MAN_H_

/*****************************************************************************/

typedef struct
{
	List *shares;       /* shares in the order in which they were added,
	                     * which will hopefully be by-directory to
	                     * maximise compression when submitting */
	ASHashTable *table; /* Points to links in shares list. Keyed by hash. */
	int      nshares;
	double   size;      /* Mb */
} ASShareMan;

/*****************************************************************************/

/* Create share manager. */
ASShareMan *as_shareman_create (void);

/* Free share manager and all shares. */
void as_shareman_free (ASShareMan *man);

/*****************************************************************************/

/* Add share to manager. If a share with the same hash is already added it
 * will be replaced with the new share. Takes ownership of share.
 */
as_bool as_shareman_add (ASShareMan *man, ASShare *share);

/* Remove and free share with specified hash. */
as_bool as_shareman_remove (ASShareMan *man, ASHash *hash);

/* Lookup share by file hash. */
ASShare *as_shareman_lookup (ASShareMan *man, ASHash *hash);

/* Submit all shares to specified supernode. */
as_bool as_shareman_submit (ASShareMan *man, ASSession *session);

/* Submit list of shares to all connected supernodes and add shares to
 * manager. Takes ownership of list values (shares).
 */
as_bool as_shareman_add_and_submit (ASShareMan *man, List *shares);

/*****************************************************************************/

#endif
