/*
 * $Id: as_upload.h,v 1.1 2004/10/03 17:37:49 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

/*****************************************************************************/

typedef struct {
	TCPC      *c;
	ASShare   *share;
	FILE      *file;
	as_uint32  start, stop, sent;
	input_id   input;
} ASUpload;

/*****************************************************************************/

ASUpload *as_upload_new (TCPC *c, ASShare *share, ASHttpHeader *req);
void as_upload_free (ASUpload *up);

/*****************************************************************************/
