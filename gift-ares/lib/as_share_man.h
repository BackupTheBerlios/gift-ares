/*
 * $Id: as_share_man.h,v 1.2 2004/09/18 19:26:38 HEx Exp $
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
	List *shares; /* shares in the order in which they were added,
			 which will hopefully be by-directory to
			 maximise compression when submitting */
	ASHashTable *table; /* shares keyed by hash */
	int      nshares;
	double   size; /* Gb */
} ASShareMan;

/*****************************************************************************/

ASShareMan *as_shareman_create (void);

void as_shareman_free (ASShareMan *man);

/*****************************************************************************/

as_bool as_shareman_add (ASShareMan *man, ASShare *share);
as_bool as_shareman_remove (ASShareMan *man, ASShare *share);
ASShare *as_shareman_lookup (ASShareMan *man, ASHash *hash);

as_bool as_shareman_submit (ASShareMan *man, ASSession *session);

#endif
