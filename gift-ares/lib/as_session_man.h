/*
 * $Id: as_session_man.h,v 1.2 2004/08/31 17:44:18 mkern Exp $
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

/*****************************************************************************/

#endif /* __AS_SESSION_MAN_H_ */
