/*
 * $Id: as_source.h,v 1.1 2004/09/06 12:58:26 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SOURCE_H
#define __AS_SOURCE_H

/*****************************************************************************/

typedef struct
{
	/* source host and port */
	in_addr_t host;
	in_port_t port; 
	
	/* source's supernode host and port */
	in_addr_t shost;
	in_addr_t sport;

	/* source's username */
	unsigned char *username;

	/* supernode we got this source from */
	in_addr_t parent_host;

} ASSource;

/*****************************************************************************/

/* create new source */
ASSource *as_source_create ();

/* create copy of source */
ASSource *as_source_copy (ASSource *source);

/* free source */
void as_source_free (ASSource *source);

/*****************************************************************************/

/* returns TRUE if the sources are the same */
as_bool as_source_equal (ASSource *a, ASSource *b);

/* returns TRUE if the source is firewalled */
as_bool as_source_firewalled (ASSource *source);

/* returns TRUE if the source has enough info to send a push */
as_bool as_source_has_push_info (ASSource *source);

/*****************************************************************************/

#if 0

/* create source from gift url */
ASSource *as_source_unserialize (const char *str);

/* create url for gift. Caller frees returned string. */
char *as_source_serialize (ASSource *source);

#endif

/*****************************************************************************/

#endif /* __AS_SOURCE_H */
