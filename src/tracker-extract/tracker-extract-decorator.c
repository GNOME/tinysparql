/*
 * Copyright (C) 2014 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-extract/tracker-extract.h>

#include "tracker-extract-decorator.h"
#include "tracker-extract-persistence.h"
#include "tracker-extract-priority-dbus.h"

enum {
	PROP_EXTRACTOR = 1
};

#define TRACKER_EXTRACT_DATA_SOURCE TRACKER_PREFIX_TRACKER "extractor-data-source"
#define TRACKER_EXTRACT_FAILURE_DATA_SOURCE TRACKER_PREFIX_TRACKER "extractor-failure-data-source"
#define MAX_EXTRACTING_FILES 1

#define TRACKER_EXTRACT_DECORATOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_EXTRACT_DECORATOR, TrackerExtractDecoratorPrivate))

typedef struct _TrackerExtractDecoratorPrivate TrackerExtractDecoratorPrivate;
typedef struct _ExtractData ExtractData;

struct _ExtractData {
	TrackerDecorator *decorator;
	TrackerDecoratorInfo *decorator_info;
	GFile *file;
};

struct _TrackerExtractDecoratorPrivate {
	TrackerExtract *extractor;
	GTimer *timer;
	guint n_extracting_files;

	TrackerExtractPersistence *persistence;
	GHashTable *recovery_files;

	/* DBus name -> AppData */
	GHashTable *apps;
	TrackerExtractDBusPriority *iface;
};

typedef struct {
	guint watch_id;
	gchar **rdf_types;
} AppData;

/* Preferably classes with tracker:notify true, if an
 * extractor module handles new ones, it must be added
 * here.
 */
static const gchar *supported_classes[] = {
	"nfo:Document",
	"nfo:Audio",
	"nfo:Image",
	"nfo:Video",
	"nfo:FilesystemImage",
	"nmm:Playlist",
	NULL
};

static GInitableIface *parent_initable_iface;

static void decorator_get_next_file (TrackerDecorator *decorator);
static void tracker_extract_decorator_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (TrackerExtractDecorator, tracker_extract_decorator,
                        TRACKER_TYPE_DECORATOR_FS,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, tracker_extract_decorator_initable_iface_init))

static void
tracker_extract_decorator_get_property (GObject    *object,
                                        guint       param_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (object)->priv;

	switch (param_id) {
	case PROP_EXTRACTOR:
		g_value_set_object (value, priv->extractor);
		break;
	}
}

static void
tracker_extract_decorator_set_property (GObject      *object,
                                        guint         param_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (object)->priv;

	switch (param_id) {
	case PROP_EXTRACTOR:
		priv->extractor = g_value_dup_object (value);
		break;
	}
}

static void
tracker_extract_decorator_finalize (GObject *object)
{
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (object)->priv;

	if (priv->extractor)
		g_object_unref (priv->extractor);

	if (priv->timer)
		g_timer_destroy (priv->timer);

	g_object_unref (priv->iface);
	g_hash_table_unref (priv->apps);
	g_hash_table_unref (priv->recovery_files);

	G_OBJECT_CLASS (tracker_extract_decorator_parent_class)->finalize (object);
}

static void
decorator_save_info (TrackerSparqlBuilder    *sparql,
                     TrackerExtractDecorator *decorator,
                     TrackerDecoratorInfo    *decorator_info,
                     TrackerExtractInfo      *info)
{
	const gchar *urn;
	TrackerResource *resource = NULL;
	gchar *sparql_command;

	g_set_object (&resource, tracker_extract_info_get_resource (info));

	if (resource == NULL) {
		g_message ("Extract module returned no resource for %s",
		           tracker_decorator_info_get_url (decorator_info));
		/* We must still insert something into the store so that the correct
		 * nie:dataSource triple get inserted. Otherwise, tracker-extract will
		 * try to re-extract this file every time it starts.
		 */
		resource = tracker_resource_new (NULL);
	}

	urn = tracker_decorator_info_get_urn (decorator_info);

	tracker_resource_set_identifier (resource, urn);
	tracker_resource_set_uri (resource, "nie:dataSource",
	        tracker_decorator_get_data_source (TRACKER_DECORATOR (decorator)));

	sparql_command = tracker_resource_print_sparql_update (
	        resource, NULL, TRACKER_OWN_GRAPH_URN);
	tracker_sparql_builder_append (sparql, sparql_command);

	g_object_unref (resource);
	g_free (sparql_command);
}

static void
get_metadata_cb (TrackerExtract *extract,
                 GAsyncResult   *result,
                 ExtractData    *data)
{
	TrackerExtractDecoratorPrivate *priv;
	TrackerExtractInfo *info;
	GError *error = NULL;
	GTask *task;

	priv = TRACKER_EXTRACT_DECORATOR (data->decorator)->priv;
	task = tracker_decorator_info_get_task (data->decorator_info);
	info = tracker_extract_file_finish (extract, result, &error);

	tracker_extract_persistence_remove_file (priv->persistence, data->file);
	g_hash_table_remove (priv->recovery_files, tracker_decorator_info_get_url (data->decorator_info));

	if (error) {
		if (error->domain == TRACKER_EXTRACT_ERROR) {
			g_message ("Extraction failed: %s\n", error ? error->message : "no error given");
			g_task_return_boolean (task, FALSE);
			g_clear_error (&error);
		} else {
			g_task_return_error (task, error);
		}
	} else {
		decorator_save_info (g_task_get_task_data (task),
		                     TRACKER_EXTRACT_DECORATOR (data->decorator),
		                     data->decorator_info, info);
		g_task_return_boolean (task, TRUE);
		tracker_extract_info_unref (info);
	}

	priv->n_extracting_files--;
	decorator_get_next_file (data->decorator);

	tracker_decorator_info_unref (data->decorator_info);
	g_object_unref (data->file);
	g_free (data);
}

static GFile *
decorator_get_recovery_file (TrackerExtractDecorator *decorator,
                             TrackerDecoratorInfo    *info)
{
	TrackerExtractDecoratorPrivate *priv;
	GFile *file;

	priv = decorator->priv;
	file = g_hash_table_lookup (priv->recovery_files,
	                            tracker_decorator_info_get_url (info));

	if (file) {
		g_object_ref (file);
	} else {
		file = g_file_new_for_uri (tracker_decorator_info_get_url (info));
	}

	return file;
}

static void
decorator_next_item_cb (TrackerDecorator *decorator,
                        GAsyncResult     *result,
                        gpointer          user_data)
{
	TrackerExtractDecoratorPrivate *priv;
	TrackerDecoratorInfo *info;
	GError *error = NULL;
	ExtractData *data;
	GTask *task;

	priv = TRACKER_EXTRACT_DECORATOR (decorator)->priv;
	info = tracker_decorator_next_finish (decorator, result, &error);

	if (!info) {
		priv->n_extracting_files--;

		if (error &&
		    error->domain == tracker_decorator_error_quark ()) {
			switch (error->code) {
			case TRACKER_DECORATOR_ERROR_EMPTY:
				g_message ("There are no further items to extract");
				break;
			case TRACKER_DECORATOR_ERROR_PAUSED:
				g_message ("Next item is on hold because miner is paused");
			}
		} else if (error) {
			g_warning ("Next item could not be processed, %s", error->message);
		}

		g_clear_error (&error);
		return;
	}

	data = g_new0 (ExtractData, 1);
	data->decorator = decorator;
	data->decorator_info = info;
	data->file = decorator_get_recovery_file (TRACKER_EXTRACT_DECORATOR (decorator), info);
	task = tracker_decorator_info_get_task (info);

	g_message ("Extracting metadata for '%s'", tracker_decorator_info_get_url (info));

	tracker_extract_persistence_add_file (priv->persistence, data->file);

	tracker_extract_file (priv->extractor,
	                      tracker_decorator_info_get_url (info),
	                      tracker_decorator_info_get_mimetype (info),
	                      g_task_get_cancellable (task),
	                      (GAsyncReadyCallback) get_metadata_cb, data);
}

static void
decorator_get_next_file (TrackerDecorator *decorator)
{
	TrackerExtractDecoratorPrivate *priv;
	guint available_items;

	priv = TRACKER_EXTRACT_DECORATOR (decorator)->priv;

	if (!tracker_miner_is_started (TRACKER_MINER (decorator)) ||
	    tracker_miner_is_paused (TRACKER_MINER (decorator)))
		return;

	available_items = tracker_decorator_get_n_items (decorator);
	while (priv->n_extracting_files < MAX_EXTRACTING_FILES &&
	       available_items > 0) {
		priv->n_extracting_files++;
		available_items--;
		tracker_decorator_next (decorator, NULL,
		                        (GAsyncReadyCallback) decorator_next_item_cb,
		                        NULL);
	}
}

static void
tracker_extract_decorator_paused (TrackerMiner *miner)
{
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (miner)->priv;
	g_message ("Decorator paused");

	if (priv->timer)
		g_timer_stop (priv->timer);
}

static void
tracker_extract_decorator_resumed (TrackerMiner *miner)
{
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (miner)->priv;
	g_message ("Decorator resumed, processing remaining %d items",
	           tracker_decorator_get_n_items (TRACKER_DECORATOR (miner)));

	if (priv->timer)
		g_timer_continue (priv->timer);

	decorator_get_next_file (TRACKER_DECORATOR (miner));
}

static void
tracker_extract_decorator_items_available (TrackerDecorator *decorator)
{
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (decorator)->priv;
	g_message ("Starting to process %d items",
	           tracker_decorator_get_n_items (decorator));

	priv->timer = g_timer_new ();
	if (tracker_miner_is_paused (TRACKER_MINER (decorator)))
		g_timer_stop (priv->timer);

	decorator_get_next_file (decorator);
}

static void
tracker_extract_decorator_finished (TrackerDecorator *decorator)
{
	TrackerExtractDecoratorPrivate *priv;
	gchar *time_str;

	priv = TRACKER_EXTRACT_DECORATOR (decorator)->priv;
	time_str = tracker_seconds_to_string ((gint) g_timer_elapsed (priv->timer, NULL), TRUE);
	g_message ("Extraction finished in %s", time_str);
	g_timer_destroy (priv->timer);
	priv->timer = NULL;
	g_free (time_str);
}

static gboolean
strv_contains (const gchar * const *strv,
               const gchar         *needle)
{
	guint i;

	for (i = 0; strv[i] != NULL; i++) {
		if (g_str_equal (strv[i], needle))
			return TRUE;
	}

	return FALSE;
}

static void
string_array_add (GPtrArray   *array,
                  const gchar *str)
{
	guint i;

	for (i = 0; i < array->len; i++) {
		if (g_str_equal (g_ptr_array_index (array, i), str))
			return;
	}

	g_ptr_array_add (array, (gchar *) str);
}

static void
priority_changed (TrackerExtractDecorator *decorator)
{
	TrackerExtractDecoratorPrivate *priv;
	GPtrArray                      *array;
	GHashTableIter                  iter;
	gpointer                        value;

	priv = TRACKER_EXTRACT_DECORATOR (decorator)->priv;

	/* Construct an strv removing dups */
	array = g_ptr_array_new ();
	g_hash_table_iter_init (&iter, priv->apps);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		AppData *data = value;
		guint i;

		for (i = 0; data->rdf_types[i] != NULL; i++) {
			string_array_add (array, data->rdf_types[i]);
		}
	}
	g_ptr_array_add (array, NULL);

	tracker_decorator_set_priority_rdf_types (TRACKER_DECORATOR (decorator),
	                                          (const gchar * const *) array->pdata);

	g_ptr_array_unref (array);
}

static void
app_data_destroy (AppData *data)
{
	g_bus_unwatch_name (data->watch_id);
	g_strfreev (data->rdf_types);
	g_slice_free (AppData, data);
}

static void
name_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
	TrackerExtractDecorator        *decorator = user_data;
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (decorator)->priv;
	g_hash_table_remove (priv->apps, name);
	priority_changed (decorator);
}

static gboolean
handle_set_rdf_types_cb (TrackerExtractDBusPriority *iface,
                         GDBusMethodInvocation      *invocation,
                         const gchar * const        *rdf_types,
                         TrackerExtractDecorator    *decorator)
{
	TrackerExtractDecoratorPrivate *priv;
	const gchar *sender;
	GDBusConnection *conn;
	AppData *data;
	guint i;

	priv = TRACKER_EXTRACT_DECORATOR (decorator)->priv;
	sender = g_dbus_method_invocation_get_sender (invocation);
	conn = g_dbus_method_invocation_get_connection (invocation);

	if (rdf_types[0] == NULL) {
		g_hash_table_remove (priv->apps, sender);
		goto out;
	}

	/* Verify all types are supported */
	for (i = 0; rdf_types[i] != NULL; i++) {
		if (!strv_contains (supported_classes, rdf_types[i])) {
			g_dbus_method_invocation_return_error (invocation,
			                                       TRACKER_DBUS_ERROR,
			                                       TRACKER_DBUS_ERROR_ASSERTION_FAILED,
			                                       "Unsupported rdf:type %s",
			                                       rdf_types[i]);
			return TRUE;
		}
	}

	data = g_hash_table_lookup (priv->apps, sender);
	if (data == NULL) {
		data = g_slice_new0 (AppData);
		data->watch_id = g_bus_watch_name_on_connection (conn,
		                                                 sender,
		                                                 G_BUS_NAME_WATCHER_FLAGS_NONE,
		                                                 NULL,
		                                                 name_vanished_cb,
		                                                 decorator, NULL);
		g_hash_table_insert (priv->apps,
		                     g_strdup (sender),
		                     data);
	} else {
		g_strfreev (data->rdf_types);
	}

	data->rdf_types = g_strdupv ((GStrv) rdf_types);

out:
	priority_changed (decorator);

	tracker_extract_dbus_priority_complete_set_rdf_types (iface, invocation);

	return TRUE;
}

static gboolean
handle_clear_rdf_types_cb (TrackerExtractDBusPriority *iface,
                           GDBusMethodInvocation      *invocation,
                           TrackerExtractDecorator    *decorator)
{
	TrackerExtractDecoratorPrivate *priv;
	const gchar *sender;

	priv = TRACKER_EXTRACT_DECORATOR (decorator)->priv;
	sender = g_dbus_method_invocation_get_sender (invocation);

	g_hash_table_remove (priv->apps, sender);
	priority_changed (decorator);

	tracker_extract_dbus_priority_complete_clear_rdf_types (iface, invocation);

	return TRUE;
}

static void
tracker_extract_decorator_class_init (TrackerExtractDecoratorClass *klass)
{
	TrackerDecoratorClass *decorator_class = TRACKER_DECORATOR_CLASS (klass);
	TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_extract_decorator_finalize;
	object_class->get_property = tracker_extract_decorator_get_property;
	object_class->set_property = tracker_extract_decorator_set_property;

	miner_class->paused = tracker_extract_decorator_paused;
	miner_class->resumed = tracker_extract_decorator_resumed;

	decorator_class->items_available = tracker_extract_decorator_items_available;
	decorator_class->finished = tracker_extract_decorator_finished;

	g_object_class_install_property (object_class,
	                                 PROP_EXTRACTOR,
	                                 g_param_spec_object ("extractor",
	                                                      "Extractor",
	                                                      "Extractor",
	                                                      TRACKER_TYPE_EXTRACT,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class,
	                          sizeof (TrackerExtractDecoratorPrivate));
}

static void
decorator_retry_file (GFile    *file,
                      gpointer  user_data)
{
	TrackerExtractDecorator *decorator = user_data;
	TrackerExtractDecoratorPrivate *priv = decorator->priv;
	gchar *path;

	path = g_file_get_uri (file);
	g_hash_table_insert (priv->recovery_files, path, g_object_ref (file));
	tracker_decorator_fs_prepend_file (TRACKER_DECORATOR_FS (decorator), file);
}

static void
decorator_ignore_file (GFile    *file,
                       gpointer  user_data)
{
	TrackerExtractDecorator *decorator = user_data;
	TrackerSparqlConnection *conn;
	GError *error = NULL;
	gchar *uri, *query;

	uri = g_file_get_uri (file);
	g_message ("Extraction on file '%s' has been attempted too many times, ignoring", uri);

	conn = tracker_miner_get_connection (TRACKER_MINER (decorator));
	query = g_strdup_printf ("INSERT { GRAPH <" TRACKER_OWN_GRAPH_URN "> {"
	                         "  ?urn nie:dataSource <" TRACKER_EXTRACT_DATA_SOURCE ">;"
	                         "       nie:dataSource <" TRACKER_EXTRACT_FAILURE_DATA_SOURCE ">."
	                         "} } WHERE {"
	                         "  ?urn nie:url \"%s\""
	                         "}", uri);

	tracker_sparql_connection_update (conn, query, G_PRIORITY_DEFAULT, NULL, &error);

	if (error) {
		g_warning ("Failed to update ignored file '%s': %s",
		           uri, error->message);
		g_error_free (error);
	}

	g_free (query);
	g_free (uri);
}

static void
tracker_extract_decorator_init (TrackerExtractDecorator *decorator)
{
	TrackerExtractDecoratorPrivate *priv;

	decorator->priv = priv = TRACKER_EXTRACT_DECORATOR_GET_PRIVATE (decorator);
	priv->recovery_files = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                              (GDestroyNotify) g_free,
	                                              (GDestroyNotify) g_object_unref);
}

static gboolean
tracker_extract_decorator_initable_init (GInitable     *initable,
                                         GCancellable  *cancellable,
                                         GError       **error)
{
	TrackerExtractDecorator        *decorator;
	TrackerExtractDecoratorPrivate *priv;
	GDBusConnection                *conn;
	gboolean                        ret = TRUE;

	decorator = TRACKER_EXTRACT_DECORATOR (initable);
	priv = decorator->priv;

	priv->apps = g_hash_table_new_full (g_str_hash,
	                                    g_str_equal,
	                                    g_free,
	                                    (GDestroyNotify) app_data_destroy);

	priv->iface = tracker_extract_dbus_priority_skeleton_new ();
	g_signal_connect (priv->iface, "handle-set-rdf-types",
	                  G_CALLBACK (handle_set_rdf_types_cb),
	                  decorator);
	g_signal_connect (priv->iface, "handle-clear-rdf-types",
	                  G_CALLBACK (handle_clear_rdf_types_cb),
	                  decorator);

	tracker_extract_dbus_priority_set_supported_rdf_types (priv->iface,
	                                                       supported_classes);

	conn = g_bus_get_sync (TRACKER_IPC_BUS, NULL, error);
	if (conn == NULL) {
		ret = FALSE;
		goto out;
	}

	if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (priv->iface),
	                                       conn,
	                                       "/org/freedesktop/Tracker1/Extract/Priority",
	                                       error)) {
		ret = FALSE;
		goto out;
	}

	/* Chainup to parent's init last, to have a chance to export our
	 * DBus interface before RequestName returns. Otherwise our iface
	 * won't be ready by the time the tracker-extract appear on the bus. */
	if (!parent_initable_iface->init (initable, cancellable, error)) {
		ret = FALSE;
	}

	priv->persistence = tracker_extract_persistence_initialize (decorator_retry_file,
	                                                            decorator_ignore_file,
	                                                            decorator);
out:
	g_clear_object (&conn);

	return ret;
}

static void
tracker_extract_decorator_initable_iface_init (GInitableIface *iface)
{
	parent_initable_iface = g_type_interface_peek_parent (iface);
	iface->init = tracker_extract_decorator_initable_init;
}

TrackerDecorator *
tracker_extract_decorator_new (TrackerExtract  *extract,
                               GCancellable    *cancellable,
                               GError         **error)
{
	return g_initable_new (TRACKER_TYPE_EXTRACT_DECORATOR,
	                       cancellable, error,
	                       "name", "Extract",
	                       "data-source", TRACKER_EXTRACT_DATA_SOURCE,
	                       "class-names", supported_classes,
	                       "extractor", extract,
	                       NULL);
}
