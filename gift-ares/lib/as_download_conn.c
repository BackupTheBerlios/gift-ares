/*
 * $Id: as_download_conn.c,v 1.4 2004/09/10 18:01:45 mkern Exp $
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
	conn->queue_len      = 0;
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
#if 0
			/* set state to connecting and raise callback */
			if (!downconn_set_state (conn, DOWNCONN_CONNECTING, TRUE))
				return FALSE; /* connection was freed by callback */

			/* TODO: Send a push request and create http client once we have a
			 * tcp connection. Or maybe we need to rethink our strategy if
			 * there are no push requests.
			 */

			return TRUE;
#else
			AS_ERR_2 ("UNSUPPORTED: tried to use firewalled source %s:%d",
			          net_ip_str (conn->source->host), conn->source->port);

			conn->chunk = NULL;
			conn->hash  = NULL;
			downconn_set_state (conn, DOWNCONN_UNUSED, FALSE);
			
			return FALSE;
#endif
		}

		/* create http client for direct connection */
		conn->client = as_http_client_create (net_ip (conn->source->host),
		                                      conn->source->port,
		                                      downconn_http_callback);

		if (!conn->client)
		{
			AS_ERR_2 ("Failed to create http client for %s:%d",
			          net_ip_str (conn->source->host), conn->source->port);

			conn->chunk = NULL;
			conn->hash  = NULL;
			downconn_set_state (conn, DOWNCONN_UNUSED, FALSE);

			return FALSE;
		}
	}

	/* set state to connecting and raise callback */
	if (!downconn_set_state (conn, DOWNCONN_CONNECTING, TRUE))
		return FALSE; /* connection was freed by callback */
	
	/* make request */
	if (!downconn_request (conn))
	{
		AS_ERR_2 ("Failed to send http request to %s:%d",
		          net_ip_str (conn->source->host), conn->source->port);

		conn->chunk = NULL;
		conn->hash = NULL;
		downconn_set_state (conn, DOWNCONN_FAILED, TRUE);

		return FALSE;
	}

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

static as_bool set_header_encoded (ASHttpHeader *header, char *name,
                                   ASPacket *packet)
{
	char *encoded;
	
	if ((encoded = as_base64_encode (packet->data, packet->used)))
	{
		as_http_header_set_field (header, name, encoded);
		free (encoded);
		return TRUE;
	}

	return FALSE;
}

static void set_header_b6mi (ASHttpHeader *request, ASSource *source)
{
	ASPacket *p;

	p = as_packet_create ();

	/* our IP and port */
	as_packet_put_ip (p, AS->netinfo->outside_ip);
	as_packet_put_le16 (p, 0xDEAD); /* FIXME */

	/* and our supernode's (use the one we got the result from) */
	as_packet_put_ip (p, source->parent_host);
	as_packet_put_le16 (p, source->parent_port);

	as_encrypt_b6mi (p->data, p->used);
	set_header_encoded (request, "X-B6MI", p);
	
	as_packet_free (p);
}

static void set_header_b6st (ASHttpHeader *request)
{
	ASPacket *p = as_packet_create ();

	as_packet_put_8 (p, 0);    /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_8 (p, 1);    /* unknown */
	as_packet_put_8 (p, 0);    /* % complete? */
	as_packet_put_le32 (p, 0); /* zero */
	as_packet_put_le32 (p, 0); /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_8 (p, 0x11); /* hardcoded */
	as_packet_put_le16 (p, 2); /* unknown */
	as_packet_put_8 (p, 0);    /* unknown */
	as_packet_put_8 (p, 0);    /* unknown */
	as_packet_put_8 (p, 0x80); /* unknown */
	
	as_encrypt_b6st (p->data, p->used);
	set_header_encoded (request, "X-B6St", p);
	
	as_packet_free (p);
}

static as_bool downconn_request (ASDownConn *conn)
{
	ASHttpHeader *request;
	char buf[AS_HASH_BASE64_SIZE + 36];
	char *encoded;
	size_t start, end;

	assert (conn->chunk);
	assert (conn->hash);

	/* create uri and request */
	encoded = as_hash_encode (conn->hash);
	snprintf (buf, sizeof (buf), "sha1:%s", encoded);
	free (encoded);

	if (!(request = as_http_header_request (HTHD_VER_11, HTHD_GET, buf)))
		return FALSE;

	/* add range header (http range is inclusive) */
	assert (conn->chunk->size > 0);
	start = conn->chunk->start + conn->chunk->received;
	end   = conn->chunk->start + conn->chunk->size - 1;
	assert (start < end);

	snprintf(buf, sizeof (buf), "bytes=%u-%u", start, end);
	as_http_header_set_field (request, "Range", range);
	AS_HEAYV_DBG_3 ("Requesting range %s from %s:%d", range,
	                net_ip_str (conn->source->host), conn->source->port);

	/* add special ares headers */
	set_header_b6mi (request, conn->source);
	set_header_b6st (request);

	/* Send off the request. This takes care of freeing it later. */
	if (!as_http_client_request (conn->client, request, TRUE))
	{
		as_http_header_free (request);
		return FALSE;
	}

	/* wait for http callback */
	return TRUE;
}

/*****************************************************************************/

static as_bool handle_reply (ASHttpClient *client)
{
	ASDownConn *conn = client->udata;
	char *p;

	/* reset queue status */
	conn->queue_pos = 0;
	conn->queue_len = 0;
	conn->queue_last_try = 0;
	conn->queue_next_try = 0;

	switch (client->reply->code)
	{
	case 200:
	case 206:
	{
		int start, stop, size;

		/* Check that range is ok. */
		p = as_http_header_get_field (client->reply, "Content-Range");

		if (p  && sscanf (p, "bytes=%d-%d/%d", &start, &stop, &size) == 3)
		{
			if (start != conn->chunk->start)
			{
				AS_WARN_4 ("Invalid range start from %s:%d. "
				           "Got %d, expected %d. Aborting.",
				           net_ip_str (conn->source->host),
			               conn->source->port,
						   start, conn->chunk->start);
				break;
			}

			if (stop - start + 1 != size ||
			    size != conn->client->content_length)
			{
				AS_WARN_5 ("Inconsistent range header from %s. "
				           "start: %d, stop: %d, size: %d, content-length: %d. "
						   "Aborting.",
				           net_ip_str (conn->source->host),
			               start, stop, size, conn->client->content_length);
				break;		
			}

			if (size != conn->chunk->size - conn->chunk->received)
			{
				AS_WARN_5 ("Got different range than request from %s:%d."
				           "Requested size: %d, received: %d. Continuing anyway.",
				           net_ip_str (conn->source->host),
			               conn->source->port,
						   conn->chunk->size - conn->chunk->received,
						   size);
			}

			/* we are in business */
			downconn_set_state (conn, DOWNCONN_TRANSFERRING, TRUE);

			return TRUE; /* go on with request */
		}

		/* TODO: if missing range headers is common allow this case */
		AS_WARN_2 ("No range header in response from %s:%d."
		           "Aborting to prevent corruption.",
		           net_ip_str (conn->source->host), conn->source->port);
		break;
	}

	case 404:
		/* file not found */
		AS_DBG_2 ("Got 404 from %s:%d", net_ip_str (conn->source->host),
		          conn->source->port);
		break;

	case 503:
	{
		unsigned int pos = 0, len = 0, limit, min, max;
		unsigned int retry = 120; /* seconds */

		p = as_http_header_get_field (client->reply, "X-Queued");

		if (p && sscanf (p, "position=%u,length=%u,limit=%u,pollMin=%u,pollMax=%u",
				         &pos, &len, &limit, &min, &max) == 5)
		{
			retry = (min + max) / 2;
		}

		AS_HEAVY_DBG_5 ("Queued on %s:%d: pos: %d, len: %d, retry: %d",
		                net_ip_str (conn->source->host), conn->source->port,
						pos, len, retry);

		conn->queue_pos = pos;
		conn->queue_len = len;
		conn->queue_last_try = time (NULL);
		conn->queue_next_try = conn->queue_last_try + retry * ESECONDS;

		/* disassociate from chunk since the next request might be for a
		 * different chunk
		 */
		conn->chunk = NULL;
		conn->hash  = NULL;
		/* may free us */
		downconn_set_state (conn, DOWNCONN_QUEUED, TRUE);

		return TRUE; /* this will keep the connection open, with any luck */
	}

	default:
		AS_WARN_4 ("Unknown http response \"%s\" (%d) from %s:%d",
		           client->reply->code_str, client->reply->code,
		           net_ip_str (conn->source->host), conn->source->port);
		break;
	}

	/* do not continue with request */
	conn->chunk = NULL;
	conn->hash  = NULL;
	/* may free us */
	downconn_set_state (conn, DOWNCONN_FAILED, TRUE);

	return FALSE; /* abort request and close connection */
}

static int downconn_http_callback (ASHttpClient *client,
                                   ASHttpClientCbCode code)
{
	ASDownConn *conn = client->udata;

	switch (code)
	{
	case HTCL_CB_REQUESTING:
		break;

	case HTCL_CB_CONNECT_FAILED:
	case HTCL_CB_REQUEST_FAILED:
		conn->chunk = NULL;
		conn->hash = NULL;
		/* this may free us */
		downconn_set_state (conn, DOWNCONN_FAILED, TRUE);

		return FALSE;

	case HTCL_CB_REPLIED:
		return handle_reply (client);

	case HTCL_CB_DATA:
		assert (conn->state == DOWNCONN_TRANSFERRING);

		if (data_cb)
			data_cb (conn, conn->client->data, conn->client->data_len);

		return TRUE;

	case HTCL_CB_DATA_LAST:
		assert (conn->state == DOWNCONN_TRANSFERRING);

		AS_HEAVY_DBG_4 ("HTCL_CB_DATA_LAST (%d/%d) from %s:%d",
		                conn->client->content_received,
		                conn->client->content_length,
		                net_ip_str (conn->source->host), conn->source->port);

		if (client->data_len > 0 && data_cb)
		{
			if (!data_cb (conn, conn->client->data, conn->client->data_len))
				return FALSE; /* connection was free by callback */
		}

		conn->chunk = NULL;
		conn->hash = NULL;
		/* this may free us */
		downconn_set_state (conn, DOWNCONN_COMPLETE, TRUE);

		return TRUE; /* try to keep connection alive */
	}

	return TRUE;
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
