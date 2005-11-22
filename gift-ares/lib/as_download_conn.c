/*
 * $Id: as_download_conn.c,v 1.21 2005/11/22 17:18:00 hex Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static as_bool downconn_request (ASDownConn *conn);

/*****************************************************************************/

static as_bool downconn_set_state (ASDownConn *conn, ASDownConnState state,
                                   as_bool raise_callback)
{
	conn->state = state;

	if (raise_callback && conn->state_cb)
		return conn->state_cb (conn, conn->state);

	return TRUE;
}

static ASDownConn *downconn_new (void)
{
	ASDownConn *conn;

	if (!(conn = malloc (sizeof (ASDownConn))))
		return NULL;

	conn->source      = NULL;
	conn->hash        = NULL;
	conn->chunk_start = 0;
	conn->chunk_size  = 0;
	conn->client      = NULL;
	conn->push        = NULL;

	conn->queue_pos      = 0;
	conn->queue_len      = 0;
	conn->queue_last_try = 0;
	conn->queue_next_try = 0;

	conn->hist_downloaded = 0;
	conn->hist_time       = 0;
	conn->curr_downloaded = 0;
	conn->request_time    = 0;
	conn->data_time       = 0;

	conn->fail_count       = 0;

	conn->state    = DOWNCONN_UNUSED;
	conn->state_cb = NULL;
	conn->data_cb  = NULL;

	conn->udata1 = NULL;
	conn->udata2 = NULL;

	return conn;
}

static void downconn_reset (ASDownConn *conn)
{
	conn->chunk_start = 0;
	conn->chunk_size = 0;
	as_pushman_remove (AS->pushman, conn->push);
	conn->push = NULL;
	as_hash_free (conn->hash);
	conn->hash = NULL;
}

/* Update stats after a request is complete or cancelled */
static void downconn_update_stats (ASDownConn *conn)
{
	if (conn->request_time == 0)
		return;

	conn->hist_downloaded += conn->curr_downloaded;
	conn->hist_time += time (NULL) - conn->request_time;

	AS_HEAVY_DBG_3 ("Updated stats for %s. last speed: %2.2f kb/s, total speed: %2.2f kb/s",
	                net_ip_str (conn->source->host),
	                (float)conn->curr_downloaded / (time (NULL) - conn->request_time) / 1024,
	                (float)conn->hist_downloaded / conn->hist_time / 1024);

	/* Reset values for last request so we do not add it multiple times */
	conn->curr_downloaded = 0;
	conn->request_time = 0;
}

/*****************************************************************************/

/* Create new download connection from source (copies source). */
ASDownConn *as_downconn_create (ASSource *source, ASDownConnStateCb state_cb,
                                ASDownConnDataCb data_cb)
{
	ASDownConn *conn;

	assert (source);

	if (!(conn = downconn_new ()))
		return NULL;

	conn->source   = as_source_copy (source);
	conn->state_cb = state_cb;
	conn->data_cb  = data_cb;

	return conn;
}

/* Free download connection. */
void as_downconn_free (ASDownConn *conn)
{
	if (!conn)
		return;

	as_downconn_cancel (conn);

	assert (conn->hash == NULL);
	assert (conn->push == NULL);

	as_source_free (conn->source);

	free (conn);
}

/*****************************************************************************/

/* Start this download from this connection with specified piece and hash. */
as_bool as_downconn_start (ASDownConn *conn, ASHash *hash, size_t start,
                           size_t size)
{
	if (conn->state == DOWNCONN_CONNECTING ||
	    conn->state == DOWNCONN_TRANSFERRING)
	{
		assert (0); /* remove later */
		return FALSE;
	}

	assert (start >= 0);
	assert (size > 0);
	assert (conn->hash == NULL);
	
	/* assign new chunk and hash */
	conn->chunk_start = start;
	conn->chunk_size  = size;
	conn->hash        = as_hash_copy (hash);
	conn->curr_downloaded = 0;
	conn->request_time = 0;
	conn->data_time = 0;
	conn->client = NULL;

	return downconn_request (conn);
}

/* Stop current download. Does not raise callback. State is set to
 * DOWNCONN_UNUSED. 
 */
void as_downconn_cancel (ASDownConn *conn)
{
	if (conn->client)
		as_http_client_cancel (conn->client);

	/* Still update stats since this might be a request cancelled because the
	 * chunk was shrunk */
	downconn_update_stats (conn);

	downconn_reset (conn);
	downconn_set_state (conn, DOWNCONN_UNUSED, FALSE);
}

/*****************************************************************************/

/* Returns average speed of this source collected from past requests in
 * bytes/sec.
 */
unsigned int as_downconn_hist_speed (ASDownConn *conn)
{
	if (conn->hist_time == 0)
		return 0;

	return (conn->hist_downloaded / conn->hist_time);
}

/* Returns average speed of this source collected from past requests and the
 * currently running one in bytes/sec.
 */
unsigned int as_downconn_speed (ASDownConn *conn)
{
	unsigned int speed = 0;

	if (conn->hist_time > 0)
		speed += conn->hist_downloaded / conn->hist_time;

	if (conn->request_time > 0)
	{
		time_t dt = time (NULL) - conn->request_time;
		
		if (dt > 0)
			speed += conn->curr_downloaded / dt;
	}

	return speed;
}

/*****************************************************************************/

/* Suspend transfer using input_suspend_all on http client socket. */
as_bool as_downconn_suspend (ASDownConn *conn)
{
	if (!conn->client || !conn->client->tcpcon)
		return FALSE;

	input_suspend_all (conn->client->tcpcon->fd);
	return TRUE;
}

/* Resume transfer using input_resume_all on http client socket. */
as_bool as_downconn_resume (ASDownConn *conn)
{
	if (!conn->client || !conn->client->tcpcon)
		return FALSE;

	input_resume_all (conn->client->tcpcon->fd);
	return TRUE;
}

/*****************************************************************************/

/* Return connection state as human readable static string. */
const char *as_downconn_state_str (ASDownConn *conn)
{
	switch (conn->state)
	{
	case DOWNCONN_UNUSED:       return "Unused";
	case DOWNCONN_CONNECTING:   return "Connecting";
	case DOWNCONN_TRANSFERRING: return "Transferring";
	case DOWNCONN_FAILED:       return "Failed";
	case DOWNCONN_COMPLETE:     return "Complete";
	case DOWNCONN_QUEUED:       return "Queued";
	}

	return "UNKNOWN";
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
	as_packet_put_le16 (p, AS->netinfo->port);

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

/*****************************************************************************/

void append_with_header (ASPacket *req, ASPacket *p, int type)
{
	as_packet_header (p, (ASPacketType)type);
	as_packet_append (req, p);
	as_packet_free (p);
}

void append_raw_with_header (ASPacket *req, unsigned char *data, int type, int len)
{
	ASPacket *p = as_packet_create();

	as_packet_put_ustr (p, data, len);
	as_packet_header (p, (ASPacketType)type);
	as_packet_append (req, p);
	as_packet_free (p);
}

void put_b6st (ASPacket *req)
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
        append_with_header (req, p, 0x06);
}

void put_0a (ASPacket *req)
{
        ASPacket *p = as_packet_create ();

	as_packet_put_8 (p, 0x01); /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_le16 (p, 0xc0); /* unknown */
	as_packet_put_8 (p, 0x0f); /* unknown */
	as_packet_put_8 (p, 0); /* unknown */
	as_packet_put_le32 (p, 0); /* unknown */
	as_packet_put_le32 (p, 0); /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_8 (p, 0x11);
	as_packet_put_le16 (p, 0x04); /* unknown */
	as_packet_put_8 (p, 0); /* unknown */
	as_packet_put_8 (p, 0); /* unknown */
	as_packet_put_8 (p, 0xff); /* unknown */

	p = as_encrypt_transfer_0a (p);

	append_with_header (req, p, 0x0a);
}

#define CLIENT_NAME "AresLite 2.0.0.2966"

as_bool send_request (ASHash *hash, TCPC *c, int start, int stop)
{
	ASPacket *p = as_packet_create();
	int ret;

	as_packet_put_8 (p, 1); /* get */

	as_packet_put_le16 (p, 3);
	as_packet_put_8 (p, 0x32);
	as_packet_put_8 (p, 1); /* enc type */
	as_packet_put_le16 (p, 0x1234); /* reply key */

	/* hash */
	as_packet_put_le16 (p, 20);
	as_packet_put_8 (p, 0x01);
	as_packet_put_hash (p, hash);
	
	/* username */
	as_packet_put_le16 (p, 0);
	as_packet_put_8 (p, 2);

	put_b6st (p);
	
	put_0a (p);
	
	/* range */
	as_packet_put_le16 (p, 8);
	as_packet_put_8 (p, 7);
	as_packet_put_le32 (p, start);
	as_packet_put_le32 (p, stop);

	/* extended range */
	as_packet_put_le16 (p, 16);
	as_packet_put_8 (p, 0x0b);
	as_packet_put_le32 (p, start);
	as_packet_put_le32 (p, 0/*start>>32*/);
	as_packet_put_le32 (p, stop);
	as_packet_put_le32 (p, 0/*stop>>32*/);

	/* client */
	append_raw_with_header (p, CLIENT_NAME, 0x09, strlen(CLIENT_NAME));

	/* node info */
	as_packet_put_le16 (p, 16);
	as_packet_put_8 (p, 0x03);
	as_packet_put_ip (p, inet_addr ("0.0.0.0"));
	as_packet_put_le16 (p, 80);
	as_packet_put_ip (p, inet_addr ("0.0.0.0"));
	as_packet_put_le16 (p, 59049);
	as_packet_put_ip (p, inet_addr ("192.168..0.1"));

	/* alt sources */
	as_packet_put_le16 (p, 0);
	as_packet_put_8 (p, 0x08);

	/* tree hash? */
#if 0
	as_packet_put_le16 (p, 1);
	as_packet_put_8 (p, 0x0c);
	as_packet_put_8 (p, 0x01);
#endif
	
	p = as_encrypt_transfer (p);

	ret = as_packet_send (p, c);
	
	as_packet_free (p);

	return ret;
}

static void do_send_request (int fd, input_id input, ASDownConn *conn)
{
	input_remove (input);

	if (net_sock_error (fd))
	{
		/* connect failed */
		AS_DBG_2 ("net_sock_error(fd) for %s:%d",
						 net_ip_str(conn->source->host), conn->source->port);

		return;
	}

	if (!send_request (conn->hash, conn->tcpcon, conn->chunk_start, conn->chunk_start+conn->chunk_size-1))
	{
		AS_ERR_2 ("error sending request to %s:%d",
						 net_ip_str(conn->source->host), conn->source->port);
		return;
	}

	/* FIXME */
	return;
}

static as_bool downconn_request (ASDownConn *conn)
{
	ASSource *source = conn->source;

	if (! (conn->tcpcon = tcp_open (source->host, source->port, FALSE)))
	{
		AS_ERR_2 ("ERROR: tcp_open() failed for %s:%d",
				   net_ip_str(source->host), source->port);

		return FALSE;
	}
	input_add (conn->tcpcon->fd, (void *)conn, INPUT_WRITE,
			   (InputCallback)do_send_request, AS_SESSION_CONNECT_TIMEOUT);

	return TRUE;
}

/*****************************************************************************/

#if 0
static as_bool handle_reply (ASHttpClient *client)
{
	return FALSE;
}

/*****************************************************************************/

static void downconn_push_callback (ASPush *push, TCPC *c)
{
}
#endif

/*****************************************************************************/
