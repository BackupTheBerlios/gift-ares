/*
 * $Id: as_source.c,v 1.3 2004/09/07 13:05:33 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* create new source */
ASSource *as_source_create ()
{
	ASSource *source;

	if (!(source = malloc (sizeof (ASSource))))
		return NULL;

	source->host = INADDR_NONE;
	source->port = 0;
	source->shost = INADDR_NONE;
	source->sport = 0;
	source->username = NULL;
	source->parent_host = INADDR_NONE;

	return source;
}

/* create copy of source */
ASSource *as_source_copy (ASSource *source)
{
	ASSource *cpy;

	if (!(cpy = as_source_create ()))
		return NULL;

	cpy->host = source->host;
	cpy->port = source->port;
	cpy->shost = source->host;
	cpy->sport = source->sport;
	cpy->username = gift_strdup (source->username);
	cpy->parent_host = source->parent_host;

	return cpy;
}

/* free source */
void as_source_free (ASSource *source)
{
	if (!source)
		return;

	free (source->username);
	free (source);
}

/*****************************************************************************/

/* returns TRUE if the sources are the same */
as_bool as_source_equal (ASSource *a, ASSource *b)
{
	if (!a || !b)
		return FALSE;

	return (a->host        == b->host  &&
	        a->port        == b->port  &&
	        a->shost       == b->shost &&
	        a->sport       == b->sport &&
	        a->parent_host == b->parent_host &&
	        gift_strcmp (a->username, b->username) == 0); 
}

/* returns TRUE if the source is firewalled */
as_bool as_source_firewalled (ASSource *source)
{
	return (!net_ip_routable (source->host)) || (source->port == 0);
}

/* returns TRUE if the source has enough info to send a push */
as_bool as_source_has_push_info (ASSource *source)
{
	return (net_ip_routable (source->shost) && source->sport != 0 &&
	        net_ip_routable (source->parent_host) &&
			source->username && source->username[0] != '\0');
}

/*****************************************************************************/

#if 0

/* create source from gift url */
ASSource *as_source_unserialize (const char *str)
{

}

/* create url for gift. Caller frees returned string. */
char *as_source_serialize (ASSource *source)
{

}

#endif

/*****************************************************************************/
