/*
 * $Id: cmd.c,v 1.11 2004/09/06 17:27:57 HEx Exp $
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
COMMAND_FUNC (save_nodes);
COMMAND_FUNC (connect);
COMMAND_FUNC (connect_to);
COMMAND_FUNC (search);
COMMAND_FUNC (info);
COMMAND_FUNC (clear);
COMMAND_FUNC (download);

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
	COMMAND (save_nodes,
	         "<file>",
	         "Save nodes file.")

	COMMAND (connect,
	         "<no_sessions>",
	         "Maintain no_sessions connections to network.")

	COMMAND (connect_to,
	         "<host> [<port>]",
	         "Create session to a given host.")

	COMMAND (search,
	         "<query>",
	         "Search connected hosts for files.")

	COMMAND (info,
		 "<result number>",
		 "Show details for a given search result.")

	COMMAND (clear,
		 "",
		 "Clear search results.")

	COMMAND (download,
		 "<ip> <port> <hash> <filename>",
		 "Download file.")

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

COMMAND_FUNC (save_nodes)
{
	if (argc != 2)
		return FALSE;

	printf ("Saving nodes to file %s.\n", argv[1]);

	if (!as_nodeman_save (AS->nodeman, argv[1]))
		printf ("Node file save failed.\n");

	return TRUE;
}

COMMAND_FUNC (connect)
{
	int i;

	if (argc != 2)
		return FALSE;

	i = atoi (argv[1]);
	assert (i >= 0);

	printf ("Telling session manager to connect to %d nodes.\n", i);

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

static const char *realm_chars="RA?S?VDI";

/* warning: ludicrously inefficient list code ahead */
static List *results = NULL;

static void search_callback (ASResult *r)
{
	printf ("%3d) %20s %10d %c [%s]\n", list_length (results),
		r->user, r->size,
		realm_chars[r->realm], r->filename);

	results = list_append (results, r);
}

static int clear_result (ASResult *result)
{
	as_result_free (result);

	return TRUE;
}

static void clear_results (void)
{
	results = list_foreach_remove (results, (ListForeachFunc)clear_result, NULL);
}

COMMAND_FUNC (search)
{
	unsigned char *query;
	int count;

	if (argc < 2)
		return FALSE;

	query = argv[1];
	
	clear_results ();

	/* FIXME */
	AS->callback = (ASResultCallback)search_callback;

	count = as_send_search (AS->sessman, query);

	printf ("sent query '%s' to %d nodes\n", query, count);

	return TRUE;
}

COMMAND_FUNC (info)
{
	int rnum;
	int i;
	ASResult *r;

	if (argc < 2)
		return FALSE;

	rnum = atoi (argv[1]);

	r = list_nth_data (results, rnum);

	if (!r)
		return FALSE;

	printf ("Filename: %s (extension '%s')\n", r->filename, r->ext);
	printf ("Filesize: %d bytes\n", r->size);
	printf ("User: %s\n", r->user);
	printf ("SHA1: ");
	for (i=0; i<20; i++)
		printf ("%02x", r->hash[i]);
	printf ("\n");
	if (r->meta[TAG_TITLE])
		printf ("Title: %s\n", r->meta[TAG_TITLE]);
	if (r->meta[TAG_ARTIST])
		printf ("Artist: %s\n", r->meta[TAG_ARTIST]);
	if (r->meta[TAG_ALBUM])
		printf ("Album: %s\n", r->meta[TAG_ALBUM]);
	if (r->meta[TAG_YEAR])
		printf ("Year: %s\n", r->meta[TAG_YEAR]);
	if (r->meta[TAG_CODEC])
		printf ("Codec: %s\n", r->meta[TAG_CODEC]);
	if (r->meta[TAG_KEYWORDS])
		printf ("Keywords: %s\n", r->meta[TAG_KEYWORDS]);

	return TRUE;
}

COMMAND_FUNC (clear)
{
	clear_results ();

	return TRUE;
}

COMMAND_FUNC (download)
{
	if (argc < 5)
		return FALSE;

	in_addr_t ip = net_ip(argv[1]);
	in_addr_t port = argv[2];
	ASSource *source;

	ASHash *hash = as_hash_decode (argv[3]);

	unsigned char *filename = argv[4];

	source = as_source_create ();

	source->host = ip;
	source->port = port;

	as_download_start (as_download_new (source, hash, filename));

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

