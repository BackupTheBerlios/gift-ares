/*
 * $Id: as_upload_man.h,v 1.2 2004/10/26 21:25:52 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

typedef struct {
	ASHashTable *uploads; /* uploads keyed by host as an integer */
	List *queue; /* list of struct queues */
	
	int max;
	int nuploads;
	int nqueued;
	int bandwidth;
} ASUploadMan;

ASUploadMan *as_upman_create (void);
void as_upman_free (ASUploadMan *man);

int as_upman_auth (ASUploadMan *man, in_addr_t host);

/* create and register a new upload */
ASUpload *as_upman_start (ASUploadMan *man, TCPC *c,
			  ASHttpHeader *req);
