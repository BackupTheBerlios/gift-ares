/*
 * $Id: as_upload.c,v 1.2 2004/10/19 23:41:48 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

#define BLOCKSIZE 4096

#define AS_HTTP_SERVER_NAME "libares/0.1"

/*****************************************************************************/

static as_bool send_reply (ASUpload *up);
static void send_file (int fd, input_id input, ASUpload *up);

ASUpload *as_upload_new (TCPC *c, ASShare *share, ASHttpHeader *req)
{
	ASUpload *up = malloc (sizeof(ASUpload));
	FILE *file;
	const char *range;

	if (!up)
		return NULL;

	up->c = c;
	up->share = share;

	range = as_http_header_get_field (req, "Range");

	if (range)
	{
		int i = sscanf (range, "bytes=%u-%u", &up->start, &up->stop);

		if (i == 1) /* only start specified */
			up->stop = share->size;
		else
			up->stop++;

		if (!i || up->stop < up->start ||
		    up->start > share->size || up->stop > share->size)
		{
			AS_ERR_2 ("invalid range header '%s' from %s",
				  range, net_ip_str (up->c->host));

			free (up);

			return NULL;
		}

	}
	else
	{
		AS_DBG_1 ("No range header from %s, assuming whole file",
			  net_ip_str (up->c->host));

		up->start = 0;
		up->stop = share->size;
	}

	up->sent = 0;

	file = fopen (share->path, "rb");

	if (!file || (fseek (file, up->start, SEEK_SET) < 0))
	{
		AS_ERR_1 ("Failed to open file for upload: %s", share->path);
		free (up);
		if (file)
			fclose(file);
		
		return NULL;
	}

	as_http_header_free (req);

	send_reply (up);

	up->file = file;

	up->input = input_add (up->c->fd, (void *)up, INPUT_WRITE,
			       (InputCallback)send_file, 0);

	return up;
}

void as_upload_free (ASUpload *up)
{
	tcp_close_null (&up->c);
	input_remove (up->input);
	fclose (up->file);
	free (up);
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

        /* our supernode's IP and port
	   (choose the first one in the list) */
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

static as_bool send_reply (ASUpload *up)
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

	as_http_header_set_field (reply, "Server", AS_HTTP_SERVER_NAME);
	set_header_b6mi (reply);

	sprintf (buf, "%08X", ntohl (net_local_ip (up->c->fd, NULL)));
	as_http_header_set_field (reply, "X-MyLIP", buf);
	if (AS->netinfo->nick)
		as_http_header_set_field (reply, "X-My-Nick", AS->netinfo->nick);

	str = as_http_header_compile (reply);

#if 0
	AS_DBG_1("%s", str->str);
#endif
	tcp_write (up->c, str->str, str->len);
	string_free (str);
	as_http_header_free (reply);

	return TRUE;
}

static void send_file (int fd, input_id input, ASUpload *up)
{
	unsigned int in, out, left, wanted;
	as_uint8 buf[BLOCKSIZE];

        if (net_sock_error (fd))
        {
                AS_ERR_3 ("net_sock_error %d after %u bytes for upload to %s",
                                   errno, up->sent, net_ip_str (up->c->host));
		as_upload_free (up);
		
                return;
        }
	
	wanted = BLOCKSIZE;

	left = (up->stop - up->start) - up->sent;

	if (wanted > left)
		wanted = left;

	in = fread (buf, 1, wanted, up->file);
	
	if (in < wanted)
	{
		AS_DBG_1 ("read failed from %s", up->share->path);

		as_upload_free (up);
		return;
	}

	out = tcp_send (up->c, buf, in);
	
	if (out < 0)
	{
		AS_WARN_2 ("failed to write %d bytes to %s",
			   in, net_ip_str (up->c->host));

		as_upload_free (up);
		return;
	}

	if (out < in)
	{
		AS_WARN_3 ("wrote %d of %d bytes to %s, rewinding",
			   out, in, net_ip_str (up->c->host));

		if (fseek (up->file, -((off_t)(in - out)), SEEK_CUR) < 0)
		{
			AS_ERR ("rewind failed");
			as_upload_free (up);
			return;
		}
	}

	up->sent += out;

	assert (up->sent <= up->stop - up->start);

	if (up->sent == up->stop - up->start)
	{
		AS_WARN_3 ("finished uploading %d bytes of '%s' to %s",
			   up->sent, up->share->path, net_ip_str (up->c->host));

		as_upload_free (up);
		return;
	}
}
