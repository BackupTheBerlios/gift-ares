/*
 * $Id: as_download_man.h,v 1.1 2004/09/09 16:12:29 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_DOWNLOAD_MAN_H
#define __AS_DOWNLOAD_MAN_H

/*****************************************************************************/

typedef struct
{
	int foo;

} ASDownMan;

/*****************************************************************************/

/* allocate and init download manager */
ASDownMan *as_downman_create ();

/* free manager */
void as_downman_free (ASDownMan *man);

/*****************************************************************************/

#endif /* __AS_DOWNLOAD_MAN_H */
