/*
 * $Id: as_download_conn.h,v 1.2 2004/09/09 16:55:46 mkern Exp $
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
	DOWNCONN_UNUSED,        /* Initial state and state after canceling */
	DOWNCONN_CONNECTING,    /* We are connecting/requesting */
	DOWNCONN_TRANSFERRING,  /* We are receiving data. */
	DOWNCONN_FAILED,        /* Connect or request failed. Chunk is
	                         * disassociated */
	DOWNCONN_COMPLETE,      /* Requested chunk completed. If possible the
	                         * connection is kept alive but the chunk is
	                         * disassociated. */
	DOWNCONN_QUEUED,        /* We are in the source's upload queue. Connection
	                         * is kept alive if possible but chunk is
	                         * disassociated. */
} ASDownConnState;

typedef struct as_down_conn_t ASDownConn;

/* Called for every state change. Return TRUE if the connection was free and
 * must no longer be accessed.
 */
typedef as_bool (*ASDownConnStateCb) (ASDownConn *conn, ASDownConnState state);

/* Called for every piece of data downloaded. Return TRUE if the connection
 * was free and must no longer be accessed.
 */
typedef as_bool (*ASDownConnDataCb) (ASDownConn *conn, as_uint8 *data,
                                     unsigned int len);

typedef struct as_down_conn_t
{
	/* data about source from search result */
	ASSource *source;

	/* the chunk we are currently downloading */
	ASDownChunk *chunk;

	/* the actually http client for downloading */
	ASHttpClient *client;

	/* remote queue handling */
	unsigned int queue_pos;        /* our position in source's queue */
	time_t queue_last_try;         /* last time we tried and got queue_pos */
	time_t queue_next_try;         /* time we were told to try next */

	/* some stats about this connection */
	unsigned int total_downloaded; /* total downloaded bytes */
	unsigned int average_speed;    /* average download speed in bytes/sec */

	/* conection state */
	ASDownConnState state;

	ASDownConnStateCb state_cb;
	ASDownConnDataCb  data_cb;

	void *udata; /* arbitrary user data */
};

/*****************************************************************************/

/* Create new download connection from source (copies source). */
ASDownConn *as_downconn_create (ASSource *source, ASDownConnStateCb state_cb,
                                ASDownConnDataCb data_cb);

/* Free download connection. */
void as_downconn_free (ASDownConn *conn);

/*****************************************************************************/

/* Associate this connection with chunk start download. */
as_bool as_downconn_start (ASDownConn *conn, ASDownChunk *chunk);

/* Stop current download and disassociate from chunk. Does not raise callback.
 * State is set to DOWNCONN_UNUSED. 
 */
as_bool as_downconn_cancel (ASDownConn *conn);

/* Returns TRUE if connection is associated with a chunk. */
as_bool as_downconn_in_use (ASDownConn *conn);

/*****************************************************************************/

#endif /* __AS_DOWNLOAD_CONN_H */
