/*
 * $Id: as_download.c,v 1.3 2004/09/09 21:50:46 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"
#include "as_encoding.h"

static int download_client_callback (ASHttpClient *client, ASHttpClientCbCode code);

ASDownload *as_download_new (ASSource *source, ASHash *hash, unsigned char *filename)
{
	char uri[AS_HASH_BASE64_SIZE+5];
	as_uint8 *encoded;
	ASHttpClient *client;
	ASDownload   *dl;
	FILE *f;

	encoded = as_hash_encode (hash);
	sprintf (uri, "sha1:%s", encoded);
	free (encoded);

	client = as_http_client_create (net_ip_str (source->host), source->port, (ASHttpClientCallback)download_client_callback);

	if (!client)
		return NULL;

	if (!(f =fopen (filename, "wb")))
	{	
		as_http_client_free (client);
		return NULL;
	}

	dl = malloc (sizeof (ASDownload));

	dl->filename = dl->filename;
	dl->source = source;
	dl->hash = hash;
	dl->c = NULL;
	dl->size = 0;
	dl->retrieved = 0;
	dl->timer = 0;
	dl->request = NULL;
	dl->client = client;
	dl->uri = strdup (uri);
	dl->f = f;

	return dl;
}

static as_bool set_header_encoded (ASHttpHeader *header, char *name, ASPacket *packet)
{
	char *encoded = as_base64_encode (packet->data, packet->used);
	
	if (encoded)
	{
		AS_DBG_2 ("%s: %s", name, encoded);
		as_http_header_set_field (header, name, encoded);
		free (encoded);

		return TRUE;
	}

	return FALSE;
}

static void set_b6mi (ASDownload *dl)
{
	ASPacket *p;
	ASSession *super = NULL;

	p = as_packet_create ();

	/* our IP and port */
	as_packet_put_ip (p, AS->netinfo->outside_ip);
	as_packet_put_le16 (p, 0xDEAD); /* FIXME */

	/* and our supernode's (well, one of them...) */
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

	as_encrypt_b6mi (p->data, p->used);
	set_header_encoded (dl->request, "X-B6MI", p);
	
	as_packet_free (p);
}

static void set_b6st (ASDownload *dl)
{
	ASPacket *p = as_packet_create ();

	as_packet_put_8 (p, 0); /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_8 (p, 1); /* unknown */
	as_packet_put_8 (p, 0); /* % complete? */
	as_packet_put_le32 (p, 0); /* zero */
	as_packet_put_le32 (p, 0); /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_8 (p, 0x11); /* hardcoded */
	as_packet_put_le16 (p, 2); /* unknown */
	as_packet_put_8 (p, 0); /* unknown */
	as_packet_put_8 (p, 0); /* unknown */
	as_packet_put_8 (p, 0x80); /* unknown */
	
	as_encrypt_b6st (p->data, p->used);
	set_header_encoded (dl->request, "X-B6St", p);
	
	as_packet_free (p);
}

as_bool as_download_start (ASDownload *dl)
{
	char range[28];
	as_bool ret;

	dl->request = as_http_header_request (HTHD_VER_11, HTHD_GET, dl->uri);
	
	if (!dl->request)
		return FALSE;

	if (dl->size && dl->retrieved)
	{
		sprintf(range, "bytes=%d-%d", dl->retrieved, dl->size);
		AS_DBG_1 ("requesting range %s", range);
		as_http_header_set_field (dl->request, "Range", range);
	}
	set_b6mi (dl);
	set_b6st (dl);

	dl->client->udata = dl;

	ret = as_http_client_request (dl->client, dl->request, TRUE);
	if (!ret)
		AS_WARN ("request init failed"); 
	else
		AS_DBG ("connecting");

	return FALSE;
}

static int download_client_callback (ASHttpClient *client, ASHttpClientCbCode code)
{
	ASDownload *dl = client->udata;

	switch (code)
	{
        case HTCL_CB_CONNECT_FAILED:
		AS_WARN ("connect failed");
		break;
        case HTCL_CB_REQUESTING:
		AS_DBG ("requesting");
		break;
        case HTCL_CB_REQUEST_FAILED:
		AS_WARN ("request failed");
		break;
        case HTCL_CB_REPLIED:
	{
		ASHttpHeader *reply = client->reply;
		char *p;

		switch (reply->code)
		{
		case 200:
		case 206:
			break;
		case 404:
			AS_WARN_1 ("not found", reply->code);
			return FALSE;
		case 503:
		{
			int pos, len, limit, min, max;
			int retry = 120 * SECONDS;

			p = as_http_header_get_field (reply, "X-Queued");

			if (p && sscanf (p, "position=%u,length=%u,limit=%u,pollMin=%u,pollMax=%u", 
					 &pos, &len, &limit, &min, &max) == 5)
				retry = ((min + max)/2) * SECONDS;

			/* queued, wait to retry */
			AS_WARN_1 ("queued: %s", p);
			dl->timer = timer_add (retry, (TimerCallback)as_download_start, dl);
			return TRUE; /* this will keep the connection open, with any luck */
		}
		default:
			AS_WARN_1 ("unknown http response code %d", reply->code);
			return FALSE;
		}

		{
			int start, stop, size;
			p = as_http_header_get_field (reply, "Content-Range");
			if (p  && sscanf (p, "bytes=%d-%d/%d", &start, &stop, &size) == 3)
			{
				if (size)
					dl->size = size;

				assert (dl->retrieved < dl->size);
			}
		}
		
		printf ("got HTTP reply\n");
		return TRUE;
	}
        case HTCL_CB_DATA:
		dl->retrieved += client->data_len;
		fwrite (client->data, client->data_len, 1, dl->f);
		break;

        case HTCL_CB_DATA_LAST:
		dl->retrieved += client->data_len;
		fwrite (client->data, client->data_len, 1, dl->f);

		if (dl->retrieved < dl->size)
		{
			AS_WARN ("retrying");
			dl->timer = timer_add (30*SECONDS, (TimerCallback)as_download_start, dl);
		}
		break;
	}

	return TRUE;
}
