/*
 * $Id: as_ares.c,v 1.4 2004/09/07 13:05:33 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

ASInstance *as_instance = NULL;	/* global library instance */

/*****************************************************************************/

/* Create library instance and initialize it. There can only be one at a time
 * though.
 */
as_bool as_init ()
{
	assert (AS == NULL);
	if (AS)
		return FALSE;

	AS_DBG ("Initializing Ares library...");

	if (!(AS = malloc (sizeof (ASInstance))))
	{
		AS_ERR ("Insufficient memory.");
		return FALSE;
	}

	if (!(AS->nodeman = as_nodeman_create ()))
	{
		AS_ERR ("Failed to create node manager");
		free (AS);
		AS = NULL;
		return FALSE;
	}

	if (!(AS->sessman = as_sessman_create ()))
	{
		AS_ERR ("Failed to create session manager");
		as_nodeman_free (AS->nodeman);
		free (AS);
		AS = NULL;
		return FALSE;
	}

	if (!(AS->searchman = as_searchman_create ()))
	{
		AS_ERR ("Failed to create search manager");
		as_sessman_free (AS->sessman);
		as_nodeman_free (AS->nodeman);
		free (AS);
		AS = NULL;
		return FALSE;
	}

	return TRUE;
}

/* Clean up library instance */
as_bool as_cleanup ()
{
	assert (AS != NULL);
	if (!AS)
		return FALSE;

	AS_DBG ("Cleaning up Ares library...");

	as_searchman_free (AS->sessman);
	as_sessman_free (AS->sessman);
	as_nodeman_free (AS->nodeman);

	free (AS);
	AS = NULL;

	return TRUE;
}

/* Start connecting, resume downloads, etc. */
as_bool as_start ()
{
	assert (AS != NULL);
	if (!AS)
		return FALSE;

	return TRUE;
}

/* Drop all connections, stop downloads, etc. */
as_bool as_stop ()
{
	assert (AS != NULL);
	if (!AS)
		return FALSE;

	return TRUE;
}

/*****************************************************************************/
