/*
 * $Id: as_download_conn.h,v 1.1 2004/09/09 16:12:29 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_DOWNLOAD_CONN_H
#define __AS_DOWNLOAD_CONN_H

/*****************************************************************************/

typedef enum
{
	DOWNCONN_UNUSED = 0,
	DOWNCONN_CONNECTING,
	DOWNCONN_TRANSFERRING,
	DOWNCONN_FAILED,
	DOWNCONN_COMPLETE,
} ASDownConnState;

typedef struct as_down_conn_t ASDownConn;

/* Called for every state change. Return TRUE if the connection was free and
 * must no longer be accessed.
 */
typedef as_bool (*ASDownConnStateCb) (ASDownConn *conn, ASDownConnState state);

/* Called for every piece of data downloaded. Return TRUE if the connection
 * was free and must no longer be accessed.
 */
typedef as_bool (*ASDownConnDataCb) (ASDownConn *conn, as_uint16 *data,
                                     unsigned int len);

typedef struct as_down_conn_t
{
	/* data about source from search result */
	ASSource *source;

	/* the chunk we are currently downloading */
	ASDownChunk *chunk;

	/* the actually http client for downloading */
	ASHttpClient *client;

	/* some stats about this connection */
	unsigned int total_downloaded; /* total downloaded bytes */
	unsigned int average_speed;    /* average download speed in bytes/sec */


	ASDownConnState state;

	ASDownConnStateCb state_cb;
	ASDownConnDataCb  data_cb;

	void *udata; /* arbitrary user data */
};

/*****************************************************************************/

/* create new download connection from source (copies source) */
ASDownConn *as_downconn_create (ASSource *source, ASDownConnStateCb state_cb,
                                ASDownConnDataCb data_cb);

/* free download connection */
void as_downconn_free (ASDownConn *conn);

/*****************************************************************************/

/* associate this connection with chunk start download */
as_bool as_downconn_start (ASDownConn *conn, ASDownChunk *chunk);

/* stop current download disassociate from chunk */
as_bool as_downconn_stop (ASDownConn *conn);

/* returns TRUE if connection is associated with a chunk */
as_bool as_downconn_in_use (ASDownConn *conn);

/*****************************************************************************/

#endif /* __AS_DOWNLOAD_CONN_H */

