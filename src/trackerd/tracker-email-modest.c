/* Tracker
 * routines for emails with Modest
 * Copyright (C) 2008, Philip Van Hoof (pvanhoof@gnome.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-config.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <dirent.h>

#include "tracker-email-modest.h"
#include "tracker-email-utils.h"
#include "tracker-db-email.h"
#include "tracker-cache.h"
#include "tracker-dbus.h"
#include "tracker-watch.h"


#define MODEST_HOME	".modest"
#define MODEST_HOME_CACHE_MAIL MODEST_HOME G_DIR_SEPARATOR_S "cache" G_DIR_SEPARATOR_S "mail"
#define MODEST_HOME_LOCAL_FOLDERS MODEST_HOME G_DIR_SEPARATOR_S "local_folders"
#define MODEST_HOME_OUTBOXES MODEST_HOME G_DIR_SEPARATOR_S "outboxes"


typedef struct {
	gchar		*mail_dir;	/* something like "/home/laurent.modest/mail" */
	GSList		*dirs;
	GSList		*dynamic_dirs;
} ModestConfig;


enum {
	MODEST_MESSAGE_ANSWERED     = 1 << 0,
	MODEST_MESSAGE_DELETED      = 1 << 1,
	MODEST_MESSAGE_DRAFT        = 1 << 2,
	MODEST_MESSAGE_FLAGGED      = 1 << 3,
	MODEST_MESSAGE_SEEN         = 1 << 4,
	MODEST_MESSAGE_ATTACHMENTS  = 1 << 5,
	MODEST_MESSAGE_CACHED       = 1 << 6,
	MODEST_MESSAGE_PARTIAL      = 1 << 7,
	MODEST_MESSAGE_EXPUNGED     = 1 << 8,
	MODEST_MESSAGE_HIGH_PRIORITY = 0<<9|1<<10,
	MODEST_MESSAGE_NORMAL_PRIORITY = 0<<9|0<<10,
	MODEST_MESSAGE_LOW_PRIORITY = 1<<9|0<<10,
	MODEST_MESSAGE_SUSPENDED = 1<<11
};

typedef struct {
	gchar            *path;                 /* path the summary file */
	FILE             *f;                    /* opened file descriptor for the file */
} SummaryFile;

typedef struct {
	gint32		version;
	gboolean	legacy;
	gint32		flags;
	gint32		nextuid;
	time_t		time;
	gint32		saved_count;
	gint32		unread_count;
	gint32		deleted_count;
	gint32		junk_count;
	gchar 		*uri_prefix;
} SummaryFileHeader;


extern Tracker		*tracker;

static ModestConfig	*modest_config = NULL;


static gboolean	load_modest_config			(ModestConfig **conf);
static void	free_modest_config			(ModestConfig *conf);


static gboolean	is_in_dir_pop				(const gchar *dir);
static gboolean	is_in_dir_imap				(const gchar *dir);
static gboolean	is_in_dir_maildir			(const gchar *dir);

typedef gboolean (* LoadSummaryFileMetaHeaderFct) (SummaryFile *summary, SummaryFileHeader *header);
typedef gboolean (* LoadMailMessageFct) (SummaryFile *summary, MailMessage **mail_msg);
typedef gboolean (* SkipMailMessageFct) (SummaryFile *summary);
typedef gboolean (* SaveOnDiskMailMessageFct) (DBConnection *db_con, MailMessage *msg);

static void	index_mail_messages_by_summary_file	(DBConnection *db_con, MailType mail_type,
							 const gchar *summary_file_path,
							 LoadSummaryFileMetaHeaderFct load_meta_header,
							 LoadMailMessageFct load_mail,
							 SkipMailMessageFct skip_mail,
							 SaveOnDiskMailMessageFct save_ondisk_mail);

static gboolean	open_summary_file			(const gchar *path, SummaryFile **summary);
static void	free_summary_file			(SummaryFile *summary);

static gboolean	load_summary_file_header		(SummaryFile *summary, SummaryFileHeader **header);
static void	free_summary_file_header		(SummaryFileHeader *header);
static gboolean	load_summary_file_meta_header_for_pop	(SummaryFile *summary, SummaryFileHeader *header);
static gboolean	load_summary_file_meta_header_for_maildir (SummaryFile *summary, SummaryFileHeader *header);
static gboolean	load_summary_file_meta_header_for_imap	(SummaryFile *summary, SummaryFileHeader *header);

static gboolean	load_mail_message_for_imap		(SummaryFile *summary, MailMessage **mail_msg);
static gboolean	load_mail_message_for_pop		(SummaryFile *summary, MailMessage **mail_msg);
static gboolean	load_mail_message_for_maildir		(SummaryFile *summary, MailMessage **mail_msg);
static gboolean	do_load_mail_message_for_imap		(SummaryFile *summary, MailMessage **mail_msg, gboolean do_skipping_of_content_info);
static gboolean	do_load_mail_message_for_pop		(SummaryFile *summary, MailMessage **mail_msg, gboolean do_skipping_of_content_info);
static gboolean	do_load_mail_message_for_maildir		(SummaryFile *summary, MailMessage **mail_msg, gboolean do_skipping_of_content_info);

static gboolean	load_mail_message			(SummaryFile *summary, MailMessage *mail_msg);

static gboolean	skip_mail_message_for_imap		(SummaryFile *summary);
static gboolean	skip_mail_message_for_pop		(SummaryFile *summary);
static gboolean	skip_mail_message_for_maildir		(SummaryFile *summary);

static gboolean	do_skip_mail_message_for_maildir	(SummaryFile *summary, gboolean do_skipping_of_content_info);
static gboolean	do_skip_mail_message_for_pop		(SummaryFile *summary, gboolean do_skipping_of_content_info);
static gboolean	do_skip_mail_message_for_imap		(SummaryFile *summary, gboolean do_skipping_of_content_info);

static gboolean	skip_mail_message			(SummaryFile *summary);

static gboolean	skip_loading_content_info		(SummaryFile *summary);
static gboolean	do_skip_loading_content_info		(SummaryFile *summary);

static gboolean	save_ondisk_email_message_for_imap	(DBConnection *db_con, MailMessage *mail_msg);
static gboolean	save_ondisk_email_message_for_pop	(DBConnection *db_con, MailMessage *mail_msg);
static gboolean	save_ondisk_email_message_for_maildir	(DBConnection *db_con, MailMessage *mail_msg);
static gboolean	do_save_ondisk_email_message_generic	(DBConnection *db_con, MailMessage *mail_msg);
static gboolean	do_save_ondisk_email_message		(DBConnection *db_con, MailMessage *mail_msg);

static GSList *	add_persons_from_internet_address_list_string_parsing	(GSList *list, const gchar *s);

static inline gboolean	decode_gint32		(FILE *f, gint32 *n);
static inline gboolean	skip_gint32_decoding	(FILE *f);
static inline gboolean	decode_guint32		(FILE *f, guint32 *n);
static inline gboolean	skip_guint32_decoding	(FILE *f);
static inline gboolean	decode_time_t		(FILE *f, time_t *t);
static inline gboolean	skip_time_t_decoding	(FILE *f);
static inline gboolean	decode_off_t		(FILE *f, off_t *t);
static inline gboolean	skip_off_t_decoding	(FILE *f);
static inline gboolean	decode_string		(FILE *f, gchar **str);
static inline gboolean	skip_string_decoding	(FILE *f);
static inline gboolean	skip_token_decoding	(FILE *f);

static void  check_summary_file (DBConnection *db_con, const gchar *filename, MailStore *store);

static void
load_current_dynamic_folders (ModestConfig *modest_config)
{
	/* TODO: Load and add existing dynamic-found dirs to modest_config->dynamic_dirs */

	email_watch_directories (modest_config->dynamic_dirs, "ModestEmails");
}

#if 0

unused

static void
add_dynamic_folder (ModestConfig *modest_config, const gchar *dir_name)
{
	char *dirn = g_strdup (dir_name);
	modest_config->dynamic_dirs = g_slist_prepend (modest_config->dynamic_dirs, dirn);
	email_watch_directory (dirn, "ModestEmails");
}

static void
del_dynamic_folder (ModestConfig *modest_config, const gchar *dir_name)
{
	GSList *found = g_slist_find_custom (modest_config->dynamic_dirs, dir_name, (GCompareFunc) strcmp);
	if (found) {
		gchar *rem_name = found->data;
		/* */
		email_unwatch_directory (rem_name, "ModestEmails");
		g_free (rem_name);
		modest_config->dynamic_dirs = g_slist_remove_link (modest_config->dynamic_dirs, found);
		g_slist_free (found);
	}
}

#endif

static gboolean
modest_module_is_running (void)
{
	return modest_config != NULL;
}
 

/********************************************************************************************
 Public functions
*********************************************************************************************/

gboolean
tracker_email_init (void)
{
	ModestConfig *conf;

	if (modest_config) {
		return TRUE;
	}

	conf = NULL;

	if (load_modest_config (&conf)) {
		modest_config = conf;
	}

	return modest_module_is_running ();
}



gboolean
tracker_email_finalize (void)
{
	if (!modest_config) {
		return TRUE;
	}

	free_modest_config (modest_config);
	modest_config = NULL;

	return !modest_module_is_running ();
}

static void
free_modest_config (ModestConfig *conf)
{
	if (!conf) {
		return;
	}

	if (conf->mail_dir) {
		g_free (conf->mail_dir);
	}

	#define FREE_MY_LIST(list, free_fct)				\
		g_slist_foreach (list, (GFunc) free_fct, NULL);		\
		g_slist_free (list);

	FREE_MY_LIST (conf->dirs, g_free);

	#undef FREE_MY_LIST

	g_slice_free (ModestConfig, conf);
}

void
tracker_email_watch_emails (DBConnection *db_con)
{
	TrackerDBResultSet *result_set;

	/* if initial indexing has not finished reset mtime on all email stuff so they are rechecked */
	if (tracker_db_get_option_int (db_con->common, "InitialIndex") == 1) {
		char *sql = g_strdup_printf ("update Services set mtime = 0 where path like '%s/.modest/%s'", g_get_home_dir (), "%");

		tracker_db_interface_execute_query (db_con->db, NULL, sql);
		g_free (sql);
	}

	/* check all registered mbox/paths for deletions */
	result_set = tracker_db_email_get_mboxes (db_con);

	if (result_set) {
		gboolean valid = TRUE;
		gchar *filename, *path;
		MailStore *store;

		while (valid) {
			tracker_db_result_set_get (result_set,
						   2, &filename,
						   3, &path,
						   -1);

			store = tracker_db_email_get_mbox_details (db_con, path);

			if (store) {
				check_summary_file (db_con, filename, store);
				tracker_db_email_free_mail_store (store);
			}

			valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	}

	email_watch_directories (modest_config->dirs, "ModestEmails");
	load_current_dynamic_folders (modest_config);
}

static gboolean
modest_file_is_interesting (FileInfo *info)
{
	g_return_val_if_fail (info, FALSE);
	g_return_val_if_fail (info->uri, FALSE);
	g_return_val_if_fail (modest_config, FALSE);
	g_return_val_if_fail (modest_config->mail_dir, FALSE);

	/* maildir/pop/imap all have summary files (*.ev-summary.mmap or "summary.mmap") */
	if ((strcmp (info->uri, "summary.mmap") == 0) || g_str_has_suffix (info->uri, "summary.mmap")) {
		return TRUE;
	} else {
		return FALSE;
	}

	return FALSE;
}


gboolean
tracker_email_index_file (DBConnection *db_con, FileInfo *info)
{
	gchar *file_name;

	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (info, FALSE);

	if (!modest_file_is_interesting (info))
		return FALSE;

	file_name = g_path_get_basename (info->uri);

	tracker_debug ("indexing email summary %s", info->uri);

	if (is_in_dir_imap (info->uri)) {
		if (strcmp (file_name, "summary.mmap") == 0) {
			index_mail_messages_by_summary_file (db_con, MAIL_TYPE_IMAP, info->uri,
							     load_summary_file_meta_header_for_imap,
							     load_mail_message_for_imap,
							     skip_mail_message_for_imap,
							     save_ondisk_email_message_for_imap);
		}
	} else

	if (is_in_dir_pop (info->uri)) {
		if (strcmp (file_name, "summary.mmap") == 0) {
			index_mail_messages_by_summary_file (db_con, MAIL_TYPE_POP, info->uri,
							     load_summary_file_meta_header_for_pop,
							     load_mail_message_for_pop,
							     skip_mail_message_for_pop,
							     save_ondisk_email_message_for_pop);
		}
	} else

	if (is_in_dir_maildir (info->uri)) {
		if (strcmp (file_name, "summary.mmap") || g_str_has_suffix (info->uri, "summary.mmap")) {
			index_mail_messages_by_summary_file (db_con, MAIL_TYPE_MAILDIR, info->uri,
							     load_summary_file_meta_header_for_maildir,
							     load_mail_message_for_maildir,
							     skip_mail_message_for_maildir,
							     save_ondisk_email_message_for_maildir);
		}
	}

	g_free (file_name);

	return TRUE;
}

const gchar *
tracker_email_get_name (void)
{
	return "ModestEmails";
}


/********************************************************************************************
 Private functions
*********************************************************************************************/


static void
check_summary_file (DBConnection *db_con, const gchar *filename, MailStore *store)
{
	SummaryFile *summary = NULL;

	g_return_if_fail (store);

	if (open_summary_file (filename, &summary)) {
		SummaryFileHeader *header;
		gchar             *path;

		header = NULL;

		tracker_log ("Scanning summary file %s for junk", filename);

		if (!load_summary_file_header (summary, &header)) {
			free_summary_file (summary);
			tracker_error ("ERROR: failed to open summary file %s", filename);
			return;
		}

		if (store->type == MAIL_TYPE_IMAP) {
			if (!load_summary_file_meta_header_for_imap (summary, header)) {
				free_summary_file_header (header);
				free_summary_file (summary);
				tracker_error ("ERROR: failed to open summary header file %s", filename);
				return;
			}

		} else if (store->type == MAIL_TYPE_POP) {
			if (!load_summary_file_meta_header_for_pop (summary, header)) {
				free_summary_file_header (header);
				free_summary_file (summary);
				tracker_error ("ERROR: failed to open summary header file %s", filename);
				return;
			}
		} else if (store->type == MAIL_TYPE_MAILDIR) {
			if (!load_summary_file_meta_header_for_pop (summary, header)) {
				free_summary_file_header (header);
				free_summary_file (summary);
				tracker_error ("ERROR: failed to open summary header file %s", filename);
				return;
			}

		} else {
			tracker_error ("ERROR: summary file not supported");
			free_summary_file_header (header);
			free_summary_file (summary);
			return;

		}

		path = tracker_db_email_get_mbox_path (db_con, filename);
		tracker_db_email_set_message_counts (db_con, path, store->mail_count, header->junk_count, header->deleted_count);
		g_free (path);

		free_summary_file_header (header);
		free_summary_file (summary);
	}
}




static GSList*
moredir (char *name, char *lastname, GSList *list) {
	DIR *dir = opendir (name);
	struct dirent *d;

	if (dir) {
		while ( (d = readdir(dir)) ) {
			struct stat st;
			if (strcmp (d->d_name, ".") != 0 && strcmp (d->d_name, "..") != 0) {
			  char *tmp = g_build_filename (name, d->d_name, NULL);
			  if (stat(tmp, &st) == 0 && S_ISDIR(st.st_mode))
				list = moredir (tmp, name, list);
			  g_free (tmp);

			  if ((strcmp (d->d_name, "summary.mmap") == 0) || g_str_has_suffix (d->d_name, "summary.mmap")) {
				tracker_log ("Adding mail_dir: %s\n", name);
				list = g_slist_prepend (list, g_strdup (name));

				/* Question from Philip Van Hoof:
				 * I think we can return here, if a maildir's root 
				 * has two summary files, for two subdirs, only
				 * monitoring the root once should suffice, I think 
				 *
				 * Example FS structure:
				 *
				 * MyMailDirRoot <- Added to maildir_dirs twice (two summary files)
				 * +- MyFolder1
				 * |   +- cur    <- Actual settled E-mails
				 * |   +- new    <- Recently created E-mails
				 * |   `- tmp    <- Being created E-mails
				*  |
				 * +- MyFolder2  <- Added to maildir_dirs once (one summary file)
				 * |   +- MyFolder3.ev-summary.mmap <- to monitor
				 * |   +- MyFolder3
				 * |   |   +- cur
				 * |   |   +- new
				 * |   |   `- tmp
				 * |   |
				 * |   +- cur
				 * |   +- new
				 * |   `- tmp
				 * +- MyFolder1.ev-summary.mmap <- to monitor
				 * `- MyFolder2.ev-summary.mmap <- to monitor
				*/
			  }
			}
		}
		closedir(dir);
	}
	return list;
}



static gboolean
load_modest_config (ModestConfig **conf)
{
	char		*dir_imap, *dir_pop, *dir_maildir;
	ModestConfig	*m_conf;

	if (*conf) {
		free_modest_config (*conf);
	}

	tracker_log ("Checking for Modest email accounts...");

	*conf = g_slice_new0 (ModestConfig);
	m_conf = *conf;

	m_conf->mail_dir = g_build_filename (g_get_home_dir (), MODEST_HOME_CACHE_MAIL, NULL);

	dir_imap = g_build_filename (m_conf->mail_dir, "imap", NULL);
	m_conf->dirs = moredir (dir_imap, m_conf->mail_dir, m_conf->dirs);
	g_free (dir_imap);

	dir_pop = g_build_filename (m_conf->mail_dir, "pop", NULL);
	m_conf->dirs = moredir (dir_pop, m_conf->mail_dir, m_conf->dirs);
	g_free (dir_pop);

	dir_maildir = g_build_filename (g_get_home_dir (), MODEST_HOME_LOCAL_FOLDERS, NULL);
	m_conf->dirs = moredir (dir_maildir, dir_maildir, m_conf->dirs);
	g_free (dir_maildir);

	dir_maildir = g_build_filename (g_get_home_dir (), MODEST_HOME_OUTBOXES, NULL);
	m_conf->dirs = moredir (dir_maildir, dir_maildir, m_conf->dirs);
	g_free (dir_maildir);

	/* Init to NULL here */
	m_conf->dynamic_dirs = NULL;

	return TRUE;
}



static gboolean
is_in_dir_imap (const gchar *path)
{
	g_return_val_if_fail (path, FALSE);
	return strstr (path, G_DIR_SEPARATOR_S "imap") != NULL;
}


static gboolean
is_in_dir_maildir (const gchar *path)
{
	g_return_val_if_fail (path, FALSE);
	return strstr (path, "ev-summary.mmap") != NULL;
}


static gboolean
is_in_dir_pop (const gchar *path)
{
	g_return_val_if_fail (path, FALSE);
	return strstr (path, G_DIR_SEPARATOR_S "pop" G_DIR_SEPARATOR_S) != NULL;
}



/* From RFC 2396 2.4.3, the characters that should always be encoded */
static const char url_encoded_char[] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x00 - 0x0f */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x10 - 0x1f */
	1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  ' ' - '/'  */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,  /*  '0' - '?'  */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  '@' - 'O'  */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,  /*  'P' - '_'  */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  '`' - 'o'  */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1,  /*  'p' - 0x7f */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

static void
append_url_encoded (GString *str, const char *in, const char *extra_enc_chars)
{
	const unsigned char *s = (const unsigned char *)in;
	while (*s) {
		if (url_encoded_char[*s] ||
		    (extra_enc_chars && strchr (extra_enc_chars, *s)))
			g_string_append_printf (str, "%%%02x", (int)*s++);
		else
			g_string_append_c (str, *s++);
	}
}

char *
url_encode (const char *part, const char *escape_extra)
{
	GString *str;
	char *encoded;
	g_return_val_if_fail (part != NULL, NULL);
	str = g_string_new (NULL);
	append_url_encoded (str, part, escape_extra);
	encoded = str->str;
	g_string_free (str, FALSE);
	return encoded;
}

char *
file_util_safe_filename (const char *name)
{
#ifdef G_OS_WIN32
	const char *unsafe_chars = "/?()'*<>:\"\\|";
#else
	const char *unsafe_chars = "/?()'*";
#endif
	if (name == NULL)
		return NULL;
	return url_encode(name, unsafe_chars);
}

#define DATA_CACHE_BITS (6)
#define DATA_CACHE_MASK ((1<<DATA_CACHE_BITS)-1)

static char *
data_cache_path(const char *path, const char *key)
{
	char *dir, *real, *tmp;
	guint32 hash;
	hash = g_str_hash (key);
	hash = (hash >> 5) & DATA_CACHE_MASK;
	dir = alloca (strlen(path) + 8 + 6);
	sprintf (dir, "%s/cache/%02x", path, hash);
	tmp = file_util_safe_filename (key);
	real = g_strdup_printf ("%s/%s", dir, tmp);
	g_free (tmp);
	return real;
}


char *maildir_get_filename (const gchar *fpath, const gchar *uid)
{
	gint len = 0;
	gchar *cur = g_strdup_printf("%s/cur", fpath);
	gchar *compare = g_strdup_printf("%s/%s", cur, uid);
	DIR *dir; struct dirent *d;
	dir = opendir(cur);

	if (dir) {
		while ( (d = readdir(dir)) ) {
			char *nname = g_strdup_printf ("%s/%s", cur, d->d_name);
			char *ptr = strstr (nname, "!");

			if (!ptr)
				strstr (nname, ":");

			if (ptr)
				len = (ptr - nname);
			else
				len = strlen (nname);

			if (!g_ascii_strncasecmp (nname, compare, len)) {
				g_free (compare);
				compare = nname;
				break;
			}
			g_free (nname);
		}
		closedir(dir);
	}
	g_free (cur);

	return compare;
}

static void
index_mail_messages_by_summary_file (DBConnection                 *db_con,
				     MailType                     mail_type,
				     const gchar                  *summary_file_path,
				     LoadSummaryFileMetaHeaderFct load_meta_header,
				     LoadMailMessageFct           load_mail,
				     SkipMailMessageFct           skip_mail,
				     SaveOnDiskMailMessageFct     save_ondisk_mail)
{
	SummaryFile *summary = NULL;

	if (!tracker->is_running)
		return; 

	if (open_summary_file (summary_file_path, &summary)) {
		SummaryFileHeader *header;
		gint32            mail_count, junk_count, delete_count;
		gchar             *dir;

		header = NULL;

		if (!load_summary_file_header (summary, &header)) {
			free_summary_file (summary);
			return;
		}

		if (!(*load_meta_header) (summary, header)) {
			free_summary_file_header (header);
			free_summary_file (summary);
			return;
		}

		if (mail_type != MAIL_TYPE_MAILDIR)
			dir = g_path_get_dirname (summary->path);
		else {
			gchar *tdir = g_strdup (summary->path);
			char *part = strstr (tdir, "ev-summary.mmap");
			/**
			 * home/user/.modest/local_folders/New folder/
			 * home/user/.modest/local_folders/New folder/cur/
			 * home/user/.modest/local_folders/New folder/new/
			 * home/user/.modest/local_folders/New folder/tmp/
			 * home/user/.modest/local_folders/New folder.ev-summary.mmap <--
			 **/
			if (part) {
				part--;
				*part = '\0';
				dir = tdir;
			} else {
				dir = g_path_get_dirname (summary->path);
				g_free (tdir);
			}
		}

		/* check summary file is registered */
		if (tracker_db_email_get_mbox_id (db_con, dir) == -1) {
			char *uri_prefix = NULL;

			if (mail_type == MAIL_TYPE_IMAP || mail_type == MAIL_TYPE_POP) {
				const gchar *pos_folders = strstr (dir, G_DIR_SEPARATOR_S "folders" G_DIR_SEPARATOR_S);
				char *piece;
				char *tdir = g_strdup (dir);
				char *loc = strstr (tdir, MODEST_HOME_CACHE_MAIL);
				char *tloc;

				loc += strlen (MODEST_HOME_CACHE_MAIL) + 1;

				loc = strchr (loc, '/'); loc++; /* word imap|pop */
				tloc = strchr (loc, '/'); /* word account name */
				if (tloc) {
					loc = tloc;
					loc++; 
				} else
					loc = tdir + strlen (tdir);

				*loc = '\0';

				char *tmp = g_build_filename (tdir, "url_string", NULL);
				FILE *f = fopen (tmp, "r");
				if (f) {
					int len;
					char str[4000];
					memset (str, 0, 4000);
					fgets (str, 4000, f);
					len = strlen (str);
					if (str [len - 1] == '\n')
						piece = g_strndup (str, len - 1);
					else
						piece = g_strndup (str, len);
				} else
					piece = g_strdup ("unknown://location/");
				g_free (tmp);
				g_free (tdir);

				if (pos_folders) {
					size_t len_pos_folders = strlen (pos_folders);
					if (len_pos_folders > 9) {
						gchar *uri_dir, *clean_uri_dir;
						pos_folders += 9;
						uri_dir = NULL;
						if (pos_folders)
							uri_dir = g_strdup (pos_folders);
						clean_uri_dir = NULL;
						if (uri_dir) {
							gchar *tmp_str = tracker_string_replace (uri_dir, "subfolders/", NULL);
							clean_uri_dir = tracker_string_replace (tmp_str, "folders/", NULL);
							g_free (tmp_str);
							g_free (uri_dir);
						}
						if (clean_uri_dir) {
							uri_prefix = g_strdup_printf ("%s/%s/", piece, clean_uri_dir);
							g_free (clean_uri_dir);
						} else
							uri_prefix = g_strdup (piece);
					} else
						uri_prefix = g_strdup (piece);
				} else
					uri_prefix = g_strdup_printf ("%s/INBOX/", piece);

				g_free (piece);

			} else if (mail_type == MAIL_TYPE_MAILDIR)
				uri_prefix = g_strdup_printf ("maildir:/%s/", dir);
			else 
				uri_prefix = g_strdup ("unknown://location/");

			tracker_db_email_register_mbox (db_con, MAIL_APP_MODEST, mail_type, dir, summary_file_path, uri_prefix);
			g_free (uri_prefix);
		}


		MailStore *store = tracker_db_email_get_mbox_details (db_con, dir);

		if (!store) {
			tracker_error ("ERROR: could not retrieve store for file %s", dir);
			free_summary_file (summary);
			free_summary_file_header (header);

			g_free (dir);
			return;
		}

		tracker_debug ("Number of existing messages in %s are %d, %d junk, %d deleted and header totals are %d, %d, %d", dir,
				store->mail_count, store->junk_count, store->delete_count, header->saved_count, header->junk_count, header->deleted_count);

		tracker->mbox_count++;
		tracker_dbus_send_index_progress_signal ("Emails", dir);

		if (header->saved_count > store->mail_count) {
			/* assume new emails received */

			gint i;

			/* skip already indexed emails */
			for (i = 0; i < store->mail_count; i++) {
				if (!(*skip_mail) (summary)) {
					tracker_error ("ERROR: skipping email no. %d in summary file", i+1);
					tracker_db_email_free_mail_store (store);
					free_summary_file (summary);
					free_summary_file_header (header);
					tracker->mbox_processed++;
					g_free (dir);
					return;
				}
			}

			mail_count = 0;
			junk_count = 0;
			delete_count = 0;

			/* now we will read the new emails */
			for (i = store->mail_count; i < header->saved_count; i++) {
				MailMessage *mail_msg = NULL;

				mail_count++;
				tracker_debug ("processing email no. %d / %" G_GINT32_FORMAT, store->mail_count + 1, header->saved_count);

				if (!(*load_mail) (summary, &mail_msg)) {
					tracker_error ("ERROR: loading email no. %d in summary file", mail_count);
					tracker_db_email_free_mail_store (store);
					free_summary_file (summary);
					free_summary_file_header (header);
					tracker->mbox_processed++;
					g_free (dir);
					return;
				}

				

				if (mail_type == MAIL_TYPE_IMAP) {
					gchar *sum_file_dir = g_path_get_dirname (summary->path);
					mail_msg->path = g_strdup_printf ("%s%s%s.", sum_file_dir, G_DIR_SEPARATOR_S, mail_msg->uid);
					g_free (sum_file_dir);
				} else if (mail_type == MAIL_TYPE_POP) {
					gchar *sum_file_dir = g_path_get_dirname (summary->path);
					mail_msg->path = data_cache_path (sum_file_dir, mail_msg->uid);
					g_free (sum_file_dir);
				} else if (mail_type == MAIL_TYPE_MAILDIR) {
					mail_msg->path = maildir_get_filename (dir, mail_msg->uid);
				} else
					mail_msg->path = NULL;

				mail_msg->uri = g_strconcat (store->uri_prefix, mail_msg->uid, NULL);

				mail_msg->store = store;

				if (!(*save_ondisk_mail) (db_con, mail_msg)) {
					tracker_log ("WARNING: Message, or message parts, could not be found locally - if you are using IMAP make sure you have selected the \"copy folder content locally for offline operation\" option in Modest");
					/* we do not have all infos but we still save them */
					if (!tracker_db_email_save_email (db_con, mail_msg, MAIL_APP_MODEST)) {
						tracker_log ("Failed to save email");
					}
				}

				if (mail_msg->junk) {
					junk_count++;
				}

				if (mail_msg->deleted) {
					delete_count++;
				}

				email_free_mail_file (mail_msg->parent_mail_file);
				email_free_mail_message (mail_msg);

				if (!tracker_cache_process_events (db_con->data, TRUE)) {
					tracker_set_status (tracker, STATUS_SHUTDOWN, 0, TRUE);
					tracker->shutdown = TRUE;
					return;
				}

				if (tracker_db_regulate_transactions (db_con->data, 500)) {

					if (tracker->index_count % 1000 == 0) {
						tracker_db_end_index_transaction (db_con->data);
						tracker_db_refresh_all (db_con->data);
						tracker_db_start_index_transaction (db_con->data);
					}
					
					tracker_dbus_send_index_progress_signal ("Emails", dir);
								
				}

			
			}

			tracker_log ("No. of new emails indexed in summary file %s is %d, %d junk, %d deleted", dir, mail_count, junk_count, delete_count);

			tracker_db_email_set_message_counts (db_con, dir, store->mail_count, store->junk_count, store->delete_count);

		} else {
			/* schedule check for junk */
			tracker_db_email_flag_mbox_junk (db_con, dir);
		}

		tracker->mbox_processed++;
		tracker_dbus_send_index_progress_signal ("Emails", dir);

		tracker_db_email_free_mail_store (store);
		free_summary_file (summary);
		free_summary_file_header (header);

		g_free (dir);
	}
}




static gboolean
open_summary_file (const gchar *path, SummaryFile **summary)
{
	gint fd;
	FILE *f;

	g_return_val_if_fail (path, FALSE);

	if (!tracker_file_is_indexable (path)) {
		return FALSE;
	}

	if (*summary) {
		free_summary_file (*summary);
		*summary = NULL;
	}

	fd = tracker_file_open (path, TRUE);

	if (fd == -1) {
		return FALSE;
	}

	f = fdopen (fd, "r");
	if (!f) {
		tracker_file_close (fd, TRUE);
		return FALSE;
	}

	*summary = g_slice_new0 (SummaryFile);
	(*summary)->f = f;
	(*summary)->path = g_strdup (path);

	return TRUE;
}


static void
free_summary_file (SummaryFile *summary)
{
	if (!summary) {
		return;
	}

	fclose (summary->f);

	g_free (summary->path);
	g_slice_free (SummaryFile, summary);
}


static gboolean
load_summary_file_header (SummaryFile *summary, SummaryFileHeader **header)
{
	SummaryFileHeader *h;
	FILE              *f;

	g_return_val_if_fail (summary, FALSE);

	if (*header) {
		free_summary_file_header (*header);
	}

	*header = g_slice_new0 (SummaryFileHeader);

	h = *header;

	h->uri_prefix = NULL;

	f = summary->f;


	if (!decode_gint32 (f, &h->version)) {
		goto error;
	}

	tracker_debug ("summary.version = %d", h->version);

	if (h->version > 0xff && (h->version & 0xff) < 12) {
		tracker_error ("ERROR: summary file header version too low");
		goto error;
	}

	h->legacy = !(h->version < 0x100 && h->version >= 13);

	if (h->legacy) {
		tracker_debug ("WARNING: summary file is a legacy version");
	}


	if (!decode_gint32 (f, &h->flags) ||
	    !decode_gint32 (f, &h->nextuid) ||
	    !decode_time_t (f, &h->time) ||
	    !decode_gint32 (f, &h->saved_count)) {
		goto error;
	}

	tracker_debug ("summary.flags = %d", h->flags);
	tracker_debug ("summary.nextuid = %d", h->nextuid);
	tracker_debug ("summary.time = %d", h->time);
	tracker_debug ("summary.count = %" G_GINT32_FORMAT, h->saved_count);

	if (!h->legacy) {
		if (!decode_gint32 (f, &h->unread_count) ||
		    !decode_gint32 (f, &h->deleted_count) ||
		    !decode_gint32 (f, &h->junk_count)) {
			goto error;
		}
	}

	tracker_debug ("summary.Unread = %d", h->unread_count);
	tracker_debug ("summary.deleted = %d", h->deleted_count);
	tracker_debug ("summary.junk = %d", h->junk_count);

	return TRUE;

 error:
	free_summary_file_header (*header);
	*header = NULL;

	return FALSE;
}


static void
free_summary_file_header (SummaryFileHeader *header)
{
	if (!header) {
		return;
	}

	if (header->uri_prefix) {
		g_free (header->uri_prefix);
	}

	g_slice_free (SummaryFileHeader, header);
}


static gboolean
load_summary_file_meta_header_maildir (SummaryFile *summary, SummaryFileHeader *header)
{
	FILE    *f;

	g_return_val_if_fail (summary, FALSE);
	g_return_val_if_fail (header, FALSE);

	f = summary->f;

	/* check for legacy version */
	if (header->version != 0x30c) {
		gint32 version;

		if (!decode_gint32 (f, &version)) {
			return FALSE;
		}

		if (version < 0) {
			tracker_error ("ERROR: summary file version too low");
			return FALSE;
		}

		/* Right now we only support summary versions 1 through 3 */
		if (version > 3) {
			tracker_error ("ERROR: reported summary version (%" G_GINT32_FORMAT ") is too new", version);
			return FALSE;
		}

		if (version == 2) {
			if (!skip_gint32_decoding (f)) {
				return FALSE;
			}
		}

	} 

	return TRUE;
}


static gboolean
load_summary_file_meta_header_pop (SummaryFile *summary, SummaryFileHeader *header)
{
	FILE    *f;

	g_return_val_if_fail (summary, FALSE);
	g_return_val_if_fail (header, FALSE);

	f = summary->f;

	return TRUE;
}

static gboolean
load_summary_file_meta_header_for_imap (SummaryFile *summary, SummaryFileHeader *header)
{
	FILE    *f;
	guint32 dummy0;

	g_return_val_if_fail (summary, FALSE);
	g_return_val_if_fail (header, FALSE);

	f = summary->f;

	/* check for legacy version */
	if (header->version != 0x30c) {
		gint32 version, dummy1;

		if (!decode_gint32 (f, &version)) {
			return FALSE;
		}

		if (version < 0) {
			tracker_error ("ERROR: summary file version too low");
			return FALSE;
		}

		/* Right now we only support summary versions 1 through 3 */
		if (version > 3) {
			tracker_error ("ERROR: reported summary version (%" G_GINT32_FORMAT ") is too new", version);
			return FALSE;
		}

		if (version == 2) {
			if (!skip_gint32_decoding (f)) {
				return FALSE;
			}
		}

		/* validity */
		if (!decode_gint32 (f, &dummy1)) {
			return FALSE;
		}
	} else {
		/* validity */
		if (!decode_guint32 (f, &dummy0)) {
			return FALSE;
		}
	}

	return TRUE;
}


static gboolean
load_summary_file_meta_header_for_pop (SummaryFile *summary, SummaryFileHeader *header)
{
	return load_summary_file_meta_header_pop (summary, header);
}

static gboolean
load_summary_file_meta_header_for_maildir (SummaryFile *summary, SummaryFileHeader *header)
{
	return load_summary_file_meta_header_maildir (summary, header);
}


static gboolean
load_mail_message_for_imap (SummaryFile *summary, MailMessage **mail_msg)
{
	return do_load_mail_message_for_imap (summary, mail_msg, TRUE);
}

static gboolean
load_mail_message_for_maildir (SummaryFile *summary, MailMessage **mail_msg)
{
	return do_load_mail_message_for_maildir (summary, mail_msg, FALSE);
}

static gboolean
load_mail_message_for_pop (SummaryFile *summary, MailMessage **mail_msg)
{
	return do_load_mail_message_for_pop (summary, mail_msg, FALSE);
}


static gboolean
do_load_mail_message_for_imap (SummaryFile *summary, MailMessage **mail_msg, gboolean do_skipping_of_content_info)
{
	guint32 server_flags;

	g_return_val_if_fail (summary, FALSE);

	if (*mail_msg) {
		email_free_mail_message (*mail_msg);
	}

	*mail_msg = email_allocate_mail_message ();

	if (!load_mail_message (summary, *mail_msg)) {
		goto error;
	}

	if (!decode_guint32 (summary->f, &server_flags)) {
		goto error;
	}

	if (do_skipping_of_content_info) {
		if (!skip_loading_content_info (summary)) {
			goto error;
		}
	}

	return TRUE;

 error:
	email_free_mail_message (*mail_msg);
	*mail_msg = NULL;
	return FALSE;
}



static gboolean
do_load_mail_message_generic (SummaryFile *summary, MailMessage **mail_msg, gboolean do_skipping_of_content_info)
{
	g_return_val_if_fail (summary, FALSE);

	if (*mail_msg) {
		email_free_mail_message (*mail_msg);
	}

	*mail_msg = email_allocate_mail_message ();

	if (!load_mail_message (summary, *mail_msg)) {
		goto error;
	}

	if (do_skipping_of_content_info) {
		if (!skip_loading_content_info (summary)) {
			goto error;
		}
	}

	return TRUE;

 error:
	email_free_mail_message (*mail_msg);
	*mail_msg = NULL;
	return FALSE;
}



static gboolean
do_load_mail_message_for_pop (SummaryFile *summary, MailMessage **mail_msg, gboolean do_skipping_of_content_info)
{

	return do_load_mail_message_generic (summary, mail_msg, do_skipping_of_content_info);
}



static gboolean
do_load_mail_message_for_maildir (SummaryFile *summary, MailMessage **mail_msg, gboolean do_skipping_of_content_info)
{
	return do_load_mail_message_generic (summary, mail_msg, do_skipping_of_content_info);
}

static gboolean
load_mail_message (SummaryFile *summary, MailMessage *mail_msg)
{
	FILE    *f;
	guint32	flags, size, count;
	time_t  date_sent, date_received;
	gchar   *uid, *to, *cc, *mlist;

	g_return_val_if_fail (summary, FALSE);
	g_return_val_if_fail (mail_msg, FALSE);

	f = summary->f;

	if (!decode_string (f, &uid) ||
	    !decode_guint32 (f, &size) ||  /* Size and flags are reversed in tny */
	    !decode_guint32 (f, &flags) ||  /* Size and flags are reversed in tny */
	    !decode_time_t (f, &date_sent) ||
	    !decode_time_t (f, &date_received) ||
	    !decode_string (f, &mail_msg->subject) ||
	    !decode_string (f, &mail_msg->from) ||
	    !decode_string (f, &to) ||
	    !decode_string (f, &cc) ||
	    !decode_string (f, &mlist)) /* mlist will be an empty string in tny */
	{

		return FALSE;
	}

	g_free (mlist);


	/* This is pointless, it's not always an int as a string*/
	mail_msg->id = strtoul (uid, NULL, 10);

	/* Only this really makes sense */
	mail_msg->uid = uid; /* g_free (uid); */

	if ((flags & MODEST_MESSAGE_DELETED) == MODEST_MESSAGE_DELETED) {
		mail_msg->deleted = TRUE;
	}

	if ((flags & MODEST_MESSAGE_EXPUNGED) == MODEST_MESSAGE_EXPUNGED) {
		mail_msg->deleted = TRUE;
	}

	mail_msg->junk = FALSE;

	mail_msg->date = (long) date_received;

	mail_msg->to = add_persons_from_internet_address_list_string_parsing (NULL, to);
	mail_msg->cc = add_persons_from_internet_address_list_string_parsing (NULL, cc);

	g_free (to);
	g_free (cc);

	skip_gint32_decoding (f);	/* mi->message_id.id.part.hi */
	skip_gint32_decoding (f);	/* mi->message_id.id.part.lo */

	/* references */
	if (decode_guint32 (f, &count) && count <= 500) {
		guint32 i;
		for (i = 0; i < count; i++) {
			skip_gint32_decoding (f);	/* mi->references->references[i].id.part.hi */
			skip_gint32_decoding (f);	/* mi->references->references[i].id.part.lo */
		}
	} else {
		return FALSE;
	}

	/* user flags */
	if (decode_guint32 (f, &count) && count <= 500) {
		guint32 i;
		for (i = 0; i < count; i++) {
			skip_string_decoding (f);	/* a flag */
		}
	} else {
		return FALSE;
	}

	/* user tags */
	if (decode_guint32 (f, &count) && count <= 500) {
		guint32 i;
		for (i = 0; i < count; i++) {
			skip_string_decoding (f);	/* tag name */
			skip_string_decoding (f);	/* tag value */
		}
	} else {
		return FALSE;
	}

	return TRUE;
}


static gboolean
skip_mail_message_for_imap (SummaryFile *summary)
{
	return do_skip_mail_message_for_imap (summary, TRUE);
}


static gboolean
skip_mail_message_for_pop (SummaryFile *summary)
{
	return do_skip_mail_message_for_pop (summary, FALSE);
}

static gboolean
skip_mail_message_for_maildir (SummaryFile *summary)
{
	return do_skip_mail_message_for_maildir (summary, FALSE);
}


static gboolean
do_skip_mail_message_for_imap (SummaryFile *summary, gboolean do_skipping_of_content_info)
{
	g_return_val_if_fail (summary, FALSE);

	if (!skip_mail_message (summary)) {
		return FALSE;
	}

	if (!skip_guint32_decoding (summary->f)) {
		return FALSE;
	}

	if (do_skipping_of_content_info) {
		if (!skip_loading_content_info (summary)) {
			return FALSE;
		}
	}

	return TRUE;
}



static gboolean
do_skip_mail_message_for_pop (SummaryFile *summary, gboolean do_skipping_of_content_info)
{
	g_return_val_if_fail (summary, FALSE);

	if (!skip_mail_message (summary)) {
		return FALSE;
	}

	if (do_skipping_of_content_info) {
		if (!skip_loading_content_info (summary)) {
			return FALSE;
		}
	}

	return TRUE;
}



static gboolean
do_skip_mail_message_for_maildir (SummaryFile *summary, gboolean do_skipping_of_content_info)
{
	g_return_val_if_fail (summary, FALSE);

	if (!skip_mail_message (summary)) {
		return FALSE;
	}

	if (do_skipping_of_content_info) {
		if (!skip_loading_content_info (summary)) {
			return FALSE;
		}
	}

	return TRUE;
}




static gboolean
skip_mail_message (SummaryFile *summary)
{
	FILE    *f;
	guint32 count;
	time_t  tt;
	guint   n;

	g_return_val_if_fail (summary, FALSE);

	f = summary->f;

	if (!skip_string_decoding (f))
		return FALSE;

	if (!decode_guint32 (f, &n))
		return FALSE;

	if (!decode_guint32 (f, &n))
		return FALSE;

	if (!decode_time_t (f, &tt))
		return FALSE;

	if (!decode_time_t (f, &tt))
		return FALSE;

	if (!skip_string_decoding (f))
		return FALSE;

	if (!skip_string_decoding (f))
		return FALSE;

	if (!skip_string_decoding (f))
		return FALSE;

	if (!skip_string_decoding (f))
		return FALSE;

	if (!skip_string_decoding (f))
		return FALSE;


	skip_gint32_decoding (f);	/* mi->message_id.id.part.hi */
	skip_gint32_decoding (f);	/* mi->message_id.id.part.lo */

	/* references , always 0 items in tny */
	if (decode_guint32 (f, &count) && count <= 500) {
		guint32 i;
		for (i = 0; i < count; i++) {
			skip_gint32_decoding (f);	/* mi->references->references[i].id.part.hi */
			skip_gint32_decoding (f);	/* mi->references->references[i].id.part.lo */
		}
	} else {
		return FALSE;
	}

	/* user flags , always 0 items in tny */
	if (decode_guint32 (f, &count) && count <= 500) {
		guint32 i;
		for (i = 0; i < count; i++) {
			skip_string_decoding (f);	/* a flag */
		}
	} else {
		return FALSE;
	}

	/* user tags , always 0 items in tny */
	if (decode_guint32 (f, &count) && count <= 500) {
		guint32 i;
		for (i = 0; i < count; i++) {
			skip_string_decoding (f);	/* tag name */
			skip_string_decoding (f);	/* tag value */
		}
	} else {
		return FALSE;
	}

	return TRUE;
}


static inline gboolean
skip_over_mail (FILE *f)
{
	return (skip_string_decoding (f) &&  /* uid */
		skip_guint32_decoding (f) && /* size switched with flags in tny */
		skip_guint32_decoding (f) && /* flags switched with size in tny */
		skip_time_t_decoding (f) &&  /* date_sent */
		skip_time_t_decoding (f) &&  /* date_received */
		skip_string_decoding (f) &&  /* subject */
		skip_string_decoding (f) &&  /* from */
		skip_string_decoding (f) &&  /* to */
		skip_string_decoding (f) &&  /* cc */
		skip_string_decoding (f));   /* mlist? */
}


static gboolean
skip_loading_content_info (SummaryFile *summary)
{
	guint32 count, i;

	g_return_val_if_fail (summary, FALSE);

	if (!do_skip_loading_content_info (summary)) {
		return FALSE;
	}

	if (!decode_guint32 (summary->f, &count)) {
		return FALSE;
	}

	if (count > 500) {
		return FALSE;
	}

	for (i = 0; i < count; i++) {
		if (!skip_loading_content_info (summary)) {
			return FALSE;
		}
	}

	return TRUE;
}


static gboolean
do_skip_loading_content_info (SummaryFile *summary)
{
	FILE    *f;
	guint32	count;
	guint32 doit = 0;

	g_return_val_if_fail (summary, FALSE);

	f = summary->f;

	if (!decode_guint32 (f, &doit))
		return FALSE;

	if (!doit)
		return TRUE;

	skip_token_decoding (f);	/* type */
	skip_token_decoding (f);	/* subtype */

	if (decode_guint32 (f, &count) && count <= 500) {
		guint32 i;
		for (i = 0; i < count; i++) {
			skip_token_decoding (f);	/* name */
			skip_token_decoding (f);	/* value */
		}
	} else {
		return FALSE;
	}

	if (!skip_token_decoding (f) ||		/* id */
	    !skip_token_decoding (f) ||		/* description */
	    !skip_token_decoding (f) ||		/* encoding */
	    !decode_guint32 (f, &count)) {	/* size */
		return FALSE;
	}

	return TRUE;
}


static gboolean
save_ondisk_email_message_for_imap (DBConnection *db_con, MailMessage *mail_msg)
{
	return do_save_ondisk_email_message_generic (db_con, mail_msg);
}


static gboolean
save_ondisk_email_message_for_pop (DBConnection *db_con, MailMessage *mail_msg)
{
	return do_save_ondisk_email_message_generic (db_con, mail_msg);
}

static gboolean
save_ondisk_email_message_for_maildir (DBConnection *db_con, MailMessage *mail_msg)
{
	return do_save_ondisk_email_message_generic (db_con, mail_msg);
}

static gboolean
do_save_ondisk_email_message_generic (DBConnection *db_con, MailMessage *mail_msg)
{
	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (mail_msg, FALSE);

	tracker_log ("Trying to index mail \"%s\"", mail_msg->uri);

	if (!do_save_ondisk_email_message (db_con, mail_msg)) {
		tracker_log ("Indexing mail without body nor attachment parsing \"%s\"", mail_msg->uri);
		tracker_db_email_save_email (db_con, mail_msg, MAIL_APP_MODEST);
	} else {
		tracker_log ("Simple index of mail \"%s\" finished", mail_msg->uri);
	}


	return TRUE;

}

static void
find_attachment (GMimeObject *obj, gpointer data) 
{
	/* For now we don't want to do anything with attachments on the device */
	return;
}

static gboolean
do_save_ondisk_email_message (DBConnection *db_con, MailMessage *mail_msg)
{
	g_return_val_if_fail (db_con, FALSE);
	g_return_val_if_fail (mail_msg, FALSE);

	if (mail_msg->path && g_file_test (mail_msg->path, G_FILE_TEST_EXISTS) && tracker_file_is_indexable (mail_msg->path)) {
		/* we have downloaded the mail message on disk so we can fully index it. */
		MailMessage *mail_msg_on_disk = NULL;

		/* The last argument is a function pointer for implementing 
		 * fetching the attachments and indexing them */

		mail_msg_on_disk = email_parse_mail_message_by_path (MAIL_APP_MODEST,
				mail_msg->path, NULL, NULL, find_attachment);

		if (mail_msg_on_disk && mail_msg_on_disk->parent_mail_file) {

			mail_msg_on_disk->parent_mail_file->next_email_offset = 0;
			mail_msg_on_disk->uri = g_strdup (mail_msg->uri);
			mail_msg_on_disk->store = mail_msg->store;

			tracker_db_email_save_email (db_con, mail_msg_on_disk, MAIL_APP_MODEST);

			email_free_mail_file (mail_msg_on_disk->parent_mail_file);
			email_free_mail_message (mail_msg_on_disk);

			return TRUE;
		}
	}

	return FALSE;
}


static GSList *
add_persons_from_internet_address_list_string_parsing (GSList *list, const gchar *s)
{
	InternetAddressList *addrs_list, *tmp;

	g_return_val_if_fail (s, NULL);

	addrs_list = internet_address_parse_string (s);

	for (tmp = addrs_list; tmp; tmp = tmp->next) {
		MailPerson *mp;

		mp = email_allocate_mail_person ();

		mp->addr = g_strdup (tmp->address->value.addr);
		if(tmp->address->name)
                	mp->name = g_strdup (tmp->address->name);
		else
			mp->name = g_strdup (tmp->address->value.addr);

		list = g_slist_prepend (list, mp);
	}

	internet_address_list_destroy (addrs_list);

	return list;
}



/***
  Functions to decode summary (or ev-summary) files.
***/


static inline gboolean
decode_string (FILE *in, gchar **str)
{
	guint32 len;
	register gchar *ret;

	if (!decode_guint32 (in, &len)) {
		*str = NULL;
		return FALSE;
	}

	/* I think I need to remove this for tinymail vs. evolution,
	 * because Tinymail's format cares about a last \0 character */

	/* len--; */

	if (len > 65536) {
		*str = NULL;
		return FALSE;
	}

	ret = g_malloc (len+1);
	if (len > 0 && fread (ret, len, 1, in) != 1) {
		g_free (ret);
		*str = NULL;
		return FALSE;
	}

	ret[len] = 0;
	*str = ret;
	return TRUE;
}


static inline gboolean
decode_gint32 (FILE *f, gint32 *dest)
{
	guint32 save;

	if (!f) return FALSE;

	if (fread (&save, sizeof (save), 1, f) == 1) {
		*dest = g_ntohl (save);
		return TRUE;
	} else {
		return FALSE;
	}
}

/* This is completely different in Tinymail (we just reuse the impl for signed) */
static inline gboolean
decode_guint32 (FILE *f, guint32 *n)
{
	return decode_gint32 (f, (gint32*)n);
}

/* These are all aliases for signed ints in tinymail's format */

#define CFU_DECODE_T(type)				\
static inline gboolean					\
decode_##type (FILE *in, type *dest)			\
{							\
	return decode_gint32 (in, (gint32*) dest);	\
}

CFU_DECODE_T(time_t)
CFU_DECODE_T(off_t)
CFU_DECODE_T(size_t)

static inline gboolean
skip_gint32_decoding (FILE *f)
{
	return fseek (f, 4, SEEK_CUR) > 0;
}


/* Same, we just reuse the impl. for signed numbers */
static inline gboolean
skip_guint32_decoding (FILE *f)
{
	return skip_gint32_decoding (f);
}

/* Same, we just reuse the impl. for signed numbers */
static inline gboolean
skip_time_t_decoding (FILE *f)
{
	return skip_gint32_decoding (f);
}

/* Same, we just reuse the impl. for signed numbers */
static inline gboolean
skip_off_t_decoding (FILE *f)
{
	return skip_gint32_decoding (f);
}


static inline gboolean
skip_string_decoding (FILE *f)
{
	guint32 len;

	if (!decode_guint32 (f, &len)) {
		return FALSE;
	}

	/* Same as decode_string, Tinymail cares about the last \0 character
	 * whereas this character is not present in Evolution's format */

	if (fseek (f, len /*- 1*/, SEEK_CUR) != 0) {
		tracker_error ("ERROR: seek failed for string with length %d with error code %d", len - 1, errno);
		return FALSE;
	}

	return TRUE;
}


static inline gboolean
skip_token_decoding (FILE *f)
{
	guint32 len;

	if (!decode_guint32 (f, &len)) {
		return FALSE;
	}

	if (len < 32) {
		return TRUE;
	}

	len -= 32;
	return fseek (f, len, SEEK_CUR) == 0;
}
