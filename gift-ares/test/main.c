/*
 * $Id: main.c,v 1.3 2004/08/24 20:56:26 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifdef WIN32
#include <process.h> /* _beginthreadex */
#endif

#include "as_ares.h"
#include "main.h"
#include "cmd.h"

/* see end of file */
static int parse_argv(char *cmdline, int *argc, char ***argv);


#ifdef WIN32

/* separate thread for reading console input in win32 */
unsigned int __stdcall console_input_func (void *data)
{
	char buf[1024*16];
	DWORD read;
	int written;
	int event_fd = (int) data;
	HANDLE console_d = GetStdHandle (STD_INPUT_HANDLE);

	SetConsoleMode (console_d, ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT |
	                ENABLE_ECHO_INPUT);

	while (ReadFile (console_d, buf, sizeof (buf) - 1, &read, NULL))
	{
		/* send entire line to event loop for processing */
		if (read == 0)
			continue;
		
		written = send (event_fd, buf, read, 0);

		if (written < 0 || written != read)
		{
			/* FIXME: handle error */
		}
	}

	/* FIXME: quit event loop? */

	return 0;
}

#endif

void stdin_cb (int fd, input_id id, void *udata)
{
	static char buf[1024*16];
	static unsigned int buf_pos = 0;
	int read, len;
	int argc;
	char **argv;

	if ((read = recv (fd, buf + buf_pos, sizeof (buf) - buf_pos, 0)) < 0)
		return;

	buf_pos += read;

	/* check for LF */
	for (len = 0; len < buf_pos; len++)
		if (buf[len] == '\n')
			break;

	if (len == buf_pos)
	{
		/* panic if buffer is too small */
		assert (buf_pos < sizeof (buf));
		return;
	}

	buf[len++] = 0;

	/* handle cmd line, argv[] points into buf */
	parse_argv (buf, &argc, &argv);

	/* dispatch	command */
	dispatch_cmd (argc, argv);

	free (argv);

	/* remove used data */
	memmove (buf, buf + len, buf_pos - len);
	buf_pos -= len;
}



int main (int argc, char *argv[])
{
	ASLogger *logger;
	int stdin_handle;

	/* winsock init */
	tcp_startup ();

	/* setup logging */
	logger = as_logger_create ();
	as_logger_add_output (logger, "stderr");

	AS_DBG ("Logging subsystem started");

	/* setup event system */
	as_event_init ();

	/* add callback for command handling */
#ifdef WIN32
	{
		HANDLE hThread;
		int fds[2];

		if (socketpair (0, 0, 0, fds) < 0)
		{
			printf ("FATAL: socketpair() failed\n");
			exit (1);
		}

		stdin_handle = fds[1];

		hThread = (HANDLE) _beginthreadex (NULL, 0, console_input_func, (void *)fds[0], 0, NULL);

		if (hThread == (HANDLE) -1 || hThread == (HANDLE) 0)
		{
			printf ("FATAL: couldn't start input thread\n");
			exit (1);
		}

		/* we don't use the thread handle past this point */
		CloseHandle (hThread);
	}
#else
	/* FIXME: *unix console input */
	stdin_handle = -1;
#endif

	input_add (stdin_handle, NULL, INPUT_READ, stdin_cb, 0);


	AS_DBG ("Entering event loop");
	
	as_event_loop ();

	AS_DBG ("Left event loop");

	/* shutdown */
	as_event_shutdown ();
	as_logger_free (logger);

	/* winsock shutdown */
	tcp_cleanup ();
}


/* modifies cmdline */
static int parse_argv(char *cmdline, int *argc, char ***argv)
{
	char *p, *token;
	int in_quotes, in_token;
	
	/* should always be enough */
	*argv = malloc (sizeof (char*) * strlen (cmdline) / 2 + 1);
	*argc = 0;

	in_token = in_quotes = 0;
	p = token = cmdline;

	for (;;)
	{
		switch (*p)
		{
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			if (in_token && !in_quotes)
			{
				*p = 0;
				(*argv)[*argc] = token;
				(*argc)++;
				in_token = 0;
				token = NULL;
			}
			break;

		case '\0':
			if (in_token)
			{
				(*argv)[*argc] = token;
				(*argc)++;
				in_token = 0;
				token = NULL;
			}

			return 0;

		case '"':
			if (in_token)
			{
				*p = 0;
				(*argv)[*argc] = token;
				(*argc)++;
				in_token = 0;			
				token = NULL;
			}

			in_quotes = !in_quotes;

			break;
		default:
			if (!in_token || (in_quotes && !in_token))
			{
				in_token = 1;
				token = p;
			}

			break;
		}

		p++;
	}	

	return 0;
}

