/*
 * $Id: as_session.h,v 1.1 2004/08/26 16:05:33 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

typedef enum {
	STATE_DISCONNECTED,
	STATE_HANDSHAKING,
	STATE_CONNECTED
} ASSessionState;

typedef struct {
	TCPC      *c;
	ASCipher  *cipher;
	timer_id   timer;
	ASSessionState state;
	ASPacket  *packet;
} ASSession;

ASSession *as_session_new (in_addr_t host, in_port_t port);
