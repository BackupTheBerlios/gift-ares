/*
 * $Id: as_share.h,v 1.1 2004/09/16 02:37:01 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

/***********************************************************************/

typedef struct
{
	char    *path; /* full path */
	char    *ext;  /* pointer to within path */
	size_t   size;
	ASHash  *hash;
	ASRealm  realm;
	ASMeta  *meta;
} ASShare;

/***********************************************************************/

ASShare *share_new (char *path, ASHash *hash, ASMeta *meta,
		    size_t size);

void as_share_free (ASShare *share);

ASPacket *share_packet (ASShare *share);
