/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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
 */

#include "config.h"

#include <string.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-ontologies.h>

#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-db-dbus.h>

#include "tracker-dbus.h"
#include "tracker-marshal.h"
#include "tracker-resources.h"
#include "tracker-resource-class.h"
#include "tracker-events.h"
#include "tracker-writeback.h"
#include "tracker-store.h"

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

/* I *know* that this is some arbitrary number that doesn't seem to
 * resemble anything. In fact it's what I experimentally measured to
 * be a good value on a default Debian testing which has
 * max_message_size set to 1 000 000 000 in session.conf. I didn't have
 * the feeling that this value was very much respected, as the size
 * of the DBusMessage when libdbus decided to exit() the process was
 * around 160 MB, and not ~ 1000 MB. So if you take 160 MB and you
 * devide it by 1000000 you have an average string size of ~ 160
 * bytes plus DBusMessage's overhead. If that makes this number less
 * arbitrary for you, then fine.
 *
 * I really hope that the libdbus people get to their senses and
 * either stop doing their exit() nonsense in a library, and instead
 * return a clean DBusError or something, or create crystal clear
 * clarity about the maximum size of a message. And make it both so
 * that I can get this length at runtime (without having to parse
 * libdbus's own configuration files) and my DBusMessage's current
 * total length. As far as I know are both not possible. So that for
 * me means that libdbus's exit() is unacceptable.
 *
 * Note for the debugger of the future, the "Disconnected" signal gets
 * sent to us by the bus, which in turn makes libdbus-glib perform exit(). */

#define DBUS_ARBITRARY_MAX_MSG_SIZE 1000000

G_DEFINE_TYPE(TrackerResources, tracker_resources, G_TYPE_OBJECT)

#define TRACKER_RESOURCES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_RESOURCES, TrackerResourcesPrivate))

enum {
	WRITEBACK,
	LAST_SIGNAL
};

typedef struct {
	GSList *event_sources;
} TrackerResourcesPrivate;

typedef struct {
	DBusGMethodInvocation *context;
	guint request_id;
	DBusMessage *reply;
} TrackerDBusMethodInfo;

typedef struct {
	DBusMessage *reply;
	GError *error;
	gpointer user_data;
} InThreadPtr;

static void tracker_resources_finalize (GObject *object);

static guint signals[LAST_SIGNAL] = { 0 };

static void
free_event_sources (TrackerResourcesPrivate *priv)
{
	if (priv->event_sources) {
		g_slist_foreach (priv->event_sources,
		                 (GFunc) g_object_unref, NULL);
		g_slist_free (priv->event_sources);

		priv->event_sources = NULL;
	}
}

static void
tracker_resources_class_init (TrackerResourcesClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_resources_finalize;

	signals[WRITEBACK] =
		g_signal_new ("writeback",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerResourcesClass, writeback),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__BOXED,
		              G_TYPE_NONE, 1,
		              TRACKER_TYPE_STR_STRV_MAP);

	g_type_class_add_private (object_class, sizeof (TrackerResourcesPrivate));
}


static void
tracker_resources_init (TrackerResources *object)
{
}

TrackerResources *
tracker_resources_new (void)
{
	return g_object_new (TRACKER_TYPE_RESOURCES, NULL);
}

/*
 * Functions
 */

static void
destroy_method_info (gpointer user_data)
{
	g_slice_free (TrackerDBusMethodInfo, user_data);
}

static void
turtle_import_callback (GError *error, gpointer user_data)
{
	TrackerDBusMethodInfo *info = user_data;

	if (error) {
		tracker_dbus_request_failed (info->request_id,
		                             info->context,
		                             &error,
		                             NULL);
		dbus_g_method_return_error (info->context, error);
		return;
	}

	tracker_dbus_request_success (info->request_id,
	                              info->context);
	dbus_g_method_return (info->context);
}

void
tracker_resources_load (TrackerResources         *object,
                        const gchar              *uri,
                        DBusGMethodInvocation    *context,
                        GError                  **error)
{
	TrackerDBusMethodInfo   *info;
	guint                    request_id;
	GFile  *file;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (uri != NULL, context);

	tracker_dbus_request_new (request_id,
	                          context,
	                          "%s(): uri:'%s'",
	                          __FUNCTION__,
	                          uri);

	file = g_file_new_for_uri (uri);

	info = g_slice_new (TrackerDBusMethodInfo);

	info->request_id = request_id;
	info->context = context;

	tracker_store_queue_turtle_import (file, turtle_import_callback,
	                                   info, destroy_method_info);

	g_object_unref (file);
}

static void
query_callback (gpointer inthread_data, GError *error, gpointer user_data)
{
	InThreadPtr *ptr = inthread_data;
	TrackerDBusMethodInfo *info = user_data;

	if (ptr && ptr->error) {
		tracker_dbus_request_failed (info->request_id,
		                             info->context,
		                             &ptr->error,
		                             NULL);
		dbus_g_method_return_error (info->context, ptr->error);
		g_error_free (ptr->error);
	} else if (error) {
		tracker_dbus_request_failed (info->request_id,
		                             info->context,
		                             &error,
		                             NULL);
		dbus_g_method_return_error (info->context, error);
	} else if (ptr) {
		tracker_dbus_request_success (info->request_id,
		                              info->context);

		dbus_g_method_send_reply (info->context, ptr->reply);
	} /* else, !ptr && !error... shouldn't happen */

	if (ptr)
		g_slice_free (InThreadPtr, ptr);
}

static gpointer
query_inthread (TrackerDBCursor *cursor, GCancellable *cancellable, GError *error, gpointer user_data)
{
	InThreadPtr *ptr = g_slice_new0 (InThreadPtr);
	TrackerDBusMethodInfo *info = user_data;
	DBusMessage *reply;
	DBusMessageIter iter, rows_iter;
	guint cols;
	GError *loop_error = NULL;
	guint length = 0;
	gboolean cont;

	if (error) {
		ptr->error = g_error_copy (error);
		return ptr;
	}

	reply = info->reply;

	dbus_message_iter_init_append (reply, &iter);

	cols = tracker_db_cursor_get_n_columns (cursor);

	dbus_message_iter_open_container (&iter,
	                                  DBUS_TYPE_ARRAY,
	                                  "as",
	                                  &rows_iter);

	cont = TRUE;

	while (tracker_db_cursor_iter_next (cursor, cancellable, &loop_error) && cont) {
		DBusMessageIter cols_iter;
		guint i;

		if (loop_error != NULL) {
			break;
		}

		dbus_message_iter_open_container (&rows_iter,
		                                  DBUS_TYPE_ARRAY,
		                                  "s",
		                                  &cols_iter);

		for (i = 0; i < cols && cont; i++, length++) {
			const gchar *result_str;
			result_str = tracker_db_cursor_get_string (cursor, i, NULL);

			if (result_str == NULL)
				result_str = "";

			dbus_message_iter_append_basic (&cols_iter, DBUS_TYPE_STRING, &result_str);

			if (length > DBUS_ARBITRARY_MAX_MSG_SIZE) {
				g_set_error (&loop_error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_INTERRUPTED,
				             "result set of the query is too large");

				cont = FALSE;
			}

		}

		dbus_message_iter_close_container (&rows_iter, &cols_iter);
	}

	dbus_message_iter_close_container (&iter, &rows_iter);

	if (loop_error) {
		ptr->error = loop_error;
		ptr->reply = NULL;
	} else {
		ptr->reply = reply;
	}

	return ptr;
}

void
tracker_resources_sparql_query (TrackerResources         *self,
                                const gchar              *query,
                                DBusGMethodInvocation    *context,
                                GError                  **error)
{
	TrackerDBusMethodInfo   *info;
	guint                 request_id;
	gchar                 *sender;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (query != NULL, context);

	tracker_dbus_request_new (request_id,
	                          context,
	                          "%s(): '%s'",
	                          __FUNCTION__,
	                          query);

	info = g_slice_new (TrackerDBusMethodInfo);

	info->request_id = request_id;
	info->context = context;
	info->reply = dbus_g_method_get_reply (context);

	sender = dbus_g_method_get_sender (context);

	tracker_store_sparql_query (query, TRACKER_STORE_PRIORITY_HIGH,
	                            query_inthread, query_callback, sender,
	                            info, destroy_method_info);

	g_free (sender);
}

static void
update_callback (GError *error, gpointer user_data)
{
	TrackerDBusMethodInfo *info = user_data;

	if (error) {
		tracker_dbus_request_failed (info->request_id,
		                             info->context,
		                             &error,
		                             NULL);
		dbus_g_method_return_error (info->context, error);
		return;
	}

	tracker_dbus_request_success (info->request_id,
	                              info->context);
	dbus_g_method_return (info->context);
}

void
tracker_resources_sparql_update (TrackerResources        *self,
                                 const gchar             *update,
                                 DBusGMethodInvocation   *context,
                                 GError                 **error)
{
	TrackerDBusMethodInfo   *info;
	guint                 request_id;
	gchar                 *sender;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (update != NULL, context);

	tracker_dbus_request_new (request_id,
	                          context,
	                          "%s(): '%s'",
	                          __FUNCTION__,
	                          update);

	info = g_slice_new (TrackerDBusMethodInfo);

	info->request_id = request_id;
	info->context = context;

	sender = dbus_g_method_get_sender (context);

	tracker_store_sparql_update (update, TRACKER_STORE_PRIORITY_HIGH, FALSE,
	                             update_callback, sender,
	                             info, destroy_method_info);

	g_free (sender);
}

static void
update_blank_callback (GPtrArray *blank_nodes, GError *error, gpointer user_data)
{
	TrackerDBusMethodInfo *info = user_data;

	if (error) {
		tracker_dbus_request_failed (info->request_id,
		                             info->context,
		                             &error,
		                             NULL);
		dbus_g_method_return_error (info->context, error);
		return;
	}

	tracker_dbus_request_success (info->request_id,
	                              info->context);
	dbus_g_method_return (info->context, blank_nodes);
}

void
tracker_resources_sparql_update_blank (TrackerResources       *self,
                                       const gchar            *update,
                                       DBusGMethodInvocation  *context,
                                       GError                **error)
{
	TrackerDBusMethodInfo   *info;
	guint                 request_id;
	gchar                 *sender;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (update != NULL, context);

	tracker_dbus_request_new (request_id,
	                          context,
	                          "%s(): '%s'",
	                          __FUNCTION__,
	                          update);

	info = g_slice_new (TrackerDBusMethodInfo);

	info->request_id = request_id;
	info->context = context;

	sender = dbus_g_method_get_sender (context);

	tracker_store_sparql_update_blank (update, TRACKER_STORE_PRIORITY_HIGH,
	                                   update_blank_callback, sender,
	                                   info, destroy_method_info);

	g_free (sender);
}

void
tracker_resources_sync (TrackerResources        *self,
                        DBusGMethodInvocation   *context,
                        GError                 **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
	                          context,
	                          "%s()",
	                          __FUNCTION__);

	tracker_data_sync ();

	tracker_dbus_request_success (request_id, context);
	dbus_g_method_return (context);
}

void
tracker_resources_batch_sparql_update (TrackerResources          *self,
                                       const gchar               *update,
                                       DBusGMethodInvocation     *context,
                                       GError                   **error)
{
	TrackerDBusMethodInfo   *info;
	guint                 request_id;
	gchar                 *sender;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (update != NULL, context);

	tracker_dbus_request_new (request_id,
	                          context,
	                          "%s(): '%s'",
	                          __FUNCTION__,
	                          update);

	info = g_slice_new (TrackerDBusMethodInfo);

	info->request_id = request_id;
	info->context = context;

	sender = dbus_g_method_get_sender (context);

	tracker_store_sparql_update (update, TRACKER_STORE_PRIORITY_LOW, TRUE,
	                             update_callback, sender,
	                             info, destroy_method_info);

	g_free (sender);
}

static void
batch_commit_callback (gpointer user_data)
{
	TrackerDBusMethodInfo *info = user_data;

	tracker_data_sync ();

	tracker_dbus_request_success (info->request_id,
	                              info->context);

	dbus_g_method_return (info->context);
}

void
tracker_resources_batch_commit (TrackerResources         *self,
                                DBusGMethodInvocation    *context,
                                GError                  **error)
{
	TrackerDBusMethodInfo *info;
	guint                 request_id;
	gchar                 *sender;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
	                          context,
	                          "%s()",
	                          __FUNCTION__);

	info = g_slice_new (TrackerDBusMethodInfo);

	info->request_id = request_id;
	info->context = context;

	sender = dbus_g_method_get_sender (context);

	tracker_store_queue_commit (batch_commit_callback, sender, info,
	                            destroy_method_info);

	g_free (sender);
}


static void
on_statements_committed (gpointer user_data)
{
	TrackerResources *resources = user_data;
	GArray *events;
	GHashTable *writebacks;
	TrackerResourcesPrivate *priv;

	priv = TRACKER_RESOURCES_GET_PRIVATE (resources);

	/* Class signals feature */
	events = tracker_events_get_pending ();

	/* Do not call tracker_events_reset before calling tracker_resource_class_emit_events
	   as we're reusing the same strings without copies */

	if (events) {
		GSList *event_sources, *l;
		guint i;
		GHashTable *to_emit = NULL;

		event_sources = priv->event_sources;

		for (i = 0; i < events->len; i++) {
			TrackerEvent *event;

			event = &g_array_index (events, TrackerEvent, i);

			for (l = event_sources; l; l = l->next) {
				TrackerResourceClass *class_ = l->data;
				if (g_strcmp0 (tracker_class_get_uri (event->class), tracker_resource_class_get_rdf_class (class_)) == 0) {
					tracker_resource_class_add_event (class_, event->subject, event->predicate, event->type);
					if (!to_emit) {
						to_emit = g_hash_table_new (NULL, NULL);
					}
					g_hash_table_insert (to_emit, class_, class_);
				}
			}
		}

		if (to_emit) {
			GHashTableIter iter;
			gpointer key, value;

			g_hash_table_iter_init (&iter, to_emit);

			while (g_hash_table_iter_next (&iter, &key, &value)) {
				TrackerResourceClass *class_ = key;
				tracker_resource_class_emit_events (class_);
			}

			g_hash_table_destroy (to_emit);
		}
	}

	tracker_events_reset ();

	/* Writeback feature */
	writebacks = tracker_writeback_get_pending ();

	if (writebacks) {
		g_signal_emit (resources, signals[WRITEBACK], 0, writebacks);

	}

	tracker_writeback_reset ();
}


static void
on_statements_rolled_back (gpointer user_data)
{
	tracker_events_reset ();
	tracker_writeback_reset ();
}

static void
on_statement_inserted (const gchar *graph,
                       const gchar *subject,
                       const gchar *predicate,
                       const gchar *object,
                       GPtrArray   *rdf_types,
                       gpointer user_data)
{
	if (g_strcmp0 (predicate, RDF_PREFIX "type") == 0) {
		tracker_events_insert (subject, predicate, object, rdf_types, TRACKER_DBUS_EVENTS_TYPE_ADD);
	} else {
		tracker_events_insert (subject, predicate, object, rdf_types, TRACKER_DBUS_EVENTS_TYPE_UPDATE);
	}

	/* For predicates it's always update here */
	tracker_writeback_check (graph, subject, predicate, object, rdf_types);
}

static void
on_statement_deleted (const gchar *graph,
                      const gchar *subject,
                      const gchar *predicate,
                      const gchar *object,
                      GPtrArray   *rdf_types,
                      gpointer user_data)
{
	if (g_strcmp0 (predicate, RDF_PREFIX "type") == 0) {
		tracker_events_insert (subject, predicate, object, rdf_types, TRACKER_DBUS_EVENTS_TYPE_DELETE);
	} else {
		tracker_events_insert (subject, predicate, object, rdf_types, TRACKER_DBUS_EVENTS_TYPE_UPDATE);
	}

	/* For predicates it's always delete here */
	tracker_writeback_check (graph, subject, predicate, object, rdf_types);
}

void
tracker_resources_prepare (TrackerResources *object,
                           GSList           *event_sources)
{
	TrackerResourcesPrivate *priv;

	priv = TRACKER_RESOURCES_GET_PRIVATE (object);

	free_event_sources (priv);

	tracker_data_add_insert_statement_callback (on_statement_inserted, object);
	tracker_data_add_delete_statement_callback (on_statement_deleted, object);
	tracker_data_add_commit_statement_callback (on_statements_committed, object);
	tracker_data_add_rollback_statement_callback (on_statements_rolled_back, object);

	priv->event_sources = event_sources;
}

static void
tracker_resources_finalize (GObject      *object)
{
	TrackerResourcesPrivate *priv;

	priv = TRACKER_RESOURCES_GET_PRIVATE (object);

	tracker_data_remove_insert_statement_callback (on_statement_inserted, object);
	tracker_data_remove_delete_statement_callback (on_statement_deleted, object);
	tracker_data_remove_commit_statement_callback (on_statements_committed, object);
	tracker_data_remove_rollback_statement_callback (on_statements_rolled_back, object);

	free_event_sources (priv);

	G_OBJECT_CLASS (tracker_resources_parent_class)->finalize (object);
}

void
tracker_resources_unreg_batches (TrackerResources *object,
                                 const gchar      *old_owner)
{
	tracker_store_unreg_batches (old_owner);
}

