/*
 * $Id: as_upload.h,v 1.2 2004/10/24 03:45:59 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

/*****************************************************************************/

typedef struct as_upload_t ASUpload;

typedef enum {
	UPLOAD_STARTED,
	UPLOAD_SENT_DATA,
	UPLOAD_COMPLETED,
	UPLOAD_CANCELLED
} ASUploadState;

typedef as_bool (*ASUploadStateCb) (ASUpload *up, ASUploadState state);

struct as_upload_t {
	TCPC      *c;
	ASShare   *share;
	FILE      *file;
	as_uint32  start, stop, sent;
	input_id   input;
	ASUploadStateCb cb;
};

/*****************************************************************************/

ASUpload *as_upload_new (TCPC *c, ASShare *share, ASHttpHeader *req,
			 ASUploadStateCb callback);
void as_upload_free (ASUpload *up);

/*****************************************************************************/
