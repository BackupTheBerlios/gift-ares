/*
 * $Id: as_event.h,v 1.1 2004/08/20 11:55:33 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_EVENT_H
#define __AS_EVENT_H

#include "as_ares.h"

/*****************************************************************************/

/* some defines from giFT so we remain compatible */

#define MSEC         (1)
#define SECONDS      (1000*MSEC)
#define MINUTES      (60*SECONDS)

#define ESECONDS     (1)
#define EMINUTES     (60*ESECONDS)
#define EHOURS       (60*EMINUTES)
#define EDAYS        (24*EHOURS)

#define TIMEOUT_DEF  (60*SECONDS)

typedef unsigned int input_id;
typedef unsigned int timer_id;

/*****************************************************************************/

/* callback raised when state of socket fd changed */
typedef void (*InputCallback) (int fd, input_id id, void *udata);

/* states on which to raise callback */
typedef enum
{
	INPUT_READ  = 0x01,
	INPUT_WRITE = 0x02,
	INPUT_ERROR = 0x04
} InputState;

/* Callback raised if timer reaches zero. If this returns TRUE the timer
 * will be reset, otherwise removed.
 */
typedef as_bool (*TimerCallback) (void *udata);

/*****************************************************************************/

typedef struct
{

} ASEventSys;

/* the evil global from as_event.c */
extern ASEventSys *g_event;

/*****************************************************************************/

/* create event system */
ASEventSys *as_event_create ();

/* free event system. do not call while loop is still running */
void as_event_free (ASEventSys *event);

/* event loop, blocks until the loop is quit */
void as_event_run (ASEventSys *event);

/* stops event loop and makes as_event_run return */
void as_event_quit (ASEventSys *event);

/*****************************************************************************/

input_id input_add (int fd, void *udata, InputState state,
                    InputCallback callback, time_t timeout);

void input_remove (input_id id);

/* remove all inputs of this fd */
void input_remove_all (int fd);

/* temporarily remove fd from event loop */
void input_suspend_all (int fd);

/* put fd back into event loop */
void input_resume_all (int fd);

/*****************************************************************************/

timer_id timer_add (time_t interval, TimerCallback callback, void *udata);

void timer_reset (timer_id id);

void timer_remove (timer_id id);

void timer_remove_zero (timer_id *id);

/*****************************************************************************/

#endif /* __AS_EVENT_H */
