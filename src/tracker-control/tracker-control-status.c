/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <glib/gi18n.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-miner/tracker-miner.h>
#include <libtracker-control/tracker-control.h>

#include "tracker-control.h"

typedef struct {
	TrackerSparqlConnection *connection;
	GHashTable *prefixes;
	GStrv filter;
} WatchData;

static gboolean parse_watch (const gchar  *option_name,
                             const gchar  *value,
                             gpointer      data,
                             GError      **error);

static GDBusConnection *connection = NULL;
static GDBusProxy *proxy = NULL;
static GMainLoop *main_loop;
static GHashTable *miners_progress;
static GHashTable *miners_status;
static gint longest_miner_name_length = 0;
static gint paused_length = 0;

static gboolean full_namespaces = FALSE; /* Can be turned on if needed, or made cmd line option */

/* Note:
 * Every time a new option is added, make sure it is considered in the
 * 'STATUS_OPTIONS_ENABLED' macro below
 */
static gboolean status;
static gboolean follow;
static gchar *watch = NULL;
static gboolean list_common_statuses;

#define STATUS_OPTIONS_ENABLED() \
	(status || follow || watch || list_common_statuses)

/* Make sure our statuses are translated (most from libtracker-miner) */
static const gchar *statuses[8] = {
	N_("Unavailable"), /* generic */
	N_("Initializing"),
	N_("Processing…"),
	N_("Fetching…"), /* miner/rss */
	N_("Crawling single directory '%s'"),
	N_("Crawling recursively directory '%s'"),
	N_("Paused"),
	N_("Idle")
};

static GOptionEntry entries[] = {
	{ "status", 'S', 0, G_OPTION_ARG_NONE, &status,
	  N_("Show current status"),
	  NULL
	},
	{ "follow", 'F', 0, G_OPTION_ARG_NONE, &follow,
	  N_("Follow status changes as they happen"),
	  NULL
	},
	{ "watch", 'w', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, parse_watch,
	  N_("Watch changes to the database in real time (e.g. resources or files being added)"),
	  N_("ONTOLOGY")
	},
	{ "list-common-statuses", 0, 0, G_OPTION_ARG_NONE, &list_common_statuses,
	  N_("List common statuses for miners and the store"),
	  NULL
	},
	{ NULL }
};

static gboolean
parse_watch (const gchar  *option_name,
             const gchar  *value,
             gpointer      data,
             GError      **error)
{
	if (!value) {
		watch = g_strdup ("");
	} else {
		watch = g_strdup (value);
	}

	return TRUE;
}

gboolean
tracker_control_status_options_enabled (void)
{
	return STATUS_OPTIONS_ENABLED ();
}

static void
signal_handler (int signo)
{
	static gboolean in_loop = FALSE;

	/* Die if we get re-entrant signals handler calls */
	if (in_loop) {
		exit (EXIT_FAILURE);
	}

	switch (signo) {
	case SIGTERM:
	case SIGINT:
		in_loop = TRUE;
		g_main_loop_quit (main_loop);

		/* Fall through */
	default:
		if (g_strsignal (signo)) {
			g_print ("\n");
			g_print ("Received signal:%d->'%s'\n",
			         signo,
			         g_strsignal (signo));
		}
		break;
	}
}

static void
initialize_signal_handler (void)
{
	struct sigaction act;
	sigset_t empty_mask;

	sigemptyset (&empty_mask);
	act.sa_handler = signal_handler;
	act.sa_mask = empty_mask;
	act.sa_flags = 0;

	sigaction (SIGTERM, &act, NULL);
	sigaction (SIGINT, &act, NULL);
	sigaction (SIGHUP, &act, NULL);
}

static gboolean
miner_get_details (TrackerMinerManager  *manager,
                   const gchar          *miner,
                   gchar               **status,
                   gdouble              *progress,
                   gint                 *remaining_time,
                   GStrv                *pause_applications,
                   GStrv                *pause_reasons)
{
	if ((status || progress || remaining_time) &&
	    !tracker_miner_manager_get_status (manager,
	                                       miner,
	                                       status,
	                                       progress,
	                                       remaining_time)) {
		g_printerr (_("Could not get status from miner: %s"), miner);
		return FALSE;
	}

	tracker_miner_manager_is_paused (manager, miner,
	                                 pause_applications,
	                                 pause_reasons);

	if (!(*pause_applications) || !(*pause_reasons)) {
		/* unable to get pause details,
		   already logged by tracker_miner_manager_is_paused */
		return FALSE;
	}

	return TRUE;
}

static void
miner_print_state (TrackerMinerManager *manager,
                   const gchar         *miner_name,
                   const gchar         *status,
                   gdouble              progress,
                   gint                 remaining_time,
                   gboolean             is_running,
                   gboolean             is_paused)
{
	const gchar *name;
	time_t now;
	gchar time_str[64];
	size_t len;
	struct tm *local_time;

	now = time ((time_t *) NULL);
	local_time = localtime (&now);
	len = strftime (time_str,
	                sizeof (time_str) - 1,
	                "%d %b %Y, %H:%M:%S:",
	                local_time);
	time_str[len] = '\0';

	name = tracker_miner_manager_get_display_name (manager, miner_name);

	if (is_running) {
		gchar *progress_str = NULL;
		gchar *remaining_time_str = NULL;

		if (progress >= 0.0 && progress < 1.0) {
			progress_str = g_strdup_printf ("%3u%%", (guint)(progress * 100));
		}

		/* Progress > 0.01 here because we want to avoid any message
		 * during crawling, as we don't have the remaining time in that
		 * case and it would just print "unknown time left" */
		if (progress > 0.01 &&
		    progress < 1.0 &&
		    remaining_time >= 0) {
			/* 0 means that we couldn't properly compute the remaining
			 * time. */
			if (remaining_time > 0) {
				gchar *seconds_str = tracker_seconds_to_string (remaining_time, TRUE);

				/* Translators: %s is a time string */
				remaining_time_str = g_strdup_printf (_("%s remaining"), seconds_str);
				g_free (seconds_str);
			} else {
				remaining_time_str = g_strdup (_("unknown time left"));
			}
		}

		g_print ("%s  %s  %-*.*s %s%-*.*s%s %s %s %s\n",
		         time_str,
		         progress_str ? progress_str : "✓   ",
		         longest_miner_name_length,
		         longest_miner_name_length,
		         name,
		         is_paused ? "(" : " ",
		         paused_length,
		         paused_length,
		         is_paused ? _("PAUSED") : " ",
		         is_paused ? ")" : " ",
		         status ? "-" : "",
		         status ? _(status) : "",
		         remaining_time_str ? remaining_time_str : "");

		g_free (progress_str);
		g_free (remaining_time_str);
	} else {
		g_print ("%s  ✗     %-*.*s  %-*.*s  - %s\n",
		         time_str,
		         longest_miner_name_length,
		         longest_miner_name_length,
		         name,
		         paused_length,
		         paused_length,
		         " ",
		         _("Not running or is a disabled plugin"));
	}
}

static void
store_print_state (const gchar *status,
                   gdouble      progress)
{
	gchar time_str[64];
	struct tm *local_time;
	time_t now;
	size_t len;

	now = time ((time_t *) NULL);
	local_time = localtime (&now);
	len = strftime (time_str,
	                sizeof (time_str) - 1,
	                "%d %b %Y, %H:%M:%S:",
	                local_time);
	time_str[len] = '\0';

	if (status) {
		gchar *operation = NULL;
		gchar *operation_status = NULL;
		gchar *progress_str;

		if (strstr (status, "-")) {
			gchar **status_split;

			status_split = g_strsplit (status, "-", 2);
			if (status_split[0] && status_split[1]) {
				operation = g_strstrip (status_split[0]);
				operation_status = g_strstrip (status_split[1]);
				/* Free the array, not the contents */
				g_free (status_split);
			} else {
				/* Free everything */
				g_strfreev (status_split);
			}
		}

		if (progress >= 0.0 && progress < 1.0) {
			progress_str = g_strdup_printf ("%3u%%", (guint)(progress * 100));
		} else {
			progress_str = g_strdup_printf ("✓   ");
		}

		g_print ("%s  %s  %-*.*s    - %s %s%s%s\n",
		         time_str,
		         progress_str ? progress_str : "    ",
		         longest_miner_name_length + paused_length,
		         longest_miner_name_length + paused_length,
		         "Store",
		         /*(operation ? _(operation) : _(status)),*/
		         /*operation ? "-" : "",*/
		         operation ? _(operation) : _(status),
		         operation_status ? "(" : "",
		         operation_status ? operation_status : "",
		         operation_status ? ")" : "");

		g_free (progress_str);
		g_free (operation);
		g_free (operation_status);
	} else {
		g_print ("%s  %s %-*.*s    - %s\n",
		         time_str,
		         "✗    ", /* Progress */
		         longest_miner_name_length + paused_length,
		         longest_miner_name_length + paused_length,
		         "Store",
		         _("Unavailable"));
	}
}

static void
store_get_and_print_state (void)
{
	GVariant *v_status, *v_progress;
	const gchar *status = NULL;
	gdouble progress = -1.0;
	GError *error = NULL;
	gchar *owner;

	owner = g_dbus_proxy_get_name_owner (proxy);
	if (!owner) {
		/* Name is not owned yet, store is not running */
		store_print_state (NULL, -1);
		return;
	}
	g_free (owner);

	/* Status */
	v_status = g_dbus_proxy_call_sync (proxy,
	                                   "GetStatus",
	                                   NULL,
	                                   G_DBUS_CALL_FLAGS_NONE,
	                                   -1,
	                                   NULL,
	                                   &error);

	if (!v_status || error) {
		g_critical ("%s, %s",
		            _("Could not retrieve tracker-store status"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
		return;
	}

	g_variant_get (v_status, "(&s)", &status);

	/* Progress */
	v_progress = g_dbus_proxy_call_sync (proxy,
	                                     "GetProgress",
	                                     NULL,
	                                     G_DBUS_CALL_FLAGS_NONE,
	                                     -1,
	                                     NULL,
	                                     &error);

	g_variant_get (v_progress, "(d)", &progress);

	if (progress < 0.0 || error) {
		g_critical ("%s, %s",
		            _("Could not retrieve tracker-store progress"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
		return;
	}

	/* Print */
	store_print_state (status, progress);

	g_variant_unref (v_progress);
	g_variant_unref (v_status);
}

static void
manager_miner_progress_cb (TrackerMinerManager *manager,
                           const gchar         *miner_name,
                           const gchar         *status,
                           gdouble              progress,
                           gint                 remaining_time)
{
	GValue *gvalue;

	gvalue = g_slice_new0 (GValue);

	g_value_init (gvalue, G_TYPE_DOUBLE);
	g_value_set_double (gvalue, progress);

	miner_print_state (manager, miner_name, status, progress, remaining_time, TRUE, FALSE);

	g_hash_table_replace (miners_status,
	                      g_strdup (miner_name),
	                      g_strdup (status));
	g_hash_table_replace (miners_progress,
	                      g_strdup (miner_name),
	                      gvalue);
}

static void
manager_miner_paused_cb (TrackerMinerManager *manager,
                         const gchar         *miner_name)
{
	GValue *gvalue;

	gvalue = g_hash_table_lookup (miners_progress, miner_name);

	miner_print_state (manager, miner_name,
	                   g_hash_table_lookup (miners_status, miner_name),
	                   gvalue ? g_value_get_double (gvalue) : 0.0,
	                   -1,
	                   TRUE,
	                   TRUE);
}

static void
manager_miner_resumed_cb (TrackerMinerManager *manager,
                          const gchar         *miner_name)
{
	GValue *gvalue;

	gvalue = g_hash_table_lookup (miners_progress, miner_name);

	miner_print_state (manager, miner_name,
	                   g_hash_table_lookup (miners_status, miner_name),
	                   gvalue ? g_value_get_double (gvalue) : 0.0,
	                   0,
	                   TRUE,
	                   FALSE);
}

static void
miners_progress_destroy_notify (gpointer data)
{
	GValue *value;

	value = data;
	g_value_unset (value);
	g_slice_free (GValue, value);
}

static gchar *
get_shorthand (GHashTable  *prefixes,
               const gchar *namespace)
{
	gchar *hash;

	hash = strrchr (namespace, '#');

	if (hash) {
		gchar *property;
		const gchar *prefix;

		property = hash + 1;
		*hash = '\0';

		prefix = g_hash_table_lookup (prefixes, namespace);

		return g_strdup_printf ("%s:%s", prefix, property);
	}

	return g_strdup (namespace);
}

static GHashTable *
get_prefixes (TrackerSparqlConnection *connection)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GHashTable *retval;
	const gchar *query;

	retval = g_hash_table_new_full (g_str_hash,
	                                g_str_equal,
	                                g_free,
	                                g_free);

	/* FIXME: Would like to get this in the same SPARQL that we
	 * use to get the info, but doesn't seem possible at the
	 * moment with the limited string manipulation features we
	 * support in SPARQL.
	 */
	query = "SELECT ?ns ?prefix "
	        "WHERE {"
	        "  ?ns a tracker:Namespace ;"
	        "  tracker:prefix ?prefix "
	        "}";

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Unable to retrieve namespace prefixes"),
			    error->message);

		g_error_free (error);
		return retval;
	}

	if (!cursor) {
		g_printerr ("%s\n", _("No namespace prefixes were returned"));
		return retval;
	}

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		const gchar *key, *value;

		key = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		value = tracker_sparql_cursor_get_string (cursor, 1, NULL);

		if (!key || !value) {
			continue;
		}

		g_hash_table_insert (retval,
		                     g_strndup (key, strlen (key) - 1),
		                     g_strdup (value));
	}

	if (cursor) {
		g_object_unref (cursor);
	}

	return retval;
}

static inline void
print_key (GHashTable  *prefixes,
           const gchar *key)
{
	if (G_UNLIKELY (full_namespaces)) {
		g_print ("'%s'\n", key);
	} else {
		gchar *shorthand;

		shorthand = get_shorthand (prefixes, key);
		g_print ("'%s'\n", shorthand);
		g_free (shorthand);
	}
}

static void
store_progress (GDBusConnection *connection,
                const gchar     *sender_name,
                const gchar     *object_path,
                const gchar     *interface_name,
                const gchar     *signal_name,
                GVariant        *parameters,
                gpointer         user_data)
{
	const gchar *status = NULL;
	gdouble progress = 0.0;

	g_variant_get (parameters, "(sd)", &status, &progress);
	store_print_state (status, progress);
}

static void
store_graph_update_interpret (WatchData  *wd,
                              GHashTable *updates,
                              gint        subject,
                              gint        predicate)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gchar *query, *key;
	gboolean ok = TRUE;

	query = g_strdup_printf ("SELECT tracker:uri (%d) tracker:uri(%d) {}",
	                         subject,
	                         predicate);
	cursor = tracker_sparql_connection_query (wd->connection,
	                                          query,
	                                          NULL,
	                                          &error);
	g_free (query);

	if (error) {
		g_critical ("%s, %s",
		            _("Could not run SPARQL query"),
		            error->message);
		g_clear_error (&error);
		return;
	}

	if (!tracker_sparql_cursor_next (cursor, NULL, &error) || error) {
		g_critical ("%s, %s",
		            _("Could not call tracker_sparql_cursor_next() on SPARQL query"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
		return;
	}

	/* Key = predicate */
	key = g_strdup (tracker_sparql_cursor_get_string (cursor, 1, NULL));
	query = g_strdup_printf ("SELECT ?t { <%s> <%s> ?t } ORDER BY DESC(<%s>)",
	                         tracker_sparql_cursor_get_string (cursor, 0, NULL),
	                         key,
	                         key);
	g_object_unref (cursor);

	cursor = tracker_sparql_connection_query (wd->connection, query, NULL, &error);
	g_free (query);

	if (error) {
		g_critical ("%s, %s",
		            _("Could not run SPARQL query"),
		            error->message);
		g_clear_error (&error);
		return;
	}

	while (ok) {
		const gchar *value;

		ok = tracker_sparql_cursor_next (cursor, NULL, &error);

		if (error) {
			g_critical ("%s, %s",
			            _("Could not call tracker_sparql_cursor_next() on SPARQL query"),
			            error ? error->message : _("No error given"));
			g_clear_error (&error);
			break;
		}

		value = tracker_sparql_cursor_get_string (cursor, 0, NULL);

		if (!key || !value) {
			continue;
		}

		/* Don't display nie:plainTextContent */
		if (strcmp (key, "http://www.semanticdesktop.org/ontologies/2007/01/19/nie#plainTextContent") == 0) {
			continue;
		}

		if (G_UNLIKELY (full_namespaces)) {
			if (wd->filter == NULL ||
			    tracker_string_in_string_list (key, wd->filter) != -1) {
				g_hash_table_replace (updates, g_strdup (key), g_strdup (value));
			}
		} else {
			gchar *shorthand;

			shorthand = get_shorthand (wd->prefixes, key);

			if (wd->filter == NULL ||
			    tracker_string_in_string_list (shorthand, wd->filter) != -1) {
				g_hash_table_replace (updates, shorthand, g_strdup (value));
			} else {
				g_free (shorthand);
			}
		}
	}

	g_free (key);
	g_object_unref (cursor);
}

static void
store_graph_update_cb (GDBusConnection *connection,
                       const gchar     *sender_name,
                       const gchar     *object_path,
                       const gchar     *interface_name,
                       const gchar     *signal_name,
                       GVariant        *parameters,
                       gpointer         user_data)

{
	WatchData *wd;
	GHashTable *updates;
	GVariantIter *iter1, *iter2;
	gchar *class_name;
	gint graph = 0, subject = 0, predicate = 0, object = 0;

	wd = user_data;

	updates = g_hash_table_new_full (g_str_hash,
	                                 g_str_equal,
	                                 (GDestroyNotify) g_free,
	                                 (GDestroyNotify) g_free);

	g_variant_get (parameters, "(&sa(iiii)a(iiii))", &class_name, &iter1, &iter2);

	while (g_variant_iter_loop (iter1, "(iiii)", &graph, &subject, &predicate, &object)) {
		store_graph_update_interpret (wd, updates, subject, predicate);
	}

	while (g_variant_iter_loop (iter2, "(iiii)", &graph, &subject, &predicate, &object)) {
		store_graph_update_interpret (wd, updates, subject, predicate);
	}

	/* Print updates sorted and filtered */
	GList *keys, *l;

	keys = g_hash_table_get_keys (updates);
	keys = g_list_sort (keys, (GCompareFunc) g_strcmp0);

	if (g_hash_table_size (updates) > 0) {
		print_key (wd->prefixes, class_name);
	}

	for (l = keys; l; l = l->next) {
		gchar *key = l->data;
		gchar *value = g_hash_table_lookup (updates, l->data);

		g_print ("  '%s' = '%s'\n", key, value);
	}

	g_list_free (keys);
	g_hash_table_unref (updates);
	g_variant_iter_free (iter1);
	g_variant_iter_free (iter2);
}

void
tracker_control_status_run_default (void)
{
	/* Enable status output in the default run */
	status = TRUE;

	tracker_control_status_run ();
}

static WatchData *
watch_data_new (TrackerSparqlConnection *sparql_connection,
                GHashTable              *sparql_prefixes,
                const gchar             *watch_filter)
{
	WatchData *data;

	data = g_new0 (WatchData, 1);
	data->connection = g_object_ref (sparql_connection);
	data->prefixes = g_hash_table_ref (sparql_prefixes);

	if (watch_filter && strlen (watch_filter) > 0) {
		data->filter = g_strsplit (watch_filter, ",", -1);
	}

	return data;
}

static void
watch_data_free (WatchData *data)
{
	if (!data) {
		return;
	}

	if (data->filter) {
		g_strfreev (data->filter);
	}

	if (data->prefixes) {
		g_hash_table_unref (data->prefixes);
	}

	if (data->connection) {
		g_object_unref (data->connection);
	}

	g_free (data);
}

gint
tracker_control_status_run (void)
{
	TrackerMinerManager *manager;

	/* --follow implies --status */
	if (follow) {
		status = TRUE;
	}

	if (watch != NULL) {
		TrackerSparqlConnection *sparql_connection;
		GHashTable *sparql_prefixes;
		GError *error = NULL;
		guint signal_id;

		sparql_connection = tracker_sparql_connection_get (NULL, &error);

		if (!sparql_connection) {
			g_critical ("%s, %s",
			            _("Could not get SPARQL connection"),
			            error ? error->message : _("No error given"));
			g_clear_error (&error);
			return EXIT_FAILURE;
		}

		if (!tracker_control_dbus_get_connection ("org.freedesktop.Tracker1",
		                                          "/org/freedesktop/Tracker1/Resources",
		                                          "org.freedesktop.Tracker1.Resources",
		                                          G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
		                                          &connection,
		                                          &proxy)) {
			g_object_unref (sparql_connection);
			return EXIT_FAILURE;
		}

		sparql_prefixes = get_prefixes (sparql_connection);

		signal_id = g_dbus_connection_signal_subscribe (connection,
		                                                TRACKER_DBUS_SERVICE,
		                                                TRACKER_DBUS_INTERFACE_RESOURCES,
		                                                "GraphUpdated",
		                                                TRACKER_DBUS_OBJECT_RESOURCES,
		                                                NULL, /* TODO: Use class-name here */
		                                                G_DBUS_SIGNAL_FLAGS_NONE,
		                                                store_graph_update_cb,
		                                                watch_data_new (sparql_connection, sparql_prefixes, watch),
		                                                (GDestroyNotify) watch_data_free);

		g_hash_table_unref (sparql_prefixes);
		g_object_unref (sparql_connection);

		g_print ("%s\n", _("Now listening for resource updates to the database"));
		g_print ("%s\n\n", _("All nie:plainTextContent properties are omitted"));
		g_print ("%s\n", _("Press Ctrl+C to stop"));

		main_loop = g_main_loop_new (NULL, FALSE);
		g_main_loop_run (main_loop);
		g_main_loop_unref (main_loop);

		g_dbus_connection_signal_unsubscribe (connection, signal_id);

		return EXIT_SUCCESS;
	}

	if (list_common_statuses) {
		gint i;

		g_print ("%s:\n", _("Common statuses include"));

		for (i = 0; i < G_N_ELEMENTS (statuses); i++) {
			g_print ("  %s\n", _(statuses[i]));
		}

		return EXIT_SUCCESS;
	}

	if (status) {
		GError *error = NULL;
		GSList *miners_available;
		GSList *miners_running;
		GSList *l;

		/* Don't auto-start the miners here */
		manager = tracker_miner_manager_new_full (FALSE, &error);
		if (!manager) {
			g_printerr (_("Could not get status, manager could not be created, %s"),
			            error ? error->message : _("No error given"));
			g_printerr ("\n");
			g_clear_error (&error);
			return EXIT_FAILURE;
		}

		miners_available = tracker_miner_manager_get_available (manager);
		miners_running = tracker_miner_manager_get_running (manager);

		/* Work out lengths for output spacing */
		paused_length = strlen (_("PAUSED"));

		for (l = miners_available; l; l = l->next) {
			const gchar *name;

			name = tracker_miner_manager_get_display_name (manager, l->data);
			longest_miner_name_length = MAX (longest_miner_name_length, strlen (name));
		}

		/* Display states */
		g_print ("%s:\n", _("Store"));

		if (!tracker_control_dbus_get_connection ("org.freedesktop.Tracker1",
		                                          "/org/freedesktop/Tracker1/Status",
		                                          "org.freedesktop.Tracker1.Status",
		                                          G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
		                                          &connection,
		                                          &proxy)) {
			return EXIT_FAILURE;
		}

		g_dbus_connection_signal_subscribe (connection,
		                                    "org.freedesktop.Tracker1",
		                                    "org.freedesktop.Tracker1.Status",
		                                    "Progress",
		                                    "/org/freedesktop/Tracker1/Status",
		                                    NULL,
		                                    G_DBUS_SIGNAL_FLAGS_NONE,
		                                    store_progress,
		                                    NULL,
		                                    NULL);

		store_get_and_print_state ();

		g_print ("\n");

		g_print ("%s:\n", _("Miners"));

		for (l = miners_available; l; l = l->next) {
			const gchar *name;
			gboolean is_running;

			name = tracker_miner_manager_get_display_name (manager, l->data);
			if (!name) {
				g_critical (_("Could not get display name for miner '%s'"),
				            (const gchar*) l->data);
				continue;
			}

			is_running = tracker_string_in_gslist (l->data, miners_running);

			if (is_running) {
				GStrv pause_applications, pause_reasons;
				gchar *status = NULL;
				gdouble progress;
				gint remaining_time;
				gboolean is_paused;

				if (!miner_get_details (manager,
				                        l->data,
				                        &status,
				                        &progress,
				                        &remaining_time,
				                        &pause_applications,
				                        &pause_reasons)) {
					continue;
				}

				is_paused = *pause_applications || *pause_reasons;

				miner_print_state (manager,
				                   l->data,
				                   status,
				                   progress,
				                   remaining_time,
				                   TRUE,
				                   is_paused);

				g_strfreev (pause_applications);
				g_strfreev (pause_reasons);
				g_free (status);
			} else {
				miner_print_state (manager, l->data, NULL, 0.0, -1, FALSE, FALSE);
			}
		}

		g_slist_foreach (miners_available, (GFunc) g_free, NULL);
		g_slist_free (miners_available);

		g_slist_foreach (miners_running, (GFunc) g_free, NULL);
		g_slist_free (miners_running);

		if (!follow) {
			/* Do nothing further */
			if (proxy) {
				g_object_unref (proxy);
			}
			g_print ("\n");
			return EXIT_SUCCESS;
		}

		g_print ("%s\n", _("Press Ctrl+C to stop"));

		g_signal_connect (manager, "miner-progress",
		                  G_CALLBACK (manager_miner_progress_cb), NULL);
		g_signal_connect (manager, "miner-paused",
		                  G_CALLBACK (manager_miner_paused_cb), NULL);
		g_signal_connect (manager, "miner-resumed",
		                  G_CALLBACK (manager_miner_resumed_cb), NULL);

		initialize_signal_handler ();

		miners_progress = g_hash_table_new_full (g_str_hash,
		                                         g_str_equal,
		                                         (GDestroyNotify) g_free,
		                                         (GDestroyNotify) miners_progress_destroy_notify);
		miners_status = g_hash_table_new_full (g_str_hash,
		                                       g_str_equal,
		                                       (GDestroyNotify) g_free,
		                                       (GDestroyNotify) g_free);

		main_loop = g_main_loop_new (NULL, FALSE);
		g_main_loop_run (main_loop);
		g_main_loop_unref (main_loop);

		g_hash_table_unref (miners_progress);
		g_hash_table_unref (miners_status);

		if (proxy) {
			g_object_unref (proxy);
		}

		if (manager) {
			g_object_unref (manager);
		}

		return EXIT_SUCCESS;
	}

	/* All known options have their own exit points */
	g_warn_if_reached ();

	return EXIT_FAILURE;
}

GOptionGroup *
tracker_control_status_get_option_group (void)
{
	GOptionGroup *group;

	/* Status options */
	group = g_option_group_new ("status",
	                            _("Status options"),
	                            _("Show status options"),
	                            NULL,
	                            NULL);
	g_option_group_add_entries (group, entries);

	return group;
}
