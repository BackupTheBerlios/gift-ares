/*
 * $Id: cmd.c,v 1.14 2004/09/07 17:16:58 mkern Exp $
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

/*****************************************************************************/

static const char *realm_chars="RA?S?VDI";

/* warning: ludicrously inefficient list code ahead */
static List *results = NULL;
static ASSearch *test_search = NULL;

static as_bool meta_tag_itr (ASMetaTag *tag, void *udata)
{
	printf ("  %s: %s\n", tag->name, tag->value);
	return TRUE;
}

static void search_callback (ASSearch *search, ASResult *r)
{
	int i;
	assert (search == test_search);

	printf ("%3d) %20s %10d %c [%s]\n", list_length (results),
		r->source->username, r->filesize,
		realm_chars[r->realm], r->filename);

#if 0
	printf ("Meta tags:\n");
	i = as_meta_foreach_tag (r->meta, meta_tag_itr, NULL);
	printf ("(%d tags total)\n", i);
#endif

	results = list_append (results, r);
}

COMMAND_FUNC (search)
{
	unsigned char *query;

	if (argc < 2)
		return FALSE;

	if (test_search)
	{
		printf ("Only one search allowed at a time in this test app\n");
		return TRUE;
	}

	assert (results == NULL);

	query = argv[1];

	test_search = as_searchman_search (AS->searchman,
	                                   (ASSearchResultCb) search_callback,
	                                   query, REALM_ANY);

	if (!test_search)
	{
		printf ("Failed to start search for \"%s\"\n", query);
		return TRUE;
	}
	
	printf ("Started search for \"%s\"\n", query);

	return TRUE;
}

COMMAND_FUNC (info)
{
	int rnum;
	int i;
	char *str;
	ASResult *r;

	if (argc < 2)
		return FALSE;

	rnum = atoi (argv[1]);

	r = list_nth_data (results, rnum);

	if (!r)
	{
		printf ("Invalid result number\n");
		return TRUE;
	}

	printf ("Filename: %s (extension '%s')\n", r->filename, r->fileext);
	printf ("Filesize: %d bytes\n", r->filesize);
	printf ("User: %s\n", r->source->username);

	printf ("SHA1: ");
	for (i=0; i < AS_HASH_SIZE; i++)
		printf ("%02x", r->hash->data[i]);
	str = as_hash_encode (r->hash);
	printf (" [%s]\n", str);
	free (str);

	printf ("Meta tags:\n");
	i = as_meta_foreach_tag (r->meta, meta_tag_itr, NULL);
	printf ("(%d tags total)\n", i);

	return TRUE;
}

COMMAND_FUNC (clear)
{
	if (!test_search)
	{
		printf ("No search to clear.\n");
		return TRUE;
	}
	
	results = list_free (results);

	as_searchman_remove (AS->searchman, test_search);
	test_search = NULL;

	return TRUE;
}

/*****************************************************************************/

COMMAND_FUNC (download)
{
	in_addr_t ip;
	in_port_t port;
	ASSource *source;
	ASHash *hash;
	unsigned char *filename;

	if (argc < 5)
		return FALSE;

	ip = net_ip (argv[1]);
	port = (in_port_t) atoi (argv[2]);
	hash = as_hash_decode (argv[3]);
	filename = argv[4];

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

