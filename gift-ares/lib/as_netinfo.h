/*
 * $Id: as_netinfo.h,v 1.2 2004/09/14 01:18:26 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_NETINFO_H
#define __AS_NETINFO_H

/*****************************************************************************/

typedef struct
{
	unsigned int users;   /* users on network */
	unsigned int files;   /* files on network */
	unsigned int size;    /* total network size in GB */

	in_addr_t outside_ip; /* out ip from the outside */
	in_port_t port; /* port we're listening on */

} ASNetInfo;

/*****************************************************************************/

/* allocate and init network info */
ASNetInfo *as_netinfo_create ();

/* free manager */
void as_netinfo_free (ASNetInfo *info);

/*****************************************************************************/

/* handle stats packet */
as_bool as_netinfo_handle_stats (ASNetInfo *info, ASSession *session,
                                 ASPacket *packet);

/* handle outside ip packet */
as_bool as_netinfo_handle_ip (ASNetInfo *info, ASSession *session,
                              ASPacket *packet);

/*****************************************************************************/

#endif /* __AS_NETINFO_H */
