/*
 * $Id: as_ares.h,v 1.7 2004/08/26 15:57:44 HEx Exp $
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#ifdef WIN32
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

#include "as_log.h"
#include "as_event.h"
#include "as_crypt.h"
#include "as_tcp.h"
#include "as_packet.h"
#include "as_session.h"
#include "as_tokenize.h"

/*****************************************************************************/

typedef struct
{
	int foo;
} ASInstance;

/*****************************************************************************/

extern ASInstance *as_instance;	/* global library instance */

#define AS (as_instance)

/*****************************************************************************/

#endif /* __A_ARES_H */
