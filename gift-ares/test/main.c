/*
 * $Id: main.c,v 1.2 2004/08/21 20:17:57 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "main.h"


as_bool heartbeat_cb (void *udata)
{
	static int count = 0;

	printf ("Heartbeat: %d\n", count);
	
	if (count++ >= 10)
		return FALSE;
	
	return TRUE; /* trigger again */
}

void stdin_cb (int fd, input_id id, void *udata)
{
	printf ("stdin_cb: fd = %d, id = %p, udata = %p\n", fd, id, udata);
}

int main (int argc, char *argv[])
{
	ASLogger *logger;
	int stdin_handle;

	/* setup logging */
	logger = as_logger_create ();
	as_logger_add_output (logger, "stderr");

	AS_DBG ("Logging subsystem started");

	/* setup event system */
	as_event_init ();

	/* add some events */
	timer_add (1*SECONDS, heartbeat_cb, NULL);

#ifdef WIN32
	stdin_handle = (int) GetStdHandle (STD_INPUT_HANDLE);
#else
	stdin_handle = -1;
#endif

	input_add (stdin_handle, NULL, INPUT_READ, stdin_cb, 0);

	AS_DBG ("Entering event loop");
	
	as_event_loop ();

	AS_DBG ("Left event loop");

	/* shutdown */
	as_event_shutdown ();
	as_logger_free (logger);
}


