/*
 * $Id: as_share.h,v 1.3 2004/09/16 17:47:31 HEx Exp $
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

ASShare *as_share_new (char *path, ASHash *hash, ASMeta *meta,
		    size_t size, ASRealm realm);

void as_share_free (ASShare *share);

ASPacket *as_share_packet (ASShare *share);
