/*
 * $Id: as_ares.h,v 1.1 2004/08/20 11:55:33 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_ARES_H
#define __AS_ARES_H

/*****************************************************************************/

typedef signed char    as_int8;
typedef unsigned char  as_uint8;
typedef signed short   as_int16;
typedef unsigned short as_uint16;
typedef signed int     as_int32;
typedef unsigned int   as_uint32;
typedef int            as_bool;

#define TRUE 1
#define FALSE 0

/*****************************************************************************/

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//#include "as_log.h"
#include "as_packet.h"
//#include "as_crypt.h"

//#include "as_tcp.h"

/*****************************************************************************/

typedef struct
{

} ASInstance;

/*****************************************************************************/

extern ASInstance *as_instance;	/* global library instance */

#define AS (as_instance)

/*****************************************************************************/

#endif /* __A_ARES_H */
