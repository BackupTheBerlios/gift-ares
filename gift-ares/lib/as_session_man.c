/*
 * $Id: as_session_man.c,v 1.1 2004/08/27 17:56:40 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/*****************************************************************************/

/* allocate and init session manager */
ASSessionMan *as_session_man_create ()
{
	ASSessionMan *man;

	if (!(man = malloc (sizeof (ASSessionMan))))
		return NULL;

	return man;
}

/* free manager */
void as_session_man_free (ASSessionMan *man)
{
	if (!man)
		return;

	free (man);
}

/*****************************************************************************/


