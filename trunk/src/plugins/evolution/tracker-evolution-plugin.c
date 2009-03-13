/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <inttypes.h>

#include <sqlite3.h>

#include <camel/camel-mime-message.h>
#include <camel/camel-i18n.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-db.h>
#include <camel/camel-offline-store.h>
#include <camel/camel-session.h>

#include <mail/mail-config.h>
#include <mail/mail-session.h>
#include <mail/em-utils.h>
#include <mail/mail-ops.h>

#include <e-util/e-config.h>

#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>

#include "tracker-evolution-plugin.h"
#include "tracker-evolution-plugin-glue.h"

/* This runs in-process of evolution (in the mailer, as a EPlugin). It has 
 * access to the CamelSession using the external variable 'session'. The header
 * mail/mail-session.h makes this variable public */

#define MAX_BEFORE_SEND 2000

G_DEFINE_TYPE (TrackerEvolutionPlugin, tracker_evolution_plugin, G_TYPE_OBJECT)

#define TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_EVOLUTION_PLUGIN, TrackerEvolutionPluginPrivate))

/* Some helper-defines */
#define CAMEL_CALLBACK(func) ((CamelObjectEventHookFunc) func)
#define EXTRACT_STRING(val) if (*part) part++; len=strtoul (part, &part, 10); if (*part) part++; val=g_strndup (part, len); part+=len;
#define EXTRACT_FIRST_DIGIT(val) val=strtoul (part, &part, 10);

/* About the locks being used: Camel's API must be used in a multi-threaded
 * fashion. Therefore it's necessary to guard against concurrent access of
 * memory. Especially given that both the mainloop and the Camel-threads will
 * be accessing the memory (mainloop for DBus calls, and Camel-threads mostly
 * during registration of accounts and folders) */

typedef struct {
	guint64 last_checkout;
	DBusGProxy *registrar;
	guint signal;
} ClientRegistry;

typedef struct {
	TrackerEvolutionPlugin *self;
	gchar *account_uri;
	guint hook_id;
} OnSummaryChangedInfo;

typedef struct {
	OnSummaryChangedInfo *hook_info;
	CamelFolder *folder;
} FolderRegistry;

typedef struct {
	EAccount *account;
	TrackerEvolutionPlugin *self;
	guint hook_id;
	CamelStore *store;
} StoreRegistry;

typedef struct {
	TrackerEvolutionPlugin *self;
	gchar *account_uri;
	ClientRegistry *info;
} IntroductionInfo;

typedef struct {
	TrackerEvolutionPlugin *self;
	gchar *uri;
	gboolean old_state;
	EAccount *account;
} RegisterInfo;

typedef struct {
	DBusGConnection *connection;
	GHashTable *registrars;
	GStaticRecMutex *mutex;
	GHashTable *registered_folders;
	GHashTable *cached_folders;
	GHashTable *registered_stores;
	GList *registered_clients;
	EAccountList *accounts;
} TrackerEvolutionPluginPrivate;

enum {
	PROP_0,
	PROP_CONNECTION
};

static DBusGProxy *dbus_proxy = NULL;
static TrackerEvolutionPlugin *manager = NULL;
static GStaticRecMutex glock = G_STATIC_REC_MUTEX_INIT;

/* Prototype declarations */
static void register_account (TrackerEvolutionPlugin *self, EAccount *account);
static void unregister_account (TrackerEvolutionPlugin *self, EAccount *account);
int e_plugin_lib_enable (EPluginLib *ep, int enable);
static void metadata_set_many (TrackerEvolutionPlugin *self, GStrv subjects, GPtrArray *predicates, GPtrArray *values);
static void metadata_unset_many (TrackerEvolutionPlugin *self, GStrv subjects);

static GList *
get_recipient_list (const gchar *str)
{
	GList *list = NULL;
	gchar **arr;
	gint i;

	if (!str) {
		return NULL;
	}

	arr = g_strsplit (str, ",", -1);

	for (i = 0; arr[i]; i++) {
		g_strstrip (arr[i]);
		list = g_list_prepend (list, g_strdup (arr[i]));
	}

	g_strfreev (arr);

	return g_list_reverse (list);
}

static void
folder_registry_free (FolderRegistry *registry)
{
	camel_object_remove_event (registry->folder, registry->hook_info->hook_id);
	camel_object_unref (registry->folder);
	g_free (registry->hook_info->account_uri);
	g_slice_free (OnSummaryChangedInfo, registry->hook_info);
	g_slice_free (FolderRegistry, registry);
}

static FolderRegistry*
folder_registry_new (const gchar *account_uri, 
		     CamelFolder *folder, 
		     TrackerEvolutionPlugin *self)
{
	FolderRegistry *registry = g_slice_new (FolderRegistry);

	registry->hook_info = g_slice_new (OnSummaryChangedInfo);
	registry->hook_info->account_uri = g_strdup (account_uri);
	registry->hook_info->self = self; /* weak */
	registry->hook_info->hook_id = 0;
	camel_object_ref (folder);
	registry->folder = folder;

	return registry;
}

static void
process_fields (GPtrArray *predicates_temp, 
		GPtrArray *values_temp, 
		gchar *uid, 
		guint flags, 
		gchar *sent, 
		gchar *subject,
		gchar *from, 
		gchar *to, 
		gchar *cc, 
		gchar *size,
		CamelFolder *folder)
{
	GList *list, *l;

	g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_UID));
	g_ptr_array_add (values_temp, g_strdup (uid));

	g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_SEEN));
	g_ptr_array_add (values_temp, g_strdup ((flags & CAMEL_MESSAGE_SEEN) ? "True" : "False"));

	g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_JUNK));
	g_ptr_array_add (values_temp, g_strdup ((flags & CAMEL_MESSAGE_JUNK) ? "True" : "False"));

	g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_DELETED));
	g_ptr_array_add (values_temp, g_strdup ((flags & CAMEL_MESSAGE_DELETED) ? "True" : "False"));

	g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_ANSWERED));
	g_ptr_array_add (values_temp, g_strdup ((flags & CAMEL_MESSAGE_ANSWERED) ? "True" : "False"));

	g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_FLAGGED));
	g_ptr_array_add (values_temp, g_strdup ((flags & CAMEL_MESSAGE_FLAGGED) ? "True" : "False"));

	g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_FORWARDED));
	g_ptr_array_add (values_temp, g_strdup ((flags & CAMEL_MESSAGE_FORWARDED) ? "True" : "False"));


	if (subject && g_utf8_validate (subject, -1, NULL)) {
		g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_SUBJECT));
		g_ptr_array_add (values_temp, g_strdup (subject));
	}

	list = get_recipient_list (to);
	for (l = list; l; l = l->next) {
		if (l->data && g_utf8_validate (l->data, -1, NULL)) {
			g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_TO));
			g_ptr_array_add (values_temp, l->data);
		} else
			g_free (l->data);
	}
	g_list_free (list);

	if (from && g_utf8_validate (from, -1, NULL)) {
		g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_FROM));
		g_ptr_array_add (values_temp, g_strdup (from));
	}

	if (size && g_utf8_validate (size, -1, NULL)) {
		g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_SIZE));
		g_ptr_array_add (values_temp, g_strdup (size));
	}

	list = get_recipient_list (cc);
	for (l = list; l; l = l->next) {
		if (l->data && g_utf8_validate (l->data, -1, NULL)) {
			g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_CC));
			g_ptr_array_add (values_temp, l->data);
		} else
			g_free (l->data);
	}
	g_list_free (list);

	if (sent && g_utf8_validate (sent, -1, NULL)) {
		g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_SENT));
		g_ptr_array_add (values_temp, g_strdup (sent));
	}

	if (folder) {
		gchar *filen = camel_folder_get_filename (folder, uid, NULL);
		if (filen) {
			if (g_file_test (filen, G_FILE_TEST_EXISTS)) {
				g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_FILE));
				g_ptr_array_add (values_temp, filen);
			} else
				g_free (filen);
		}
	}
}

/* When new messages arrive to- or got deleted from the summary, called in
 * mainloop or by a thread (unknown, depends on Camel and Evolution code that 
 * executes the reason why this signal gets emitted) */

static void
on_folder_summary_changed (CamelFolder *folder, 
			   CamelFolderChangeInfo *changes, 
			   gpointer user_data)
{
	OnSummaryChangedInfo *info = user_data;
	TrackerEvolutionPlugin *self  = info->self;
	CamelFolderSummary *summary;
	gchar *account_uri = info->account_uri;
	GPtrArray *merged;
	guint i;
	gchar *em_uri;

	if (!folder)
		return;

	summary = folder->summary;
	em_uri = em_uri_from_camel (account_uri);

	merged = g_ptr_array_new ();

	/* the uid_added member contains the added-to-the-summary items */

	if (changes->uid_added && changes->uid_added->len > 0) {
		for (i = 0; i < changes->uid_added->len; i++)
			g_ptr_array_add (merged, changes->uid_added->pdata[i]);
	}

	/* the uid_changed member contains the changed-in-the-summary items */

	if (changes->uid_changed && changes->uid_changed->len > 0) {
		gboolean found = FALSE;
		guint y;

		for (i = 0; i < changes->uid_changed->len; i++) {
			for (y = 0; y < merged->len; y++) {
				if (strcmp (merged->pdata[y], changes->uid_changed->pdata[i]) == 0) {
					found = TRUE;
					break;
				}
			}

			if (!found)
				g_ptr_array_add (merged, changes->uid_changed->pdata[i]);
		}
	}

	if (merged->len > 0) {
		GPtrArray *predicates_array = g_ptr_array_new ();
		GPtrArray *values_array = g_ptr_array_new ();
		gchar **subjects = (gchar **) g_malloc0 (sizeof (gchar *) * merged->len + 1);
		guint y;

		y = 0;

		for (i = 0; i< merged->len; i++) {
			gchar *subject, *to, *from, *cc, *uid = NULL, *sent, *size;
			guint flags;
			gchar **values, **predicates;
			CamelMessageInfo *linfo;
			GPtrArray *values_temp = g_ptr_array_new ();
			GPtrArray *predicates_temp = g_ptr_array_new ();
			const CamelTag *ctags;
			const CamelFlag *cflags;

			linfo = camel_folder_summary_uid (summary, merged->pdata[i]);

			if (linfo)
				uid = (gchar *) camel_message_info_uid (linfo);

			if (linfo && uid) {
				guint j, max;

				subject = (gchar *) camel_message_info_subject (linfo);
				to =      (gchar *) camel_message_info_to (linfo);
				from =    (gchar *) camel_message_info_from (linfo);
				cc =      (gchar *) camel_message_info_cc (linfo);
				flags =   (guint)   camel_message_info_flags (linfo);

				/* Camel returns a time_t, I think a uint64 is the best fit here */
				sent = g_strdup_printf ("%"PRIu64, (unsigned long long) camel_message_info_date_sent (linfo));

				/* Camel returns a uint32, so %u */
				size = g_strdup_printf ("%u", camel_message_info_size (linfo));

				process_fields (predicates_temp, values_temp, uid,
						flags, sent, subject, from, to, cc, 
						size, folder);

				cflags = camel_message_info_user_flags (linfo);
				while (cflags) {
					g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_TAG));
					g_ptr_array_add (values_temp, g_strdup_printf ("%s=True", cflags->name));
					cflags = cflags->next;
				}

				ctags = camel_message_info_user_tags (linfo);
				while (ctags) {
					g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_TAG));
					g_ptr_array_add (values_temp, g_strdup_printf ("%s=%s", ctags->name, ctags->value));
					ctags = ctags->next;
				}

				if (values_temp->len != predicates_temp->len)
					g_critical ("values_temp->len != predicates_temp->len");

				max = MIN (values_temp->len, predicates_temp->len);

				values = (gchar **) g_malloc0 (sizeof (gchar*) * max + 1);
				predicates = (gchar **) g_malloc0 (sizeof (gchar*) * max + 1);

				for (j = 0; j < max; j++) {
					predicates[j] = predicates_temp->pdata[j];
					values[j] = values_temp->pdata[j];
				}

				predicates[j] = NULL;
				values[j] = NULL;

				g_ptr_array_add (values_array, values);
				g_ptr_array_add (predicates_array, predicates);

				/* This is not a path but a URI, don't use the 
				 * OS's directory separator here */

				subjects[y] = g_strdup_printf ("%s%s/%s", 
							       em_uri, 
							       camel_folder_get_full_name (folder),
							       uid);

				g_ptr_array_free (predicates_temp, TRUE);
				g_ptr_array_free (values_temp, TRUE);

				y++;
			}

			if (linfo)
				camel_message_info_free (linfo);
		}

		subjects[y] = NULL;

		/* This goes to all currently registered registrars */

		metadata_set_many (self, subjects, predicates_array, values_array);

		g_strfreev (subjects);
		for (i = 0; i < values_array->len; i++)
			g_strfreev (values_array->pdata[i]);
		g_ptr_array_free (values_array, TRUE);
		for (i = 0; i < predicates_array->len; i++)
			g_strfreev (predicates_array->pdata[i]);
		g_ptr_array_free (predicates_array, TRUE);
	}

	g_ptr_array_free (merged, TRUE);

	/* the uid_removed member contains the removed-from-the-summary items */

	if (changes->uid_removed && changes->uid_removed->len > 0) {
		gchar **subjects = (gchar **) g_malloc0 (sizeof (gchar *) * changes->uid_removed->len + 1);

		for (i = 0; i< changes->uid_removed->len; i++) {

			/* This is not a path but a URI, don't use the OS's 
			 * directory separator here */

			subjects[i] = g_strdup_printf ("%s%s/%s", em_uri, 
						       camel_folder_get_full_name (folder),
						       (char*) changes->uid_removed->pdata[i]);
		}

		subjects[i] = NULL;

		/* This goes to all currently registered registrars */

		metadata_unset_many (self, subjects);

		g_strfreev (subjects);
	}
	g_free (em_uri);
}

/* Initial upload of more recent than last_checkout items, called in the mainloop */
static void
introduce_walk_folders_in_folder (TrackerEvolutionPlugin *self, 
				  CamelFolderInfo *iter, 
				  CamelStore *store, 
				  gchar *account_uri, 
				  ClientRegistry *info)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	gchar *em_uri = em_uri_from_camel (account_uri);

	while (iter) {
		guint i, ret = SQLITE_OK;
		CamelDB *cdb_r = store->cdb_r;
		gchar *query;
		sqlite3_stmt *stmt = NULL;
		gboolean more = TRUE;

		query = sqlite3_mprintf ("SELECT uid, flags, read, deleted, "            /* 0  - 3  */
					        "replied, important, junk, attachment, " /* 4  - 7  */
					        "size, dsent, dreceived, subject, "      /* 8  - 11 */
					        "mail_from, mail_to, mail_cc, mlist, "   /* 12 - 15 */
					        "labels, usertags "                      /* 16 - 17 */
					 "FROM %Q "
					 "WHERE modified > %"PRIu64, 

					 iter->full_name, 
					 info->last_checkout);

		g_mutex_lock (cdb_r->lock);

		ret = sqlite3_prepare_v2 (cdb_r->db, query, -1, &stmt, NULL);

		while (more) {
			GPtrArray *subjects_a = g_ptr_array_new ();
			GPtrArray *predicates_array = g_ptr_array_new ();
			GPtrArray *values_array = g_ptr_array_new ();
			guint count = 0;

			more = FALSE;

			while (ret == SQLITE_OK || ret == SQLITE_BUSY || ret == SQLITE_ROW) {
				gchar **values, **predicates;
				gchar *subject, *to, *from, *cc, *sent, *uid, *size;
				gchar *part, *label, *p;
				guint flags;

				ret = sqlite3_step (stmt);

				if (ret == SQLITE_BUSY) {
					usleep (10);
					continue;
				}

				if ((ret != SQLITE_OK && ret != SQLITE_ROW) || ret == SQLITE_DONE) {
					more = FALSE;
					break;
				}

				uid = (gchar *) sqlite3_column_text (stmt, 0);

				if (uid) {
					GPtrArray *predicates_temp = g_ptr_array_new ();
					GPtrArray *values_temp = g_ptr_array_new ();
					CamelFolder *folder;
					guint max = 0, j;

					flags =   (guint  ) sqlite3_column_int  (stmt, 1);
					size =    (gchar *) sqlite3_column_text (stmt, 8);
					sent =    (gchar *) sqlite3_column_text (stmt, 9);
					subject = (gchar *) sqlite3_column_text (stmt, 11);
					from =    (gchar *) sqlite3_column_text (stmt, 12);
					to =      (gchar *) sqlite3_column_text (stmt, 13);
					cc =      (gchar *) sqlite3_column_text (stmt, 14);

					g_static_rec_mutex_lock (priv->mutex);

					folder = g_hash_table_lookup (priv->cached_folders, iter->full_name);

					process_fields (predicates_temp, values_temp, uid, flags, sent, 
							subject, from, to, cc, size, folder);

					g_static_rec_mutex_unlock (priv->mutex);

					/* Extract User flags/labels */
					p = part = g_strdup ((const gchar *) sqlite3_column_text (stmt, 16));
					if (part) {
						label = part;
						for (j=0; part[j]; j++) {

							if (part[j] == ' ') {
								part[j] = 0;
								g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_TAG));
								g_ptr_array_add (values_temp, g_strdup_printf ("%s=True", label));
								label = &(part[j+1]);
							}
						}
						g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_TAG));
						g_ptr_array_add (values_temp, g_strdup (label));
					}
					g_free (p);

					/* Extract User tags */
					p = part = g_strdup ((const gchar *) sqlite3_column_text (stmt, 17));
					EXTRACT_FIRST_DIGIT (max)
					for (j = 0; j < max; j++) {
						int len;
						char *name, *value;
						EXTRACT_STRING (name)
						EXTRACT_STRING (value)
						if (name && g_utf8_validate (name, -1, NULL) &&
						    value && g_utf8_validate (value, -1, NULL)) {
							g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_TAG));
							g_ptr_array_add (values_temp, g_strdup_printf ("%s=%s", name, value));
						}
						g_free(name);
						g_free(value);
					}
					g_free (p);

					if (values_temp->len != predicates_temp->len)
						g_critical ("values_temp->len != predicates_temp->len");

					max = MIN (values_temp->len, predicates_temp->len);

					values = (gchar **) g_malloc0 (sizeof (gchar*) * max + 1);
					predicates = (gchar **) g_malloc0 (sizeof (gchar*) * max + 1);

					for (j = 0; j < max; j++) {
						predicates[j] = predicates_temp->pdata[j];
						values[j] = values_temp->pdata[j];
					}

					predicates[j] = NULL;
					values[j] = NULL;

					/* This is not a path but a URI, don't use the 
					 * OS's directory separator here */

					g_ptr_array_add (subjects_a, g_strdup_printf ("%s%s/%s", em_uri, 
										      iter->full_name, uid));

					g_ptr_array_add (predicates_array, predicates);
					g_ptr_array_add (values_array, values);

					g_ptr_array_free (predicates_temp, TRUE);
					g_ptr_array_free (values_temp, TRUE);

					count++;
				}

				if (count > MAX_BEFORE_SEND) {

					/* Yield per MAX_BEFORE_SEND. This function is 
					 * called as a result of a DBus call, so it runs
					 * in the mainloop. Therefore, yield he mainloop
					 * sometimes, indeed */

					g_main_context_iteration (NULL, TRUE);

					more = TRUE;
					break;
				}

				more = FALSE;
			}


			if (count > 0) {
				gchar **subjects;

				subjects = (gchar **) g_malloc0 (sizeof (gchar *) * subjects_a->len + 1);
				for (i = 0; i < subjects_a->len; i++)
					subjects[i] = g_ptr_array_index (subjects_a, i);
				subjects[i] = NULL;

				dbus_g_proxy_call_no_reply (info->registrar,
							    "SetMany",
							    G_TYPE_STRV, subjects,
							    TRACKER_TYPE_G_STRV_ARRAY, predicates_array,
							    TRACKER_TYPE_G_STRV_ARRAY, values_array,
							    G_TYPE_UINT, (guint) time (NULL),
							    G_TYPE_INVALID, 
							    G_TYPE_INVALID);

				g_strfreev (subjects);
			}

			g_ptr_array_free (subjects_a, TRUE);

			for (i = 0; i < values_array->len; i++)
				g_strfreev (values_array->pdata[i]); 
			g_ptr_array_free (values_array, TRUE);

			for (i = 0; i < predicates_array->len; i++)
				g_strfreev (predicates_array->pdata[i]); 
			g_ptr_array_free (predicates_array, TRUE);
		}

		sqlite3_finalize (stmt);
		sqlite3_free (query);

		g_mutex_unlock (cdb_r->lock);

		if (iter->child) {
			introduce_walk_folders_in_folder (self, iter->child, store, account_uri, info);
		}

		iter = iter->next;
	}

	g_free (em_uri);
}

/* Initial notify of deletes that are more recent than last_checkout, called in 
 * the mainloop */

static void
introduce_store_deal_with_deleted (TrackerEvolutionPlugin *self, 
				   CamelStore *store, 
				   char *account_uri, 
				   gpointer user_data)
{
	ClientRegistry *info = user_data;
	gboolean more = TRUE;
	gchar *query;
	sqlite3_stmt *stmt = NULL;
	CamelDB *cdb_r;
	guint i, ret;
	gchar *em_uri = em_uri_from_camel (account_uri);

	query = sqlite3_mprintf ("SELECT uid, mailbox FROM Deletes WHERE modified > %" PRIu64, 
				 info->last_checkout);

	cdb_r = store->cdb_r;

	g_mutex_lock (cdb_r->lock);

	sqlite3_prepare_v2 (cdb_r->db, query, -1, &stmt, NULL);

	ret = SQLITE_OK;

	while (more) {
		GPtrArray *subjects_a = g_ptr_array_new ();
		guint count = 0;

		more = FALSE;

		while (ret == SQLITE_OK || ret == SQLITE_BUSY || ret == SQLITE_ROW) {
			const gchar *uid;
			const gchar *mailbox;

			ret = sqlite3_step (stmt);

			if (ret == SQLITE_BUSY) {
				usleep (10);
				continue;
			}

			if ((ret != SQLITE_OK && ret != SQLITE_ROW) || ret == SQLITE_DONE) {
				more = FALSE;
				break;
			}

			uid     = (const gchar *) sqlite3_column_text (stmt, 0);
			mailbox = (const gchar *) sqlite3_column_text (stmt, 1);

			/* This is not a path but a URI, don't use the OS's 
			 * directory separator here */

			g_ptr_array_add (subjects_a, g_strdup_printf ("%s%s/%s", em_uri, 
								      mailbox, uid));

			if (count > MAX_BEFORE_SEND) {

				/* Yield per MAX_BEFORE_SEND. This function is 
				 * called as a result of a DBus call, so it runs
				 * in the mainloop. Therefore, yield he mainloop
				 * sometimes, indeed */

				g_main_context_iteration (NULL, TRUE);

				more = TRUE;
				break;
			}

			count++;

			more = FALSE;
		}

		if (count > 0) {
			gchar **subjects;

			subjects = (gchar **) g_malloc0 (sizeof (gchar *) * subjects_a->len + 1);
			for (i = 0; i < subjects_a->len; i++)
				subjects[i] = g_ptr_array_index (subjects_a, i);
			subjects[i] = NULL;

			dbus_g_proxy_call_no_reply (info->registrar,
						    "UnsetMany",
						    G_TYPE_STRV, subjects,
						    G_TYPE_UINT, (guint) time (NULL),
						    G_TYPE_INVALID,
						    G_TYPE_INVALID);

			g_strfreev (subjects);
		}

		g_ptr_array_free (subjects_a, TRUE);

	}

	sqlite3_finalize (stmt);
	sqlite3_free (query);

	g_mutex_unlock (cdb_r->lock);

	g_free (em_uri);
}

/* Get the oldest date in all of the deleted-tables, called in the mainloop */

static guint64
get_last_deleted_time (TrackerEvolutionPlugin *self)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	guint64 smallest = (guint64) time (NULL);

	if (priv->accounts) {
		EIterator *it;

		for (it = e_list_get_iterator (E_LIST (priv->accounts)); e_iterator_is_valid (it); e_iterator_next (it)) {
			EAccount *account = (EAccount *) e_iterator_get (it);
			CamelProvider *provider;
			CamelStore *store;
			CamelException ex;
			char *uri;
			CamelDB *cdb_r;
			sqlite3_stmt *stmt = NULL;
			gchar *query;
			guint ret = SQLITE_OK;
			guint64 latest = smallest;

			camel_exception_init (&ex);

			if (!account->enabled || !(uri = account->source->url))
				continue;

			if (!(provider = camel_provider_get(uri, NULL))) {
				camel_exception_clear (&ex);
				continue;
			}

			if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
				continue;

			if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
				camel_exception_clear (&ex);
				continue;
			}

			cdb_r = store->cdb_r;

			query = sqlite3_mprintf ("SELECT time FROM Deletes ORDER BY time LIMIT 1");

			g_mutex_lock (cdb_r->lock);

			ret = sqlite3_prepare_v2 (cdb_r->db, query, -1, &stmt, NULL);

			ret = sqlite3_step (stmt);
			if (ret == SQLITE_OK || ret == SQLITE_ROW)
				latest = sqlite3_column_int64 (stmt, 0);

			if (latest < smallest)
				smallest = latest;

			sqlite3_finalize (stmt);
			sqlite3_free (query);

			g_mutex_unlock (cdb_r->lock);

		}

		g_object_unref (it);
	}

	return smallest;
}


static void
register_walk_folders_in_folder (TrackerEvolutionPlugin *self, 
				 CamelFolderInfo *iter, 
				 CamelStore *store, 
				 gchar *account_uri)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);

	while (iter) {
		CamelFolder *folder;

		folder = camel_store_get_folder (store, iter->full_name, 0, NULL);

		if (folder) {
			guint hook_id;
			FolderRegistry *registry;

			registry = folder_registry_new (account_uri, folder, self);

			g_static_rec_mutex_lock (priv->mutex);

			if (!priv->registered_folders) {
				priv->registered_folders = g_hash_table_new_full (g_int_hash, g_int_equal,
										  (GDestroyNotify) NULL,
										  (GDestroyNotify) folder_registry_free);
				priv->cached_folders = g_hash_table_new_full (g_str_hash, g_str_equal,
									      (GDestroyNotify) g_free,
									      (GDestroyNotify) NULL);
			}

			hook_id = camel_object_hook_event (folder, "folder_changed", 
							   CAMEL_CALLBACK (on_folder_summary_changed), 
							   registry->hook_info);
			registry->hook_info->hook_id = hook_id;

			g_hash_table_replace (priv->registered_folders, &hook_id, 
					      registry);
			g_hash_table_replace (priv->cached_folders, g_strdup (iter->full_name), 
					      folder);

			g_static_rec_mutex_unlock (priv->mutex);

			camel_object_unref (folder);
		}

		if (iter->child) {
			register_walk_folders_in_folder (self, iter->child, store, 
							 account_uri);
		}

		iter = iter->next;
	}
}


static void
unregister_walk_folders_in_folder (TrackerEvolutionPlugin *self, 
				   CamelFolderInfo *titer, 
				   CamelStore *store, 
				   gchar *account_uri)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);

	while (titer) {
		CamelFolder *folder;
		GHashTableIter iter;
		gpointer key, value;

		folder = camel_store_get_folder (store, titer->full_name, 0, NULL);

		if (folder) {
			g_static_rec_mutex_lock (priv->mutex);

			g_hash_table_iter_init (&iter, priv->registered_folders);
			while (g_hash_table_iter_next (&iter, &key, &value)) {
				FolderRegistry *registry = value;

				if (folder == registry->folder) {
					g_hash_table_remove (priv->cached_folders, titer->full_name);
					g_hash_table_iter_remove (&iter);
					break;
				}
			}

			camel_object_unref (folder);

			g_static_rec_mutex_unlock (priv->mutex);
		}

		if (titer->child) {
			unregister_walk_folders_in_folder (self, titer->child, store, 
							   account_uri);
		}

		titer = titer->next;
	}
}

static void
client_registry_info_free (ClientRegistry *info)
{
	if (info->signal != 0) /* known (see below) */
		g_signal_handler_disconnect (info->registrar, info->signal);
	g_object_unref (info->registrar);
	g_slice_free (ClientRegistry, info);
}

static ClientRegistry*
client_registry_info_copy (ClientRegistry *info)
{
	ClientRegistry *ninfo = g_slice_new0 (ClientRegistry);

	ninfo->signal = 0; /* known */
	ninfo->last_checkout = info->last_checkout;
	ninfo->registrar = g_object_ref (info->registrar);

	return ninfo;
}

static gboolean
on_got_folderinfo_introduce (CamelStore *store, 
			     CamelFolderInfo *iter, 
			     void *data)
{
	IntroductionInfo *intro_info = data;

	introduce_walk_folders_in_folder (intro_info->self, iter, store, 
					  intro_info->account_uri, 
					  intro_info->info);

	client_registry_info_free (intro_info->info);
	g_free (intro_info->account_uri);
	g_object_unref (intro_info->self);
	g_free (intro_info);

	return TRUE;
}

static void
introduce_account_to (TrackerEvolutionPlugin *self, 
		      EAccount *account, 
		      ClientRegistry *info)
{
	CamelProvider *provider;
	CamelStore *store;
	CamelException ex;
	char *uri, *account_uri, *ptr;
	IntroductionInfo *intro_info;

	if (!account->enabled || !(uri = account->source->url))
		return;

	camel_exception_init (&ex);
	if (!(provider = camel_provider_get(uri, &ex))) {
		camel_exception_clear (&ex);
		return;
	}

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;

	if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
		camel_exception_clear (&ex);
		return;
	}

	account_uri = g_strdup (uri);

	ptr = strchr (account_uri, ';');

	if (ptr)
		*ptr = '\0';

	introduce_store_deal_with_deleted (self, store, account_uri, info);

	intro_info = g_new0 (IntroductionInfo, 1);

	intro_info->self = g_object_ref (self);
	intro_info->info = client_registry_info_copy (info);
	intro_info->account_uri = account_uri; /* is freed in on_got above */

	mail_get_folderinfo (store, NULL, on_got_folderinfo_introduce, intro_info);

	camel_object_unref (store);

}


static void
introduce_account_to_all (TrackerEvolutionPlugin *self, 
			  EAccount *account)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	GList *copy = priv->registered_clients;

	while (copy) {
		ClientRegistry *info = copy->data;
		introduce_account_to (self, account, info);
		copy = g_list_next (copy);
	}

}

static void
introduce_accounts_to (TrackerEvolutionPlugin *self, 
		       ClientRegistry *info)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	EIterator *it;

	for (it = e_list_get_iterator (E_LIST (priv->accounts)); e_iterator_is_valid (it); e_iterator_next (it))
		introduce_account_to (self, (EAccount *) e_iterator_get (it), info);

	g_object_unref (it);
}


static void
register_client (TrackerEvolutionPlugin *self, 
		 guint64 last_checkout, 
		 DBusGProxy *registrar, 
		 guint dsignal)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	guint64 too_old = get_last_deleted_time (self);
	ClientRegistry *info = g_slice_new (ClientRegistry);

	info->signal = dsignal;
	info->registrar = g_object_ref (registrar);

	if (last_checkout < too_old) {
		dbus_g_proxy_call_no_reply (registrar,
					    "Cleanup",
					    G_TYPE_UINT, (guint) time (NULL),
					    G_TYPE_INVALID,
					    G_TYPE_INVALID);
		info->last_checkout = 0;
	} else
		info->last_checkout = last_checkout;

	introduce_accounts_to (self, info);

	priv->registered_clients = 
		g_list_prepend (priv->registered_clients, info);

}


static void
metadata_set_many (TrackerEvolutionPlugin *self, 
		   GStrv subjects, 
		   GPtrArray *predicates, 
		   GPtrArray *values)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	GHashTableIter iter;
	gpointer key, value;

	g_static_rec_mutex_lock (priv->mutex);

	g_hash_table_iter_init (&iter, priv->registrars);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		DBusGProxy *registrar = value;

		dbus_g_proxy_call_no_reply (registrar,
					    "SetMany",
					    G_TYPE_STRV, subjects,
					    TRACKER_TYPE_G_STRV_ARRAY, predicates,
					    TRACKER_TYPE_G_STRV_ARRAY, values,
					    G_TYPE_UINT, (guint) time (NULL),
					    G_TYPE_INVALID, 
					    G_TYPE_INVALID);
	}

	g_static_rec_mutex_unlock (priv->mutex);
}

static void
metadata_unset_many (TrackerEvolutionPlugin *self, 
		     GStrv subjects)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	GHashTableIter iter;
	gpointer key, value;

	g_static_rec_mutex_lock (priv->mutex);

	g_hash_table_iter_init (&iter, priv->registrars);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		DBusGProxy *registrar = value;

		dbus_g_proxy_call_no_reply (registrar,
					    "UnsetMany",
					    G_TYPE_STRV, subjects,
					    G_TYPE_UINT, (guint) time (NULL),
					    G_TYPE_INVALID, 
					    G_TYPE_INVALID);
	}

	g_static_rec_mutex_unlock (priv->mutex);

}

static void
on_folder_created (CamelStore *store, void *event_data, 
		   StoreRegistry *registry)
{
	unregister_account (registry->self, registry->account);
	register_account (registry->self, registry->account);
	introduce_account_to_all (registry->self, registry->account);
}

static void
on_folder_deleted (CamelStore *store, 
		   void *event_data, 
		   StoreRegistry *registry)
{
	unregister_account (registry->self, registry->account);
	register_account (registry->self, registry->account);
	introduce_account_to_all (registry->self, registry->account);
}

static void
on_folder_renamed (CamelStore *store, 
		   CamelRenameInfo *info, 
		   StoreRegistry *registry)
{
	unregister_account (registry->self, registry->account);
	register_account (registry->self, registry->account);
	introduce_account_to_all (registry->self, registry->account);
}

static StoreRegistry*
store_registry_new (gpointer co, 
		    EAccount *account, 
		    TrackerEvolutionPlugin *self)
{
	StoreRegistry *registry = g_slice_new (StoreRegistry);

	registry->store = co;
	registry->account = account; /* weak */
	registry->self = self; /* weak */
	camel_object_ref (co);

	return registry;
}

static void
store_registry_free (StoreRegistry *registry) 
{
	camel_object_remove_event (registry->store, registry->hook_id);
	camel_object_unref (registry->store);
	g_slice_free (StoreRegistry, registry);
}


static gboolean
on_got_folderinfo_register (CamelStore *store, 
			    CamelFolderInfo *iter, 
			    void *data)
{
	RegisterInfo *reg_info = data;
	TrackerEvolutionPlugin *self = reg_info->self;
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	EAccount *account = reg_info->account;
	StoreRegistry *registry;
	gchar *uri = reg_info->uri;
	guint hook_id;

	g_static_rec_mutex_lock (priv->mutex);

	if (!priv->registered_stores)
		priv->registered_stores = g_hash_table_new_full (g_int_hash, g_int_equal,
								 (GDestroyNotify) NULL,
								 (GDestroyNotify) store_registry_free);

	/* Hook up catching folder changes in the store */
	registry = store_registry_new (store, account, self);
	hook_id = camel_object_hook_event (store, "folder_created", 
					   CAMEL_CALLBACK (on_folder_created), 
					   registry);
	registry->hook_id = hook_id;
	g_hash_table_replace (priv->registered_stores, &hook_id, registry);

	registry = store_registry_new (store, account, self);
	hook_id = camel_object_hook_event (store, "folder_renamed", 
					   CAMEL_CALLBACK (on_folder_renamed), 
					   registry);
	registry->hook_id = hook_id;
	g_hash_table_replace (priv->registered_stores, &hook_id, registry);

	registry = store_registry_new (store, account, self);
	hook_id = camel_object_hook_event (store, "folder_deleted", 
					   CAMEL_CALLBACK (on_folder_deleted), 
					   registry);
	registry->hook_id = hook_id;
	g_hash_table_replace (priv->registered_stores, &hook_id, registry);

	g_static_rec_mutex_unlock (priv->mutex);

	/* Register each folder to hook folder_changed everywhere */
	register_walk_folders_in_folder (self, iter, store, uri);

	g_object_unref (reg_info->account);
	g_object_unref (reg_info->self);
	g_free (reg_info->uri);
	g_free (reg_info);

	return TRUE;
}

static void
register_account (TrackerEvolutionPlugin *self,
		  EAccount *account)
{
	CamelProvider *provider;
	CamelStore *store;
	CamelException ex;
	char *uri;
	RegisterInfo *reg_info;

	if (!account->enabled || !(uri = account->source->url))
		return;

	camel_exception_init (&ex);
	if (!(provider = camel_provider_get(uri, &ex))) {
		camel_exception_clear (&ex);
		return;
	}

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;

	if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
		camel_exception_clear (&ex);
		return;
	}

	reg_info = g_new0 (RegisterInfo, 1);

	reg_info->self = g_object_ref (self);
	reg_info->uri = g_strdup (uri);
	reg_info->account = g_object_ref (account);

	/* Get the account's folder-info and register it asynchronously */
	mail_get_folderinfo (store, NULL, on_got_folderinfo_register, reg_info);

	camel_object_unref (store);
}

static gboolean
on_got_folderinfo_unregister (CamelStore *store, 
			      CamelFolderInfo *titer,
			      void *data)
{
	RegisterInfo *reg_info = data;
	TrackerEvolutionPlugin *self = reg_info->self;
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	gchar *uri = reg_info->uri;
	GHashTableIter iter;
	gpointer key, value;

	unregister_walk_folders_in_folder (self, titer, store, uri);

	g_static_rec_mutex_lock (priv->mutex);

	if (priv->registered_stores) {
		g_hash_table_iter_init (&iter, priv->registered_stores);

		while (g_hash_table_iter_next (&iter, &key, &value)) {
			if (value == store) 
				g_hash_table_iter_remove (&iter);
		}
	}

	g_static_rec_mutex_unlock (priv->mutex);

	g_object_unref (reg_info->self);
	g_free (reg_info->uri);
	g_free (reg_info);

	return TRUE;
}

static void
unregister_account (TrackerEvolutionPlugin *self,
		    EAccount *account)
{
	CamelProvider *provider;
	CamelStore *store;
	CamelException ex;
	char *uri;
	RegisterInfo *reg_info;


	camel_exception_init (&ex);
	if (!(provider = camel_provider_get(uri, &ex))) {
		camel_exception_clear (&ex);
		return;
	}

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;

	if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
		camel_exception_clear (&ex);
		return;
	}

	reg_info = g_new0 (RegisterInfo, 1);

	reg_info->self = g_object_ref (self);
	reg_info->uri = g_strdup (uri);
	reg_info->account = NULL;

	/* Get the account's folder-info and unregister asynchronously */
	mail_get_folderinfo (store, NULL, on_got_folderinfo_unregister, reg_info);

	camel_object_unref (store);
}

static void
on_account_added (EAccountList *list, 
		  EAccount *account, 
		  TrackerEvolutionPlugin *self)
{
	register_account (self, account);
	introduce_account_to_all (self, account);
}

static void
on_account_removed (EAccountList *list,
		    EAccount *account,
		    TrackerEvolutionPlugin *self)
{
	unregister_account (self, account);
}

static void
on_account_changed (EAccountList *list, 
		    EAccount *account,
		    TrackerEvolutionPlugin *self)
{
	unregister_account (self, account);
	register_account (self, account);
	introduce_account_to_all (self, account);
}

static void
disable_plugin (void) 
{
	GError *error = NULL;
	guint result;

	org_freedesktop_DBus_release_name (dbus_proxy, TRACKER_EVOLUTION_MANAGER_SERVICE, 
					   &result, &error);

	if (!error) {
		if (manager) {
			g_object_unref (manager);
			manager = NULL;
		}

		if (dbus_proxy) {
			g_object_unref (dbus_proxy);
			dbus_proxy = NULL;
		}
	} else {
		g_warning ("Could not setup DBus, ReleaseName of %s: %s\n", 
			   TRACKER_EVOLUTION_MANAGER_SERVICE, error->message);

		g_error_free (error);
	}
}

static void
enable_plugin (void)
{
	DBusGConnection *connection;
	GError *error = NULL;
	guint result;

	if (dbus_proxy && manager)
		return;

	if ((dbus_proxy && !manager) || (!dbus_proxy && manager))
		disable_plugin ();

	if ((dbus_proxy && !manager) || (!dbus_proxy && manager))
		return;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (error)
		goto error_handler;

	dbus_proxy = dbus_g_proxy_new_for_name (connection, 
						DBUS_SERVICE_DBUS,
						DBUS_PATH_DBUS,
						DBUS_INTERFACE_DBUS);

	if (!org_freedesktop_DBus_request_name (dbus_proxy, TRACKER_EVOLUTION_MANAGER_SERVICE,
						DBUS_NAME_FLAG_DO_NOT_QUEUE,
						&result, &error)) {

		g_warning ("Could not setup DBus, failed at RequestName for %s\n", 
			   TRACKER_EVOLUTION_MANAGER_SERVICE);

		goto error_handler;
	}

	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {

		g_warning ("Could not setup DBus, can't become primary owner of %s\n", 
			   TRACKER_EVOLUTION_MANAGER_SERVICE);

		goto error_handler;
	}

	if (error)
		goto error_handler;

	manager = g_object_new (TRACKER_TYPE_EVOLUTION_PLUGIN, 
				"connection", connection, NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (manager), 
					 &dbus_glib_tracker_evolution_plugin_object_info);

	dbus_g_connection_register_g_object (connection, 
					     TRACKER_EVOLUTION_MANAGER_PATH, 
					     G_OBJECT (manager));

	error_handler:

	if (error) {
		g_warning ("Could not setup DBus, %s\n", error->message);
		disable_plugin();
		g_error_free (error);
	}
}

static gboolean 
do_remove_or_not (gpointer key, gpointer value, gpointer user_data)
{
	if (user_data == value)
		return TRUE;

	return FALSE;
}

static void
service_gone (DBusGProxy *lproxy, TrackerEvolutionPlugin *plugin)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (plugin);
	GList *copy = priv->registered_clients;
	GList *to_delete = NULL;

	g_static_rec_mutex_lock (priv->mutex);

	g_hash_table_foreach_remove (priv->registrars, 
				     do_remove_or_not,
				     lproxy);

	while (copy) {
		ClientRegistry *creg = copy->data;
		if (creg->registrar == lproxy)
			to_delete = g_list_prepend (to_delete, copy);
		copy = g_list_next (copy);
	}

	copy = to_delete;
	while (copy) {
		GList *node = copy->data;
		ClientRegistry *creg = node->data;
		priv->registered_clients = g_list_delete_link (priv->registered_clients, node);
		client_registry_info_free (creg);
		copy = g_list_next (copy);
	}

	g_list_free (to_delete);

	g_static_rec_mutex_unlock (priv->mutex);
}

void 
tracker_evolution_plugin_register  (TrackerEvolutionPlugin *plugin, 
				    gchar *registrar_path,
				    guint last_checkout, 
				    DBusGMethodInvocation *context,
				    GError *derror)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (plugin);
	gchar *sender;
	DBusGProxy *registrar;
	guint dsignal;

	g_static_rec_mutex_lock (priv->mutex);

	sender = dbus_g_method_get_sender (context);

	registrar = dbus_g_proxy_new_for_name (priv->connection, sender, 
					       registrar_path,
					       TRACKER_EVOLUTION_REGISTRAR_INTERFACE);

	g_hash_table_replace (priv->registrars, g_strdup (sender), 
			      registrar);

	dsignal = g_signal_connect (registrar, "destroy",
				    G_CALLBACK (service_gone),
				    plugin);

	g_static_rec_mutex_unlock (priv->mutex);

	/* Passing uint64 over DBus ain't working :-\ */
	register_client (plugin, (guint64) last_checkout, registrar, dsignal);

	dbus_g_method_return (context);
}


int
e_plugin_lib_enable (EPluginLib *ep, int enabled)
{
	g_static_rec_mutex_lock (&glock);

	if (enabled)
		enable_plugin ();
	else
		disable_plugin ();

	g_static_rec_mutex_unlock (&glock);

	return 0;
}


static void
tracker_evolution_plugin_finalize (GObject *plugin)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (plugin);

	g_static_rec_mutex_lock (priv->mutex);

	g_list_foreach (priv->registered_clients,
			(GFunc) client_registry_info_free,
			NULL);

	g_list_free (priv->registered_clients);

	if (priv->registered_folders) {
		g_hash_table_destroy (priv->registered_folders);
		g_hash_table_destroy (priv->cached_folders);
		priv->cached_folders = NULL;
		priv->registered_folders = NULL;
	}

	if (priv->registered_stores) {
		g_hash_table_destroy (priv->registered_stores);
		priv->registered_stores = NULL;
	}

	g_object_unref (priv->accounts);

	g_hash_table_destroy (priv->registrars);

	if (priv->connection)
		dbus_g_connection_unref (priv->connection);

	g_static_rec_mutex_unlock (priv->mutex);

	g_slice_free (GStaticRecMutex, priv->mutex);

	G_OBJECT_CLASS (tracker_evolution_plugin_parent_class)->finalize (plugin);
}

static void 
tracker_evolution_plugin_set_connection (TrackerEvolutionPlugin *plugin, 
					 DBusGConnection *connection)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (plugin);

	if (priv->connection)
		dbus_g_connection_unref (priv->connection);

	priv->connection = dbus_g_connection_ref (connection);
}

static void
tracker_evolution_plugin_set_property (GObject     *plugin,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_CONNECTION:
		tracker_evolution_plugin_set_connection (TRACKER_EVOLUTION_PLUGIN (plugin),
							 g_value_get_pointer (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (plugin, prop_id, pspec);
	}
}

static void
tracker_evolution_plugin_get_property (GObject   *plugin,
				      guint       prop_id,
				      GValue     *value,
				      GParamSpec *pspec)
{
	TrackerEvolutionPluginPrivate *priv;

	priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (plugin);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_pointer (value, priv->connection);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (plugin, prop_id, pspec);
	}
}

static void
tracker_evolution_plugin_class_init (TrackerEvolutionPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_evolution_plugin_finalize;
	object_class->set_property = tracker_evolution_plugin_set_property;
	object_class->get_property = tracker_evolution_plugin_get_property;

	g_object_class_install_property (object_class,
					 PROP_CONNECTION,
					 g_param_spec_pointer ("connection",
							       "DBus connection",
							       "DBus connection",
							       G_PARAM_READWRITE |
							       G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerEvolutionPluginPrivate));
}

static void
tracker_evolution_plugin_init (TrackerEvolutionPlugin *plugin)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (plugin);
	EIterator *it;

	priv->mutex = g_slice_new0 (GStaticRecMutex);
	g_static_rec_mutex_init (priv->mutex);

	g_static_rec_mutex_lock (priv->mutex);

	priv->registrars = g_hash_table_new_full (g_str_hash, g_str_equal,
						  (GDestroyNotify) g_free, 
						  (GDestroyNotify) g_object_unref);


	priv->cached_folders = NULL;
	priv->registered_folders = NULL;
	priv->registered_stores = NULL;
	priv->registered_clients = NULL;

	g_static_rec_mutex_unlock (priv->mutex);

	priv->accounts = g_object_ref (mail_config_get_accounts ());

	for (it = e_list_get_iterator (E_LIST (priv->accounts)); e_iterator_is_valid (it); e_iterator_next (it))
		register_account (plugin, (EAccount *) e_iterator_get (it));

	g_object_unref (it);

	g_signal_connect (priv->accounts, "account-added", 
			  G_CALLBACK (on_account_added), plugin);
	g_signal_connect (priv->accounts, "account-removed", 
			  G_CALLBACK (on_account_removed), plugin);
	g_signal_connect (priv->accounts, "account-changed", 
			  G_CALLBACK (on_account_changed), plugin);
}
