/*
 * $Id: as_ares.c,v 1.14 2004/10/03 14:59:22 HEx Exp $
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

	/* Start in defined state so as_cleanup works right. */
	AS->nodeman   = NULL;
	AS->sessman   = NULL;
	AS->netinfo   = NULL;
	AS->searchman = NULL;
	AS->downman   = NULL;
	AS->pushman   = NULL;
	AS->shareman  = NULL;
	AS->server    = NULL;

	if (!(AS->nodeman = as_nodeman_create ()))
	{
		AS_ERR ("Failed to create node manager");
		as_cleanup ();
		return FALSE;
	}

	if (!(AS->sessman = as_sessman_create ()))
	{
		AS_ERR ("Failed to create session manager");
		as_cleanup ();
		return FALSE;
	}

	if (!(AS->netinfo = as_netinfo_create ()))
	{
		AS_ERR ("Failed to create network info");
		as_cleanup ();
		return FALSE;
	}

	if (AS_LISTEN_PORT)
	{
		if (!(AS->server = as_http_server_create (
			      AS_LISTEN_PORT,
			      (ASHttpServerRequestCb)as_incoming_http,
			      (ASHttpServerPushCb)as_incoming_push,
			      (ASHttpServerBinaryCb)NULL
			      )))
		{
			AS_ERR_1 ("Failed to create server on port %d",
				  AS_LISTEN_PORT);
		}
		else
			/* Set port so we can use it for pushes, sharing, etc */
			AS->netinfo->port = AS->server->port;
	}
	else
	{
		AS->server = NULL;
		AS_WARN ("HTTP server not started (no port set)");
	}

	if (!(AS->searchman = as_searchman_create ()))
	{
		AS_ERR ("Failed to create search manager");
		as_cleanup ();
		return FALSE;
	}

	if (!(AS->shareman = as_shareman_create ()))
	{
		AS_ERR ("Failed to create share manager");
		as_cleanup ();
		return FALSE;
	}

	if (!(AS->pushman = as_pushman_create ()))
	{
		AS_ERR ("Failed to create push manager");
		as_cleanup ();
		return FALSE;
	}

	if (!(AS->downman = as_downman_create ()))
	{
		AS_ERR ("Failed to create download manager");
		as_cleanup ();
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

	as_downman_free (AS->downman);
	as_pushman_free (AS->pushman); /* Don't free before downman */
	as_searchman_free (AS->searchman);
	as_netinfo_free (AS->netinfo);
	as_shareman_free (AS->shareman);
	as_sessman_free (AS->sessman);
	as_nodeman_free (AS->nodeman);
	as_http_server_free (AS->server);

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
