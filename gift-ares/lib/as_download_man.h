/*
 * $Id: as_download_man.h,v 1.2 2004/09/18 19:11:45 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_DOWNLOAD_MAN_H
#define __AS_DOWNLOAD_MAN_H

/*****************************************************************************/

typedef struct as_downman_t ASDownMan;

/* Called for every state change of a managed download. Return FALSE if the
 * download was freed and must no longer be accessed.
 */
typedef as_bool (*ASDownManStateCb) (ASDownMan *man, ASDownload *dl,
                                     ASDownloadState state);

typedef struct as_downman_t
{
	/* List of downloads */
	List *downloads;

	/* Hashtable keyed by file hash and pointing to links in downloads. */
	ASHashTable *hash_index;

	/* the state callback we trigger for all downloads */
	ASDownManStateCb state_cb;

	/* Whether all downloads are queued. */
	as_bool stopped;
};

/*****************************************************************************/

/* Allocate and init download manager. */
ASDownMan *as_downman_create ();

/* Free manager. */
void as_downman_free (ASDownMan *man);

/* Set callback triggered for every state change in one of the downloads. */
void as_downman_set_state_cb (ASDownMan *man, ASDownManStateCb state_cb);

/*****************************************************************************/

/* If stop is TRUE all downloads are stopped (put in state DOWNLOAD_QUEUED).
 * If stop is FALSE downloads are resumed. Call this with TRUE before
 * shutting down to write clean state of all downloads to disk.
 */
as_bool as_downman_stop_all (ASDownMan *man, as_bool stop);

/*****************************************************************************/

/* Create download from a search result. Looks up other search results with
 * the same hash and adds them as sources. If a download with the same hash
 * already exists it is returned and no new download is created.
 */
ASDownload *as_downman_start_result (ASDownMan *man, ASResult *result,
                                     const char *save_path);

/* Create download from ares hash link and start source search. If a download
 * with the same hash already exists it is returned and no new download is
 * created.
 */
ASDownload *as_downman_start_link (ASDownMan *man, const char *hash_link,
                                   const char *save_path);

/*****************************************************************************/

/* Creates download from incomplete file pointed to by path. */
ASDownload *as_downman_restart_file (ASDownMan *man, const char *path);


/* Resumes all incomplete downloads in specified directory. Returns the number
 * of resumed downloads or -1 on error. Callback is raised for each started
 * download as usual.
 */
int as_downman_restart_dir (ASDownMan *man, const char *dir);

/*****************************************************************************/

/* Pause download if pause is TRUE or resume it if pause is FALSE. */
as_bool as_downman_pause (ASDownMan *man, ASDownload *dl, as_bool pause);

/* Cancel download but do not remove it. */
as_bool as_downman_cancel (ASDownMan *man, ASDownload *dl);

/* Remove and free finished, failed or cancelled download. */
as_bool as_downman_remove (ASDownMan *man, ASDownload *dl);

/*****************************************************************************/

/* Return state of download. The advantage to as_download_state is that this
 * makes sure the download is actually still in the list and thus valid
 * before accessing it. If the download is invalid DOWNLOAD_INVALID is
 * returned.
 */
ASDownloadState as_downman_state (ASDownMan *man, ASDownload *dl);

/* Return download by file hash or NULL if there is none. */
ASDownload *as_downman_lookup_hash (ASDownMan *man, ASHash *hash);

/*****************************************************************************/

#endif /* __AS_DOWNLOAD_MAN_H */
