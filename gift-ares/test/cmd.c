/*
 * $Id: cmd.c,v 1.4 2004/08/26 16:00:40 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "cmd.h"
#include "as_ares.h"

/*****************************************************************************/

#define COMMAND_FUNC(func) static void command_##func (int argc, char *argv[])
#define COMMAND(func,param_str,descr) { #func, param_str, descr, command_##func },
#define COMMAND_NULL { NULL, NULL, NULL, NULL }

COMMAND_FUNC (help);

COMMAND_FUNC (event_test);

COMMAND_FUNC (connect);

COMMAND_FUNC (quit);

static struct command_t
{
	char *name;
	char *param_str;
	char *descr;
	void (*func)(int argc, char *argv[]);
}
commands[] =
{
	COMMAND (help,
	         "",
	         "Display list of commands.")

	COMMAND (event_test,
	         "",
	         "Test event system.")

	COMMAND (connect,
	         "<host> <port>",
	         "Connect to a given host.")

	COMMAND (quit,
             "",
             "Quit this application.")

	COMMAND_NULL
};


/*****************************************************************************/

as_bool dispatch_cmd (int argc, char *argv[])
{
	struct command_t *cmd;

	if (argc < 1)
		return FALSE;

	/* find handler and call it */
	for (cmd = commands; cmd->name; cmd++)
		if (!strcmp (cmd->name, argv[0]))
		{
			cmd->func (argc, argv);
			return TRUE;
		}

	printf ("Unknown command \"%s\", try \"help\"\n", argv[0]);

	return FALSE;
}

/*****************************************************************************/

COMMAND_FUNC (help)
{
	struct command_t *cmd;
	
	printf ("Available commands:\n\n");

	for (cmd = commands; cmd->name; cmd++)
	{
			printf ("* %s %s\n", cmd->name, cmd->param_str);
			printf ("      %s\n", cmd->descr);
	}	
}

/*****************************************************************************/

COMMAND_FUNC (event_test)
{
	test_event_system ();
}

/*****************************************************************************/

COMMAND_FUNC (connect)
{
	if (argc < 3)
		return;

	in_addr_t ip = net_ip (argv[1]);
	in_port_t port = atoi (argv[2]);

	printf ("connecting to %s (%08x), port %d\n", argv[1], ip, port);
	as_session_new (ip, port);
}

/*****************************************************************************/

COMMAND_FUNC (quit)
{
	/* FIXME: cleanup properly */
	
	printf ("Terminating event loop...\n");
	as_event_quit ();
}

/*****************************************************************************/

