/*
 * $Id: cmd.c,v 1.7 2004/08/31 20:05:25 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "cmd.h"
#include "as_ares.h"

/*****************************************************************************/

#define COMMAND_FUNC(func) static as_bool command_##func (int argc, char *argv[])
#define COMMAND(func,param_str,descr) { #func, param_str, descr, command_##func },
#define COMMAND_NULL { NULL, NULL, NULL, NULL }

COMMAND_FUNC (help);

COMMAND_FUNC (event_test);

COMMAND_FUNC (load_nodes);
COMMAND_FUNC (connect);
COMMAND_FUNC (connect_to);

COMMAND_FUNC (quit);

static struct command_t
{
	char *name;
	char *param_str;
	char *descr;
	as_bool (*func)(int argc, char *argv[]);
}
commands[] =
{
	COMMAND (help,
	         "",
	         "Display list of commands.")

	COMMAND (event_test,
	         "",
	         "Test event system.")

	COMMAND (load_nodes,
	         "<file>",
	         "Load nodes file.")

	COMMAND (connect,
	         "<no_sessions>",
	         "Maintain no_sessions connections to network.")

	COMMAND (connect_to,
	         "<host> [<port>]",
	         "Create session to a given host.")

	COMMAND (quit,
             "",
             "Quit this application.")

	COMMAND_NULL
};


/*****************************************************************************/

void print_cmd_usage (struct command_t *cmd)
{
	printf ("Command \"%s\" failed. Usage:\n", cmd->name);
	printf ("  %s %s\n", cmd->name, cmd->param_str);
}

as_bool dispatch_cmd (int argc, char *argv[])
{
	struct command_t *cmd;

	if (argc < 1)
		return FALSE;

	/* find handler and call it */
	for (cmd = commands; cmd->name; cmd++)
		if (!strcmp (cmd->name, argv[0]))
		{
			if (cmd->func (argc, argv))
				return TRUE;

			print_cmd_usage (cmd);
			return FALSE;
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

	return TRUE;
}

/*****************************************************************************/

COMMAND_FUNC (event_test)
{
	test_event_system ();
	return TRUE;
}

/*****************************************************************************/

COMMAND_FUNC (load_nodes)
{
	if (argc != 2)
		return FALSE;

	printf ("Loading nodes from file %s.\n", argv[1]);

	if (!as_nodeman_load (AS->nodeman, argv[1]))
		printf ("Node file load failed.\n");

	return TRUE;
}

COMMAND_FUNC (connect)
{
	int i;

	if (argc != 2)
		return FALSE;

	i = atoi (argv[1]);
	assert (i >= 0);

	printf ("Telling seesion manager to connect to %d nodes.\n", i);

	as_sessman_connect (AS->sessman, i);

	return TRUE;
}

COMMAND_FUNC (connect_to)
{
	ASSession *sess;
	in_addr_t ip;
	in_port_t port;

	if (argc < 2)
		return FALSE;

	ip = net_ip (argv[1]);
	
	/* if no port is given derive it from ip */
	if (argc > 2)
		port = atoi (argv[2]);
	else
		port = as_ip2port (ip);

	printf ("connecting to %s (%08x), port %d\n", argv[1], ip, port);
	sess = as_session_create (NULL, NULL);
	assert (sess);
	as_session_connect (sess, ip, port);

	return TRUE;
}

/*****************************************************************************/

COMMAND_FUNC (quit)
{
	/* FIXME: cleanup properly */
	
	printf ("Terminating event loop...\n");
	as_event_quit ();
	return TRUE;
}

/*****************************************************************************/

