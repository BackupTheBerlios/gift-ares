/*
 * $Id: as_session.h,v 1.4 2004/08/31 22:05:58 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SESSION_H_
#define __AS_SESSION_H_

/*****************************************************************************/

typedef enum
{
	SESSION_DISCONNECTED, /* tcp disconnected */
	SESSION_CONNECTING,   /* tcp connecting */
	SESSION_FAILED,       /* tcp connect failed */
	SESSION_HANDSHAKING,  /* tcp connected, working out crypto */
	SESSION_CONNECTED     /* session established */
} ASSessionState;

typedef enum
{
	PACKET_SYN   = 0x5A,  /* first packet sent by connecting party */
	PACKET_ACK   = 0x33,  /* first packet sent back by supernode */
	PACKET_SHARE = 0x32   /* used to send shares to supernode, compressed! */

} ASPacketType;

/*****************************************************************************/

typedef struct as_session_t ASSession;

/* Called for state changes. Return FALSE if the session has been freed and
 * can no longer be used after the callback.
 */
typedef as_bool (*ASSessionStateCb) (ASSession *session,
                                     ASSessionState new_state);

/* Called for all non-handshake packets. Return FALSE if the session has been
 * freed and can no longer be used after the callback.
 */
typedef as_bool (*ASSessionPacketCb) (ASSession *session, ASPacketType type,
                                      ASPacket *packet);

struct as_session_t
{
	TCPC          *c;
	input_id       input;   /* input id of event associated with c */

	ASCipher      *cipher;
	ASPacket      *packet;  /* buffer for incoming data */

	ASSessionState state;

	ASSessionStateCb  state_cb;
	ASSessionPacketCb packet_cb;

	void *udata; /* user data */
};

/*****************************************************************************/

/* Create new session with specified callbacks. */
ASSession *as_session_create (ASSessionStateCb state_cb,
                              ASSessionPacketCb packet_cb);

/* Disconnect but does not free session. Triggers state callback. */
void as_session_free (ASSession *session);

/*****************************************************************************/

/* Returns current state of session */
ASSessionState as_session_state (ASSession *session);

/* Connect to ip and port. Fails if already connected. */
as_bool as_session_connect (ASSession *session, in_addr_t host,
                            in_port_t port);

/* Disconnect but does not free session. */
void as_session_disconnect (ASSession *session);

/*****************************************************************************/

#endif /* __AS_SESSION_H_ */
