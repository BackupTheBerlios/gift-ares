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

#endif
