/*
 * $Id: as_upload.c,v 1.11 2004/10/30 23:52:06 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

#define BLOCKSIZE 4096

/* ares displays this (up to the first slash or space) as the network name,
   so we should be consistent with what we tell supernodes */
#define AS_HTTP_SERVER_NAME  AS_CLIENT_NAME " (libares/0.1)"

/* Log http reply headers. */
/* #define LOG_HTTP_REPLIES */

/*****************************************************************************/

static as_bool send_reply_success (ASUpload *up);
static as_bool send_reply_queued (ASUpload *up, int queue_pos,
                                  int queue_length);
static as_bool send_reply_error (ASUpload *up);
static as_bool send_reply_not_found (ASUpload *up);
static void send_file (int fd, input_id input, ASUpload *up);

/*****************************************************************************/

static as_bool upload_set_state (ASUpload *up, ASUploadState state,
                                 as_bool raise_callback)
{
	up->state = state;

	/* raise callback if specified */
	if (raise_callback && up->state_cb)
		return up->state_cb (up, up->state);

	return TRUE;
}

/*****************************************************************************/

/* Create new upload from HTTP request. Takes ownership of tcp connection and
 * request object if successful.
 */
ASUpload *as_upload_create (TCPC *c, ASHttpHeader *request,
                            ASUploadStateCb state_cb,
                            ASUploadAuthCb auth_cb)
{
	ASUpload *up;

	assert (c);
	assert (request);
	
	if (!(up = malloc (sizeof (ASUpload))))
		return NULL;

	up->c = c;
	up->host = up->c->host;
	up->username = NULL;
	up->request = request;
	up->share = NULL;
	up->file = NULL;
	up->start = up->stop = up->sent = 0;
	up->input = INVALID_INPUT;
	
	up->state = UPLOAD_NEW;
	up->state_cb = state_cb;
	up->auth_cb = auth_cb;
	up->data_cb = NULL;

	up->upman = NULL;
	up->udata = NULL;

	return up;
}

/* Free upload object. */
void as_upload_free (ASUpload *up)
{
	if (!up)
		return;

	input_remove (up->input);
	tcp_close_null (&up->c);

	as_http_header_free (up->request);
	as_share_free (up->share);

	if (up->file)
		fclose (up->file);

	free (up->username);
	free (up);
}

/* Set data callback for upload. */
void as_upload_set_data_cb (ASUpload *up, ASUploadDataCb data_cb)
{
	up->data_cb = data_cb;
}

/*****************************************************************************/

/* Send reply back to requester. Looks up share in shares manager, raises auth
 * callback and sends reply. Either sends 503, 404, queued status, or
 * requested data. Returns FALSE if the connection has been closed after a
 * failure reply was sent and TRUE if data transfer was started.
 */
as_bool as_upload_start (ASUpload *up)
{
	ASHash *hash;
	const char *range;
	int queue_pos, queue_length;

	if (up->state != UPLOAD_NEW)
	{
		assert (up->state == UPLOAD_NEW);
		return FALSE;
	}

	/* Get username from request. */
	if ((up->username = as_http_header_get_field (up->request, "X-My-Nick")))
	{
		if (*up->username == '\0')
			up->username = NULL;
		else
			up->username = strdup (up->username);
	}

	/* Get hash from request header. */
	if ((strncmp (up->request->uri, "sha1:", 5) &&
	     strncmp (up->request->uri, "/hack", 5)) || /* for debugging */
	    !(hash = as_hash_decode (up->request->uri + 5)))
	{
		AS_WARN_2 ("Malformed uri '%s' from %s",
		           up->request->uri, net_ip_str (up->host));
		send_reply_error (up);
		return FALSE;
	}

	/* Lookup share. */
	if (!(up->share = as_shareman_lookup (AS->shareman, hash)))
	{
		AS_DBG_2 ("Unknown share request '%s' from %s",
		          as_hash_str (hash), net_ip_str (up->host));
		send_reply_not_found (up);
		as_hash_free (hash);
		return FALSE;
	}

	as_hash_free (hash);

	/* Make copy of share object in case share list changes during upload. */
	if (!(up->share = as_share_copy (up->share)))
	{
		AS_ERR ("Insufficient memory.");
		send_reply_error (up);
		return FALSE;
	}

	/* Get range from request header. */
	if ((range = as_http_header_get_field (up->request, "Range")))
	{
		int i = sscanf (range, "bytes=%u-%u", &up->start, &up->stop);

		if (i == 1) /* only start specified */
			up->stop = up->share->size;
		else
			up->stop++; /* make range exclusive end */

		if (i == 0 || up->stop <= up->start ||
		    up->start >= up->share->size || up->stop > up->share->size)
		{
			AS_ERR_2 ("Invalid range header '%s' from %s",
			          range, net_ip_str (up->host));
			send_reply_error (up);
			return FALSE;
		}

	}
	else
	{
		AS_DBG_1 ("No range header from %s, assuming whole file",
		          net_ip_str (up->host));

		up->start = 0;
		up->stop = up->share->size;
	}

	AS_DBG_4 ("Upload request: '%s' (%d, %d) from %s",
	          up->share->path, up->start, up->stop, net_ip_str (up->host));

	/* Ask auth callback what to do. */
	queue_pos = 0; /* send data */
	queue_length = 0;

	if (up->auth_cb)
		queue_pos = up->auth_cb (up, &queue_length);

	if (queue_pos < 0)
	{
		/* Callback requested we tell this user to go away. */
		send_reply_not_found (up);
		return FALSE;
	}
	else if (queue_pos > 0)
	{
		/* User is queued */
		send_reply_queued (up, queue_pos, queue_length);
		return FALSE;
	}

	/* Send data. */
	assert (queue_pos == 0);

	up->file = fopen (up->share->path, "rb");

	if (!up->file || (fseek (up->file, up->start, SEEK_SET) < 0))
	{
		AS_ERR_1 ("Failed to open file for upload: %s", up->share->path);

		if (up->file)
		{
			fclose (up->file);
			up->file = NULL;
		}
		
		send_reply_error (up);
		return FALSE;
	}

	/* Send 206 reply. */
	if (!send_reply_success (up))
	{
		AS_ERR_1 ("Failed to send 206 reply for upload: %s", up->share->path);

		if (up->file)
		{
			fclose (up->file);
			up->file = NULL;
		}
		
		return FALSE;
	}

	if (!upload_set_state (up, UPLOAD_ACTIVE, TRUE))
		return FALSE; /* Callback freed us */

	/* Wait until we can write file data. */
	up->input = input_add (up->c->fd, (void *)up, INPUT_WRITE,
	                       (InputCallback)send_file, 0);

	return TRUE;
}

/* Cancel data transfer and close connection. Raises state callback. */
as_bool as_upload_cancel (ASUpload *up)
{
	if (up->state != UPLOAD_ACTIVE)
		return FALSE;

	input_remove (up->input);
	up->input = INVALID_INPUT;
	tcp_close_null (&up->c);

	if (up->file)
	{
		fclose (up->file);
		up->file = NULL;
	}

	if (!upload_set_state (up, UPLOAD_CANCELLED, TRUE))
		return FALSE; /* Callback freed us */

	return TRUE;
}

/*****************************************************************************/

/* Returns current upload state */
ASUploadState as_upload_state (ASUpload *up)
{
	return up->state;
}

/* Return upload state as human readable static string. */
const char *as_upload_state_str (ASUpload *up)
{
	switch (up->state)
	{
	case UPLOAD_INVALID:   return "Invalid";
	case UPLOAD_NEW:       return "New";
	case UPLOAD_ACTIVE:    return "Active";
	case UPLOAD_FAILED:    return "Failed";
	case UPLOAD_QUEUED:    return "Queued";
	case UPLOAD_COMPLETE:  return "Completed";
	case UPLOAD_CANCELLED: return "Cancelled";
	}
	return "UNKNOWN";
}

/*****************************************************************************/

/* MOVEME */
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

static void set_header_b6mi (ASHttpHeader *request)
{
	ASPacket *p;
	ASSession *super = NULL;

	p = as_packet_create ();

	/* our supernode's IP and port (choose the first one in the list) */
	if (AS && AS->sessman && AS->sessman->connected)
		super = AS->sessman->connected->data;

	if (super)
	{
		as_packet_put_ip (p, super->host);
		as_packet_put_le16 (p, super->port);
	}
	else
	{
		as_packet_put_ip (p, INADDR_NONE);
		as_packet_put_le16 (p, 0);
	}

	/* our IP and port */
	as_packet_put_ip (p, AS->netinfo->outside_ip);
	as_packet_put_le16 (p, AS->netinfo->port);

	as_encrypt_b6mi (p->data, p->used);
	set_header_encoded (request, "X-B6MI", p);
        
	as_packet_free (p);
}

static void set_common_headers (ASUpload *up, ASHttpHeader *reply)
{
	char buf[32];

	as_http_header_set_field (reply, "Server", AS_HTTP_SERVER_NAME);
	set_header_b6mi (reply);

	sprintf (buf, "%08X", ntohl (net_local_ip (up->c->fd, NULL)));
	as_http_header_set_field (reply, "X-MyLIP", buf);
	if (AS->netinfo->nick)
		as_http_header_set_field (reply, "X-My-Nick", AS->netinfo->nick);
}

/*****************************************************************************/

static as_bool send_reply_success (ASUpload *up)
{
	ASHttpHeader *reply;
	char buf[32];
	String *str;

	reply = as_http_header_reply (HTHD_VER_11,
	                              (up->start == 0 &&
	                              up->stop == up->share->size) ? 200 : 206);

#if 0
	sprintf (buf, "bytes=%u-%u/%u", up->start, up->stop-1,
	         up->share->size); /* Ares */
#else
	sprintf (buf, "bytes %u-%u/%u", up->start, up->stop-1, 
		     up->share->size); /* HTTP */
#endif
	as_http_header_set_field (reply, "Content-Range", buf);

	sprintf (buf, "%u", up->stop - up->start);
	as_http_header_set_field (reply, "Content-Length", buf);

	set_common_headers (up, reply);

	str = as_http_header_compile (reply);

#ifdef LOG_HTTP_REPLIES
	AS_DBG_1 ("Reply Success:\n%s", str->str);
#endif

	/* Immediately send reply since there might be a race condition between
	 * the tcp write queue and our file send input. I think it is not defined
	 * which input event gets triggered first if there are multiple.
	 */
	if (tcp_send (up->c, str->str, str->len) != str->len)
	{
		AS_ERR_1 ("Short send in reply for upload '%s'", up->share->path);

		string_free (str);
		as_http_header_free (reply);
		return FALSE;
	}

	string_free (str);
	as_http_header_free (reply);

	return TRUE;
}

static as_bool send_reply_queued (ASUpload *up, int queue_pos,
                                  int queue_length)
{
	ASHttpHeader *reply;
	String *str;
	char buf[128];

	assert (queue_pos != 0);
	assert (queue_length > 0);
	assert (queue_pos <= queue_length);

	reply = as_http_header_reply (HTHD_VER_11, 503);
	set_common_headers (up, reply);

	/* Omit queue header if position is < 0 */
	if (queue_pos > 0)
	{
		sprintf (buf, "position=%u,length=%u,limit=%u,pollMin=%u,pollMax=%u",
		         queue_pos, queue_length,
		         1                    /* max active downloads (per user?) */,
		         AS_UPLOAD_QUEUE_MIN, /* min/max recheck intervals in seconds */
		         AS_UPLOAD_QUEUE_MAX);
		as_http_header_set_field (reply, "X-Queued", buf);
	}

	str = as_http_header_compile (reply);

#ifdef LOG_HTTP_REPLIES
	AS_DBG_1 ("Reply Queued:\n%s", str->str);
#endif

	/* Immediately send reply and close connection. */
	tcp_send (up->c, str->str, str->len);
	tcp_close_null (&up->c);

	string_free (str);
	as_http_header_free (reply);

	if (!upload_set_state (up, UPLOAD_QUEUED, TRUE))
		return FALSE; /* Callback freed us */

	return TRUE;
}

static as_bool send_reply_error (ASUpload *up)
{
	ASHttpHeader *reply;
	String *str;

	reply = as_http_header_reply (HTHD_VER_11, 500);
	set_common_headers (up, reply);
	str = as_http_header_compile (reply);

#ifdef LOG_HTTP_REPLIES
	AS_DBG_1 ("Reply Error:\n%s", str->str);
#endif

	/* Immediately send reply and close connection. */
	tcp_send (up->c, str->str, str->len);
	tcp_close_null (&up->c);

	string_free (str);
	as_http_header_free (reply);

	if (!upload_set_state (up, UPLOAD_FAILED, TRUE))
		return FALSE; /* Callback freed us */

	return TRUE;
}

static as_bool send_reply_not_found (ASUpload *up)
{
	ASHttpHeader *reply;
	String *str;

	reply = as_http_header_reply (HTHD_VER_11, 404);
	set_common_headers (up, reply);
	str = as_http_header_compile (reply);

#ifdef LOG_HTTP_REPLIES
	AS_DBG_1 ("Reply Not Found:\n%s", str->str);
#endif

	/* Immediately send reply and close connection. */
	tcp_send (up->c, str->str, str->len);
	tcp_close_null (&up->c);

	string_free (str);
	as_http_header_free (reply);

	if (!upload_set_state (up, UPLOAD_FAILED, TRUE))
		return FALSE; /* Callback freed us */

	return TRUE;
}

/*****************************************************************************/

static as_bool send_error (ASUpload *up)
{
	input_remove (up->input);
	up->input = INVALID_INPUT;
	tcp_close_null (&up->c);

	if (up->file)
	{
		fclose (up->file);
		up->file = NULL;
	}

	return upload_set_state (up, UPLOAD_CANCELLED, TRUE); /* may free us */
}

static void send_file (int fd, input_id input, ASUpload *up)
{
	int in, out;
	unsigned int left, wanted;
	as_uint8 buf[BLOCKSIZE];

	if (net_sock_error (fd))
	{
		AS_DBG_3 ("net_sock_error %d after %u bytes for upload to %s",
		          errno, up->sent, net_ip_str (up->host));

		send_error (up); /* may free us */
		return;
	}
	
	wanted = BLOCKSIZE;

	left = (up->stop - up->start) - up->sent;

	if (wanted > left)
		wanted = left;

	in = fread (buf, 1, wanted, up->file);
	
	if (in < (int)wanted)
	{
		AS_WARN_3 ("Read (%d of %d) failed from %s. Cancelling upload.",
		           in, wanted, up->share->path);

		send_error (up); /* may free us */
		return;
	}

	out = tcp_send (up->c, buf, in);
	
	if (out < 0)
	{
		AS_DBG_2 ("Failed to write %d bytes to %s. Cancelling upload.",
		          in, net_ip_str (up->host));

		send_error (up); /* may free us */
		return;
	}

	if (out < in)
	{
		AS_DBG_3 ("Wrote %d of %d bytes to %s, rewinding",
		           out, in, net_ip_str (up->host));

		if (fseek (up->file, -((off_t)(in - out)), SEEK_CUR) < 0)
		{
			AS_ERR ("Rewind failed. Cancelling upload.");

			send_error (up); /* may free us */
			return;
		}
	}

	up->sent += out;

	/* Raise data callback if there is one */
	if (up->data_cb)
		up->data_cb (up, out);

	/* Check if upload is complete */
	assert (up->sent <= up->stop - up->start);

	if (up->sent == up->stop - up->start)
	{
		AS_DBG_3 ("Finished uploading %d bytes of '%s' to %s",
		           up->sent, up->share->path, net_ip_str (up->host));

		input_remove (up->input);
		up->input = INVALID_INPUT;
		tcp_close_null (&up->c);
		fclose (up->file);
		up->file = NULL;

		upload_set_state (up, UPLOAD_COMPLETE, TRUE); /* may free us */
		return;
	}
}

/*****************************************************************************/
