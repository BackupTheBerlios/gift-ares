/*
 * $Id: as_download_conn.h,v 1.9 2004/09/19 17:53:43 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_DOWNLOAD_CONN_H
#define __AS_DOWNLOAD_CONN_H

/*****************************************************************************/

/* TODO:
 *   - calculate average speed
 */

/*****************************************************************************/

typedef enum
{
	DOWNCONN_UNUSED,        /* Initial state and state after canceling */
	DOWNCONN_CONNECTING,    /* We are connecting/requesting */
	DOWNCONN_TRANSFERRING,  /* We are receiving data. */
	DOWNCONN_FAILED,        /* Connect or request failed. */
	DOWNCONN_COMPLETE,      /* Requested chunk completed or connection was
	                         * closed. If possible the connection is kept
	                         * alive. */
	DOWNCONN_QUEUED         /* We are in the source's upload queue. Connection
	                         * is kept alive if possible. */
} ASDownConnState;

typedef struct as_down_conn_t ASDownConn;

/* Called for every state change. Return FALSE if the connection was freed and
 * must no longer be accessed.
 */
typedef as_bool (*ASDownConnStateCb) (ASDownConn *conn, ASDownConnState state);

/* Called for every piece of data downloaded. Return FALSE if the connection
 * was freed and must no longer be accessed.
 */
typedef as_bool (*ASDownConnDataCb) (ASDownConn *conn, as_uint8 *data,
                                     unsigned int len);

struct as_down_conn_t
{
	/* data about source from search result */
	ASSource *source;

	/* the hash and chunk we are currently downloading */
	ASHash *hash;
	size_t chunk_start;
	size_t chunk_size;

	/* the actually http client for downloading */
	ASHttpClient *client;
	as_bool pushed;        /* TRUE if the connection was pushed to us */

	/* remote queue handling */
	unsigned int queue_pos;        /* our position in source's queue */
	unsigned int queue_len;        /* length source's queue */
	time_t queue_last_try;         /* last time we tried and got queue_pos */
	time_t queue_next_try;         /* time we were told to try next */

	/* some stats about this connection */
	unsigned int total_downloaded; /* total downloaded bytes */
	unsigned int average_speed;    /* average download speed in bytes/sec */
	unsigned int fail_count;       /* number of times a request has failed
	                                * for this connection */

	/* conection state */
	ASDownConnState state;

	ASDownConnStateCb state_cb;
	ASDownConnDataCb  data_cb;

	void *udata1; /* arbitrary user data */
	void *udata2;
};

/*****************************************************************************/

/* Create new download connection from source (copies source). */
ASDownConn *as_downconn_create (ASSource *source, ASDownConnStateCb state_cb,
                                ASDownConnDataCb data_cb);

/* Create new download connection from pushed connection. */
ASDownConn *as_downconn_create_tcpc (TCPC *c, ASDownConnStateCb state_cb,
                                     ASDownConnDataCb data_cb);

/* Free download connection. */
void as_downconn_free (ASDownConn *conn);

/*****************************************************************************/

/* Start this download from this connection with specified piece and hash. */
as_bool as_downconn_start (ASDownConn *conn, ASHash *hash, size_t start,
                           size_t size);

/* Stop current download. Does not raise callback. State is set to
 * DOWNCONN_UNUSED. 
 */
void as_downconn_cancel (ASDownConn *conn);

/*****************************************************************************/

/* Return connection state as human readable static string. */
const char *as_downconn_state_str (ASDownConn *conn);

/*****************************************************************************/

#endif /* __AS_DOWNLOAD_CONN_H */

