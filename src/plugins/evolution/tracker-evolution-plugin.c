/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <glib-object.h>
#include <gio/gio.h>

#include <sqlite3.h>

#include <camel/camel-mime-message.h>
#include <camel/camel-i18n.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-db.h>
#include <camel/camel-offline-store.h>
#include <camel/camel-session.h>
#include <camel/camel-url.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-multipart.h>
#include <camel/camel-multipart-encrypted.h>
#include <camel/camel-multipart-signed.h>
#include <camel/camel-medium.h>
#include <camel/camel-gpg-context.h>
#include <camel/camel-smime-context.h>
#include <camel/camel-string-utils.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-stream-null.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-mime-filter-windows.h>

#include <mail/mail-config.h>
#include <mail/mail-session.h>
#include <mail/em-utils.h>
#include <mail/mail-ops.h>

#include <e-util/e-config.h>

#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>

#ifdef HAVE_EDS_2_29_1
#include <e-util/e-account-utils.h>
#endif

#include <libtracker-client/tracker.h>
#include <libtracker-client/tracker-sparql-builder.h>

#include <libtracker-common/tracker-date-time.h>
#include <libtracker-common/tracker-ontologies.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-type-utils.h>

#include "tracker-evolution-plugin.h"

/* This runs in-process of evolution (in the mailer, as a EPlugin). It has
 * access to the CamelSession using the external variable 'session'. The header
 * mail/mail-session.h makes this variable public */

/* Note to people who are scared about this plugin using the CamelDB directly:
 * The code uses camel_db_clone to create a new connection to the DB. We hope
 * that's sufficient for not having to lock the store instances (sqlite3 has
 * its own locks, and we only clone the db_r instance, we also only ever do
 * reads, never writes). We hope that's sufficient for not having to get our
 * code involved in Camel's cruel inneryard of having to lock the db_r ptr. */

#define TRACKER_SERVICE                         "org.freedesktop.Tracker1"

#define NIE_DATASOURCE                  TRACKER_NIE_PREFIX "DataSource"
#define RDF_PREFIX                      TRACKER_RDF_PREFIX
#define NMO_PREFIX                      TRACKER_NMO_PREFIX
#define NCO_PREFIX                      TRACKER_NCO_PREFIX
#define NAO_PREFIX                      TRACKER_NAO_PREFIX
#define DATASOURCE_URN                  "urn:nepomuk:datasource:1cb1eb90-1241-11de-8c30-0800200c9a66"

G_DEFINE_TYPE (TrackerEvolutionPlugin, tracker_evolution_plugin, TRACKER_TYPE_MINER)

#define TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_EVOLUTION_PLUGIN, TrackerEvolutionPluginPrivate))

/* Some helper-defines (Copied from cruel Camel code, might be wrong as soon as
 * the cruel and nasty Camel coders decide to change the format of the fields in
 * the database) - guys, encoding things in fields of a table in a database is
 * cruel and prone to error. Anyway). */

#define CAMEL_CALLBACK(func) ((CamelObjectEventHookFunc) func)
#define EXTRACT_STRING(val) if (*part) part++; len=strtoul (part, &part, 10); if (*part) part++; val=g_strndup (part, len); part+=len;
#define EXTRACT_FIRST_DIGIT(val) val=strtoul (part, &part, 10);

/* About the locks being used: Camel's API must be used in a multi-threaded
 * fashion. Therefore it's necessary to guard against concurrent access of
 * memory. Especially given that both the mainloop and the Camel-threads will
 * be accessing the memory (mainloop for DBus calls, and Camel-threads mostly
 * during registration of accounts and folders). I know this is cruel. I know. */

typedef struct {
	TrackerClient *client;
	gchar *sparql;
	gboolean commit;
	gint prio;
	GMutex *mutex;
	GCond *cond;
	gboolean has_happened;
	gpointer pool;
	gboolean dont_free;
} PoolItem;

typedef struct {
	GThreadPool *pool;
	GList *items;
	GMutex *mutex;
	GFunc func, freeup;
	gboolean dying;
	GCancellable *cancel;
} ThreadPool;

typedef struct {
	TrackerEvolutionPlugin *self; /* weak */
	guint64 last_checkout;
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
	GHashTable *registered_folders;
	GHashTable *cached_folders;
	GHashTable *registered_stores;
	GList *registered_clients;
	EAccountList *accounts;
	TrackerClient *client;
	DBusGProxy *dbus_proxy;
	DBusGConnection *connection;
	time_t last_time;
	gboolean resuming, paused;
	guint total_popped, of_total;
} TrackerEvolutionPluginPrivate;

typedef struct {
	IntroductionInfo *intro_info;
	CamelStore *store;
	CamelDB *cdb_r;
	CamelFolderInfo *iter;
} TryAgainInfo;

typedef struct {
	TrackerEvolutionPlugin *self;
	gchar *account_uri;
	CamelFolderInfo *iter;
} GetFolderInfo;

static TrackerEvolutionPlugin *manager = NULL;
static GStaticRecMutex glock = G_STATIC_REC_MUTEX_INIT;
static guint register_count = 0, walk_count = 0;
static ThreadPool *sparql_pool = NULL, *folder_pool = NULL;

/* Prototype declarations */
static void register_account (TrackerEvolutionPlugin *self, EAccount *account);
static void unregister_account (TrackerEvolutionPlugin *self, EAccount *account);
int e_plugin_lib_enable (EPlugin *ep, int enable);
static void miner_started (TrackerMiner *miner);
static void miner_stopped (TrackerMiner *miner);
static void miner_paused (TrackerMiner *miner);
static void miner_resumed (TrackerMiner *miner);

/* First a bunch of helper functions. */
#if 0
static ssize_t
camel_stream_format_text (CamelDataWrapper *dw, CamelStream *stream)
{
	CamelStreamFilter *filter_stream;
	CamelMimeFilterCharset *filter;
	const char *charset = "UTF-8"; /* I default to UTF-8, like it or not */
	CamelMimeFilterWindows *windows = NULL;
	ssize_t bytes = -1;

	if (dw->mime_type && (charset = camel_content_type_param
	                      (dw->mime_type, "charset")) &&
	    g_ascii_strncasecmp(charset, "iso-8859-", 9) == 0)
		{
			CamelStream *null;

			/* Since a few Windows mailers like to claim they sent
			 * out iso-8859-# encoded text when they really sent
			 * out windows-cp125#, do some simple sanity checking
			 * before we move on... */

			null = camel_stream_null_new();
			filter_stream = camel_stream_filter_new_with_stream(null);
			camel_object_unref(null);
			windows = (CamelMimeFilterWindows *)camel_mime_filter_windows_new(charset);
			camel_stream_filter_add (filter_stream, (CamelMimeFilter *)windows);
			camel_data_wrapper_decode_to_stream (dw, (CamelStream *)filter_stream);
			camel_stream_flush ((CamelStream *)filter_stream);
			camel_object_unref (filter_stream);
			charset = camel_mime_filter_windows_real_charset (windows);
		}

	filter_stream = camel_stream_filter_new_with_stream (stream);

	if ((filter = camel_mime_filter_charset_new_convert (charset, "UTF-8"))) {
		camel_stream_filter_add (filter_stream, (CamelMimeFilter *) filter);
		camel_object_unref (filter);
	}

	bytes = camel_data_wrapper_decode_to_stream (dw, (CamelStream *)filter_stream);
	camel_stream_flush ((CamelStream *)filter_stream);
	camel_object_unref (filter_stream);

	if (windows)
		camel_object_unref(windows);

	return bytes;
}

#endif

static void
get_email_and_fullname (const gchar *line, gchar **email, gchar **fullname)
{
	gchar *ptr = g_utf8_strchr (line, -1, '<');

	if (ptr) {
		gchar *holder;

		holder = g_strdup (line);
		ptr = g_utf8_strchr (holder, -1, '<');
		*ptr = '\0';
		ptr++;
		*fullname = holder;
		holder = ptr;
		ptr = g_utf8_strchr (ptr, -1, '>');
		if (ptr) {
			*ptr = '\0';
		}
		*email = g_strdup (holder);

	} else {
		*email = g_strdup (line);
		*fullname = NULL;
	}
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
free_pool_item (gpointer data, gpointer user_data)
{
	PoolItem *item = data;
	g_free (item->sparql);
	g_object_unref (item->client);
	g_slice_free (PoolItem, item);
}

static void
thread_pool_exec (gpointer data, gpointer user_data)
{
	ThreadPool *pool = user_data;
	PoolItem *item;
	gboolean dying;

	g_mutex_lock (pool->mutex);
	dying = pool->dying;
	pool->items = g_list_remove (pool->items, data);
	g_mutex_unlock (pool->mutex);


	if (!dying)
		pool->func (data, pool->cancel);

	item = data;
	if (!item->dont_free)
		pool->freeup (data, pool->cancel);
}


static void 
reply_void (GError *error, gpointer  user_data)
{
	PoolItem *item = user_data;
	ThreadPool *pool = item->pool;

	if (error) {
		g_debug ("Tracker plugin: Error updating data: %s\n", error->message);
	}

	g_mutex_lock (item->mutex);
	g_cond_broadcast (item->cond);
	item->has_happened = TRUE;
	g_mutex_unlock (item->mutex);

	if (item->dont_free)
		pool->freeup (item, pool->cancel);
}

static void
exec_update (gpointer data, gpointer user_data)
{
	PoolItem *item = data;
	GCancellable *cancel = user_data;
	gboolean no_patience = TRUE;

	if (g_cancellable_is_cancelled (cancel))
		return;

	item->mutex = g_mutex_new ();
	item->cond = g_cond_new ();
	item->has_happened = FALSE;

	if (item->commit) {
		tracker_resources_batch_commit_async (item->client, reply_void, item);
	} else {
		tracker_resources_batch_sparql_update_async (item->client, item->sparql,
		                                             reply_void, item);
	}

	g_mutex_lock (item->mutex);
	if (!item->has_happened) {
		GTimeVal val;
		g_get_current_time (&val);
		g_time_val_add (&val, 5 * 1000000); /* 5 seconds worth of patience */
		no_patience = g_cond_timed_wait (item->cond, item->mutex, &val);
		item->dont_free = !no_patience;
	}
	g_mutex_unlock (item->mutex);

	/* Don't hammer DBus too much, else Evolution's UI sometimes becomes slugish
	 * due to a dbus_watch_handle call on its mainloop */

	if (no_patience)
		g_usleep (300);
}

static gint 
pool_sort_func (gconstpointer a,
                gconstpointer b,
                gpointer user_data)
{
	PoolItem *item_a = (PoolItem *) a;
	PoolItem *item_b = (PoolItem *) b;

	return item_a->prio - item_b->prio;
}

static ThreadPool*
thread_pool_new (GFunc func, GFunc freeup, GCompareDataFunc sorter)
{
	ThreadPool *wrap = g_new0 (ThreadPool, 1);

	wrap->pool = g_thread_pool_new (thread_pool_exec, wrap, 1, FALSE, NULL);
	if (sorter)
		g_thread_pool_set_sort_function (wrap->pool, sorter, NULL);
	wrap->items = NULL;
	wrap->dying = FALSE;
	wrap->func = func;
	wrap->freeup = freeup;
	wrap->mutex = g_mutex_new ();
	wrap->cancel = g_cancellable_new ();

	return wrap;
}

static void
thread_pool_push (ThreadPool *pool, gpointer item, gpointer user_data)
{
	g_mutex_lock (pool->mutex);
	pool->items = g_list_prepend (pool->items, item);
	if (!pool->dying)
		g_thread_pool_push (pool->pool, item, user_data);
	g_mutex_unlock (pool->mutex);
}

static gpointer
destroyer_thread (gpointer user_data)
{
	ThreadPool *pool = user_data;

	g_mutex_lock (pool->mutex);
	g_thread_pool_free (pool->pool, TRUE, TRUE);
	g_list_foreach (pool->items, pool->freeup, NULL);
	g_mutex_unlock (pool->mutex);

	g_object_unref (pool->cancel);
	g_free (pool);

	return NULL;
}

static void
thread_pool_destroy (ThreadPool *pool)
{
	g_mutex_lock (pool->mutex);
	g_cancellable_cancel (pool->cancel);
	pool->dying = TRUE;
	g_mutex_unlock (pool->mutex);

	g_thread_create (destroyer_thread, pool, FALSE, NULL);
}

static void
send_sparql_update (TrackerEvolutionPlugin *self, const gchar *sparql, gint prio)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);

	if (priv->client) {
		PoolItem *item = g_slice_new (PoolItem);

		if (!sparql_pool)
			sparql_pool = thread_pool_new (exec_update, free_pool_item, pool_sort_func);

		item->pool = sparql_pool;
		item->dont_free = FALSE;
		item->prio = prio;
		item->commit = FALSE;
		item->client = g_object_ref (priv->client);
		item->sparql = g_strdup (sparql);

		thread_pool_push (sparql_pool, item, NULL);
	}
}

static void
send_sparql_commit (TrackerEvolutionPlugin *self, gboolean update)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);

	if (priv->client) {
		PoolItem *item = g_slice_new (PoolItem);

		if (update) {
			gchar *date_s = tracker_date_to_string (time (NULL));
			gchar *update = g_strdup_printf ("DELETE FROM <"DATASOURCE_URN"> { <" DATASOURCE_URN "> nie:contentLastModified ?d } "
			                                 "WHERE { <" DATASOURCE_URN "> a nie:InformationElement ; nie:contentLastModified ?d } \n"
			                                 "INSERT INTO <"DATASOURCE_URN"> { <" DATASOURCE_URN "> nie:contentLastModified \"%s\" }",
			                                 date_s);

			send_sparql_update (self, update, 0);

			g_free (update);
			g_free (date_s);
		}

		if (!sparql_pool)
			sparql_pool = thread_pool_new (exec_update, free_pool_item, pool_sort_func);

		item->pool = sparql_pool;
		item->dont_free = FALSE;
		item->prio = 0;
		item->commit = TRUE;
		item->client = g_object_ref (priv->client);
		item->sparql = NULL;

		thread_pool_push (sparql_pool, item, NULL);
	}
}

static void
add_contact (TrackerSparqlBuilder *sparql, const gchar *predicate, const gchar *uri, const gchar *value)
{
	gchar *email_uri, *email = NULL, *fullname = NULL;

	get_email_and_fullname (value, &email, &fullname);

	email_uri = tracker_uri_printf_escaped ("mailto:%s", email);

	tracker_sparql_builder_subject_iri (sparql, email_uri);
	tracker_sparql_builder_predicate (sparql, "rdf:type");
	tracker_sparql_builder_object (sparql, "nco:EmailAddress");

	tracker_sparql_builder_subject_iri (sparql, email_uri);
	tracker_sparql_builder_predicate (sparql, "nco:emailAddress");
	tracker_sparql_builder_object_string (sparql, email);

	tracker_sparql_builder_subject_iri (sparql, uri);
	tracker_sparql_builder_predicate (sparql, predicate);

	tracker_sparql_builder_object_blank_open (sparql);

	tracker_sparql_builder_predicate (sparql, "rdf:type");
	tracker_sparql_builder_object (sparql, "nco:Contact");

	if (fullname) {
		tracker_sparql_builder_predicate (sparql, "nco:fullname");
		tracker_sparql_builder_object_string (sparql, fullname);
		g_free (fullname);
	}

	tracker_sparql_builder_predicate (sparql, "nco:hasEmailAddress");
	tracker_sparql_builder_object_iri (sparql, email_uri);

	tracker_sparql_builder_object_blank_close (sparql);

	g_free (email_uri);
	g_free (email);
}

static void
process_fields (TrackerSparqlBuilder *sparql, const gchar *uid, guint flags,
                time_t sent, const gchar *subject, const gchar *from, const gchar *to,
                const gchar *cc, const gchar *size, CamelFolder *folder, const gchar *uri)
{
	gchar **arr;
	guint i;

	tracker_sparql_builder_subject_iri (sparql, DATASOURCE_URN);
	tracker_sparql_builder_predicate (sparql, "rdf:type");
	tracker_sparql_builder_object_iri (sparql, NIE_DATASOURCE);

	/* for contentLastModified */
	tracker_sparql_builder_predicate (sparql, "rdf:type");
	tracker_sparql_builder_object (sparql, "nie:InformationElement");

	tracker_sparql_builder_subject_iri (sparql, uri);
	tracker_sparql_builder_predicate (sparql, "rdf:type");
	tracker_sparql_builder_object (sparql, "nmo:Email");

	tracker_sparql_builder_predicate (sparql, "rdf:type");
	tracker_sparql_builder_object (sparql, "nmo:MailboxDataObject");

	tracker_sparql_builder_predicate (sparql, "tracker:available");
	tracker_sparql_builder_object_boolean (sparql, TRUE);

	/* Laying the link between the IE and the DO. We use IE = DO */
	tracker_sparql_builder_predicate (sparql, "nie:isStoredAs");
	tracker_sparql_builder_object_iri (sparql, uri);

	/* The URL of the DataObject (because IE = DO, this is correct) */
	tracker_sparql_builder_predicate (sparql, "nie:url");
	tracker_sparql_builder_object_string (sparql, uri);

	tracker_sparql_builder_predicate (sparql, "nie:dataSource");
	tracker_sparql_builder_object_iri (sparql, DATASOURCE_URN);

	if (size && g_utf8_validate (size, -1, NULL)) {
		tracker_sparql_builder_predicate (sparql, "nie:byteSize");
		tracker_sparql_builder_object_string (sparql, size);
	}

	if (subject && g_utf8_validate (subject, -1, NULL)) {
		tracker_sparql_builder_predicate (sparql, "nmo:messageSubject");
		tracker_sparql_builder_object_string (sparql, subject);
	}

	tracker_sparql_builder_predicate (sparql, "nmo:receivedDate");
	tracker_sparql_builder_object_date (sparql, &sent);

	tracker_sparql_builder_predicate (sparql, "nmo:isDeleted");
	tracker_sparql_builder_object_boolean (sparql, (flags & CAMEL_MESSAGE_DELETED));

	tracker_sparql_builder_predicate (sparql, "nmo:isAnswered");
	tracker_sparql_builder_object_boolean (sparql, (flags & CAMEL_MESSAGE_ANSWERED));

	tracker_sparql_builder_predicate (sparql, "nmo:isFlagged");
	tracker_sparql_builder_object_boolean (sparql, (flags & CAMEL_MESSAGE_FLAGGED));

	tracker_sparql_builder_predicate (sparql, "nmo:isRead");
	tracker_sparql_builder_object_boolean (sparql, (flags & CAMEL_MESSAGE_SEEN));

	/*
	  g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_UID));
	  g_ptr_array_add (values_temp, g_strdup (uid));

	  g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_JUNK));
	  g_ptr_array_add (values_temp, g_strdup ((flags & CAMEL_MESSAGE_JUNK) ? "True" : "False"));

	  g_ptr_array_add (predicates_temp, g_strdup (TRACKER_EVOLUTION_PREDICATE_FORWARDED));
	  g_ptr_array_add (values_temp, g_strdup ((flags & CAMEL_MESSAGE_FORWARDED) ? "True" : "False"));
	*/

	if (to && (arr = g_strsplit (to, ",", -1)) != NULL) {
		for (i = 0; arr[i]; i++) {
			g_strstrip (arr[i]);

			if (g_utf8_validate (arr[i], -1, NULL)) {
				add_contact (sparql, "nmo:to", uri, arr[i]);
			}
		}
		g_strfreev (arr);
	}

	if (from && g_utf8_validate (from, -1, NULL)) {
		add_contact (sparql, "nmo:from", uri, from);
	}

	if (cc && (arr = g_strsplit (cc, ",", -1)) != NULL) {
		for (i = 0; arr[i]; i++) {
			g_strstrip (arr[i]);
			if (g_utf8_validate (arr[i], -1, NULL)) {
				add_contact (sparql, "nmo:cc", uri, arr[i]);
			}
		}
		g_strfreev (arr);
	}

#if 0
	/* This massively slows down Evolution, we need to do this in a queue
	 * instead. Therefore I'm disabling this code for now. The code does
	 * a parse of each already-once-downloaded E-mail. This is obviously
	 * excessive and expensive for the performance of Evolution. */

	if (folder) {
		gchar *filen = camel_folder_get_filename (folder, uid, NULL);
		if (filen) {
			if (g_file_test (filen, G_FILE_TEST_EXISTS)) {
				CamelMimeMessage *mime = camel_folder_get_message (folder, uid, NULL);
				if (mime) {
					CamelDataWrapper *containee;
					containee = camel_medium_get_content_object (CAMEL_MEDIUM (mime));

					if (CAMEL_IS_MULTIPART (containee)) {
						guint i, parts = camel_multipart_get_number (CAMEL_MULTIPART (containee));
						for (i = 0; i < parts; i++) {
							CamelMimePart *tpart = camel_multipart_get_part (CAMEL_MULTIPART (containee), i);
							CamelContentType *type;

							type = camel_mime_part_get_content_type (tpart);
							if (camel_content_type_is (type, "text", "*")) {
								CamelStream *stream = camel_stream_mem_new ();
								CamelDataWrapper *wrapper;
								CamelStreamMem *mem = (CamelStreamMem *) stream;
								gssize bytes = -1;

								wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (tpart));
								if (!wrapper) {
									wrapper = camel_data_wrapper_new ();
									camel_medium_set_content_object (CAMEL_MEDIUM (tpart), wrapper);
									camel_object_unref (wrapper);
								}

								if (wrapper->stream) {
									camel_stream_reset (wrapper->stream);

									if (camel_content_type_is (wrapper->mime_type, "text", "plain"))
										bytes = camel_stream_format_text (wrapper, stream);
									else
										bytes = camel_data_wrapper_decode_to_stream (wrapper, stream);

									/* The validate check always fails for me, don't know why yet */
									if (bytes > 0 && g_utf8_validate ((gchar *) mem->buffer->data, -1, NULL)) {
										tracker_sparql_builder_subject_iri (sparql, uri);
										tracker_sparql_builder_predicate (sparql, "nie:plainTextContent");
										tracker_sparql_builder_object_string (sparql, (gchar *) mem->buffer->data);
									}
								}

								camel_object_unref (stream);
							}
						}
					}
					camel_object_unref (mime);
				}
			}
			g_free (filen);
		}
	}
#endif
}

/* When new messages arrive to- or got deleted from the summary, called in
 * mainloop or by a thread (unknown, depends on Camel and Evolution code that
 * executes the reason why this signal gets emitted).
 *
 * This one is the reason why we registered all those folders during init below. */

static void
on_folder_summary_changed (CamelFolder *folder,
                           CamelFolderChangeInfo *changes,
                           gpointer user_data)
{
	OnSummaryChangedInfo *info = user_data;
	CamelFolderSummary *summary;
	gchar *account_uri = info->account_uri;
	GPtrArray *merged;
	guint i;
	gchar *em_uri;

	if (!folder)
		return;

	summary = folder->summary;
	em_uri = em_uri_from_camel (account_uri);
	em_uri [strlen (em_uri) - 1] = '\0';

	merged = g_ptr_array_new ();

	/* the uid_added member contains the added-to-the-summary items */

	if (changes->uid_added && changes->uid_added->len > 0) {
		for (i = 0; i < changes->uid_added->len; i++) {
			g_ptr_array_add (merged, changes->uid_added->pdata[i]);
		}
	}

	/* the uid_changed member contains the changed-in-the-summary items */

	if (changes->uid_changed && changes->uid_changed->len > 0) {
		guint y;

		for (i = 0; i < changes->uid_changed->len; i++) {
			gboolean found = FALSE;

			for (y = 0; y < merged->len; y++) {
				if (strcmp (merged->pdata[y], changes->uid_changed->pdata[i]) == 0) {
					found = TRUE;
					break;
				}
			}

			if (!found) {
				g_ptr_array_add (merged, changes->uid_changed->pdata[i]);
			}
		}
	}

	for (i = 0; i< merged->len; i++) {
		const gchar *subject, *to, *from, *cc, *uid = NULL;
		gchar *size;
		time_t sent;
		guint flags;
		CamelMessageInfo *linfo;
		const CamelTag *ctags;
		const CamelFlag *cflags;

		linfo = camel_folder_summary_uid (summary, merged->pdata[i]);

		if (linfo) {
			uid = (gchar *) camel_message_info_uid (linfo);
		}

		if (linfo && uid) {
			gchar *uri;
			TrackerSparqlBuilder *sparql;

			subject = camel_message_info_subject (linfo);
			to      = camel_message_info_to (linfo);
			from    = camel_message_info_from (linfo);
			cc      = camel_message_info_cc (linfo);
			flags   = (guint) camel_message_info_flags (linfo);

			/* Camel returns a time_t, I think a uint64 is the best fit here */
			sent = camel_message_info_date_sent (linfo);

			/* Camel returns a uint32, so %u */
			size = g_strdup_printf ("%u", camel_message_info_size (linfo));

			/* This is not a path but a URI, don't use the
			 * OS's directory separator here */

			uri = tracker_uri_printf_escaped ("%s/%s/%s",
			                                  em_uri,
			                                  camel_folder_get_full_name (folder),
			                                  uid);

			sparql = tracker_sparql_builder_new_update ();

			tracker_sparql_builder_drop_graph (sparql, uri);

			tracker_sparql_builder_insert_open (sparql, uri);

			process_fields (sparql, uid, flags, sent, subject,
			                from, to, cc, size, folder, uri);

			cflags = camel_message_info_user_flags (linfo);
			while (cflags) {
				tracker_sparql_builder_subject_iri (sparql, uri);

				tracker_sparql_builder_predicate (sparql, "nao:hasTag");
				tracker_sparql_builder_object_blank_open (sparql);

				tracker_sparql_builder_predicate (sparql, "rdf:type");
				tracker_sparql_builder_object (sparql, "nao:Tag");

				tracker_sparql_builder_predicate (sparql, "nao:prefLabel");
				tracker_sparql_builder_object_string (sparql, cflags->name);
				tracker_sparql_builder_object_blank_close (sparql);

				cflags = cflags->next;
			}

			ctags = camel_message_info_user_tags (linfo);
			while (ctags) {
				tracker_sparql_builder_subject_iri (sparql, uri);

				tracker_sparql_builder_predicate (sparql, "nao:hasProperty");
				tracker_sparql_builder_object_blank_open (sparql);

				tracker_sparql_builder_predicate (sparql, "rdf:type");
				tracker_sparql_builder_object (sparql, "nao:Property");

				tracker_sparql_builder_predicate (sparql, "nao:propertyName");
				tracker_sparql_builder_object_string (sparql, ctags->name);

				tracker_sparql_builder_predicate (sparql, "nao:propertyValue");
				tracker_sparql_builder_object_string (sparql, ctags->value);

				tracker_sparql_builder_object_blank_close (sparql);
				ctags = ctags->next;
			}

			tracker_sparql_builder_insert_close (sparql);

			send_sparql_update (info->self, tracker_sparql_builder_get_result (sparql), 100);

			g_object_set (info->self, "progress",
			              (gdouble) i / merged->len,
			              NULL);

			g_object_unref (sparql);

			g_free (size);
			g_free (uri);
		}

		if (linfo)
			camel_message_info_free (linfo);
	}

	g_ptr_array_free (merged, TRUE);

	/* the uid_removed member contains the removed-from-the-summary items */

	if (changes->uid_removed && changes->uid_removed->len > 0) {

		/* The FROM uri is not exactly right here, but we just want
		 * graph != NULL in tracker-store/tracker-writeback.c */

		GString *sparql = g_string_new ("");

		for (i = 0; i< changes->uid_removed->len; i++) {
			gchar *uri;

			/* This is not a path but a URI, don't use the OS's
			 * directory separator here */
			uri = tracker_uri_printf_escaped ("%s/%s/%s",
			                                  em_uri,
			                                  camel_folder_get_full_name (folder),
			                                  (char*) changes->uid_removed->pdata[i]);

			g_string_append_printf (sparql, "DELETE FROM <%s> { <%s> a rdfs:Resource }\n ", uri, uri);
			g_free (uri);
		}

		send_sparql_update (info->self, sparql->str, 100);
		g_string_free (sparql, TRUE);
	}

	send_sparql_commit (info->self, TRUE);

	g_object_set (info->self, "progress",
	              1.0, NULL);

	g_free (em_uri);
}


/* Initial upload of more recent than last_checkout items, called in the mainloop */
static void
introduce_walk_folders_in_folder (TrackerEvolutionPlugin *self,
                                  CamelFolderInfo *iter,
                                  CamelStore *store, CamelDB *cdb_r,
                                  gchar *account_uri,
                                  ClientRegistry *info,
                                  GCancellable *cancel)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	gchar *em_uri;

	if (g_cancellable_is_cancelled (cancel)) {
		return;
	}

	em_uri = em_uri_from_camel (account_uri);
	em_uri [strlen (em_uri) - 1] = '\0';

	while (iter) {
		guint count = 0;
		guint ret = SQLITE_OK;
		gchar *query;
		sqlite3_stmt *stmt = NULL;

		/* This query is the culprint of the functionality: it fetches
		 * all the metadata from the summary table where modified is
		 * more recent than the client-registry's modseq. Note that we
		 * pass time(NULL) to all methods, which is why comparing
		 * against the modified column that Evolution > 2.25.5 stores
		 * works (otherwise this wouldn't work, of course).
		 *
		 * The idea is that only the changes must initially be pushed,
		 * not everything each time (which would be unefficient). The
		 * specification (http://live.gnome.org/Evolution/Metadata)
		 * allows this 'modseq' optimization (is in fact recommending
		 * it over using Cleanup() each time) */

		/* TODO: add bodystructure and then prepare a full MIME structure
		 * using the NMO ontology, by parsing the bodystructure.
		 * Bodystructures can be found in %s_bodystructure when they
		 * exist (not guaranteed). In IMAP BODYSTRUCTURE format. */

		query = sqlite3_mprintf ("SELECT uid, flags, read, deleted, "            /* 0  - 3  */
		                         "replied, important, junk, attachment, " /* 4  - 7  */
		                         "size, dsent, dreceived, subject, "      /* 8  - 11 */
		                         "mail_from, mail_to, mail_cc, mlist, "   /* 12 - 15 */
		                         "labels, usertags "                      /* 16 - 17 */
		                         "FROM %Q "
		                         "WHERE modified > %"G_GUINT64_FORMAT,

		                         iter->full_name,
		                         info->last_checkout);


		ret = sqlite3_prepare_v2 (cdb_r->db, query, -1, &stmt, NULL);

		while (ret == SQLITE_OK || ret == SQLITE_BUSY || ret == SQLITE_ROW) {
			TrackerSparqlBuilder *sparql = NULL;
			gchar *subject, *to, *from, *cc, *uid, *size;
			time_t sent;
			gchar *part, *label, *p;
			guint flags;

			if (g_cancellable_is_cancelled (cancel)) {
				break;
			}

			ret = sqlite3_step (stmt);

			if (ret == SQLITE_BUSY) {
				usleep (10);
				continue;
			}

			if ((ret != SQLITE_OK && ret != SQLITE_ROW) || ret == SQLITE_DONE) {
				break;
			}

			uid = (gchar *) sqlite3_column_text (stmt, 0);

			if (uid) {
				const gchar *query;
				CamelFolder *folder;
				guint max = 0, j;
				gchar *uri;
				gboolean opened = FALSE;

				flags =   (guint  ) sqlite3_column_int  (stmt, 1);
				size =    (gchar *) sqlite3_column_text (stmt, 8);
				sent =    (time_t)  sqlite3_column_int64 (stmt, 9);
				subject = (gchar *) sqlite3_column_text (stmt, 11);
				from =    (gchar *) sqlite3_column_text (stmt, 12);
				to =      (gchar *) sqlite3_column_text (stmt, 13);
				cc =      (gchar *) sqlite3_column_text (stmt, 14);

				folder = g_hash_table_lookup (priv->cached_folders, iter->full_name);

				uri = tracker_uri_printf_escaped ("%s/%s/%s", em_uri, iter->full_name, uid);

				if (!sparql) {
					sparql = tracker_sparql_builder_new_update ();
				}

				tracker_sparql_builder_drop_graph (sparql, uri);

				tracker_sparql_builder_insert_open (sparql, uri);

				process_fields (sparql, uid, flags, sent,
				                subject, from, to, cc, size,
				                folder, uri);

				/* Extract User flags/labels */
				p = part = g_strdup ((const gchar *) sqlite3_column_text (stmt, 16));
				if (part) {
					label = part;
					for (j=0; part[j]; j++) {

						if (part[j] == ' ') {
							part[j] = 0;

							if (!opened) {
								tracker_sparql_builder_subject_iri (sparql, uri);
								opened = TRUE;
							}

							tracker_sparql_builder_predicate (sparql, "nao:hasTag");
							tracker_sparql_builder_object_blank_open (sparql);

							tracker_sparql_builder_predicate (sparql, "rdf:type");
							tracker_sparql_builder_object (sparql, "nao:Tag");

							tracker_sparql_builder_predicate (sparql, "nao:prefLabel");
							tracker_sparql_builder_object_string (sparql, label);
							tracker_sparql_builder_object_blank_close (sparql);
							label = &(part[j+1]);
						}
					}
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

								if (!opened) {
									tracker_sparql_builder_subject_iri (sparql, uri);
									opened = TRUE;
								}

								tracker_sparql_builder_predicate (sparql, "nao:hasProperty");
								tracker_sparql_builder_object_blank_open (sparql);

								tracker_sparql_builder_predicate (sparql, "rdf:type");
								tracker_sparql_builder_object (sparql, "nao:Property");

								tracker_sparql_builder_predicate (sparql, "nao:propertyName");
								tracker_sparql_builder_object_string (sparql, name);

								tracker_sparql_builder_predicate (sparql, "nao:propertyValue");
								tracker_sparql_builder_object_string (sparql, value);

								tracker_sparql_builder_object_blank_close (sparql);
							}
						g_free(name);
						g_free(value);
					}

				g_free (uri);
				g_free (p);

				tracker_sparql_builder_insert_close (sparql);
				query = tracker_sparql_builder_get_result (sparql);
				count++;
				send_sparql_update (self, query, 0);
				g_object_unref (sparql);
			}
		}

		send_sparql_commit (self, TRUE);
		g_object_set (self, "progress",
		              1.0, NULL);

		sqlite3_finalize (stmt);
		sqlite3_free (query);

		if (iter->child) {
			introduce_walk_folders_in_folder (self, iter->child,
			                                  store, cdb_r,
			                                  account_uri, info,
			                                  cancel);
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
	gchar *em_uri;

	em_uri = em_uri_from_camel (account_uri);
	em_uri [strlen (em_uri) - 1] = '\0';

	query = sqlite3_mprintf ("SELECT uid, mailbox "
	                         "FROM Deletes "
	                         "WHERE modified > %" G_GUINT64_FORMAT,
	                         info->last_checkout);

	/* This creates a thread apparently */
	cdb_r = camel_db_clone (store->cdb_r, NULL);

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

			g_ptr_array_add (subjects_a,
			                 tracker_uri_printf_escaped ("%s/%s/%s", em_uri,
			                                             mailbox, uid));

			if (count > 100) {
				more = TRUE;
				break;
			}

			count++;

			more = FALSE;
		}

		if (count > 0) {
			/* The FROM uri is not exactly right here, but we just want
			 * graph != NULL in tracker-store/tracker-writeback.c */

			GString *sparql = g_string_new ("");

			for (i = 0; i < subjects_a->len; i++) {
				g_string_append_printf (sparql, "DELETE FROM <%s> { <%s> a rdfs:Resource } \n",
				                        (gchar *) g_ptr_array_index (subjects_a, i),
				                        (gchar *) g_ptr_array_index (subjects_a, i));
			}

			g_string_append_c (sparql, '}');

			send_sparql_update (self, sparql->str, 100);
			g_string_free (sparql, TRUE);

		}

		g_ptr_array_free (subjects_a, TRUE);

	}

	send_sparql_commit (self, FALSE);

	sqlite3_finalize (stmt);
	sqlite3_free (query);

	camel_db_close (cdb_r);

	g_free (em_uri);
}

/* Get the oldest date in all of the deleted-tables, called in the mainloop. We
 * need this to test whether we should use Cleanup() or not. */

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

			if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE)) {
				continue;
			}

			if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
				camel_exception_clear (&ex);
				continue;
			}

			cdb_r = camel_db_clone (store->cdb_r, NULL);

			query = sqlite3_mprintf ("SELECT time "
			                         "FROM Deletes "
			                         "ORDER BY time LIMIT 1");

			ret = sqlite3_prepare_v2 (cdb_r->db, query, -1, &stmt, NULL);

			ret = sqlite3_step (stmt);

			if (ret == SQLITE_OK || ret == SQLITE_ROW) {
				latest = sqlite3_column_int64 (stmt, 0);
			}

			if (latest < smallest) {
				smallest = latest;
			}

			sqlite3_finalize (stmt);
			sqlite3_free (query);

			camel_db_close (cdb_r);
		}

		g_object_unref (it);
	}

	return smallest;
}

static void
register_on_get_folder (gchar *uri, CamelFolder *folder, gpointer user_data)
{
	GetFolderInfo *info = user_data;
	gchar *account_uri = info->account_uri;
	CamelFolderInfo *iter = info->iter;
	TrackerEvolutionPlugin *self = info->self;
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	guint hook_id;
	FolderRegistry *registry;

	if (!folder) {
		goto fail_register;
	}

	registry = folder_registry_new (account_uri, folder, self);

	if (!priv->registered_folders || !priv->cached_folders) {
		goto not_ready;
	}

	hook_id = camel_object_hook_event (folder, "folder_changed",
	                                   CAMEL_CALLBACK (on_folder_summary_changed),
	                                   registry->hook_info);
	registry->hook_info->hook_id = hook_id;

	g_hash_table_replace (priv->registered_folders,
	                      GINT_TO_POINTER (hook_id),
	                      registry);

	g_hash_table_replace (priv->cached_folders,
	                      g_strdup (iter->full_name),
	                      folder);

 not_ready:
 fail_register:

	camel_folder_info_free (info->iter);
	g_free (info->account_uri);
	g_object_unref (info->self);
	g_free (info);

	register_count--;
}

static void
register_walk_folders_in_folder (TrackerEvolutionPlugin *self,
                                 CamelFolderInfo *iter,
                                 CamelStore *store,
                                 gchar *account_uri)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);

	if (!priv->registered_folders) {
		priv->registered_folders = g_hash_table_new_full (g_direct_hash, g_direct_equal,
		                                                  (GDestroyNotify) NULL,
		                                                  (GDestroyNotify) folder_registry_free);

		priv->cached_folders = g_hash_table_new_full (g_str_hash, g_str_equal,
		                                              (GDestroyNotify) g_free,
		                                              (GDestroyNotify) NULL);
	}

	/* Recursively walks all the folders in store */

	while (iter) {
		GetFolderInfo *info = g_new0 (GetFolderInfo, 1);

		info->self = g_object_ref (self);
		info->account_uri = g_strdup (account_uri);
		info->iter = camel_folder_info_clone (iter);

		register_count++;

		/* This is asynchronous and hooked to the mail/ API, so nicely
		 * integrated with the Evolution UI application */

		mail_get_folder (iter->uri, 0, register_on_get_folder, info,
		                 mail_msg_unordered_push);

		if (iter->child) {
			register_walk_folders_in_folder (self, iter->child,
			                                 store,
			                                 account_uri);
		}

		iter = iter->next;
	}
}


static void
unregister_on_get_folder (gchar *uri, CamelFolder *folder, gpointer user_data)
{
	GetFolderInfo *info = user_data;
	CamelFolderInfo *titer = info->iter;
	TrackerEvolutionPlugin *self = info->self;
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	GHashTableIter iter;
	gpointer key, value;

	if (!folder) {
		goto fail_unregister;
	}

	if (!priv->registered_folders) {
		goto no_folders;
	}

	g_hash_table_iter_init (&iter, priv->registered_folders);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		FolderRegistry *registry = value;

		if (folder == registry->folder) {
			g_hash_table_remove (priv->cached_folders, titer->full_name);
			g_hash_table_iter_remove (&iter);
			break;
		}
	}

 no_folders:
 fail_unregister:

	camel_folder_info_free (info->iter);
	g_free (info->account_uri);
	g_object_unref (info->self);
	g_free (info);
}

static void
unregister_walk_folders_in_folder (TrackerEvolutionPlugin *self,
                                   CamelFolderInfo *titer,
                                   CamelStore *store,
                                   gchar *account_uri)
{
	/* Recursively walks all the folders in store */

	while (titer) {
		GetFolderInfo *info = g_new0 (GetFolderInfo, 1);

		info->self = g_object_ref (self);
		info->account_uri = g_strdup (account_uri);
		info->iter = camel_folder_info_clone (titer);

		/* This is asynchronous and hooked to the mail/ API, so nicely
		 * integrated with the Evolution UI application */

		mail_get_folder (titer->uri, 0, unregister_on_get_folder, info,
		                 mail_msg_unordered_push);

		if (titer->child) {
			unregister_walk_folders_in_folder (self, titer->child,
			                                   store,
			                                   account_uri);
		}

		titer = titer->next;
	}
}

static void
client_registry_info_free (ClientRegistry *info)
{
	g_slice_free (ClientRegistry, info);
}

static ClientRegistry*
client_registry_info_copy (ClientRegistry *info)
{
	ClientRegistry *ninfo = g_slice_new0 (ClientRegistry);

	ninfo->last_checkout = info->last_checkout;

	return ninfo;
}

typedef struct {
	IntroductionInfo *intro_info;
	CamelFolderInfo *iter;
	CamelStore *store;
	CamelDB *cdb_r;
} WorkerThreadinfo;

static void
free_introduction_info (IntroductionInfo *intro_info)
{
	client_registry_info_free (intro_info->info);
	g_free (intro_info->account_uri);
	g_object_unref (intro_info->self);
	g_free (intro_info);
}

static void
free_worker_thread_info (gpointer data, gpointer user_data)
{
	WorkerThreadinfo *winfo = data;

	/* Ownership was transfered to us in try_again */
	free_introduction_info (winfo->intro_info);
	camel_db_close (winfo->cdb_r);
	camel_object_unref (winfo->store);
	camel_folder_info_free (winfo->iter);
	g_free (winfo);
}

static void
folder_worker (gpointer data, gpointer user_data)
{
	WorkerThreadinfo *winfo = data;

	introduce_walk_folders_in_folder (winfo->intro_info->self,
	                                  winfo->iter,
	                                  winfo->store,
	                                  winfo->cdb_r,
	                                  winfo->intro_info->account_uri,
	                                  winfo->intro_info->info,
	                                  user_data);

	return;
}

/* For info about this try-again stuff, look at on_got_folderinfo_introduce */

static gboolean
try_again (gpointer user_data)
{
	if (register_count == 0) {
		TryAgainInfo *info = user_data;
		WorkerThreadinfo *winfo = g_new (WorkerThreadinfo, 1);

		winfo->intro_info = info->intro_info; /* owner transfer */
		winfo->iter = info->iter; /* owner transfer */
		winfo->store = info->store; /* owner transfer */
		winfo->cdb_r = info->cdb_r; /* owner transfer */

		if (!folder_pool)
			folder_pool = thread_pool_new (folder_worker, free_worker_thread_info, NULL);

		thread_pool_push (folder_pool, winfo, NULL);

		return FALSE;
	}

	return TRUE;
}

static gboolean
on_got_folderinfo_introduce (CamelStore *store,
                             CamelFolderInfo *iter,
                             void *data)
{
	TryAgainInfo *info = g_new0 (TryAgainInfo, 1);

	/* Ownership of these is transfered in try_again */

	camel_object_ref (store);
	info->store = store;
	/* This apparently creates a thread */
	info->cdb_r = camel_db_clone (store->cdb_r, NULL);
	info->iter = camel_folder_info_clone (iter);
	info->intro_info = data;

	/* If a registrar is running while Evolution is starting up, we decide
	 * not to slow down Evolution's startup by immediately going through
	 * all CamelFolder instances (the UI is doing the same thing, we can
	 * better allow the UI to do this first, and cache the folders that
	 * way)
	 *
	 * Regretfully doesn't Evolution's plugin interfaces give me a better
	 * hook to detect the startup of the UI application of Evolution, else
	 * it would of course be better to use that instead.
	 *
	 * The register_count is the amount of folders that we register, a
	 * registry has been made asynchronous using the high-level API
	 * mail_get_folder, so in the callback we decrement the number, before
	 * the call we increment the number. If we're at zero, it means we're
	 * fully initialized. If not, we wait ten seconds and retry until
	 * finally we're fully initialized. (it's not as magic as it looks) */

	if (register_count != 0) {
		g_timeout_add_seconds_full (G_PRIORITY_DEFAULT, 1,
		                            try_again, info,
		                            (GDestroyNotify) g_free);
	} else {
		try_again (info);
		g_free (info);
	}

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

	for (it = e_list_get_iterator (E_LIST (priv->accounts)); e_iterator_is_valid (it); e_iterator_next (it)) {
		introduce_account_to (self, (EAccount *) e_iterator_get (it), info);
	}

	g_object_unref (it);
}

static void
register_client_second_half (ClientRegistry *info)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (info->self);
	guint64 too_old = get_last_deleted_time (info->self);

	/* If registrar's modseq is too old, send Cleanup (). This means that
	 * we tell it to start over (it must invalidate what it has). */

	if (info->last_checkout < too_old) {

		send_sparql_update (info->self, "DELETE FROM <"DATASOURCE_URN"> { ?s a rdfs:Resource } "
		                    "WHERE { ?s nie:dataSource <" DATASOURCE_URN "> }", 0);
		send_sparql_commit (info->self, FALSE);

		info->last_checkout = 0;
	}

	priv->last_time = info->last_checkout;

	introduce_accounts_to (info->self, info);

	priv->registered_clients =
		g_list_prepend (priv->registered_clients, info);
}

static void
on_register_client_qry (GPtrArray *results,
                        GError    *error,
                        gpointer   user_data)
{
	ClientRegistry *info = user_data;
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (info->self);
	guint i;

	if (error) {
		g_warning ("%s\n", error->message);
		g_error_free (error);
		g_slice_free (ClientRegistry, info);
		return;
	}

	if (!results) {
		if (priv->resuming) {
			info->last_checkout = priv->last_time;
		} else {
			info->last_checkout = 0;
		}
	} else {
		if (results->len == 0 && priv->resuming && priv->last_time != 0) {
			info->last_checkout = priv->last_time;
		} else {
			if (results->len == 0) {
				info->last_checkout = 0;
			} else {
				for (i = 0; i < results->len; i++) {
					const gchar **str = g_ptr_array_index (results, i);
					GError *new_error = NULL;

					info->last_checkout = (guint64) tracker_string_to_date (str[0], NULL, &new_error);

					if (new_error) {
						g_warning ("%s", new_error->message);
						g_error_free (error);
						g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
						g_ptr_array_free (results, TRUE);
						return;
					}

					break;
				}
			}
		}
		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}

	register_client_second_half (info);
}

static void
register_client (TrackerEvolutionPlugin *self)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (self);
	ClientRegistry *info;
	const gchar *query;

	if (!priv->client) {
		return;
	}

	info = g_slice_new0 (ClientRegistry);
	info->self = self; /* weak */

	priv->total_popped = 0;
	priv->of_total = 0;

	query = "SELECT ?c "
		"WHERE { <" DATASOURCE_URN "> nie:contentLastModified ?c }";

	tracker_resources_sparql_query_async (priv->client, query,
	                                      on_register_client_qry,
	                                      info);
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

	/* This is where it all starts for a registrar registering itself */

	if (!priv->registered_stores) {
		priv->registered_stores = g_hash_table_new_full (g_direct_hash, g_direct_equal,
		                                                 (GDestroyNotify) NULL,
		                                                 (GDestroyNotify) store_registry_free);
	}

	/* Hook up catching folder changes in the store */
	registry = store_registry_new (store, account, self);
	hook_id = camel_object_hook_event (store, "folder_created",
	                                   CAMEL_CALLBACK (on_folder_created),
	                                   registry);
	registry->hook_id = hook_id;
	g_hash_table_replace (priv->registered_stores,
	                      GINT_TO_POINTER (hook_id),
	                      registry);

	registry = store_registry_new (store, account, self);
	hook_id = camel_object_hook_event (store, "folder_renamed",
	                                   CAMEL_CALLBACK (on_folder_renamed),
	                                   registry);
	registry->hook_id = hook_id;
	g_hash_table_replace (priv->registered_stores,
	                      GINT_TO_POINTER (hook_id),
	                      registry);

	registry = store_registry_new (store, account, self);
	hook_id = camel_object_hook_event (store, "folder_deleted",
	                                   CAMEL_CALLBACK (on_folder_deleted),
	                                   registry);
	registry->hook_id = hook_id;
	g_hash_table_replace (priv->registered_stores,
	                      GINT_TO_POINTER (hook_id),
	                      registry);

	/* Register each folder to hook folder_changed everywhere (recursive) */
	register_walk_folders_in_folder (self, iter, store, uri);

	g_object_unref (reg_info->account);
	g_object_unref (reg_info->self);
	g_free (reg_info->uri);
	g_free (reg_info);

	walk_count--;

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

	walk_count++;

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

	if (priv->registered_stores) {
		g_hash_table_iter_init (&iter, priv->registered_stores);

		while (g_hash_table_iter_next (&iter, &key, &value)) {
			if (value == store)
				g_hash_table_iter_remove (&iter);
		}
	}

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
	char *uri = account->source->url;
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
	if (sparql_pool) {
		ThreadPool *pool = sparql_pool;
		sparql_pool = NULL;
		thread_pool_destroy (pool);
	}

	if (folder_pool) {
		ThreadPool *pool = folder_pool;
		folder_pool = NULL;
		thread_pool_destroy (pool);
	}

	if (manager) {
		g_object_unref (manager);
		manager = NULL;
	}
}


static void
list_names_reply_cb (DBusGProxy     *proxy,
                     DBusGProxyCall *call,
                     gpointer        user_data)
{
	GError *error = NULL;
	GStrv names = NULL;
	guint i = 0;

	dbus_g_proxy_end_call (proxy, call, &error,
	                       G_TYPE_STRV, &names,
	                       G_TYPE_INVALID);

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		if (names)
			g_strfreev (names);
		return;
	}

	while (names[i] != NULL) {
		if (g_strcmp0 (names[i], TRACKER_SERVICE) == 0) {
			register_client (user_data);
			break;
		}
		i++;
	}

	g_strfreev (names);
}


static void
name_owner_changed_cb (DBusGProxy *proxy,
                       gchar *name,
                       gchar *old_owner,
                       gchar *new_owner,
                       gpointer user_data)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (user_data);

	if (g_strcmp0 (name, TRACKER_SERVICE) == 0) {
		 if (tracker_is_empty_string (new_owner) && !tracker_is_empty_string (old_owner)) {
			if (priv->client) {
				TrackerClient *client = priv->client;

				priv->client = NULL; 

				if (sparql_pool) {
					ThreadPool *pool = sparql_pool;
					sparql_pool = NULL;
					thread_pool_destroy (pool);
				}

				if (folder_pool) {
					ThreadPool *pool = folder_pool;
					folder_pool = NULL;
					thread_pool_destroy (pool);
				}

				g_object_unref (client);
			}
		}

		if (tracker_is_empty_string (old_owner) && !tracker_is_empty_string (new_owner)) {
			if (!priv->client) {
				priv->client = tracker_client_new (0, G_MAXINT);
			}
			register_client (user_data);
		}
	}
}

static void
enable_plugin_real (void)
{
	manager = g_object_new (TRACKER_TYPE_EVOLUTION_PLUGIN,
		                        "name", "Emails", NULL);

	g_signal_emit_by_name (manager, "started");
}

static gboolean 
enable_plugin_try (gpointer user_data)
{
	if (walk_count == 0) {
		enable_plugin_real ();
		return FALSE;
	}

	return TRUE;
}

static void
enable_plugin (void)
{
	/* Deal with https://bugzilla.gnome.org/show_bug.cgi?id=606940 */

	if (sparql_pool) {
		ThreadPool *pool = sparql_pool;
		sparql_pool = NULL;
		thread_pool_destroy (pool);
	}

	if (folder_pool) {
		ThreadPool *pool = folder_pool;
		folder_pool = NULL;
		thread_pool_destroy (pool);
	}

	if (manager) {
		g_object_unref (manager);
	}

	if (walk_count > 0) {
		g_timeout_add_seconds_full (G_PRIORITY_DEFAULT, 1,
		                            enable_plugin_try, NULL, NULL);
	} else {
		enable_plugin_real ();
	}
}


int
e_plugin_lib_enable (EPlugin *ep, int enabled)
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

	if (priv->registered_folders) {
		g_hash_table_unref (priv->registered_folders);
		g_hash_table_unref (priv->cached_folders);
		priv->cached_folders = NULL;
		priv->registered_folders = NULL;
	}

	if (priv->registered_stores) {
		g_hash_table_unref (priv->registered_stores);
		priv->registered_stores = NULL;
	}

	g_object_unref (priv->accounts);

	if (priv->client) {
		TrackerClient *client = priv->client;

		priv->client = NULL;

		if (sparql_pool) {
			ThreadPool *pool = sparql_pool;
			sparql_pool = NULL;
			thread_pool_destroy (pool);
		}

		if (folder_pool) {
			ThreadPool *pool = folder_pool;
			folder_pool = NULL;
			thread_pool_destroy (pool);
		}

		g_object_unref (client);
	}

	if (priv->dbus_proxy) {
		g_object_unref (priv->dbus_proxy);
	}

	if (priv->connection) {
		dbus_g_connection_unref (priv->connection);
	}

	G_OBJECT_CLASS (tracker_evolution_plugin_parent_class)->finalize (plugin);
}


static void
tracker_evolution_plugin_class_init (TrackerEvolutionPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);

	miner_class->started = miner_started;
	miner_class->stopped = miner_stopped;
	miner_class->paused  = miner_paused;
	miner_class->resumed = miner_resumed;

	object_class->finalize = tracker_evolution_plugin_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerEvolutionPluginPrivate));
}

static void
tracker_evolution_plugin_init (TrackerEvolutionPlugin *plugin)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (plugin);
	EIterator *it;
	GError *error = NULL;

	priv->client = NULL;
	priv->last_time = 0;
	priv->resuming = FALSE;
	priv->paused = FALSE;

	priv->cached_folders = NULL;
	priv->registered_folders = NULL;
	priv->registered_stores = NULL;
	priv->registered_clients = NULL;

	priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (error) {
		goto error_handler;
	}

	priv->dbus_proxy = dbus_g_proxy_new_for_name (priv->connection,
	                                              DBUS_SERVICE_DBUS,
	                                              DBUS_PATH_DBUS,
	                                              DBUS_INTERFACE_DBUS);

	dbus_g_proxy_add_signal (priv->dbus_proxy, "NameOwnerChanged",
	                         G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
	                         G_TYPE_INVALID);

#ifdef HAVE_EDS_2_29_1
	priv->accounts = g_object_ref (e_get_account_list ());
#else
	priv->accounts = g_object_ref (mail_config_get_accounts ());
#endif

	for (it = e_list_get_iterator (E_LIST (priv->accounts)); e_iterator_is_valid (it); e_iterator_next (it)) {
		register_account (plugin, (EAccount *) e_iterator_get (it));
	}

	g_object_unref (it);

	g_signal_connect (priv->accounts, "account-added",
	                  G_CALLBACK (on_account_added), plugin);
	g_signal_connect (priv->accounts, "account-removed",
	                  G_CALLBACK (on_account_removed), plugin);
	g_signal_connect (priv->accounts, "account-changed",
	                  G_CALLBACK (on_account_changed), plugin);
 error_handler:

	if (error) {
		g_warning ("Could not setup DBus for Tracker plugin, %s\n", error->message);
		g_signal_emit_by_name (plugin, "error");
		g_error_free (error);
	}
}

static void
listnames_fini (gpointer data)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (data);

	dbus_g_proxy_connect_signal (priv->dbus_proxy, "NameOwnerChanged",
	                             G_CALLBACK (name_owner_changed_cb),
	                             data,
	                             NULL);

	g_object_unref (data);
}

static void
miner_started (TrackerMiner *miner)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (miner);

	if (!priv->client) {
		priv->client = tracker_client_new (0, G_MAXINT);
	}

	dbus_g_proxy_begin_call (priv->dbus_proxy, "ListNames",
	                         list_names_reply_cb,
	                         g_object_ref (miner),
	                         listnames_fini,
	                         G_TYPE_INVALID,
	                         G_TYPE_INVALID);

	g_object_set (miner,  "progress", 0.0,  "status", "Initializing", NULL);
}

static void
miner_stopped (TrackerMiner *miner)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (miner);
	miner_paused (miner);
	priv->paused = FALSE;
}

static void
miner_paused (TrackerMiner *miner)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (miner);

	/* We don't really pause, we just completely stop */

	dbus_g_proxy_disconnect_signal (priv->dbus_proxy, "NameOwnerChanged",
	                                G_CALLBACK (name_owner_changed_cb),
	                                miner);

	priv->paused = TRUE;
	priv->last_time = 0;

	if (priv->client) {
		TrackerClient *client = priv->client;

		priv->client = NULL;

		if (sparql_pool) {
			ThreadPool *pool = sparql_pool;
			sparql_pool = NULL;
			thread_pool_destroy (pool);
		}

		if (folder_pool) {
			ThreadPool *pool = folder_pool;
			folder_pool = NULL;
			thread_pool_destroy (pool);
		}

		g_object_unref (client);

		/* By setting this to NULL, events will still be catched by our
		 * handlers, but the send_sparql_* calls will just ignore it.
		 * This is fine as a solution (at least for now). It allows us
		 * to avoid having to unregister everything and risk the chance
		 * of missing something (like a folder or account creation). */
	}
}

static gboolean
unset_resuming (gpointer data)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (data);
	priv->resuming = FALSE;
	g_object_unref (data);
	return FALSE;
}

static void
resuming_fini (gpointer data)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (data);

	g_timeout_add_seconds (1, unset_resuming, g_object_ref (data));

	dbus_g_proxy_connect_signal (priv->dbus_proxy, "NameOwnerChanged",
	                             G_CALLBACK (name_owner_changed_cb),
	                             data,
	                             NULL);

	g_object_unref (data);
}

static void
miner_resumed (TrackerMiner *miner)
{
	TrackerEvolutionPluginPrivate *priv = TRACKER_EVOLUTION_PLUGIN_GET_PRIVATE (miner);

	/* We don't really resume, we just completely restart */

	priv->resuming = TRUE;
	priv->paused = FALSE;
	priv->total_popped = 0;
	priv->of_total = 0;

	if (!priv->client) {
		priv->client = tracker_client_new (0, G_MAXINT);
	}

	g_object_set (miner,  "progress", 0.0,  "status", _("Processing…"), NULL);

	dbus_g_proxy_begin_call (priv->dbus_proxy, "ListNames",
	                         list_names_reply_cb,
	                         g_object_ref (miner),
	                         resuming_fini,
	                         G_TYPE_INVALID,
	                         G_TYPE_INVALID);
}
