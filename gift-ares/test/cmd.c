/*
 * $Id: cmd.c,v 1.24 2004/09/16 22:19:55 HEx Exp $
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
COMMAND_FUNC (go);
COMMAND_FUNC (search);
COMMAND_FUNC (info);
COMMAND_FUNC (result_stats);
COMMAND_FUNC (clear);
COMMAND_FUNC (download);
COMMAND_FUNC (dl);
COMMAND_FUNC (resume);
COMMAND_FUNC (share);
COMMAND_FUNC (share_stats);
COMMAND_FUNC (network_stats);
COMMAND_FUNC (exec);

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

	COMMAND (go,
	         "",
	         "Loads nodes and connects to network.")

	COMMAND (search,
	         "<query>",
	         "Search connected hosts for files.")

	COMMAND (info,
	         "<result number>",
	         "Show details for a given search result.")

	COMMAND (result_stats,
	         "",
	         "Show stats about search results.")

	COMMAND (clear,
	         "",
	         "Clear search results.")

	COMMAND (download,
	         "<ip> <port> <hash> <filename>",
	         "Download file.")

	COMMAND (dl,
	         "<result number>",
	         "Download search result.")

	COMMAND (resume,
	         "<file>",
	         "Resume incomplete download.")

	COMMAND (share,
	         "<path> <size> <realm> <hash> [<metadata pairs>...]",
	         "Share file.")

	COMMAND (share_stats,
	         "",
	         "Show stats about shares.")

	COMMAND (network_stats,
	         "",
	         "Show stats about the network.")

	COMMAND (exec,
		 "<file>",
		 "Read commands from file.")

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

COMMAND_FUNC (go)
{
	static char *load[]    = { "load_nodes", "nodes" };
	static char *connect[] = { "connect", "1" };
	
	dispatch_cmd (sizeof (load) / sizeof (char*), load);
	dispatch_cmd (sizeof (connect) / sizeof (char*), connect);

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
	assert (search == test_search);

	printf ("%3d) %25s %10d %c [%s]\n", list_length (results),
		r->source->username, r->filesize,
		realm_chars[r->realm], r->filename);

#if 0
	{
	int i;

	printf ("Meta tags:\n");
	i = as_meta_foreach_tag (r->meta, meta_tag_itr, NULL);
	printf ("(%d tags total)\n", i);
	}
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
	printf ("User: %s (%s:%d)\n", r->source->username,
		net_ip_str (r->source->host), r->source->port);

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

COMMAND_FUNC (result_stats)
{
	ASResult *r;
	List *l;
	int result_count = 0, firewalled = 0, push_info = 0;

	if (!results)
	{
		printf ("No results.\n");
		return FALSE;
	}

	for (l = results; l; l = l->next)
	{
		r = l->data;
		result_count++;

		if (as_source_firewalled (r->source))
			firewalled++;

		if (as_source_has_push_info (r->source))
			push_info++;
	}

	printf ("Results: %d\n", result_count);
	printf ("Firewalled: %d (%d%%)\n", firewalled,
	        firewalled == 0 ? 0 : result_count * 100 / firewalled);
	/* push info is probably not needed at all if pushes are triggerd by hash
	 * searches 
	 */
	printf ("Have push info: %d (%d%%)\n", push_info,
	        push_info == 0 ? 0 : result_count * 100 / push_info);

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

	printf ("Cleared search results.\n");

	return TRUE;
}

/*****************************************************************************/

as_bool download_cb (ASDownload *dl, ASDownloadState state)
{
	switch (state)
	{
	case DOWNLOAD_ACTIVE:
		printf ("Download ACTIVE: %s\n", dl->filename);
		break;
	case DOWNLOAD_QUEUED:
		printf ("Download QUEUED: %s\n", dl->filename);
		break;
	case DOWNLOAD_PAUSED:
		printf ("Download PAUSED: %s\n", dl->filename);
		break;
	case DOWNLOAD_COMPLETE:
		printf ("Download COMPLETE: %s\n", dl->filename);
		break;
	case DOWNLOAD_FAILED:
		printf ("Download FAILED: %s\n", dl->filename);
		break;
	case DOWNLOAD_CANCELLED:
		printf ("Download CANCELLED: %s\n", dl->filename);
		break;
	case DOWNLOAD_VERIFYING:
		printf ("Download VERIFYING: %s\n", dl->filename);
		break;
	}

	return TRUE;
}

/* max number of sources added to download */
#define NUM_SOURCES 100

COMMAND_FUNC (dl)
{
	int rnum;
	int i;
	ASResult *r;
	ASDownload *dl;
	List *l;

	if (argc < 2)
		return FALSE;

	rnum = atoi (argv[1]);

	if (!(r = list_nth_data (results, rnum)))
	{
		printf ("Invalid result number\n");
		return TRUE;
	}

	if (!(dl = as_download_create (download_cb)))
	{
		printf ("Download creation failed\n");
		return TRUE;
	}

	/* add all sources of all results with the same hash */
	for (i = 0, l = results; l && i < NUM_SOURCES; l = l->next)
	{
		ASResult *res  = l->data;

		if (as_hash_equal (res->hash, r->hash))
		{
			as_download_add_source (dl, res->source);
			i++;
		}
	}

	if (i == 0)
	{
		printf ("Failed to add any sources\n");
		as_download_free (dl);
		return TRUE;
	}

	printf ("Added %d sources to download\n", i);

	/* start download */
	if (!as_download_start (dl, r->hash, r->filesize, r->filename))
	{
		printf ("Download start failed\n");
		as_download_free (dl);
		return TRUE;
	}

	printf ("Download of \"%s\" started\n", r->filename);

	return TRUE;
}

COMMAND_FUNC (resume)
{
	ASDownload *dl;
	char *filename;

	if (argc < 2)
		return FALSE;

	filename = argv[1];

	if (!(dl = as_download_create (download_cb)))
	{
		printf ("Download creation failed\n");
		return TRUE;
	}

	/* restart download */
	if (!as_download_restart (dl, filename))
	{
		printf ("Download restart failed\n");
		as_download_free (dl);
		return TRUE;
	}

	printf ("Download of \"%s\" restarted\n", dl->filename);

	return TRUE;
}

COMMAND_FUNC (share)
{
	if (argc < 5)
		return FALSE;

	if (!(argc & 1))
		return FALSE;

	char *path = argv[1];
	int size = atoi(argv[2]);
	ASRealm realm = atoi(argv[3]);
	ASHash *hash = as_hash_decode (argv[4]);
	ASMeta *meta = as_meta_create ();
	ASShare *share;
	int i;

	if (!meta)
		return FALSE;

	for (i = 5; i < argc - 1; i += 2)
	{
		char *name = argv[i], *value = argv[i+1];

		as_meta_add_tag (meta, name, value);
	}

	share = as_share_new (path, hash, meta, size, realm);
	
	if (!share)
	{
		as_meta_free (meta);
		return FALSE;
	}
	
	if (!as_shareman_add (AS->shareman, share))
	{
		as_share_free (share);
		return FALSE;
	}

	return TRUE;
}

COMMAND_FUNC (share_stats)
{
	printf ("%d files shared, %.2f Gb\n",
		AS->shareman->nshares, AS->shareman->size);

	return TRUE;
}

COMMAND_FUNC (network_stats)
{
	printf ("Connected to %u supernodes. %u users online\n",
		list_length (AS->sessman->connected), AS->netinfo->users);
	printf ("%u total files, %u Gb\n",
		AS->netinfo->files, AS->netinfo->size);

	return TRUE;
}

COMMAND_FUNC (exec)
{
	FILE *f;
	int count = 0;

	if (argc < 2)
		return FALSE;

	if (!(f = fopen (argv[1], "r")))
		return FALSE;

	while (read_command (fileno (f)))
		count++;

	printf ("%d commands processed.\n", count);

	return TRUE;
}

COMMAND_FUNC (download)
{
#if 0
	in_addr_t ip;
	in_port_t port;
	ASSource *source;
	ASHash *hash;
	unsigned char *filename;
	ASDownload *dl;

	if (argc < 5)
		return FALSE;

	ip = net_ip (argv[1]);
	port = (in_port_t) atoi (argv[2]);
	hash = as_hash_decode (argv[3]);
	filename = argv[4];

	source = as_source_create ();

	source->host = ip;
	source->port = port;

	dl = as_download_new (source, hash, filename);

	as_download_start (dl);

	return TRUE;
#endif
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

