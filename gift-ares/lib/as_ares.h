/*
 * $Id: as_ares.h,v 1.70 2007/01/14 14:29:31 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_ARES_H
#define __AS_ARES_H

/*****************************************************************************/

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <assert.h>

#include <zlib.h>

#ifndef WIN32
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <unistd.h>
#else /* WIN32 */
# include <winsock2.h>
#endif /* WIN32 */

/* assume we have this on non-win32 systems (it's POSIX after all), in
 * the absence of any autoconf-ish setup that could detect it for us */
#ifndef WIN32
#define HAVE_DIRENT_H
#endif

#ifdef HAVE_DIRENT_H
# include <dirent.h> /* opendir, readdir... */
#else
# include <io.h> /* _findfirst and friends */
#endif

#ifdef GIFT_PLUGIN
#include <libgift/libgift.h>
#include <libgift/proto/protocol.h>
#include <libgift/proto/share.h>
#include <libgift/file.h>
#include <libgift/mime.h>
#include <libgift/proto/if_event_api.h>
#define INVALID_INPUT 0
#define INVALID_TIMER 0
#endif

/*****************************************************************************/

typedef signed char    as_int8;
typedef unsigned char  as_uint8;
typedef signed short   as_int16;
typedef unsigned short as_uint16;
typedef signed int     as_int32;
typedef unsigned int   as_uint32;
typedef int            as_bool;

/* TODO: get from autoconf later */
#ifdef WIN32
/* u_long and u_short are defined in winsock.h */
# define in_addr_t u_long
# define in_port_t u_short

# define vsnprintf _vsnprintf
# define snprintf _snprintf

#else
# define in_addr_t as_uint32
# define in_port_t as_uint16
#endif

#ifndef S_ISREG
# define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#ifndef VA_COPY
# ifdef _MSC_VER
#  define VA_COPY(d,s) (d) = (s)
# else
#  define VA_COPY __va_copy
# endif
#endif /* !VA_COPY */

#define TRUE 1
#define FALSE 0

/*****************************************************************************/

/* beware the ordering of these */
#include "as_log.h"
#include "as_file.h"
#include "as_hashtable.h"

#ifndef GIFT_PLUGIN
#  include "as_list.h"
#  include "as_parse.h"
#  include "as_strobj.h"
#  include "as_event.h"
#  include "as_tcp.h"
#endif

#include "as_util.h"
#include "as_config.h"
#include "as_sha1.h"
#include "as_encoding.h"
#include "as_hash.h"
#include "as_packet.h"
#include "as_crypt.h"
#include "as_node.h"
#include "as_node_man.h"
#include "as_session.h"
#include "as_session_man.h"
#include "as_netinfo.h"
#include "as_source.h"
#include "as_meta.h"
#include "as_tokenize.h"
#include "as_search_result.h"
#include "as_search.h"
#include "as_search_man.h"
#include "as_http_header.h"
#include "as_http_client.h"
#include "as_http_server.h"
#include "as_push.h"
#include "as_push_man.h"
#include "as_download_chunk.h"
#include "as_download_conn.h"

#ifndef GIFT_PLUGIN
#  include "as_download.h"
#  include "as_download_state.h"
#  include "as_download_man.h"
#else
typedef void ASDownMan;
#endif

#include "as_incoming.h"
#include "as_share.h"
#include "as_share_man.h"
#include "as_upload.h"
#include "as_upload_man.h"
#include "as_push_reply.h"

/*****************************************************************************/

/* IDs of config variables. See as_ares.c for definitions. */

typedef enum
{
	AS_LISTEN_PORT                 = 0, /* HTTP server port. */
	AS_USER_NAME                   = 1, /* User name. */
	AS_DOWNLOAD_MAX_ACTIVE         = 2, /* Limit for the number of max active
	                                     * downloads at any given time. */
	AS_DOWNLOAD_MAX_ACTIVE_SOURCES = 3, /* Maximum number of active
	                                     * connections per download. */
	AS_UPLOAD_MAX_ACTIVE           = 4, /* Maximum number of simultaneous
	                                     * uploads. */
	AS_SEARCH_TIMEOUT              = 5  /* Time from first sent query packet
	                                     * to timeout of search in seconds. */
} ASAresConfigValues;

/*****************************************************************************/

#ifdef GIFT_PLUGIN
extern Protocol *gift_proto;
#endif

/* The client name we send to supernodes. After the initial release of this
 * plugin the author of Ares released a new version (build 2951) which blocked
 * certain client names ("aREs", "Warez", ?) from Ares supernodes by simply
 * not sending them any search results. For this reason we now have to keep
 * our client name at "Ares" to prevent future problems effectively rendering
 * client identification useless. Why the author of Ares would cripple his own
 * network in this way is beyond me.
 */
#define AS_CLIENT_NAME "Ares"

/* The login string was added in Ares build 2951 probably to block out other
 * clients in the future. It is sent to the supernode in encrypted form during
 * handshaking. Just like the client name we now have to keep it in sync with
 * the latest Ares client. Yes, it's pointless.
 *
 * Update: As of Ares build 2955 this has been removed again. Lets hope this
 * is the beginning of a peaceful coexistence.
 */
#if 0
# define AS_LOGIN_STRING "Ares 1.8.1.2951"
#endif

/* The user agent string we send with encrypted download requests. Ignored in
 * Open Source Ares but the build number may habe been used to discriminate
 * against us in older versions.
 */
#define AS_DOWNLOAD_AGENT "Ares 1.8.1.2958"

/* The server name returned in upload replies. Ares displays this (up to the
 * first slash or space) as the network name, so we should be consistent with
 * what we tell supernodes.
 */
#ifdef GIFT_PLUGIN
#  define AS_UPLOAD_AGENT AS_CLIENT_NAME " (libares; " PACKAGE "/" VERSION ")"
#else
#  define AS_UPLOAD_AGENT AS_CLIENT_NAME " (libares)"
#endif

/* Timeout for supernode tcp connections. */
#define AS_SESSION_CONNECT_TIMEOUT (20 * SECONDS)

/* Timeout for supernode handshake after connecting */
#define AS_SESSION_HANDSHAKE_TIMEOUT (30 * SECONDS)

/* Time between stats requests (pings) sent to supernode. Ares seems to
 * disconnects us if we don't sent a ping every 5 minutes. */
#define AS_SESSION_PING_INTERVAL (3 * MINUTES)

/* Timeout for ping replies */
#define AS_SESSION_PING_TIMEOUT (2 * MINUTES)

/* Number of simultaneous connection attempts when connecting to supernodes */
#define AS_SESSION_PARALLEL_ATTEMPTS (10)

/* Maximum number of nodes saved in node file */
#define AS_MAX_NODEFILE_SIZE (400)

/* Number of supernodes each search is sent to. If zero the search will be
 * sent to all available sessions and all new ones becoming available while
 * the search is running.
 */
#define AS_SEARCH_SEND_COUNT (0)

/* Minimum chunk size in bytes */
#define AS_DOWNLOAD_MIN_CHUNK_SIZE (128*1024)

/* Number of request fails after which a source is removed */
#define AS_DOWNLOAD_SOURCE_MAX_FAIL (2)

/* Time between download manager's progress callbacks */
#define AS_DOWNLOAD_PROGRESS_INTERVAL (1 * SECONDS)

/* Filename prefix for incomplete files */
#define AS_DOWNLOAD_INCOMPLETE_PREFIX "___ARESTRA___"

/* Minimum and maximum periods between queue pings, in seconds */
#define AS_UPLOAD_QUEUE_MIN     60
#define AS_UPLOAD_QUEUE_MAX     180

/* How long before we remove an entry from the queue, in seconds */ 
#define AS_UPLOAD_QUEUE_TIMEOUT 180

/* Time between upload manager's progress callbacks */
#define AS_UPLOAD_PROGRESS_INTERVAL (1 * SECONDS)

/* If AS_UPLOAD_KEEP_ALIVE is defined we do not close connections after
 * upload but return them to the http server so a new request can be received.
 */
#define AS_UPLOAD_KEEP_ALIVE

/* Timeout for tcp connect to firewalled source's supernode. */
#define AS_PUSH_CONNECT_TIMEOUT (20 * SECONDS)

/* Timeout for push after request was sent */
#define AS_PUSH_TIMEOUT (30 * SECONDS)

/* Uncomment to compress arbitrary sufficiently-large packets, just
   because we can (it's unlikely to improve efficiency).  This might
   affect compatibility with other clients. */
/* define AS_COMPRESS_ALL */

/*****************************************************************************/

typedef struct
{
	/* config */
	ASConfig *config;

	/* node manager */
	ASNodeMan *nodeman;

	/* session manager */
	ASSessMan *sessman;

	/* network info */
	ASNetInfo *netinfo;

	/* search manager */
	ASSearchMan *searchman;

	/* download manager */
	ASDownMan *downman;

	/* upload manager */
	ASUpMan *upman;

	/* manager for pushes we requested */
	ASPushMan *pushman;

	/* manager for pushes we make to other hosts */
	ASPushReplyMan *pushreplyman;

	/* share manager */
	ASShareMan *shareman;

	/* HTTP (and other stuff) server */
	ASHttpServer *server;
} ASInstance;

/*****************************************************************************/

extern ASInstance *as_instance;	/* global library instance */

#define AS (as_instance)
#define AS_CONF (as_instance->config)

/* Easier access to config variables */
#define AS_CONF_INT(id) (as_config_get_int (AS_CONF, id))
#define AS_CONF_STR(id) (as_config_get_str (AS_CONF, id))

/*****************************************************************************/

/* NOTE: The library expects networking, logging, and the event system to be
 * set up and ready to use.
 */

/* Create library instance and initialize it. There can only be one at a time
 * though.
 */
as_bool as_init ();

/* Clean up library instance */
as_bool as_cleanup ();

/* Start connecting, resume downloads, etc. */
as_bool as_start ();

/* Drop all connections, stop downloads, etc. */
as_bool as_stop ();

/*****************************************************************************/

#endif /* __AS_ARES_H */
