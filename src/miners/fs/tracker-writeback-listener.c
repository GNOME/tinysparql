/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-miner/tracker-miner-dbus.h>

#include "tracker-writeback-listener.h"

#define TRACKER_SERVICE                 "org.freedesktop.Tracker1"
#define TRACKER_RESOURCES_OBJECT        "/org/freedesktop/Tracker1/Resources"
#define TRACKER_INTERFACE_RESOURCES     "org.freedesktop.Tracker1.Resources"

typedef struct {
	TrackerMinerFiles *files_miner;
	GDBusConnection *d_connection;
	TrackerSparqlConnection *connection;
	guint d_signal;
} TrackerWritebackListenerPrivate;

typedef struct {
	TrackerWritebackListener *self;
	gint subject_id;
	GStrv rdf_types;
} QueryData;

enum {
	PROP_0,
	PROP_FILES_MINER
};

#define TRACKER_WRITEBACK_LISTENER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_WRITEBACK_LISTENER, TrackerWritebackListenerPrivate))

static void     writeback_listener_set_property    (GObject              *object,
                                                    guint                 param_id,
                                                    const GValue         *value,
                                                    GParamSpec           *pspec);
static void     writeback_listener_get_property    (GObject              *object,
                                                    guint                 param_id,
                                                    GValue               *value,
                                                    GParamSpec           *pspec);
static void     writeback_listener_finalize        (GObject              *object);
static gboolean writeback_listener_initable_init   (GInitable            *initable,
                                                    GCancellable         *cancellable,
                                                    GError              **error);
static void     on_writeback_cb                    (GDBusConnection      *connection,
                                                    const gchar          *sender_name,
                                                    const gchar          *object_path,
                                                    const gchar          *interface_name,
                                                    const gchar          *signal_name,
                                                    GVariant             *parameters,
                                                    gpointer              user_data);

static void
writeback_listener_initable_iface_init (GInitableIface *iface)
{
	iface->init = writeback_listener_initable_init;
}

G_DEFINE_TYPE_WITH_CODE (TrackerWritebackListener, tracker_writeback_listener, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                writeback_listener_initable_iface_init));

static void
tracker_writeback_listener_class_init (TrackerWritebackListenerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = writeback_listener_finalize;
	object_class->set_property = writeback_listener_set_property;
	object_class->get_property = writeback_listener_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_FILES_MINER,
	                                 g_param_spec_object ("files_miner",
	                                                      "files_miner",
	                                                      "The FS Miner",
	                                                      TRACKER_TYPE_MINER_FILES,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (klass, sizeof (TrackerWritebackListenerPrivate));
}

static void
writeback_listener_set_property (GObject      *object,
                                 guint         param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
	TrackerWritebackListenerPrivate *priv;

	priv = TRACKER_WRITEBACK_LISTENER_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_FILES_MINER:
		priv->files_miner = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
writeback_listener_get_property (GObject    *object,
                                 guint       param_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
	TrackerWritebackListenerPrivate *priv;

	priv = TRACKER_WRITEBACK_LISTENER_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_FILES_MINER:
		g_value_set_object (value, priv->files_miner);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
writeback_listener_finalize (GObject *object)
{
	TrackerWritebackListenerPrivate *priv = TRACKER_WRITEBACK_LISTENER_GET_PRIVATE (object);

	if (priv->connection && priv->d_signal) {
		g_dbus_connection_signal_unsubscribe (priv->d_connection, priv->d_signal);
	}

	if (priv->connection) {
		g_object_unref (priv->connection);
	}

	if (priv->d_connection) {
		g_object_unref (priv->d_connection);
	}

	if (priv->files_miner) {
		g_object_unref (priv->files_miner);
	}
}


static void
tracker_writeback_listener_init (TrackerWritebackListener *object)
{
}

static gboolean
writeback_listener_initable_init (GInitable    *initable,
                                  GCancellable *cancellable,
                                  GError       **error)
{
	TrackerWritebackListenerPrivate *priv;
	GError *internal_error = NULL;

	priv = TRACKER_WRITEBACK_LISTENER_GET_PRIVATE (initable);

	priv->connection = tracker_sparql_connection_get (NULL, &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	priv->d_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	priv->d_signal = g_dbus_connection_signal_subscribe (priv->d_connection,
	                                                     TRACKER_SERVICE,
	                                                     TRACKER_INTERFACE_RESOURCES,
	                                                     "Writeback",
	                                                     TRACKER_RESOURCES_OBJECT,
	                                                     NULL,
	                                                     G_DBUS_SIGNAL_FLAGS_NONE,
	                                                     on_writeback_cb,
	                                                     initable,
	                                                     NULL);

	return TRUE;
}

TrackerWritebackListener *
tracker_writeback_listener_new (TrackerMinerFiles  *miner_files,
                                GError            **error)
{
	GObject *miner;
	GError *internal_error = NULL;

	miner =  g_initable_new (TRACKER_TYPE_WRITEBACK_LISTENER,
	                         NULL,
	                         &internal_error,
	                         "files-miner", miner_files,
	                         NULL);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return NULL;
	}

	return (TrackerWritebackListener *) miner;
}

static QueryData*
query_data_new (TrackerWritebackListener *self,
                gint subject_id)
{
	QueryData *data = g_slice_new0 (QueryData);

	data->self = g_object_ref (self);
	data->subject_id = subject_id;

	return data;
}

static void
query_data_free (QueryData *data)
{
	g_object_unref (data->self);
	if (data->rdf_types) {
		g_strfreev (data->rdf_types);
	}
	g_slice_free (QueryData, data);
}

static void
sparql_query_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	QueryData *data = user_data;
	TrackerWritebackListener *self = TRACKER_WRITEBACK_LISTENER (data->self);
	TrackerWritebackListenerPrivate *priv;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	priv = TRACKER_WRITEBACK_LISTENER_GET_PRIVATE (self);

	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (object), result, &error);

	if (!error) {
		guint cols = tracker_sparql_cursor_get_n_columns (cursor);
		GPtrArray *results = NULL;
		GFile *file = NULL;

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			GStrv row = NULL;
			guint i;

			if (file == NULL) {
				file = g_file_new_for_uri (tracker_sparql_cursor_get_string (cursor, 0, NULL));
				if (!g_file_query_exists (file, NULL)) {
					g_object_unref (file);
					g_message ("  No files qualify for updates");
					query_data_free (data);
					g_object_unref (cursor);
					return;
				}
			}

			for (i = 0; i < cols; i++) {
				if (!row) {
					row = g_new0 (gchar*, cols);
				}
				row[i] = g_strdup (tracker_sparql_cursor_get_string (cursor, i, NULL));
			}

			if (!results) {
				results = g_ptr_array_new_with_free_func ((GDestroyNotify) g_strfreev);
			}

			g_ptr_array_add (results, row);
		}

		if (results != NULL && results->len > 0) {
			tracker_miner_fs_writeback_file (TRACKER_MINER_FS (priv->files_miner),
			                                 file,
			                                 data->rdf_types,
			                                 results);
		} else {
			g_message ("  No files qualify for updates");
		}

		g_object_unref (file);
		g_ptr_array_unref (results);
		g_object_unref (cursor);
	} else {
		g_message ("  No files qualify for updates (%s)", error->message);
		g_error_free (error);
	}

	query_data_free (data);
}

static void
rdf_types_to_uris_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
	QueryData *data = user_data;
	TrackerWritebackListener *self = TRACKER_WRITEBACK_LISTENER (data->self);
	TrackerWritebackListenerPrivate *priv;
	TrackerSparqlCursor *cursor;
	TrackerSparqlConnection *connection;
	GError *error = NULL;

	priv = TRACKER_WRITEBACK_LISTENER_GET_PRIVATE (self);
	connection = priv->connection;

	cursor = tracker_sparql_connection_query_finish (connection, result, &error);

	if (!error) {
		gchar *query;
		const gchar *subject;
		GArray *rdf_types;

		rdf_types = g_array_new (TRUE, TRUE, sizeof (gchar *));

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			gchar *uri = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
			g_array_append_val (rdf_types, uri);
		}

		data->rdf_types = (GStrv) rdf_types->data;
		g_array_free (rdf_types, FALSE);

		g_object_unref (cursor);

		query = g_strdup_printf ("SELECT tracker:uri (%d) {}", data->subject_id);
		cursor = tracker_sparql_connection_query (connection, query, NULL, NULL);
		g_free (query);

		if (cursor && tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			subject = tracker_sparql_cursor_get_string (cursor, 0, NULL);
			if (!subject) {
				g_object_unref (cursor);
				goto trouble;
			}
		} else {
			if (cursor)
				g_object_unref (cursor);
			goto trouble;
		}

		query = g_strdup_printf ("SELECT ?url '%s' ?predicate ?object { "
		                            "<%s> a nfo:FileDataObject . "
		                            "<%s> ?predicate ?object ; nie:url ?url . "
		                            "?predicate tracker:writeback true . "
		                            "FILTER (NOT EXISTS { GRAPH <"TRACKER_MINER_FS_GRAPH_URN"> "
		                              "{ <%s> ?predicate ?object } }) } ",
		                         subject, subject, subject, subject);

		tracker_sparql_connection_query_async (priv->connection,
		                                       query,
		                                       NULL,
		                                       sparql_query_cb,
		                                       data);

		g_free (query);
		g_object_unref (cursor);

	} else {
		goto trouble;
	}

	return;

trouble:
	if (error) {
		g_message ("  No files qualify for updates (%s)", error->message);
		g_error_free (error);
	}
	query_data_free (data);
}

static void
on_writeback_cb (GDBusConnection      *connection,
                 const gchar          *sender_name,
                 const gchar          *object_path,
                 const gchar          *interface_name,
                 const gchar          *signal_name,
                 GVariant             *parameters,
                 gpointer              user_data)
{
	TrackerWritebackListener *self = TRACKER_WRITEBACK_LISTENER (user_data);
	TrackerWritebackListenerPrivate *priv;
	GVariantIter *iter1, *iter2;
	gint subject_id = 0, rdf_type = 0;

	priv = TRACKER_WRITEBACK_LISTENER_GET_PRIVATE (self);

	g_variant_get (parameters, "(a{iai})", &iter1);
	while (g_variant_iter_loop (iter1, "{iai}", &subject_id, &iter2)) {
		GString *query;
		gboolean comma = FALSE;
		QueryData *data;

		data = query_data_new (self, subject_id);

		query = g_string_new ("SELECT ?resource { ?resource a rdfs:Class . "
		                      "FILTER (tracker:id (?resource) IN (");

		while (g_variant_iter_loop (iter2, "i", &rdf_type)) {
			if (comma) {
				g_string_append_printf (query, ", %d", rdf_type);
			} else {
				g_string_append_printf (query, "%d", rdf_type);
				comma = TRUE;
			}
		}

		g_string_append (query, ")) }");

		tracker_sparql_connection_query_async (priv->connection,
		                                       query->str,
		                                       NULL,
		                                       rdf_types_to_uris_cb,
		                                       data);

		g_string_free (query, TRUE);
	}

	g_variant_iter_free (iter1);
}
