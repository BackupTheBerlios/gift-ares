/*
 * $Id: as_download_conn.c,v 1.2 2004/09/09 22:52:59 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static as_bool downconn_request (ASDownConn *conn);

static int downconn_http_callback (ASHttpClient *client,
                                   ASHttpClientCbCode code);

static as_bool downconn_set_state (ASDownConn *conn, ASDownConnState state,
                                   as_bool raise_callback);

/*****************************************************************************/

/* Create new download connection from source (copies source). */
ASDownConn *as_downconn_create (ASSource *source, ASDownConnStateCb state_cb,
                                ASDownConnDataCb data_cb)
{
	ASDownConn *conn;

	assert (source);

	if (!(conn = malloc (sizeof (ASDownConn))))
		return NULL;

	conn->source = as_source_copy (source);
	conn->chunk  = NULL;
	conn->hash   = NULL;
	conn->client = NULL;

	conn->queue_pos      = 0;
	conn->queue_last_try = 0;
	conn->queue_next_try = 0;

	conn->total_downloaded = 0;
	conn->average_speed    = 0;

	conn->state    = DOWNCONN_UNUSED;
	conn->state_cb = state_cb;
	conn->data_cb  = data_cb;

	conn->udata = NULL;

	return conn;
}

/* Free download connection. */
void as_downconn_free (ASDownConn *conn)
{
	if (!conn)
		return;

	as_downconn_cancel (conn);
	assert (conn->chunk == NULL);
	assert (conn->hash == NULL);

	as_source_free (conn->source);
	as_http_client_free (conn->client);

	free (conn);
}

/*****************************************************************************/

/* Associate this connection with chunk and hash and start download of file */
as_bool as_downconn_start (ASDownConn *conn, ASHash *hash,
                           ASDownChunk *chunk)
{
	if (conn->state == DOWNCONN_CONNECTING ||
	    conn->state == DOWNCONN_TRANSFERRING)
	{
		assert (0); /* remove later */
		return FALSE;
	}

	assert (conn->chunk == NULL);
	assert (conn->hash == NULL);
	/* assign new chunk and hash */
	conn->chunk = chunk;
	conn->hash  = hash;

	if (!conn->client)
	{
		/* we need to create a new http client */
		if (as_source_firewalled (conn->source))
		{
			/* TODO: set state to connecing, send a push request and create
			 * http client once we have a tcp connection.
			 */
			conn->chunk = NULL;
			conn->hash  = NULL;
			downconn_set_state (conn, DOWNCONN_UNUSED, FALSE);
			
			AS_ERR_2 ("UNSUPPORTED: tried to use firewalled source %s:%d",
			          net_ip_str (conn->source->host), conn->source->port);
			return FALSE;
		}

		/* create http client for direct connection */
		conn->client = as_http_client_create (net_ip (conn->source->host),
		                                      conn->source->port,
		                                      downconn_http_callback);

		if (!conn->client)
		{
			conn->chunk = NULL;
			conn->hash  = NULL;
			downconn_set_state (conn, DOWNCONN_UNUSED, FALSE);

			AS_ERR_2 ("Failed to create http client for %s:%d",
			          net_ip_str (conn->source->host), conn->source->port);
			return FALSE;
		}
	}

	/* set state to connecting and raise callback */
	if (!downconn_set_state (conn, DOWNCONN_CONNECTING, TRUE))
		return FALSE; /* connection was freed by callback */
	
	/* make request */
	if (!downconn_request (conn))
		return FALSE;

	return TRUE;
}

/* Stop current download and disassociate from chunk and hash. Does not raise
 * callback. State is set to DOWNCONN_UNUSED. 
 */
as_bool as_downconn_cancel (ASDownConn *conn)
{
	if (conn->client)
		as_http_client_cancel (conn->client);

	conn->chunk = NULL;
	conn->hash = NULL;
	downconn_set_state (conn, DOWNCONN_UNUSED, FALSE);

	return TRUE;
}

/* Returns TRUE if connection is associated with a chunk. */
as_bool as_downconn_in_use (ASDownConn *conn)
{
	return (conn->chunk != NULL);
}

/*****************************************************************************/

static as_bool downconn_request (ASDownConn *conn)
{
	
}

/*****************************************************************************/

static int downconn_http_callback (ASHttpClient *client,
                                   ASHttpClientCbCode code)
{

}

/*****************************************************************************/

static as_bool downconn_set_state (ASDownConn *conn, ASDownConnState state,
                                   as_bool raise_callback)
{
	conn->state = state;

	if (raise_callback && conn->state_cb)
		return conn->state_cb (conn, conn->state);

	return TRUE;
}

/*****************************************************************************/
