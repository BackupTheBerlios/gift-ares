/*
 * $Id: as_session_man.h,v 1.3 2004/09/01 18:05:55 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SESSION_MAN_H_
#define __AS_SESSION_MAN_H_

/*****************************************************************************/

typedef struct
{
	unsigned int connections; /* number of conections to keep established */

	List *connecting;         /* connecting sessions */
	List *connected;          /* established sessions */

} ASSessMan;

/*****************************************************************************/

/* allocate and init session manager */
ASSessMan *as_sessman_create ();

/* free manager */
void as_sessman_free (ASSessMan *man);

/*****************************************************************************/

/* Returns number of actually established sessions */
unsigned int as_sessman_established (ASSessMan *man);

/*****************************************************************************/

/* Set number of sessions that should be maintained at all times. Setting this
 * to zero will disconnect from the network. Anything non-zero will start
 * connecting.
 */
void as_sessman_connect (ASSessMan *man, unsigned int connections);

/* search all connected hosts, returns number of searches sent */
int as_send_search (ASSessMan *man, unsigned char *query);

/*****************************************************************************/

#endif /* __AS_SESSION_MAN_H_ */
