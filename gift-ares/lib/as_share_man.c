/*
 * $Id: as_share_man.c,v 1.1 2004/09/16 15:46:30 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/***********************************************************************/

ASShareMan *as_shareman_create (void)
{
	ASShareMan *man = malloc (sizeof (ASShareMan));

	if (!man)
		return NULL;

	man->shares  = NULL;
	man->table   = as_hashtable_create_mem (FALSE);
	man->nshares = 0;
	man->size    = 0;

	return man;
}

static int free_share (ASShare *share, void *udata)
{
	as_share_free (share);

	return TRUE;
}

void as_shareman_free (ASShareMan *man)
{
	as_hashtable_free (man->table, FALSE);
	list_foreach_remove (man->shares, (ListForeachFunc)free_share, NULL);
	free (man);
}

as_bool as_shareman_add (ASShareMan *man, ASShare *share)
{
	if (!as_hashtable_insert (man->table, share->hash->data,
			     sizeof (share->hash->data), share))
		return FALSE;

	man->shares = list_prepend (man->shares, share);
	man->nshares++;
	man->size += ((double)share->size) / 1048576;

	return TRUE;
}

as_bool as_shareman_remove (ASShareMan *man, ASShare *share)
{
	ASShare *s;

	/* this might fail if there are duplicates */
	if ((s = as_hashtable_lookup (man->table, share->hash->data,
				      sizeof (share->hash->data))) &&
	    s == share)
		as_hashtable_remove (man->table, share->hash->data,
				     sizeof (share->hash->data));
	    
	/* the important one, but we don't get to know if it worked or
	 * not :( */
	man->shares = list_remove (man->shares, share);

	man->nshares--;
	man->size -= ((double)share->size) / 1048576;
	as_share_free (share);

	return TRUE;
}


static int share_send (ASShare *share, ASSession *session)
{
	ASPacket *p = share_packet (share);
	
	if (!p)
		return FALSE;

#if 0
	return as_session_send (session, PACKET_SHARE, p, PACKET_ENCRYPT);
#else
	as_packet_header (p, PACKET_SHARE);

	/* FIXME: chunk these */
	return as_session_send (session, PACKET_COMPRESSED, p, PACKET_COMPRESS);
#endif
}

as_bool as_shareman_submit (ASShareMan *man, ASSession *session)
{
	/* dammit, WTF does this return void?! */
	list_foreach (man->shares, (ListForeachFunc)share_send, session);

	return TRUE;
}
