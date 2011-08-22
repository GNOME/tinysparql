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

#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <sqlite3.h>

#include <camel/camel.h>

#include <mail/mail-config.h>
#include <mail/em-utils.h>
#include <mail/mail-ops.h>

#ifdef EVOLUTION_SHELL_3_2
#include <mail/e-mail.h>
#endif

#ifdef EVOLUTION_SHELL_2_91
#include <mail/e-mail-session.h>
#else
#include <mail/mail-session.h>
#endif

#include <mail/e-mail-backend.h>
#include <shell/e-shell.h>

#include <e-util/e-config.h>
#include <e-util/e-account-utils.h>

#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>

#include <libtracker-sparql/tracker-sparql.h>

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

#define TRACKER_SERVICE                 "org.freedesktop.Tracker1"
#define DATASOURCE_URN                  "urn:nepomuk:datasource:1cb1eb90-1241-11de-8c30-0800200c9a66"
#define TRACKER_EVOLUTION_GRAPH_URN     "urn:uuid:9a96d750-5182-11e0-b8af-0800200c9a66"

#define UIDS_CHUNK_SIZE 200

#define TRACKER_MINER_EVOLUTION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_MINER_EVOLUTION, TrackerMinerEvolutionPrivate))

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
	TrackerSparqlConnection *connection;
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
	TrackerMinerEvolution *self; /* weak */
	guint64 last_checkout;
} ClientRegistry;

typedef struct {
	TrackerMinerEvolution *self;
	gchar *account_uri;
	guint hook_id;
} OnSummaryChangedInfo;

typedef struct {
	OnSummaryChangedInfo *hook_info;
	CamelFolder *folder;
} FolderRegistry;

typedef struct {
	EAccount *account;
	TrackerMinerEvolution *self;
	guint hook_id;
	CamelStore *store;
} StoreRegistry;

typedef struct {
	TrackerMinerEvolution *self;
	gchar *account_uri;
	ClientRegistry *info;
} IntroductionInfo;

typedef struct {
	TrackerMinerEvolution *self;
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
	time_t last_time;
	gboolean resuming, paused;
	guint total_popped, of_total;
	guint watch_name_id;
	GCancellable *sparql_cancel;
	GTimer *timer_since_stopped;
} TrackerMinerEvolutionPrivate;

typedef struct {
	IntroductionInfo *intro_info;
	CamelStore *store;
	CamelFolderInfo *iter;
} TryAgainInfo;

typedef struct {
	TrackerMinerEvolution *self;
	gchar *account_uri;
	CamelFolderInfo *iter;
} GetFolderInfo;

typedef struct {
	IntroductionInfo *intro_info;
	CamelFolderInfo *iter;
	CamelStore *store;
} WorkerThreadinfo;

/* Prototype declarations */
int             e_plugin_lib_enable                 (EPlugin                *ep,
                                                     int                     enable);

static void     register_account                    (TrackerMinerEvolution  *self,
                                                     EAccount               *account);
static void     unregister_account                  (TrackerMinerEvolution  *self,
                                                     EAccount               *account);
static void     miner_evolution_initable_iface_init (GInitableIface         *iface);
static gboolean miner_evolution_initable_init       (GInitable              *initable,
                                                     GCancellable           *cancellable,
                                                     GError                **error);
static void     miner_started                       (TrackerMiner           *miner);
static void     miner_stopped                       (TrackerMiner           *miner);
static void     miner_paused                        (TrackerMiner           *miner);
static void     miner_resumed                       (TrackerMiner           *miner);
static void     miner_start_watching                (TrackerMiner           *miner);
static void     miner_stop_watching                 (TrackerMiner           *miner);

static GInitableIface *miner_evolution_initable_parent_iface;
static TrackerMinerEvolution *manager = NULL;

static GStaticRecMutex glock = G_STATIC_REC_MUTEX_INIT;
static guint register_count = 0;
static guint walk_count = 0;
static ThreadPool *folder_pool = NULL;

#ifdef EVOLUTION_SHELL_2_91
static EMailSession *session = NULL;
#endif

G_DEFINE_TYPE_WITH_CODE (TrackerMinerEvolution, tracker_miner_evolution, TRACKER_TYPE_MINER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                miner_evolution_initable_iface_init));

/* First a bunch of helper functions. */
static void
get_email_and_fullname (const gchar  *line,
                        gchar       **email,
                        gchar       **fullname)
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
	g_signal_handler_disconnect (registry->folder, registry->hook_info->hook_id);
	g_object_unref (registry->folder);
	g_free (registry->hook_info->account_uri);
	g_slice_free (OnSummaryChangedInfo, registry->hook_info);
	g_slice_free (FolderRegistry, registry);
}

static FolderRegistry*
folder_registry_new (const gchar           *account_uri,
                     CamelFolder           *folder,
                     TrackerMinerEvolution *self)
{
	FolderRegistry *registry = g_slice_new (FolderRegistry);

	registry->hook_info = g_slice_new (OnSummaryChangedInfo);
	registry->hook_info->account_uri = g_strdup (account_uri);
	registry->hook_info->self = self; /* weak */
	registry->hook_info->hook_id = 0;
	g_object_ref (folder);
	registry->folder = folder;

	return registry;
}

static void
thread_pool_exec (gpointer data,
                  gpointer user_data)
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

static ThreadPool*
thread_pool_new (GFunc            func,
                 GFunc            freeup,
                 GCompareDataFunc sorter)
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
thread_pool_push (ThreadPool *pool,
                  gpointer    item,
                  gpointer    user_data)
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
send_sparql_update (TrackerMinerEvolution *self,
                    const gchar           *sparql,
                    gint                   prio)
{
	TrackerMinerEvolutionPrivate *priv;

	/* FIXME: prio is unused */

	g_static_rec_mutex_lock (&glock);

	priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (self);

	if (!priv->timer_since_stopped || g_timer_elapsed (priv->timer_since_stopped, NULL) > 5) {
		TrackerSparqlConnection *connection;

		connection = tracker_miner_get_connection (TRACKER_MINER (self));

		if (connection) {
			tracker_sparql_connection_update (connection,
			                                  sparql,
			                                  G_PRIORITY_DEFAULT,
			                                  priv->sparql_cancel,
			                                  NULL);
		}
	}

	g_static_rec_mutex_unlock (&glock);
}

static void
send_sparql_commit (TrackerMinerEvolution *self,
                    gboolean               update)
{
	if (update) {
		gchar *date_s = tracker_date_to_string (time (NULL));

		/* TODO: We should probably do this per folder instead of a datasource
		 * for the entire Evolution store. This way if the user interrupts
		 * the synchronization, then at least the folders that are already
		 * finished don't have to be repeated next time. Right now an interrupt
		 * means starting over from scratch. */

		gchar *update = g_strdup_printf ("DELETE { <" DATASOURCE_URN "> nie:contentLastModified ?d } "
		                                 "WHERE { <"  DATASOURCE_URN "> a nie:InformationElement ; nie:contentLastModified ?d } \n"
		                                 "INSERT { <" DATASOURCE_URN "> a nie:InformationElement ; nie:contentLastModified \"%s\" }",
		                                 date_s);

		send_sparql_update (self, update, 0);

		g_free (update);
		g_free (date_s);
	}
}

static void
add_contact (TrackerSparqlBuilder *sparql,
             const gchar          *predicate,
             const gchar          *uri,
             const gchar          *value)
{
	gchar *email = NULL;
	gchar *fullname = NULL;
	gchar *email_uri;

	get_email_and_fullname (value, &email, &fullname);

	email_uri = g_strdup_printf ("mailto:%s", email);

	tracker_sparql_builder_subject_iri (sparql, email_uri);
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nco:EmailAddress");

	tracker_sparql_builder_subject_iri (sparql, email_uri);
	tracker_sparql_builder_predicate (sparql, "nco:emailAddress");
	tracker_sparql_builder_object_string (sparql, email);

	tracker_sparql_builder_subject_iri (sparql, uri);
	tracker_sparql_builder_predicate (sparql, predicate);

	tracker_sparql_builder_object_blank_open (sparql);

	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nco:Contact");

	if (fullname) {
		tracker_sparql_builder_predicate (sparql, "nco:fullname");
		tracker_sparql_builder_object_string (sparql, fullname);
	}

	tracker_sparql_builder_predicate (sparql, "nco:hasEmailAddress");
	tracker_sparql_builder_object_iri (sparql, email_uri);

	tracker_sparql_builder_object_blank_close (sparql);
	g_free (email_uri);

	g_free (email);
	g_free (fullname);
}

static void
process_fields (TrackerSparqlBuilder *sparql,
                const gchar          *uid,
                guint                 flags,
                time_t                sent,
                const gchar          *subject,
                const gchar          *from,
                const gchar          *to,
                const gchar          *cc,
                const gchar          *size,
                CamelFolder          *folder,
                const gchar          *uri)
{
	gchar **arr;
	guint i;

	tracker_sparql_builder_subject_iri (sparql, DATASOURCE_URN);
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nie:DataSource");

	/* for contentLastModified */
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nie:InformationElement");

	tracker_sparql_builder_subject_iri (sparql, uri);
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nmo:Email");

	tracker_sparql_builder_predicate (sparql, "a");
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
}

static gchar *
message_uri_build (CamelFolder *folder,
                   const gchar *uid)
{
#ifdef EVOLUTION_SHELL_3_2
	gchar *uri, *folder_uri;

	folder_uri = e_mail_folder_uri_from_folder (folder);
	uri = g_strdup_printf ("%s#%s", folder_uri, uid);
	g_free (folder_uri);

	return uri;
#else
	CamelURL *a_url, url;
	const gchar *path;
	gchar *uri, *qry, *ppath;

	a_url = CAMEL_SERVICE (camel_folder_get_parent_store (folder))->url;
	path = camel_folder_get_full_name (folder);

	ppath = g_strdup_printf ("/%s", path);

	/* This converts a CamelURL plus path and uid components to a Evolution
	 * compatible URL. Evolution has its own strange URL format, so .. ok */

	url = camel_url_copy (a_url);

	/* This would be the right way, but em_uri_from_camel ignores ?uid=x parts,
	 * so instead we append it manually with a g_strdup_printf lower
	 *
	 * qry = g_strdup_printf ("uid=%s", uid);
	 * camel_url_set_query (url, qry);
	 * g_free (qry); */

	camel_url_set_path (url, ppath);
	g_free (ppath);
	uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);

	qry = em_uri_from_camel (uri);
	g_free (uri);
	uri = g_strdup_printf ("%s?uid=%s", qry, uid);
	g_free (qry);
	camel_url_free (url);

	return uri;
#endif
}

/* When new messages arrive to- or got deleted from the summary, called in
 * mainloop or by a thread (unknown, depends on Camel and Evolution code that
 * executes the reason why this signal gets emitted).
 *
 * This one is the reason why we registered all those folders during init below.
 */
static void
on_folder_summary_changed (CamelFolder           *folder,
                           CamelFolderChangeInfo *changes,
                           gpointer               user_data)
{
	OnSummaryChangedInfo *info = user_data;
	CamelFolderSummary *summary;
	GPtrArray *merged;
	gboolean did_work;
	guint i;

	if (!folder)
		return;

	summary = folder->summary;

	merged = g_ptr_array_new ();
	did_work = FALSE;

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
		time_t sent;
		guint flags;
		CamelMessageInfo *linfo;
		const CamelTag *ctags;
		const CamelFlag *cflags;
		gchar *full_sparql;

		linfo = camel_folder_summary_uid (summary, merged->pdata[i]);

		if (linfo) {
			uid = (gchar *) camel_message_info_uid (linfo);
		}

		if (linfo && uid) {
			gchar *uri;
			gchar *size;
			gchar *str;
			const gchar *folder_name;
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

			uri = message_uri_build (folder, uid);

			sparql = tracker_sparql_builder_new_update ();

			tracker_sparql_builder_insert_silent_open (sparql, NULL);
			tracker_sparql_builder_graph_open (sparql, TRACKER_EVOLUTION_GRAPH_URN);

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

			tracker_sparql_builder_graph_close (sparql);
			tracker_sparql_builder_insert_close (sparql);

			full_sparql = g_strdup_printf ("DELETE {"
			                               "  GRAPH <%s> {"
			                               "    <%s> ?p ?o"
			                               "  } "
			                               "} "
			                               "WHERE {"
			                               "  GRAPH <%s> {"
			                               "    <%s> ?p ?o"
			                               "    FILTER (?p != rdf:type && ?p != nie:contentCreated)"
			                               "  } "
			                               "} "
			                               "%s",
			                               TRACKER_EVOLUTION_GRAPH_URN,
			                               uri,
			                               TRACKER_EVOLUTION_GRAPH_URN,
			                               uri,
			                               tracker_sparql_builder_get_result (sparql));

			send_sparql_update (info->self, full_sparql, 100);

			g_free (full_sparql);

			/* FIXME: Actually report accurate percentages and don't spam */
			g_debug ("Tracker plugin setting progress to '%2.2f' and status to 'Updating an E-mail'",
			         (gdouble) i / merged->len);

#ifdef EVOLUTION_SHELL_3_2
			folder_name = camel_folder_get_display_name (folder);
#else
			folder_name = camel_folder_get_name (folder);
#endif

			if (folder_name && *folder_name) {
				str = g_strdup_printf ("Updating E-mails for %s",
				                       folder_name);
			} else {
				str = g_strdup ("Updating E-mails");
			}

			g_object_set (info->self, "progress",
			              (gdouble) i / merged->len,
			              "status", str,
			              NULL);

			g_free (str);
			g_object_unref (sparql);

			g_free (size);
			g_free (uri);
		}

		if (linfo)
			camel_message_info_free (linfo);
	}

	/* Set flag if we did work here */
	did_work = merged->len > 0;

	g_ptr_array_free (merged, TRUE);

	/* the uid_removed member contains the removed-from-the-summary items */

	if (changes->uid_removed && changes->uid_removed->len > 0) {
		/* The FROM uri is not exactly right here, but we just want
		 * graph != NULL in tracker-store/tracker-writeback.c */
		GString *sparql = g_string_new ("");

		for (i = 0; i < changes->uid_removed->len; i++) {
			gchar *uri;

			g_object_set (info->self,
			              "progress", (gdouble) i / changes->uid_removed->len,
			              "status", "Cleaning up deleted E-mails",
			              NULL);

			/* This is not a path but a URI, don't use the OS's
			 * directory separator here */
			uri = message_uri_build (folder, (char*) changes->uid_removed->pdata[i]);

			g_string_append_printf (sparql, "DELETE FROM <%s> { <%s> a rdfs:Resource }\n ", uri, uri);
			g_free (uri);
		}

		send_sparql_update (info->self, sparql->str, 100);
		g_string_free (sparql, TRUE);

		/* Set flag if we did work here */
		did_work = TRUE;
	}

	send_sparql_commit (info->self, FALSE);

	if (did_work) {
		g_debug ("Tracker plugin setting progress to '1.0' and status to 'Idle'");
		g_object_set (info->self, "progress", 1.0, "status", "Idle", NULL);
	}
}

static gchar *
uids_to_chunk (GPtrArray *uids,
               guint      cur,
               guint      max)
{
	guint i;
	GString *str = g_string_new ("");

	for (i = 0; i < max && i < uids->len; i++) {
		if (i != 0) {
			g_string_append (str, ", ");
		}
		g_string_append (str, g_ptr_array_index (uids, i));
	}

	return g_string_free (str, FALSE);
}

/* Initial upload of more recent than last_checkout items, called in the mainloop */
static void
introduce_walk_folders_in_folder (TrackerMinerEvolution *self,
                                  CamelFolderInfo       *iter,
                                  CamelStore            *store,
                                  gchar                 *account_uri,
                                  ClientRegistry        *info,
                                  GCancellable          *cancel)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (self);
	CamelDB *cdb_r;
	gboolean did_work;

	if (g_cancellable_is_cancelled (cancel)) {
		return;
	}

	cdb_r = camel_db_clone (store->cdb_r, NULL);
	did_work = FALSE;

	while (iter) {
		guint uids_i;
		guint count = 0;
		guint ret = SQLITE_OK;
		gchar *query, *status;
		sqlite3_stmt *stmt = NULL;
		GPtrArray *uids = g_ptr_array_new_with_free_func (g_free);

		did_work = TRUE;

		query = sqlite3_mprintf ("SELECT uid FROM %Q "
		                         "WHERE modified > %"G_GUINT64_FORMAT,
		                         iter->full_name,
		                         info->last_checkout);

#ifdef EVOLUTION_SHELL_3_2
		status = g_strdup_printf ("Processing folder %s", iter->display_name);
#else
		status = g_strdup_printf ("Processing folder %s", iter->name);
#endif
		g_object_set (self,  "progress", 0.01, "status", status, NULL);

		ret = sqlite3_prepare_v2 (cdb_r->db, query, -1, &stmt, NULL);
		while (ret == SQLITE_OK || ret == SQLITE_BUSY || ret == SQLITE_ROW) {
			gchar *uid;

			if (g_cancellable_is_cancelled (cancel))
				break;
			ret = sqlite3_step (stmt);
			if (ret == SQLITE_BUSY) {
				usleep (10);
				continue;
			}
			if ((ret != SQLITE_OK && ret != SQLITE_ROW) || ret == SQLITE_DONE)
				break;

			uid = (gchar *) sqlite3_column_text (stmt, 0);

			if (uid) {
				g_ptr_array_add (uids, g_strdup (uid));
			}
		}

		sqlite3_finalize (stmt);
		sqlite3_free (query);

		for (uids_i = 0; uids_i < uids->len; uids_i += UIDS_CHUNK_SIZE) {
			gchar *uids_chunk = uids_to_chunk (uids, uids_i, UIDS_CHUNK_SIZE);

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

			query = sqlite3_mprintf ("SELECT uid, flags, read, deleted, "     /* 0  - 3  */
			                         "replied, important, junk, attachment, " /* 4  - 7  */
			                         "size, dsent, dreceived, subject, "      /* 8  - 11 */
			                         "mail_from, mail_to, mail_cc, mlist, "   /* 12 - 15 */
			                         "labels, usertags "                      /* 16 - 17 */
			                         "FROM %Q "
			                         "WHERE modified > %"G_GUINT64_FORMAT" "
			                         "AND uid IN (%s)",
			                         iter->full_name,
			                         info->last_checkout,
			                         uids_chunk);

			g_free (uids_chunk);

			ret = sqlite3_prepare_v2 (cdb_r->db, query, -1, &stmt, NULL);

			while (ret == SQLITE_OK || ret == SQLITE_BUSY || ret == SQLITE_ROW) {
				TrackerSparqlBuilder *sparql = NULL;
				gchar *subject, *to, *from, *cc, *uid, *size;
				time_t sent;
				gchar *part, *label, *p;
				guint flags;

				if (g_cancellable_is_cancelled (cancel))
					break;
				ret = sqlite3_step (stmt);
				if (ret == SQLITE_BUSY) {
					usleep (10);
					continue;
				}
				if ((ret != SQLITE_OK && ret != SQLITE_ROW) || ret == SQLITE_DONE)
					break;

				uid = (gchar *) sqlite3_column_text (stmt, 0);

				if (uid) {
					CamelFolder *folder;
					guint max = 0, j;
					gchar *uri;
					gboolean opened = FALSE;
					gchar *full_sparql;

					flags =   (guint  ) sqlite3_column_int  (stmt, 1);
					size =    (gchar *) sqlite3_column_text (stmt, 8);
					sent =    (time_t)  sqlite3_column_int64 (stmt, 9);
					subject = (gchar *) sqlite3_column_text (stmt, 11);
					from =    (gchar *) sqlite3_column_text (stmt, 12);
					to =      (gchar *) sqlite3_column_text (stmt, 13);
					cc =      (gchar *) sqlite3_column_text (stmt, 14);

					folder = g_hash_table_lookup (priv->cached_folders, iter->full_name);

					uri = message_uri_build (folder, uid);

					if (!sparql) {
						sparql = tracker_sparql_builder_new_update ();
					}

					tracker_sparql_builder_insert_silent_open (sparql, NULL);
					tracker_sparql_builder_graph_open (sparql, TRACKER_EVOLUTION_GRAPH_URN);

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

					g_free (p);

					tracker_sparql_builder_graph_close (sparql);
					tracker_sparql_builder_insert_close (sparql);

					full_sparql = g_strdup_printf ("DELETE {"
					                               "  GRAPH <%s> {"
					                               "    <%s> ?p ?o"
					                               "  } "
					                               "} "
					                               "WHERE {"
					                               "  GRAPH <%s> {"
					                               "    <%s> ?p ?o"
					                               "    FILTER (?p != rdf:type && ?p != nie:contentCreated)"
					                               "  } "
					                               "} "
					                               "%s",
					                               TRACKER_EVOLUTION_GRAPH_URN,
					                               uri,
					                               TRACKER_EVOLUTION_GRAPH_URN,
					                               uri,
					                               tracker_sparql_builder_get_result (sparql));

					g_free (uri);
					count++;
					send_sparql_update (self, full_sparql, 0);
					g_free (full_sparql);
					g_object_unref (sparql);
				}
			}

			g_debug ("Tracker plugin setting progress to '%f' and status to '%s'",
			         ((gdouble) uids_i / (gdouble) uids->len),
			         status);
			g_object_set (self, "progress",
			              ((gdouble) uids_i / (gdouble) uids->len),
			              "status", status,
			              NULL);

			sqlite3_finalize (stmt);
			sqlite3_free (query);
		}

		send_sparql_commit (self, FALSE);

		if (iter->child) {
			introduce_walk_folders_in_folder (self, iter->child,
			                                  store,
			                                  account_uri, info,
			                                  cancel);
		}

		iter = iter->next;
		g_ptr_array_unref (uids);
		g_free (status);
	}

	if (did_work) {
		g_debug ("Tracker plugin setting progress to '1.0' and status to 'Idle'");
		g_object_set (self, "progress", 1.0, "status", "Idle", NULL);
	}

	camel_db_close (cdb_r);
}

/* Initial notify of deletes that are more recent than last_checkout, called in
 * the mainloop */

static void
introduce_store_deal_with_deleted (TrackerMinerEvolution *self,
                                   CamelStore            *store,
                                   char                  *account_uri,
                                   gpointer               user_data)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (self);
	ClientRegistry *info = user_data;
	gboolean more = TRUE;
	gchar *query;
	sqlite3_stmt *stmt = NULL;
	CamelDB *cdb_r;
	guint i, ret;

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
			CamelFolder *folder;
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

			folder = g_hash_table_lookup (priv->cached_folders, mailbox);


			/* This is not a path but a URI, don't use the OS's
			 * directory separator here */

			g_ptr_array_add (subjects_a,
			                 message_uri_build (folder, uid));

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
}

/* Get the oldest date in all of the deleted-tables, called in the mainloop. We
 * need this to test whether we should use Cleanup() or not. */

static guint64
get_last_deleted_time (TrackerMinerEvolution *self)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (self);
	guint64 smallest = (guint64) time (NULL);

	if (priv->accounts) {
		EIterator *it;

		for (it = e_list_get_iterator (E_LIST (priv->accounts)); e_iterator_is_valid (it); e_iterator_next (it)) {
			EAccount *account = (EAccount *) e_iterator_get (it);
			CamelProvider *provider;
			CamelStore *store;
			char *uri;
			CamelDB *cdb_r;
			sqlite3_stmt *stmt = NULL;
			gchar *query;
			guint ret = SQLITE_OK;
			guint64 latest = smallest;

			if (!account->enabled || !(uri = account->source->url))
				continue;

			if (!(provider = camel_provider_get(uri, NULL))) {
				continue;
			}

			if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE)) {
				continue;
			}

			if (!(store = (CamelStore *) camel_session_get_service (CAMEL_SESSION (session),
#ifdef EVOLUTION_SHELL_3_2
			                                                        account->uid))) {
#else
			                                                        uri,
			                                                        CAMEL_PROVIDER_STORE,
			                                                        NULL))) {
#endif
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

#ifdef EVOLUTION_SHELL_3_2
static void
register_on_get_folder (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
	CamelFolder *folder = camel_store_get_folder_finish (CAMEL_STORE (source_object), res, NULL);
#else
static void
register_on_get_folder (gchar       *uri,
                        CamelFolder *folder,
                        gpointer     user_data)
{
#endif
	GetFolderInfo *info = user_data;
	gchar *account_uri = info->account_uri;
	CamelFolderInfo *iter = info->iter;
	TrackerMinerEvolution *self = info->self;
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (self);
	guint hook_id;
	FolderRegistry *registry;

	if (!folder) {
		goto fail_register;
	}

	registry = folder_registry_new (account_uri, folder, self);

	if (!priv->registered_folders || !priv->cached_folders) {
		goto not_ready;
	}

	hook_id = g_signal_connect (folder, "changed",
	                            G_CALLBACK (on_folder_summary_changed),
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
register_walk_folders_in_folder (TrackerMinerEvolution *self,
                                 CamelFolderInfo       *iter,
                                 CamelStore            *store,
                                 gchar                 *account_uri)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (self);

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

#ifdef EVOLUTION_SHELL_3_2
		camel_store_get_folder (store,
		                        iter->full_name,
		                        0,
		                        0,
		                        NULL,
		                        register_on_get_folder,
		                        info);
#else
		mail_get_folder (
#ifdef EVOLUTION_SHELL_2_91
		                 session,
#endif
		                 iter->uri,
		                 0,
		                 register_on_get_folder,
		                 info,
		                 mail_msg_unordered_push);
#endif

		if (iter->child) {
			register_walk_folders_in_folder (self, iter->child,
			                                 store,
			                                 account_uri);
		}

		iter = iter->next;
	}
}

#ifdef EVOLUTION_SHELL_3_2
static void
unregister_on_get_folder (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
	CamelFolder *folder = camel_store_get_folder_finish (CAMEL_STORE (source_object), res, NULL);
#else
static void
unregister_on_get_folder (gchar       *uri,
                          CamelFolder *folder,
                          gpointer     user_data)
{
#endif
	GetFolderInfo *info = user_data;
	CamelFolderInfo *titer = info->iter;
	TrackerMinerEvolution *self = info->self;
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (self);
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
unregister_walk_folders_in_folder (TrackerMinerEvolution *self,
                                   CamelFolderInfo       *titer,
                                   CamelStore            *store,
                                   gchar                 *account_uri)
{
	/* Recursively walks all the folders in store */

	while (titer) {
		GetFolderInfo *info = g_new0 (GetFolderInfo, 1);

		info->self = g_object_ref (self);
		info->account_uri = g_strdup (account_uri);
		info->iter = camel_folder_info_clone (titer);

		/* This is asynchronous and hooked to the mail/ API, so nicely
		 * integrated with the Evolution UI application */

#ifdef EVOLUTION_SHELL_3_2
		camel_store_get_folder (store,
		                        titer->full_name,
		                        0,
		                        0,
		                        NULL,
		                        unregister_on_get_folder,
		                        info);
#else
		mail_get_folder (
#ifdef EVOLUTION_SHELL_2_91
		                 session,
#endif
		                 titer->uri,
		                 0,
		                 unregister_on_get_folder,
		                 info,
		                 mail_msg_unordered_push);
#endif

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

static void
free_introduction_info (IntroductionInfo *intro_info)
{
	client_registry_info_free (intro_info->info);
	g_free (intro_info->account_uri);
	g_object_unref (intro_info->self);
	g_free (intro_info);
}

static void
free_worker_thread_info (gpointer data,
                         gpointer user_data)
{
	WorkerThreadinfo *winfo = data;

	/* Ownership was transfered to us in try_again */
	free_introduction_info (winfo->intro_info);
	g_object_unref (winfo->store);
	camel_folder_info_free (winfo->iter);
	g_free (winfo);
}

static void
folder_worker (gpointer data,
               gpointer user_data)
{
	WorkerThreadinfo *winfo = data;

	introduce_walk_folders_in_folder (winfo->intro_info->self,
	                                  winfo->iter,
	                                  winfo->store,
	                                  winfo->intro_info->account_uri,
	                                  winfo->intro_info->info,
	                                  user_data);

	send_sparql_commit (winfo->intro_info->self, TRUE);

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

		if (!folder_pool)
			folder_pool = thread_pool_new (folder_worker, free_worker_thread_info, NULL);

		thread_pool_push (folder_pool, winfo, NULL);

		return FALSE;
	}

	return TRUE;
}

#ifdef EVOLUTION_SHELL_3_2
static void
on_got_folderinfo_introduce (GObject      *source_object,
                             GAsyncResult *res,
                             gpointer      data)
{
	CamelStore *store = CAMEL_STORE (source_object);
	CamelFolderInfo *iter = camel_store_get_folder_info_finish (store, res, NULL);
#else
static gboolean
on_got_folderinfo_introduce (CamelStore      *store,
                             CamelFolderInfo *iter,
                             void            *data)
{
#endif
	TryAgainInfo *info = g_new0 (TryAgainInfo, 1);

	/* Ownership of these is transfered in try_again */

	g_object_ref (store);
	info->store = store;
	/* This apparently creates a thread */
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

#ifdef EVOLUTION_SHELL_3_2
	camel_store_free_folder_info (store, iter);
#else
	return TRUE;
#endif
}

static void
introduce_account_to (TrackerMinerEvolution *self,
                      EAccount              *account,
                      ClientRegistry        *info)
{
	CamelProvider *provider;
	CamelStore *store;
	char *uri, *account_uri, *ptr;
	IntroductionInfo *intro_info;

	if (!account->enabled || !(uri = account->source->url))
		return;

	if (!(provider = camel_provider_get(uri, NULL))) {
		return;
	}

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;

	if (!(store = (CamelStore *) camel_session_get_service (CAMEL_SESSION (session),
#ifdef EVOLUTION_SHELL_3_2
	                                                        account->uid))) {
#else
	                                                        uri,
	                                                        CAMEL_PROVIDER_STORE,
	                                                        NULL))) {
#endif
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

#ifdef EVOLUTION_SHELL_3_2
	camel_store_get_folder_info (store,
	                             NULL,
	                             CAMEL_STORE_FOLDER_INFO_FAST |
	                             CAMEL_STORE_FOLDER_INFO_RECURSIVE |
	                             CAMEL_STORE_FOLDER_INFO_SUBSCRIBED,
	                             G_PRIORITY_DEFAULT,
	                             NULL,
	                             on_got_folderinfo_introduce,
	                             intro_info);
#else
	mail_get_folderinfo (store, NULL, on_got_folderinfo_introduce, intro_info);
#endif

	g_object_unref (store);

}

static void
introduce_account_to_all (TrackerMinerEvolution *self,
                          EAccount              *account)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (self);
	GList *copy = priv->registered_clients;

	while (copy) {
		ClientRegistry *info = copy->data;
		introduce_account_to (self, account, info);
		copy = g_list_next (copy);
	}

}

static void
introduce_accounts_to (TrackerMinerEvolution *self,
                       ClientRegistry        *info)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (self);
	EIterator *it;

	for (it = e_list_get_iterator (E_LIST (priv->accounts)); e_iterator_is_valid (it); e_iterator_next (it)) {
		introduce_account_to (self, (EAccount *) e_iterator_get (it), info);
	}

	g_object_unref (it);
}

static void
register_client_second_half (ClientRegistry *info)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (info->self);
	guint64 too_old = get_last_deleted_time (info->self);

	/* If registrar's modseq is too old, send Cleanup (). This means that
	 * we tell it to start over (it must invalidate what it has). */

	if (info->last_checkout < too_old) {
		send_sparql_update (info->self, "DELETE { ?s a rdfs:Resource } "
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
on_register_client_qry (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
	TrackerSparqlCursor *cursor;
	ClientRegistry *info = user_data;
	TrackerMinerEvolutionPrivate *priv;
	GError *error = NULL;

	priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (info->self);
	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (source_object),
	                                                 res, &error);

	if (error) {
		g_warning ("%s\n", error->message);
		g_error_free (error);
		g_slice_free (ClientRegistry, info);
		if (cursor) {
			g_object_unref (cursor);
		}
		return;
	}

	if (!tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		if (priv->resuming) {
			info->last_checkout = priv->last_time;
		} else if (priv->resuming && priv->last_time != 0) {
			info->last_checkout = priv->last_time;
		} else {
			info->last_checkout = 0;
		}
	} else {
		do {
			const gchar *str = tracker_sparql_cursor_get_string (cursor, 0, NULL);
			GError *new_error = NULL;

			info->last_checkout = (guint64) tracker_string_to_date (str, NULL, &new_error);

			if (new_error) {
				g_warning ("%s", new_error->message);
				g_error_free (error);
				g_object_unref (cursor);
				return;
			}

			break;
		} while (tracker_sparql_cursor_next (cursor, NULL, NULL));
	}

	register_client_second_half (info);

	g_object_unref (cursor);
}

static void
register_client (TrackerMinerEvolution *self)
{
	TrackerMinerEvolutionPrivate *priv;
	TrackerSparqlConnection *connection;
	ClientRegistry *info;
	const gchar *query;

	priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (self);

	connection = tracker_miner_get_connection (TRACKER_MINER (self));
	if (!connection) {
		return;
	}

	info = g_slice_new0 (ClientRegistry);
	info->self = self; /* weak */

	priv->total_popped = 0;
	priv->of_total = 0;

	query = "SELECT ?c "
		"WHERE { <" DATASOURCE_URN "> nie:contentLastModified ?c }";

	tracker_sparql_connection_query_async (connection,
	                                       query,
	                                       NULL, /* FIXME: should use a cancellable */
	                                       on_register_client_qry,
	                                       info);
}


static void
on_folder_created (CamelStore    *store,
                   void          *event_data,
                   StoreRegistry *registry)
{
	unregister_account (registry->self, registry->account);
	register_account (registry->self, registry->account);
	introduce_account_to_all (registry->self, registry->account);
}

static void
on_folder_deleted (CamelStore    *store,
                   void          *event_data,
                   StoreRegistry *registry)
{
	unregister_account (registry->self, registry->account);
	register_account (registry->self, registry->account);
	introduce_account_to_all (registry->self, registry->account);
}

static void
on_folder_renamed (CamelStore    *store,
                   gchar         *old_name,
                   StoreRegistry *registry)
{
	unregister_account (registry->self, registry->account);
	register_account (registry->self, registry->account);
	introduce_account_to_all (registry->self, registry->account);
}

static StoreRegistry*
store_registry_new (gpointer               co,
                    EAccount              *account,
                    TrackerMinerEvolution *self)
{
	StoreRegistry *registry = g_slice_new (StoreRegistry);

	registry->store = co;
	registry->account = account; /* weak */
	registry->self = self; /* weak */
	g_object_ref (co);

	return registry;
}

static void
store_registry_free (StoreRegistry *registry)
{
	g_signal_handler_disconnect (registry->store, registry->hook_id);
	g_object_unref (registry->store);
	g_slice_free (StoreRegistry, registry);
}


#ifdef EVOLUTION_SHELL_3_2
static void
on_got_folderinfo_register (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      data)
{
	CamelStore *store = CAMEL_STORE (source_object);
	CamelFolderInfo *iter = camel_store_get_folder_info_finish (store, res, NULL);
#else
static gboolean
on_got_folderinfo_register (CamelStore      *store,
                            CamelFolderInfo *iter,
                            void            *data)
{
#endif
	RegisterInfo *reg_info = data;
	TrackerMinerEvolution *self = reg_info->self;
	TrackerMinerEvolutionPrivate *priv;
	EAccount *account = reg_info->account;
	StoreRegistry *registry;
	gchar *uri = reg_info->uri;
	guint hook_id;

	priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (self);

	/* This is where it all starts for a registrar registering itself */

	if (!priv->registered_stores) {
		priv->registered_stores = g_hash_table_new_full (g_direct_hash, g_direct_equal,
		                                                 (GDestroyNotify) NULL,
		                                                 (GDestroyNotify) store_registry_free);
	}

	/* Hook up catching folder changes in the store */
	registry = store_registry_new (store, account, self);
	hook_id = g_signal_connect (store, "folder-created",
	                            G_CALLBACK (on_folder_created),
	                            registry);
	registry->hook_id = hook_id;
	g_hash_table_replace (priv->registered_stores,
	                      GINT_TO_POINTER (hook_id),
	                      registry);

	registry = store_registry_new (store, account, self);
	hook_id = g_signal_connect (store, "folder-renamed",
	                            G_CALLBACK (on_folder_renamed),
	                            registry);
	registry->hook_id = hook_id;
	g_hash_table_replace (priv->registered_stores,
	                      GINT_TO_POINTER (hook_id),
	                      registry);

	registry = store_registry_new (store, account, self);
	hook_id = g_signal_connect (store, "folder-deleted",
	                            G_CALLBACK (on_folder_deleted),
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

#ifdef EVOLUTION_SHELL_3_2
	camel_store_free_folder_info (store, iter);
#else
	return TRUE;
#endif
}

static void
register_account (TrackerMinerEvolution *self,
                  EAccount              *account)
{
	CamelProvider *provider;
	CamelStore *store;
	char *uri;
	RegisterInfo *reg_info;

	if (!account->enabled || !(uri = account->source->url)) {
		return;
	}

	if (!(provider = camel_provider_get (uri, NULL))) {
		return;
	}

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE)) {
		return;
	}

	if (!(store = (CamelStore *) camel_session_get_service (CAMEL_SESSION (session),
#ifdef EVOLUTION_SHELL_3_2
	                                                        account->uid))) {
#else
	                                                        uri,
	                                                        CAMEL_PROVIDER_STORE,
	                                                        NULL))) {
#endif
		return;
	}

	reg_info = g_new0 (RegisterInfo, 1);

	reg_info->self = g_object_ref (self);
	reg_info->uri = g_strdup (uri);
	reg_info->account = g_object_ref (account);

	walk_count++;

	/* Get the account's folder-info and register it asynchronously */
#ifdef EVOLUTION_SHELL_3_2
	camel_store_get_folder_info (store,
	                             NULL,
	                             CAMEL_STORE_FOLDER_INFO_FAST |
	                             CAMEL_STORE_FOLDER_INFO_RECURSIVE |
	                             CAMEL_STORE_FOLDER_INFO_SUBSCRIBED,
	                             G_PRIORITY_DEFAULT,
	                             NULL,
	                             on_got_folderinfo_register,
	                             reg_info);
#else
	mail_get_folderinfo (store, NULL, on_got_folderinfo_register, reg_info);
#endif

	g_object_unref (store);
}

#ifdef EVOLUTION_SHELL_3_2
static void
on_got_folderinfo_unregister (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      data)
{
	CamelStore *store = CAMEL_STORE (source_object);
	CamelFolderInfo *titer = camel_store_get_folder_info_finish (store, res, NULL);
#else
static gboolean
on_got_folderinfo_unregister (CamelStore      *store,
                              CamelFolderInfo *titer,
                              void            *data)
{
#endif
	RegisterInfo *reg_info = data;
	TrackerMinerEvolution *self = reg_info->self;
	TrackerMinerEvolutionPrivate *priv;
	gchar *uri = reg_info->uri;
	GHashTableIter iter;
	gpointer key, value;

	priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (self);
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

#ifdef EVOLUTION_SHELL_3_2
	camel_store_free_folder_info (store, titer);
#else
	return TRUE;
#endif
}

static void
unregister_account (TrackerMinerEvolution *self,
                    EAccount              *account)
{
	CamelProvider *provider;
	CamelStore *store;
	char *uri = account->source->url;
	RegisterInfo *reg_info;

	if (!(provider = camel_provider_get(uri, NULL))) {
		return;
	}

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;

	if (!(store = (CamelStore *) camel_session_get_service (CAMEL_SESSION (session),
#ifdef EVOLUTION_SHELL_3_2
	                                                        account->uid))) {
#else
	                                                        uri,
	                                                        CAMEL_PROVIDER_STORE,
	                                                        NULL))) {
#endif
		return;
	}

	reg_info = g_new0 (RegisterInfo, 1);

	reg_info->self = g_object_ref (self);
	reg_info->uri = g_strdup (uri);
	reg_info->account = NULL;

	/* Get the account's folder-info and unregister asynchronously */
#ifdef EVOLUTION_SHELL_3_2
	camel_store_get_folder_info (store,
	                             NULL,
	                             CAMEL_STORE_FOLDER_INFO_FAST |
	                             CAMEL_STORE_FOLDER_INFO_RECURSIVE |
	                             CAMEL_STORE_FOLDER_INFO_SUBSCRIBED,
	                             G_PRIORITY_DEFAULT,
	                             NULL,
	                             on_got_folderinfo_unregister,
	                             reg_info);
#else
	mail_get_folderinfo (store, NULL, on_got_folderinfo_unregister, reg_info);
#endif

	g_object_unref (store);
}

static void
on_account_added (EAccountList          *list,
                  EAccount              *account,
                  TrackerMinerEvolution *self)
{
	register_account (self, account);
	introduce_account_to_all (self, account);
}

static void
on_account_removed (EAccountList          *list,
                    EAccount              *account,
                    TrackerMinerEvolution *self)
{
	unregister_account (self, account);
}

static void
on_account_changed (EAccountList          *list,
                    EAccount              *account,
                    TrackerMinerEvolution *self)
{
	unregister_account (self, account);
	register_account (self, account);
	introduce_account_to_all (self, account);
}

static void
disable_plugin (void)
{
	g_debug ("Tracker plugin disabled");

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
enable_plugin_real (void)
{
	GError *error = NULL;

	g_debug ("Tracker plugin creating new object...");

	manager = g_initable_new (TRACKER_TYPE_MINER_EVOLUTION,
	                          NULL,
	                          &error,
	                          "name", "Emails",
	                          NULL);

	if (error) {
		g_critical ("Could not start Tracker plugin, %s", error->message);
		g_error_free (error);
		return;
	}

	tracker_miner_start (TRACKER_MINER (manager));
}

static gboolean 
enable_plugin_try (gpointer user_data)
{
	if (walk_count == 0) {
		enable_plugin_real ();
		return FALSE;
	}

	g_debug ("Tracker plugin already enabled, doing nothing");

	return TRUE;
}

static void
miner_prepare (TrackerMinerEvolutionPrivate *priv)
{
	if (priv->timer_since_stopped && g_timer_elapsed (priv->timer_since_stopped, NULL) > 5) {
		g_timer_destroy (priv->timer_since_stopped);
		priv->timer_since_stopped = NULL;
	}
}

static void
miner_cleanup (TrackerMinerEvolutionPrivate *priv)
{
	if (folder_pool) {
		ThreadPool *pool = folder_pool;

		folder_pool = NULL;
		thread_pool_destroy (pool);
	}

	if (!priv->timer_since_stopped) {
		priv->timer_since_stopped = g_timer_new ();
	}

	if (priv->sparql_cancel) {
		/* We reuse the cancellable */
		g_cancellable_cancel (priv->sparql_cancel);
	}
}

static void
enable_plugin (void)
{
	g_debug ("Tracker Evolution plugin enabled");

	/* Deal with https://bugzilla.gnome.org/show_bug.cgi?id=606940 */

	if (manager) {
		TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (manager);

		miner_cleanup (priv);
		g_object_unref (manager);
	}

	if (walk_count > 0) {
		g_timeout_add_seconds_full (G_PRIORITY_DEFAULT, 1, enable_plugin_try, NULL, NULL);
	} else {
		enable_plugin_real ();
	}
}

int
e_plugin_lib_enable (EPlugin *ep,
                     int      enabled)
{
	g_static_rec_mutex_lock (&glock);

	if (enabled) {
		enable_plugin ();
	} else {
		disable_plugin ();
	}

	g_static_rec_mutex_unlock (&glock);

	return 0;
}

static void
miner_evolution_initable_iface_init (GInitableIface *iface)
{
	miner_evolution_initable_parent_iface = g_type_interface_peek_parent (iface);
	iface->init = miner_evolution_initable_init;
}

static gboolean
miner_evolution_initable_init (GInitable     *initable,
                               GCancellable  *cancellable,
                               GError       **error)
{
	GError *inner_error = NULL;

	/* Chain up parent's initable callback before calling child's one */
	if (!miner_evolution_initable_parent_iface->init (initable, cancellable, &inner_error)) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}

static void
tracker_miner_evolution_finalize (GObject *plugin)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (plugin);

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

	miner_cleanup (priv);

	if (priv->timer_since_stopped) {
		g_timer_destroy (priv->timer_since_stopped);
		priv->timer_since_stopped = NULL;
	}

	if (priv->sparql_cancel) {
		g_cancellable_cancel (priv->sparql_cancel);
		g_object_unref (priv->sparql_cancel);
	}

	G_OBJECT_CLASS (tracker_miner_evolution_parent_class)->finalize (plugin);
}

static void
tracker_miner_evolution_class_init (TrackerMinerEvolutionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);

	g_setenv ("TRACKER_SPARQL_BACKEND", "bus", TRUE);

	miner_class->started = miner_started;
	miner_class->stopped = miner_stopped;
	miner_class->paused  = miner_paused;
	miner_class->resumed = miner_resumed;

	object_class->finalize = tracker_miner_evolution_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerMinerEvolutionPrivate));
}

static void
tracker_miner_evolution_init (TrackerMinerEvolution *plugin)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (plugin);
	EIterator *it;

#ifdef EVOLUTION_SHELL_2_91
	if (!session) {
		EShell *shell;
		EShellBackend *shell_backend;

		shell = e_shell_get_default ();
		shell_backend = e_shell_get_backend_by_name (shell, "mail");
		session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));
	}
#endif

	priv->sparql_cancel = g_cancellable_new ();

	priv->last_time = 0;
	priv->resuming = FALSE;
	priv->paused = FALSE;

	priv->cached_folders = NULL;
	priv->registered_folders = NULL;
	priv->registered_stores = NULL;
	priv->registered_clients = NULL;

	priv->accounts = g_object_ref (e_get_account_list ());

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
}

static void
on_tracker_store_appeared (GDBusConnection *d_connection,
                           const gchar     *name,
                           const gchar     *name_owner,
                           gpointer         user_data)

{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (user_data);

	miner_prepare (priv);

	register_client (user_data);
}

static void
miner_start_watching (TrackerMiner *miner)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (miner);

	priv->watch_name_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
	                                        TRACKER_SERVICE,
	                                        G_BUS_NAME_WATCHER_FLAGS_NONE,
	                                        on_tracker_store_appeared,
	                                        NULL,
	                                        miner,
	                                        NULL);
}

static void
miner_stop_watching (TrackerMiner *miner)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (miner);

	if (priv->watch_name_id != 0)
		g_bus_unwatch_name (priv->watch_name_id);

	miner_cleanup (priv);
}

static void
miner_started (TrackerMiner *miner)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (miner);

	miner_prepare (priv);

	miner_start_watching (miner);

	g_debug ("Tracker plugin setting progress to '0.0' and status to 'Initializing'");
	g_object_set (miner,  "progress", 0.0, "status", "Initializing", NULL);
}

static void
miner_stopped (TrackerMiner *miner)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (miner);

	miner_paused (miner);
	priv->paused = FALSE;
}

static void
miner_paused (TrackerMiner *miner)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (miner);

	/* We don't really pause, we just completely stop */

	miner_stop_watching (miner);

	priv->paused = TRUE;
	priv->last_time = 0;

	miner_cleanup (priv);
}

static gboolean
unset_resuming (gpointer data)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (data);

	priv->resuming = FALSE;
	g_object_unref (data);

	return FALSE;
}

static void
miner_resumed (TrackerMiner *miner)
{
	TrackerMinerEvolutionPrivate *priv = TRACKER_MINER_EVOLUTION_GET_PRIVATE (miner);

	/* We don't really resume, we just completely restart */

	miner_prepare (priv);

	priv->resuming = TRUE;
	priv->paused = FALSE;
	priv->total_popped = 0;
	priv->of_total = 0;

	g_debug ("Tracker plugin setting progress to '0.0' and status to 'Processing'");
	g_object_set (miner,  "progress", 0.0, "status", _("Processing…"), NULL);

	miner_start_watching (miner);

	g_timeout_add_seconds (1, unset_resuming, g_object_ref (miner));
}
