#define GIFT_PLUGIN

#include "as_ares.h"

static Protocol *proto;

static int asp_giftcb_start (Protocol *proto)
{
	ASLogger *logger;
        logger = as_logger_create ();
        as_logger_add_output (logger, "stderr");

	if (!as_init ())
		return FALSE;

	if (!as_nodeman_load (AS->nodeman, gift_conf_path ("Ares/nodes")))
		return FALSE;

	if (!as_start ())
		return FALSE;

	as_sessman_connect (AS->sessman, 4);

	return TRUE;
}

static void asp_giftcb_destroy (Protocol *proto)
{
	as_nodeman_save (AS->nodeman, gift_conf_path ("Ares/nodes"));
	as_cleanup ();
}

static as_bool add_meta (ASMetaTag *tag, Share *share)
{
	share_set_meta (share, tag->name, tag->value);

	return TRUE;
}

static void search_callback (ASSearch *search, ASResult *r, as_bool duplicate)
{
	Share *share;

	if (!r->filename)
		return;

	if (!(share = share_new (NULL)))
		return;

	share->p    = proto;
	share->size = r->filesize;
	share_set_path (share, r->filename);
	share_set_mime (share, mime_type (r->filename));

	share_set_hash (share, "SHA1", r->hash->data, AS_HASH_SIZE, FALSE);

	as_meta_foreach_tag (r->meta, (ASMetaForeachFunc)add_meta, share);

	proto->search_result (proto, search->udata, r->source->username,
			      NULL, "FIXME", 1, share);

	share_free (share);
}

int asp_giftcb_search (Protocol *p, IFEvent *event, char *query, char *exclude,
		       char *realm, Dataset *meta)
{
	ASSearch *search;

	search = as_searchman_search (AS->searchman,
				      (ASSearchResultCb) search_callback,
				      query, SEARCH_ANY);

	if (!search)
		return FALSE;

	search->udata = event;

	return TRUE;
}

static as_bool find_search (ASHashTableEntry *entry, IFEvent *event)
{
	ASSearch *s = entry->val;
	
	if (s->udata == event)
	{
		as_bool ret;
		ret = as_searchman_remove (AS->searchman, s);
		assert (ret);
	}

	return FALSE;
}

void asp_giftcb_search_cancel (Protocol *p, IFEvent *event)
{
	as_hashtable_foreach (AS->searchman->searches,
				    (ASHashTableForeachFunc)find_search,
				    event);
}

int asp_giftcb_stats (Protocol *p, unsigned long *users, unsigned long *files,
                                          double *size, Dataset **extra)
{
	*users = AS->netinfo->users;
	*files = AS->netinfo->files;
	*size  = AS->netinfo->size;

	return AS->netinfo->conn_have;
}

unsigned char *asp_giftcb_hash (const char *path, size_t *len)
{
	ASHash *hash = as_hash_file (path);

	if (len && hash)
		*len = AS_HASH_SIZE;

	return (unsigned char *)hash;
}

unsigned char *asp_giftcb_hash_encode (unsigned char *data)
{
	ASHash *hash;
	unsigned char *encoded;

	hash = as_hash_create (data, AS_HASH_SIZE);

	if (!hash)
		return NULL;
	
	encoded = as_hash_encode (hash);
	as_hash_free (hash);

	return encoded;
}

int Ares_init (Protocol *p)
{
	p->version_str = strdup ("foo");

	p->support (p, "range-get", TRUE);
	p->support (p, "hash-unique", TRUE);

	p->hash_handler (p, "SHA1", HASH_PRIMARY,
			 (HashFn)asp_giftcb_hash,
			 (HashDspFn)asp_giftcb_hash_encode);

	p->start          = asp_giftcb_start;
	p->destroy        = asp_giftcb_destroy;
	p->search         = asp_giftcb_search;
	p->search_cancel  = asp_giftcb_search_cancel;
	p->stats          = asp_giftcb_stats;

	proto = p;

	return TRUE;
}
