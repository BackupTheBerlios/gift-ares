/*
 * $Id: as_incoming.h,v 1.1 2004/09/14 12:53:07 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

int as_incoming_http (ASHttpServer *server, TCPC *tcpcon,
			ASHttpHeader *request);

int as_incoming_push (ASHttpServer *server, TCPC *tcpcon,
		      String *buf);
