/*
 * Copyright (C) 2014 Carlos Garnacho  <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#include "config.h"

#include <glib.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-decorator-private.h"
#include "tracker-decorator-fs.h"

#define TRACKER_DECORATOR_FS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DECORATOR_FS, TrackerDecoratorFSPrivate))

/**
 * SECTION:tracker-decorator-fs
 * @short_description: Filesystem implementation for TrackerDecorator
 * @include: libtracker-miner/tracker-miner.h
 * @title: TrackerDecoratorFS
 * @see_also: #TrackerDecorator
 *
 * #TrackerDecoratorFS is used to handle extended metadata extraction
 * for resources on file systems that are mounted or unmounted.
 **/

typedef struct _TrackerDecoratorFSPrivate TrackerDecoratorFSPrivate;

struct _TrackerDecoratorFSPrivate {
	GVolumeMonitor *volume_monitor;
};

static GInitableIface *parent_initable_iface;

static void tracker_decorator_fs_initable_iface_init (GInitableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (TrackerDecoratorFS, tracker_decorator_fs,
                                  TRACKER_TYPE_DECORATOR,
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, tracker_decorator_fs_initable_iface_init))

static void
tracker_decorator_fs_finalize (GObject *object)
{
	TrackerDecoratorFSPrivate *priv;

	priv = TRACKER_DECORATOR_FS (object)->priv;

	if (priv->volume_monitor)
		g_object_unref (priv->volume_monitor);

	G_OBJECT_CLASS (tracker_decorator_fs_parent_class)->finalize (object);
}

static void
tracker_decorator_fs_class_init (TrackerDecoratorFSClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_decorator_fs_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerDecoratorFSPrivate));
}

static void
process_files_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
	TrackerSparqlConnection *conn;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	conn = TRACKER_SPARQL_CONNECTION (object);
	cursor = tracker_sparql_connection_query_finish (conn, result, &error);

        if (error) {
                g_critical ("Could not check files on mount point for missing metadata: %s", error->message);
                g_error_free (error);
		return;
	}

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		gint id = tracker_sparql_cursor_get_integer (cursor, 0);
		gint class_name_id = tracker_sparql_cursor_get_integer (cursor, 1);
		tracker_decorator_prepend_id (TRACKER_DECORATOR (user_data), id, class_name_id);
	}

	g_object_unref (cursor);
}

static void
remove_files_cb (GObject *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	TrackerSparqlConnection *conn;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	conn = TRACKER_SPARQL_CONNECTION (object);
	cursor = tracker_sparql_connection_query_finish (conn, result, &error);

        if (error) {
                g_critical ("Could not remove files on mount point with missing metadata: %s", error->message);
                g_error_free (error);
		return;
	}

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		gint id = tracker_sparql_cursor_get_integer (cursor, 0);
		tracker_decorator_delete_id (TRACKER_DECORATOR (user_data), id);
	}

	g_object_unref (cursor);
}

static void
_tracker_decorator_query_append_rdf_type_filter (TrackerDecorator *decorator,
                                                 GString          *query)
{
       const gchar **class_names;
       gint i = 0;

       class_names = tracker_decorator_get_class_names (decorator);

       if (!class_names || !*class_names)
               return;

       g_string_append (query, "&& ?type IN (");

       while (class_names[i]) {
               if (i != 0)
                       g_string_append (query, ",");

               g_string_append (query, class_names[i]);
               i++;
       }

       g_string_append (query, ") ");
}

static void
check_files (TrackerDecorator    *decorator,
             const gchar         *mount_point_urn,
             gboolean             available,
             GAsyncReadyCallback  callback)
{
	TrackerSparqlConnection *sparql_conn;
	const gchar *data_source;
	GString *query;

	data_source = tracker_decorator_get_data_source (decorator);
	query = g_string_new ("SELECT tracker:id(?urn) tracker:id(?type) { ?urn ");

	if (mount_point_urn) {
		g_string_append_printf (query,
		                        " nie:dataSource <%s> ;",
		                        mount_point_urn);
	}

	g_string_append (query, " a nfo:FileDataObject ;"
	                        " a ?type .");
	g_string_append_printf (query,
	                        "FILTER (! EXISTS { ?urn nie:dataSource <%s> } ",
	                        data_source);

	_tracker_decorator_query_append_rdf_type_filter (decorator, query);

	if (available)
		g_string_append (query, "&& BOUND(tracker:available(?urn))");

	g_string_append (query, ")}");

	sparql_conn = tracker_miner_get_connection (TRACKER_MINER (decorator));
	tracker_sparql_connection_query_async (sparql_conn, query->str,
	                                       NULL, callback, decorator);
	g_string_free (query, TRUE);
}

static inline gchar *
mount_point_get_uuid (GMount *mount)
{
	GVolume *volume;
	gchar *uuid = NULL;

	volume = g_mount_get_volume (mount);
	if (volume) {
		uuid = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UUID);
		if (!uuid) {
			gchar *mount_name;

			mount_name = g_mount_get_name (mount);
			uuid = g_compute_checksum_for_string (G_CHECKSUM_MD5, mount_name, -1);
			g_free (mount_name);
		}

		g_object_unref (volume);
	}

	return uuid;
}

static void
mount_point_added_cb (GVolumeMonitor *monitor,
                      GMount         *mount,
                      gpointer        user_data)
{
	gchar *uuid;
	gchar *urn;

	uuid = mount_point_get_uuid (mount);
	urn = g_strdup_printf (TRACKER_PREFIX_DATASOURCE_URN "%s", uuid);
	check_files (user_data, urn, TRUE, process_files_cb);
	g_free (urn);
	g_free (uuid);
}

static void
mount_point_removed_cb (GVolumeMonitor *monitor,
                        GMount         *mount,
                        gpointer        user_data)
{
	gchar *uuid;
	gchar *urn;

	uuid = mount_point_get_uuid (mount);
	urn = g_strdup_printf (TRACKER_PREFIX_DATASOURCE_URN "%s", uuid);
	_tracker_decorator_invalidate_cache (user_data);
	check_files (user_data, urn, FALSE, remove_files_cb);
	g_free (urn);
	g_free (uuid);
}

static gboolean
tracker_decorator_fs_iface_init (GInitable     *initable,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
	TrackerDecoratorFSPrivate *priv;

	priv = TRACKER_DECORATOR_FS (initable)->priv;

	priv->volume_monitor = g_volume_monitor_get ();
	g_signal_connect_object (priv->volume_monitor, "mount-added",
	                         G_CALLBACK (mount_point_added_cb), initable, 0);
	g_signal_connect_object (priv->volume_monitor, "mount-pre-unmount",
	                         G_CALLBACK (mount_point_removed_cb), initable, 0);
	g_signal_connect_object (priv->volume_monitor, "mount-removed",
	                         G_CALLBACK (mount_point_removed_cb), initable, 0);

	return parent_initable_iface->init (initable, cancellable, error);
}

static void
tracker_decorator_fs_initable_iface_init (GInitableIface *iface)
{
	parent_initable_iface = g_type_interface_peek_parent (iface);
	iface->init = tracker_decorator_fs_iface_init;
}

static void
tracker_decorator_fs_init (TrackerDecoratorFS *decorator)
{
	decorator->priv = TRACKER_DECORATOR_FS_GET_PRIVATE (decorator);
}

/**
 * tracker_decorator_fs_prepend_file:
 * @decorator: a #TrackerDecoratorFS
 * @file: a #GFile to process
 *
 * Prepends a file for processing.
 *
 * Returns: the tracker:id of the element corresponding to the file
 *
 * Since: 1.2
 **/
gint
tracker_decorator_fs_prepend_file (TrackerDecoratorFS *decorator,
                                   GFile              *file)
{
	TrackerSparqlConnection *sparql_conn;
	TrackerSparqlCursor *cursor;
	gchar *query, *uri;
	gint id, class_id;

	g_return_val_if_fail (TRACKER_IS_DECORATOR_FS (decorator), 0);
	g_return_val_if_fail (G_IS_FILE (file), 0);

	uri = g_file_get_uri (file);
	query = g_strdup_printf ("SELECT tracker:id(?urn) tracker:id(?type) {"
	                         "  ?urn a ?type; nie:url \"%s\" "
	                         "}", uri);
	g_free (uri);

	sparql_conn = tracker_miner_get_connection (TRACKER_MINER (decorator));
	cursor = tracker_sparql_connection_query (sparql_conn, query,
	                                          NULL, NULL);
	g_free (query);

	if (!cursor)
		return 0;

	if (!tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		g_object_unref (cursor);
		return 0;
	}

	id = tracker_sparql_cursor_get_integer (cursor, 0);
	class_id = tracker_sparql_cursor_get_integer (cursor, 1);

	tracker_decorator_prepend_id (TRACKER_DECORATOR (decorator),
	                              id, class_id);
	g_object_unref (cursor);

	return id;
}
