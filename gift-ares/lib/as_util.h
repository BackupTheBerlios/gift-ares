/*
 * $Id: as_util.h,v 1.2 2004/12/04 11:37:41 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_UTIL_H
#define __AS_UTIL_H

/*****************************************************************************/

/* returns TRUE if ip is routable on the internet */
as_bool net_ip_routable (in_addr_t ip);

/*****************************************************************************/

/* These functions are not in libgift */

/* Insert link after prev. Returns prev. */
List *list_insert_link (List *prev, List *link);

/* Remove link from list but do not free it. */
List *list_unlink_link (List *head, List *link);

/* Same as list_insert_sorted but uses supplied link instead of creating a
 * new one.
 */
List *list_insert_link_sorted (List *head, CompareFunc func, List *link);

/*****************************************************************************/

#endif /* __AS_UTIL_H */
