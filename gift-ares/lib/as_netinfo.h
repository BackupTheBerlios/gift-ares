/*
 * $Id: as_netinfo.h,v 1.4 2004/10/10 15:22:48 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_NETINFO_H
#define __AS_NETINFO_H

/*****************************************************************************/

typedef struct as_net_info_t ASNetInfo;

/* Called whenever user, files, size or connect state changes */
typedef void (*ASNetInfoStatsCb) (ASNetInfo *info);

struct as_net_info_t
{
	unsigned int conn_want; /* number of supernode connections we want */
	unsigned int conn_have; /* number of supernode connections we have */
	unsigned int users;     /* users on network */
	unsigned int files;     /* files on network */
	unsigned int size;      /* total network size in GB */

	ASNetInfoStatsCb stats_cb;

	in_addr_t outside_ip; /* out ip from the outside */
	in_port_t port; /* port we're listening on */

	as_uint32 unk1; /* always 0x400? */
	as_uint16 unk2; /* unknown */
};

/*****************************************************************************/

/* allocate and init network info */
ASNetInfo *as_netinfo_create ();

/* free manager */
void as_netinfo_free (ASNetInfo *info);

/*****************************************************************************/

/* set callback for stats changes */
void as_netinfo_set_stats_cb (ASNetInfo *info, ASNetInfoStatsCb stats_cb);

/*****************************************************************************/

/* handle connect state change */
void as_netinfo_handle_connect (ASNetInfo *info, unsigned int conn_want,
                                unsigned int conn_have);

/* handle stats packet */
as_bool as_netinfo_handle_stats (ASNetInfo *info, ASSession *session,
                                 ASPacket *packet);

/* handle outside ip packet */
as_bool as_netinfo_handle_ip (ASNetInfo *info, ASSession *session,
                              ASPacket *packet);

/*****************************************************************************/

#endif /* __AS_NETINFO_H */
