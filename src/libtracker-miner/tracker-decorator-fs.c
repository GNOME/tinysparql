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

#include <libtracker-common/tracker-ontologies.h>
#include <libtracker-common/tracker-storage.h>

#include "tracker-decorator-fs.h"
#include "tracker-decorator-internal.h"

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
	TrackerStorage *storage;
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

	if (priv->storage)
		g_object_unref (priv->storage);

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

static void
mount_point_added_cb (TrackerStorage *storage,
                      const gchar    *uuid,
                      const gchar    *mount_point,
                      const gchar    *mount_name,
                      gboolean        removable,
                      gboolean        optical,
                      gpointer        user_data)
{
	gchar *urn;

	urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s", uuid);
	check_files (user_data, urn, TRUE, process_files_cb);
	g_free (urn);
}

static void
mount_point_removed_cb (TrackerStorage *storage,
                        const gchar    *uuid,
                        const gchar    *mount_point,
                        gpointer        user_data)
{
	gchar *urn;

	urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s", uuid);
	check_files (user_data, urn, FALSE, remove_files_cb);
	g_free (urn);
}

static gboolean
tracker_decorator_fs_iface_init (GInitable     *initable,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
	TrackerDecoratorFSPrivate *priv;

	priv = TRACKER_DECORATOR_FS (initable)->priv;
	priv->storage = tracker_storage_new ();

	g_signal_connect (priv->storage, "mount-point-added",
	                  G_CALLBACK (mount_point_added_cb), initable);

	g_signal_connect (priv->storage, "mount-point-removed",
	                  G_CALLBACK (mount_point_removed_cb), initable);

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
