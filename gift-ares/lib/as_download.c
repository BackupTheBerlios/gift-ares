/*
 * $Id: as_download.c,v 1.2 2004/09/07 13:14:04 mkern Exp $
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
	ASHttpHeader *req;
	ASHttpClient *client;
	ASDownload   *dl;

	encoded = as_hash_encode (hash);
	sprintf (uri, "sha1:%s", encoded);
	free (encoded);

	req = as_http_header_request (HTHD_VER_11, HTHD_GET, uri);
	
	if (!req)
		return NULL;

	client = as_http_client_create (net_ip_str (source->host), source->port, (ASHttpClientCallback)download_client_callback);

	if (!client)
	{
		as_http_header_free (req);
		return NULL;
	}

	if (!fopen (filename, "wb"))
	{	
		as_http_header_free (req);
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
	dl->request = req;
	dl->client = client;

	return dl;
}

as_bool as_download_start (ASDownload *dl)
{
	char range[28];
	as_bool ret;
	if (dl->size && dl->retrieved)
	{
		sprintf(range, "bytes=%d-%d", dl->retrieved, dl->size);
		AS_WARN_1 ("requesting range %s", range);
		as_http_header_set_field (dl->request, "Range", range);
	}

	dl->client->udata = dl;

	ret = as_http_client_request (dl->client, dl->request, TRUE);

	return ret;
}

static int download_client_callback (ASHttpClient *client, ASHttpClientCbCode code)
{
	ASDownload *dl = client->udata;

	switch (code)
	{
        case HTCL_CB_CONNECT_FAILED:
		break;
        case HTCL_CB_REQUESTING:
		break;
        case HTCL_CB_REQUEST_FAILED:
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
			/* FIXME: get retry interval from headers */
#if 0
			p = as_http_header_get_field (reply, "");
#endif
			/* queued, wait to retry */
			dl->timer = timer_add (30*SECONDS, (TimerCallback)as_download_start, dl);
			return FALSE;
		}
		default:
			AS_WARN_1 ("unknown http response code %d", reply->code);
			return FALSE;
		}

		{
			int start, stop, size;
			p = as_http_header_get_field (reply, "Content-Range");
			if (sscanf (p, "bytes=%d-%d/%d", &start, &stop, &size) == 3)
				if (size)
					dl->size = size;

			assert (dl->retrieved < dl->size);
		}
		
		printf ("got HTTP reply\n");
		return TRUE;
	}
        case HTCL_CB_DATA:
	{
		fwrite (client->data, client->data_len, 1, dl->f);
		break;
	}
        case HTCL_CB_DATA_LAST:
		
		dl->timer = timer_add (30*SECONDS, (TimerCallback)as_download_start, dl);
	}

	return TRUE;
}
