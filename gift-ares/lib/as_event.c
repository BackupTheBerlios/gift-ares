/*
 * $Id: as_event.c,v 1.10 2004/08/31 20:05:25 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"
#include "event.h"    /* libevent */

/*****************************************************************************/

typedef enum
{
	AS_EVINPUT,
	AS_EVTIMER
} ASEventType;

typedef struct as_event_t
{
	ASEventType type;

	union
	{
		/* timer data */
		struct
		{
			struct timeval interval;
			TimerCallback  cb;
		} timer;

		/* input data */
		struct
		{
			int fd;
			InputState state;
			as_bool suspended;
			struct timeval timeout;
			as_bool validate;
			InputCallback cb;
		} input;
	};

	/* user data */
	void *udata;

	/* the libevent struct */
	struct event ev;

} ASEvent;

/*****************************************************************************/

/* We keep a table of all input events in order to look them up by fd */
static ASHashTable *input_table = NULL;

/*****************************************************************************/

/* init event system */
as_bool as_event_init ()
{
	assert (input_table == NULL);

	if (!(input_table = as_hashtable_create_int ()))
		return FALSE;
	
	/* initialize libevent */
	event_init ();

	return TRUE;
}

/* shutdown event system. do not call while loop is still running */
void as_event_shutdown ()
{
	/* FIXME: free remaining events? */

	as_hashtable_free (input_table);
	input_table = NULL;

	return;
}

/* event loop, blocks until the loop is quit. returns FALSE if there was an
 * error.
 */
as_bool as_event_loop ()
{
	int ret;

	ret = event_dispatch ();

	if (ret < 0)
	{
		AS_ERR_1 ("Event loop terminated with error. errno = %d", errno);
		return FALSE;
	}

	AS_DBG ("Event loop terminated gracefully.");

	return TRUE;
}

/* stops event loop and makes as_event_loop return */
void as_event_quit ()
{
	event_loopexit (NULL);

	AS_DBG ("Forcing event loop to quit. Better had removed all events for this!");
}

/*****************************************************************************/

static ASEvent *event_create (ASEventType type, void *udata)
{
	ASEvent *ev;
	
	if (!(ev = malloc (sizeof (ASEvent))))
	{
		AS_ERR ("event_create: couldn't alloc event object!");
		return NULL;
	}

	ev->type = type;
	ev->udata = udata;

	ev->timer.interval.tv_sec = 0;
	ev->timer.interval.tv_usec = 0;
	ev->timer.cb = NULL;

	ev->input.fd = -1;
	ev->input.state = 0;
	ev->input.suspended = 0;
	ev->input.timeout.tv_sec = 0;
	ev->input.timeout.tv_usec = 0;
	ev->input.validate = FALSE;
	ev->input.cb = NULL;

	/* zero libevent struct */
	memset (&ev->ev, 0, sizeof (ev->ev));

	return ev;
}

static void event_free (ASEvent *ev)
{
	if (!ev)
		return;

	free (ev);
}

/*****************************************************************************/

/* the function which gets called by libevent for all events */
static void libevent_cb (int fd, short event, void *arg)
{
	ASEvent *ev = (ASEvent *) arg;

#if 0
	AS_HEAVY_DBG_3 ("libevent_cb: fd: %d, event: 0x%02x, arg: %p", fd,
	                event, arg);
#endif

	if (ev->type == AS_EVTIMER)
	{
		assert (fd == -1);
		assert (event & EV_TIMEOUT);

		/* raise the callback */
		if (ev->timer.cb (ev->udata))
		{
			/* Reset timer if callback returned true. This will crash if
			 * callback removed timer which is expected behaviour.
			 */
			timer_reset (ev);	
		}
		else
		{
			/* This will crash if the callback removed the timer itself using
			 * timer_remove(). Just don't do that.
			 */
			timer_remove (ev);
		}	
	}
	else if (ev->type == AS_EVINPUT)
	{
		assert (fd == ev->input.fd);
		assert (fd >= 0);

		if (event & EV_TIMEOUT)
		{
			InputCallback cb;
			void *udata;

			/* libgift closes fd and removes all inputs. WTF? */
			net_close (ev->input.fd);
#if 0
			input_remove_all (ev->input.fd);
#endif
			
			/* copy things we need for callback */
			cb = ev->input.cb;
			udata = ev->udata;

			/* event is presistent so remove it now */
			input_remove (ev);

			/* raise callback with bad fd and no input_id */
			cb (-1, 0, udata);
		}
		else if (event & (EV_READ | EV_WRITE))
		{
			/* libgift removes the timer after the first succesfull input
			 * event. libevent resets the timeout so we remove the input and
			 * add it again without a timer.
			 */
			if (ev->input.validate)
			{
				ev->input.validate = FALSE;
	
				if (event_del (&ev->ev) != 0)
					AS_ERR ("libevent_cb: event_del() failed!");
				
				if (event_add (&ev->ev, NULL) != 0)
				{
					AS_ERR ("libevent_cb: event_add() failed!");
					/* remove from hash table */
					as_hashtable_remove_int (input_table,
					                         (as_uint32) ev->input.fd);
					event_free (ev);

					assert (0);
					return; /* hmm, callback is not raised */
				}
			}

			/* raise callback */
			ev->input.cb (fd, (input_id)ev, ev->udata);
		}
		else
		{
			/* cannot happen */
			assert (0);
		}
	}
	else
	{
		/* cannot happen */
		assert (0);
	}
}

/*****************************************************************************/

input_id input_add (int fd, void *udata, InputState state,
                    InputCallback callback, time_t timeout)
{
	ASEvent *ev;
	int ret;
	short trigger;

	assert (callback);
	assert (fd >= 0);

	if (!(ev = event_create (AS_EVINPUT, udata)))
		return NULL;

	ev->input.fd = fd;
	ev->input.state = state;
	ev->input.suspended = FALSE;
	ev->input.timeout.tv_sec = timeout / 1000;
	ev->input.timeout.tv_usec = (timeout % 1000) * 1000;
	ev->input.validate = (ev->input.timeout.tv_sec > 0) || 
	                     (ev->input.timeout.tv_usec > 0);
	ev->input.cb = callback;

	trigger = ((ev->input.state & INPUT_READ)  ? EV_READ   : 0) |
	          ((ev->input.state & INPUT_WRITE) ? EV_WRITE  : 0) |
	          EV_PERSIST;

	/* Not supported by libevent except for my hacked windows build.
	 * On windows all fds with EV_WRITE will also be added to the exception
	 * set by libevent to work around non-standard behaviour in window's
	 * select().
	 */
	assert ((ev->input.state & INPUT_ERROR) == 0);

	event_set (&ev->ev, ev->input.fd, trigger, libevent_cb, (void *)ev);

	if (ev->input.validate)
		ret = event_add (&ev->ev, &ev->input.timeout);
	else
		ret = event_add (&ev->ev, NULL);
	
	if (ret != 0)
	{
		AS_ERR ("input_add: event_add() failed!");
		event_free (ev);
		return NULL;
	}

	/* add to hash table. this overwrites previous entries with the same fd
	 * which is bad.
	 */
	as_hashtable_insert_int (input_table, (as_uint32) ev->input.fd, ev);

	return (input_id) ev;
}

void input_remove (input_id id)
{
	ASEvent *ev = (ASEvent *) id;

	if (id == (input_id) 0)
		return;

	if (event_del (&ev->ev) != 0)
		AS_ERR ("input_remove: event_del() failed!");

	/* remove from hash table */
	as_hashtable_remove_int (input_table, (as_uint32) ev->input.fd);

	event_free (ev);
}

/* remove all inputs of this fd */
void input_remove_all (int fd)
{
	ASEvent *ev;

	if (!(ev = as_hashtable_lookup_int (input_table, (as_uint32) fd)))
		return;

	/* FIXME: only one entry per fd is removed since the hash table can not
	 * hold multiple entries with the same key (on purpose). So this is
	 * basically just a removal by fd for whichever input was added last.
	 */
	input_remove (ev);
}

/* temporarily remove fd from event loop */
void input_suspend_all (int fd)
{
	assert (0);
}

/* put fd back into event loop */
void input_resume_all (int fd)
{
	assert (0);
}

/*****************************************************************************/

timer_id timer_add (time_t interval, TimerCallback callback, void *udata)
{
	ASEvent *ev;

	assert (callback);

	if (!(ev = event_create (AS_EVTIMER, udata)))
		return NULL;

	ev->timer.cb = callback;
	ev->timer.interval.tv_sec = interval / 1000;
	ev->timer.interval.tv_usec = (interval % 1000) * 1000;

	event_set (&ev->ev, -1, 0, libevent_cb, (void *)ev);
	
	if (event_add (&ev->ev, &ev->timer.interval) != 0)
	{
		AS_ERR ("timer_add: event_add() failed!");
		event_free (ev);
		return NULL;
	}

	return (timer_id) ev;
}

void timer_reset (timer_id id)
{
	ASEvent *ev = (ASEvent *) id;

	assert (ev);

	/* simply add it again, this reset the timeout if it was already added */
	if (event_add (&ev->ev, &ev->timer.interval) != 0)
	{
		AS_ERR ("timer_reset: event_add() failed!");
		event_free (ev);
		return;
	}
}

void timer_remove (timer_id id)
{
	ASEvent *ev = (ASEvent *) id;

	assert (ev);

	if (event_del (&ev->ev) != 0)
		AS_ERR ("timer_remove: event_del() failed!");

	event_free (ev);
}

void timer_remove_zero (timer_id *id)
{
	assert (id);

	timer_remove (*id);
	*id = NULL;
}

/*****************************************************************************/

