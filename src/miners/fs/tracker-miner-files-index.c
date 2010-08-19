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

#include "tracker-miner-files-index.h"
#include "tracker-dbus.h"
#include "tracker-marshal.h"

typedef struct {
	guint request_id;
	DBusGMethodInvocation *context;
	TrackerSparqlConnection *connection;
	TrackerMinerFiles *miner_files;
} MimeTypesData;

typedef struct {
	TrackerMinerFiles *files_miner;
} TrackerMinerFilesIndexPrivate;

enum {
	PROP_0,
	PROP_FILES_MINER
};

#define TRACKER_MINER_FILES_INDEX_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_FILES_INDEX, TrackerMinerFilesIndexPrivate))

static void     index_set_property        (GObject              *object,
                                           guint                 param_id,
                                           const GValue         *value,
                                           GParamSpec           *pspec);
static void     index_get_property        (GObject              *object,
                                           guint                 param_id,
                                           GValue               *value,
                                           GParamSpec           *pspec);
static void     index_finalize            (GObject              *object);

G_DEFINE_TYPE(TrackerMinerFilesIndex, tracker_miner_files_index, G_TYPE_OBJECT)

static void
tracker_miner_files_index_class_init (TrackerMinerFilesIndexClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = index_finalize;
	object_class->set_property = index_set_property;
	object_class->get_property = index_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_FILES_MINER,
	                                 g_param_spec_object ("files_miner",
	                                                      "files_miner",
	                                                      "The FS Miner",
	                                                      TRACKER_TYPE_MINER_FILES,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (klass, sizeof (TrackerMinerFilesIndexPrivate));
}

static void
index_set_property (GObject      *object,
                    guint         param_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
	TrackerMinerFilesIndexPrivate *priv;

	priv = TRACKER_MINER_FILES_INDEX_GET_PRIVATE (object);

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
index_get_property (GObject    *object,
                    guint       param_id,
                    GValue     *value,
                    GParamSpec *pspec)
{
	TrackerMinerFilesIndexPrivate *priv;

	priv = TRACKER_MINER_FILES_INDEX_GET_PRIVATE (object);

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
index_finalize (GObject *object)
{
	TrackerMinerFilesIndexPrivate *priv = TRACKER_MINER_FILES_INDEX_GET_PRIVATE (object);
	g_object_unref (priv->files_miner);
}

static void
tracker_miner_files_index_init (TrackerMinerFilesIndex *object)
{
}

static MimeTypesData *
mime_types_data_new (guint                    request_id,
                     DBusGMethodInvocation   *context,
                     TrackerSparqlConnection *connection,
                     TrackerMinerFiles       *miner_files)
{
	MimeTypesData *mtd;

	mtd = g_slice_new0 (MimeTypesData);

	mtd->miner_files = g_object_ref (miner_files);
	mtd->request_id = request_id;
	mtd->context = context;
	mtd->connection = g_object_ref (connection);

	return mtd;
}

static void
mime_types_data_destroy (gpointer data)
{
	MimeTypesData *mtd;

	mtd = data;

	g_object_unref (mtd->miner_files);
	g_object_unref (mtd->connection);

	g_slice_free (MimeTypesData, mtd);
}

static void
mime_types_cb (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
	MimeTypesData *mtd = user_data;
        TrackerSparqlCursor *cursor;
        GError *error = NULL;

        cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (object), result, &error);

	if (!error) {
		tracker_dbus_request_comment (mtd->request_id, mtd->context,
		                              "Found files that will need reindexing");

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
                        GFile *file;
			const gchar *url;

                        url = tracker_sparql_cursor_get_string (cursor, 0, NULL);
                        file = g_file_new_for_uri (url);
                        tracker_miner_fs_check_file (TRACKER_MINER_FS (mtd->miner_files), file, FALSE);
                        g_object_unref (file);
		}

		tracker_dbus_request_success (mtd->request_id, mtd->context);
		dbus_g_method_return (mtd->context);
	} else {
		tracker_dbus_request_success (mtd->request_id, mtd->context);
		dbus_g_method_return_error (mtd->context, error);
	}


	mime_types_data_destroy (user_data);
}

TrackerMinerFilesIndex *
tracker_miner_files_index_new (TrackerMinerFiles *miner_files)
{
	return g_object_new (TRACKER_TYPE_MINER_FILES_INDEX,
	                     "files-miner", miner_files,
	                     NULL);
}

void
tracker_miner_files_index_reindex_mime_types (TrackerMinerFilesIndex  *object,
                                              gchar                  **mime_types,
                                              DBusGMethodInvocation   *context,
                                              GError                 **error)
{
	TrackerMinerFilesIndexPrivate *priv;
	GString *query;
        GError *inner_error = NULL;
	TrackerSparqlConnection *connection;
	guint request_id;
	gint len, i;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (mime_types != NULL, context);

	len = g_strv_length (mime_types);
	tracker_dbus_async_return_if_fail (len > 0, context);

	tracker_dbus_request_new (request_id, context, "%s()", __FUNCTION__);

	connection = tracker_sparql_connection_get (&inner_error);

	if (!connection) {
		tracker_dbus_request_failed (request_id,
		                             context,
		                             &inner_error,
		                             NULL);
		dbus_g_method_return_error (context, inner_error);
		g_error_free (inner_error);
		return;
	}

	tracker_dbus_request_comment (request_id, context,
	                              "Attempting to reindex the following mime types:");


	query = g_string_new ("SELECT ?url "
	                      "WHERE {"
	                      "  ?resource nie:url ?url ;"
	                      "  nie:mimeType ?mime ."
	                      "  FILTER(");

	for (i = 0; i < len; i++) {
		tracker_dbus_request_comment (request_id, context, "  %s", mime_types[i]);
		g_string_append_printf (query, "?mime = '%s'", mime_types[i]);

		if (i < len - 1) {
			g_string_append (query, " || ");
		}
	}

	g_string_append (query, ") }");

	priv = TRACKER_MINER_FILES_INDEX_GET_PRIVATE (object);

	/* FIXME: save last call id */
	tracker_sparql_connection_query_async (connection,
                                               query->str,
                                               NULL,
                                               mime_types_cb,
                                               mime_types_data_new (request_id,
                                                                    context,
                                                                    connection,
                                                                    priv->files_miner));

	g_string_free (query, TRUE);
	g_object_unref (connection);
}

void
tracker_miner_files_index_index_file (TrackerMinerFilesIndex    *object,
                                      gchar                     *file_uri,
                                      DBusGMethodInvocation     *context,
                                      GError                   **error)
{
	TrackerMinerFilesIndexPrivate *priv;
	TrackerConfig *config;
	guint request_id;
	GFile *file, *parent;
	GError *err;

	tracker_dbus_async_return_if_fail (file_uri != NULL, context);

	request_id = tracker_dbus_get_next_request_id ();
	tracker_dbus_request_new (request_id, context, "%s()", __FUNCTION__);

	priv = TRACKER_MINER_FILES_INDEX_GET_PRIVATE (object);
	file = g_file_new_for_uri (file_uri);
	parent = g_file_get_parent (file);

	g_object_get (priv->files_miner,
	              "config", &config,
	              NULL);

	if (parent) {
		gboolean found = FALSE;
		GSList *l;

		if (!tracker_miner_files_check_directory (parent,
		                                          tracker_config_get_index_recursive_directories (config),
		                                          tracker_config_get_index_single_directories (config),
		                                          tracker_config_get_ignored_directory_paths (config),
		                                          tracker_config_get_ignored_directory_patterns (config))) {
			err = g_error_new_literal (1, 0, "File is not eligible to be indexed");
			dbus_g_method_return_error (context, err);
			tracker_dbus_request_success (request_id, context);

			g_error_free (err);

			return;
		}

		l = tracker_config_get_index_recursive_directories (config);

		while (l && !found) {
			GFile *dir;

			dir = g_file_new_for_path ((gchar *) l->data);

			if (g_file_equal (parent, dir) ||
			    g_file_has_prefix (parent, dir)) {
				found = TRUE;
			}

			g_object_unref (dir);
			l = l->next;
		}

		l = tracker_config_get_index_single_directories (config);

		while (l && !found) {
			GFile *dir;

			dir = g_file_new_for_path ((gchar *) l->data);

			if (g_file_equal (parent, dir)) {
				found = TRUE;
			}

			g_object_unref (dir);
			l = l->next;
		}

		if (!found) {
			err = g_error_new_literal (1, 0, "File is not eligible to be indexed");
			dbus_g_method_return_error (context, err);
			tracker_dbus_request_success (request_id, context);

			g_error_free (err);

			return;
		}

		g_object_unref (parent);
	}

	if (!tracker_miner_files_check_file (file,
	                                     tracker_config_get_ignored_file_paths (config),
	                                     tracker_config_get_ignored_file_patterns (config))) {
		err = g_error_new_literal (1, 0, "File is not eligible to be indexed");
		dbus_g_method_return_error (context, err);
		tracker_dbus_request_success (request_id, context);

		g_error_free (err);

		return;
	}

	tracker_miner_fs_check_file (TRACKER_MINER_FS (priv->files_miner), file, TRUE);

	tracker_dbus_request_success (request_id, context);
	dbus_g_method_return (context);

	g_object_unref (file);
	g_object_unref (config);
}
