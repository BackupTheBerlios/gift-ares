/*
 * $Id: as_session_man.h,v 1.1 2004/08/27 17:56:40 mkern Exp $
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
	int foo;
	
} ASSessionMan;

/*****************************************************************************/

/* allocate and init session manager */
ASSessionMan *as_session_man_create ();

/* free manager */
void as_session_man_free (ASSessionMan *man);

/*****************************************************************************/

/*****************************************************************************/

#endif /* __AS_SESSION_MAN_H_ */
