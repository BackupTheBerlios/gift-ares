/*
 * $Id: as_ares.h,v 1.16 2004/09/04 18:21:51 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_ARES_H
#define __AS_ARES_H

/*****************************************************************************/

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <assert.h>

#ifndef WIN32
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <unistd.h>
#else /* WIN32 */
# include <winsock.h>
#endif /* WIN32 */

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


#define TRUE 1
#define FALSE 0

/*****************************************************************************/

#include "as_sha1.h"
#include "as_list.h"
#include "as_hashtable.h"
#include "as_log.h"
#include "as_event.h"
#include "as_crypt.h"
#include "as_tcp.h"
#include "as_parse.h"
#include "as_strobj.h"
#include "as_packet.h"
#include "as_http_header.h"
#include "as_http_client.h"
#include "as_http_server.h"
#include "as_tokenize.h"
#include "as_search.h"
#include "as_node.h"
#include "as_node_man.h"
#include "as_session.h"
#include "as_session_man.h"

/*****************************************************************************/

/* The client name we send to supernodes */
#define AS_CLIENT_NAME "aREs"

/* Timeout for supernode tcp connections. */
#define AS_SESSION_CONNECT_TIMEOUT (20 * SECONDS)

/* Timeout for supernode handshake after connecting */
#define AS_SESSION_HANDSHAKE_TIMEOUT (30 * SECONDS)

/* Number of simultaneous connection attemps when connecting to supernodes */
#define AS_SESSION_PARALLEL_ATTEMPTS (3)

/* Maximum number of nodes saved in node file */
#define AS_MAX_NODEFILE_SIZE (400)

/*****************************************************************************/

typedef struct
{
	/* node manager */
	ASNodeMan *nodeman;

	/* session manager */
	ASSessMan *sessman;


} ASInstance;

/*****************************************************************************/

extern ASInstance *as_instance;	/* global library instance */

#define AS (as_instance)

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
