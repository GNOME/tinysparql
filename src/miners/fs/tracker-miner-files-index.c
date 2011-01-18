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

#include "tracker-miner-files-index.h"
#include "tracker-marshal.h"


static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.freedesktop.Tracker1.Miner.Files.Index'>"
  "    <method name='ReindexMimeTypes'>"
  "      <arg type='as' name='mime_types' direction='in' />"
  "    </method>"
  "    <method name='IndexFile'>"
  "      <arg type='s' name='file_uri' direction='in' />"
  "    </method>"
  "  </interface>"
  "</node>";

/* If defined, then a file provided to be indexed MUST be a child in
 * an configured path. if undefined, any file can be indexed, however
 * it is up to applications to maintain files outside the configured
 * locations.
 */
#undef REQUIRE_LOCATION_IN_CONFIG

typedef struct {
	TrackerDBusRequest *request;
	GDBusMethodInvocation *invocation;
	TrackerSparqlConnection *connection;
	TrackerMinerFiles *miner_files;
} MimeTypesData;

typedef struct {
	TrackerMinerFiles *files_miner;
	GDBusConnection *d_connection;
	GDBusNodeInfo *introspection_data;
	guint registration_id;
	guint own_id;
	gchar *full_name;
	gchar *full_path;
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
static void     index_constructed         (GObject              *object);

G_DEFINE_TYPE(TrackerMinerFilesIndex, tracker_miner_files_index, G_TYPE_OBJECT)

static void
tracker_miner_files_index_class_init (TrackerMinerFilesIndexClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = index_finalize;
	object_class->set_property = index_set_property;
	object_class->get_property = index_get_property;
	object_class->constructed  = index_constructed;

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

	if (priv->own_id != 0) {
		g_bus_unown_name (priv->own_id);
	}

	if (priv->registration_id != 0) {
		g_dbus_connection_unregister_object (priv->d_connection,
		                                     priv->registration_id);
	}

	if (priv->introspection_data) {
		g_dbus_node_info_unref (priv->introspection_data);
	}

	if (priv->d_connection) {
		g_object_unref (priv->d_connection);
	}

	g_free (priv->full_name);
	g_free (priv->full_path);

	g_object_unref (priv->files_miner);
}

static MimeTypesData *
mime_types_data_new (TrackerDBusRequest      *request,
                     GDBusMethodInvocation   *invocation,
                     TrackerSparqlConnection *connection,
                     TrackerMinerFiles       *miner_files)
{
	MimeTypesData *mtd;

	mtd = g_slice_new0 (MimeTypesData);

	mtd->miner_files = g_object_ref (miner_files);
	mtd->request = request;
	mtd->invocation = invocation;
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

	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (object),
	                                                 result,
	                                                 &error);

	if (cursor) {
		tracker_dbus_request_comment (mtd->request,
		                              "Found files that will need reindexing");

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			GFile *file;
			const gchar *url;

			url = tracker_sparql_cursor_get_string (cursor, 0, NULL);
			file = g_file_new_for_uri (url);
			tracker_miner_fs_check_file (TRACKER_MINER_FS (mtd->miner_files), file, FALSE);
			g_object_unref (file);
		}

		tracker_dbus_request_end (mtd->request, NULL);
		g_dbus_method_invocation_return_value (mtd->invocation, NULL);
	} else {
		tracker_dbus_request_end (mtd->request, error);
		g_dbus_method_invocation_return_gerror (mtd->invocation, error);
	}

	mime_types_data_destroy (user_data);
}

static void
tracker_miner_files_index_reindex_mime_types (TrackerMinerFilesIndex *miner,
                                              GDBusMethodInvocation  *invocation,
                                              GVariant               *parameters)
{
	TrackerMinerFilesIndexPrivate *priv;
	GString *query;
	GError *inner_error = NULL;
	TrackerSparqlConnection *connection;
	TrackerDBusRequest *request;
	gint len, i;
	GStrv mime_types = NULL;

	priv = TRACKER_MINER_FILES_INDEX_GET_PRIVATE (miner);

	g_variant_get (parameters, "(^a&s)", &mime_types);

	len = mime_types ? g_strv_length (mime_types) : 0;

	tracker_gdbus_async_return_if_fail (len > 0, invocation);

	request = tracker_g_dbus_request_begin (invocation, "%s(%d mime types)",
	                                        __FUNCTION__,
	                                        len);

	connection = tracker_sparql_connection_get (NULL, &inner_error);

	if (!connection) {
		g_free (mime_types);
		tracker_dbus_request_end (request, inner_error);
		g_dbus_method_invocation_return_gerror (invocation, inner_error);
		g_error_free (inner_error);
		return;
	}

	tracker_dbus_request_comment (request,
	                              "Attempting to reindex the following mime types:");

	query = g_string_new ("SELECT ?url "
	                      "WHERE {"
	                      "  ?resource nie:url ?url ;"
	                      "  nie:mimeType ?mime ."
	                      "  FILTER(");

	for (i = 0; i < len; i++) {
		tracker_dbus_request_comment (request, "  %s", mime_types[i]);
		g_string_append_printf (query, "?mime = '%s'", mime_types[i]);

		if (i < len - 1) {
			g_string_append (query, " || ");
		}
	}

	g_string_append (query, ") }");

	/* FIXME: save last call id */
	tracker_sparql_connection_query_async (connection,
	                                       query->str,
	                                       NULL,
	                                       mime_types_cb,
	                                       mime_types_data_new (request,
	                                                            invocation,
	                                                            connection,
	                                                            priv->files_miner));

	tracker_dbus_request_end (request, NULL);
	g_dbus_method_invocation_return_value (invocation, NULL);

	g_string_free (query, TRUE);
	g_object_unref (connection);
	g_free (mime_types);
}

static void
handle_method_call_index_file (TrackerMinerFilesIndex *miner,
                               GDBusMethodInvocation  *invocation,
                               GVariant               *parameters)
{
	TrackerMinerFilesIndexPrivate *priv;
	TrackerConfig *config;
	TrackerDBusRequest *request;
	GFile *file, *dir;
	GFileInfo *file_info;
	gboolean is_dir;
	gboolean do_checks = FALSE;
	GError *internal_error;
	const gchar *file_uri;

	priv = TRACKER_MINER_FILES_INDEX_GET_PRIVATE (miner);

	g_variant_get (parameters, "(&s)", &file_uri);

	tracker_gdbus_async_return_if_fail (file_uri != NULL, invocation);

	request = tracker_g_dbus_request_begin (invocation, "%s(uri:'%s')", __FUNCTION__, file_uri);

	file = g_file_new_for_uri (file_uri);

	g_object_get (priv->files_miner,
	              "config", &config,
	              NULL);

	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_TYPE,
	                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                               NULL, NULL);

	if (!file_info) {
		internal_error = g_error_new_literal (1, 0, "File does not exist");
		tracker_dbus_request_end (request, internal_error);
		g_dbus_method_invocation_return_gerror (invocation, internal_error);

		g_error_free (internal_error);

		return;
	}

	is_dir = (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY);
	g_object_unref (file_info);

#ifdef REQUIRE_LOCATION_IN_CONFIG
	do_checks = TRUE;
#endif /* REQUIRE_LOCATION_IN_CONFIG */

	if (is_dir) {
		dir = g_object_ref (file);
	} else {
#ifdef REQUIRE_LOCATION_IN_CONFIG
		if (!tracker_miner_files_check_file (file,
		                                     tracker_config_get_ignored_file_paths (config),
		                                     tracker_config_get_ignored_file_patterns (config))) {
			internal_error = g_error_new_literal (1, 0, "File is not eligible to be indexed");
			tracker_dbus_request_end (request, internal_error);
			g_dbus_method_invocation_return_gerror (invocation, internal_error);

			g_error_free (internal_error);

			return;
		}
#endif /* REQUIRE_LOCATION_IN_CONFIG */

		dir = g_file_get_parent (file);
	}

	if (dir) {
#ifdef REQUIRE_LOCATION_IN_CONFIG
		gboolean found = FALSE;
		GSList *l;

		if (!tracker_miner_files_check_directory (dir,
		                                          tracker_config_get_index_recursive_directories (config),
		                                          tracker_config_get_index_single_directories (config),
		                                          tracker_config_get_ignored_directory_paths (config),
		                                          tracker_config_get_ignored_directory_patterns (config))) {
			internal_error = g_error_new_literal (1, 0, "File is not eligible to be indexed");
			tracker_dbus_request_end (request, internal_error);
			g_dbus_method_invocation_return_gerror (context, internal_error);

			g_error_free (internal_error);

			return;
		}

		l = tracker_config_get_index_recursive_directories (config);

		while (l && !found) {
			GFile *config_dir;

			config_dir = g_file_new_for_path ((gchar *) l->data);

			if (g_file_equal (dir, config_dir) ||
			    g_file_has_prefix (dir, config_dir)) {
				found = TRUE;
			}

			g_object_unref (config_dir);
			l = l->next;
		}

		l = tracker_config_get_index_single_directories (config);

		while (l && !found) {
			GFile *config_dir;

			config_dir = g_file_new_for_path ((gchar *) l->data);

			if (g_file_equal (dir, config_dir)) {
				found = TRUE;
			}

			g_object_unref (config_dir);
			l = l->next;
		}

		if (!found) {
			internal_error = g_error_new_literal (1, 0, "File is not eligible to be indexed");
			tracker_dbus_request_end (request, internal_error);
			g_dbus_method_invocation_return_gerror (invocation, internal_error);

			g_error_free (internal_error);

			return;
		}
#endif /* REQUIRE_LOCATION_IN_CONFIG */

		g_object_unref (dir);
	}

	if (is_dir) {
		tracker_miner_fs_check_directory (TRACKER_MINER_FS (priv->files_miner), file, do_checks);
	} else {
		tracker_miner_fs_check_file (TRACKER_MINER_FS (priv->files_miner), file, do_checks);
	}

	tracker_dbus_request_end (request, NULL);
	g_dbus_method_invocation_return_value (invocation, NULL);

	g_object_unref (file);
	g_object_unref (config);
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
	TrackerMinerFilesIndex *miner = user_data;

	tracker_gdbus_async_return_if_fail (miner != NULL, invocation);
	tracker_gdbus_async_return_if_fail (TRACKER_IS_MINER_FILES_INDEX (miner), invocation);

	if (g_strcmp0 (method_name, "ReindexMimeTypes") == 0) {
		tracker_miner_files_index_reindex_mime_types (miner, invocation, parameters);
	} else if (g_strcmp0 (method_name, "IndexFile") == 0) {
		handle_method_call_index_file (miner, invocation, parameters);
	} else {
		g_assert_not_reached ();
	}
}

static GVariant *
handle_get_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data)
{
	g_assert_not_reached ();
	return NULL;
}

static gboolean
handle_set_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data)
{
	g_assert_not_reached ();
	return TRUE;
}

static void
index_constructed (GObject *miner)
{
	TrackerMinerFilesIndexPrivate *priv;
	gchar *full_path, *full_name;
	GError *error = NULL;
	GDBusInterfaceVTable interface_vtable = {
		handle_method_call,
		handle_get_property,
		handle_set_property
	};

	priv = TRACKER_MINER_FILES_INDEX_GET_PRIVATE (miner);

	priv->d_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (!priv->d_connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            error ? error->message : "no error given.");
		g_clear_error (&error);
		return;
	}

	priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

	full_name = g_strconcat (TRACKER_MINER_DBUS_NAME_PREFIX, "Files.Index", NULL);

	priv->own_id = g_bus_own_name_on_connection (priv->d_connection,
	                                             full_name,
	                                             G_BUS_NAME_OWNER_FLAGS_NONE,
	                                             NULL, NULL, NULL, NULL);
	priv->full_name = full_name;

	/* Register the service name for the miner */
	full_path = g_strconcat (TRACKER_MINER_DBUS_PATH_PREFIX, "Files/Index", NULL);

	g_message ("Registering D-Bus object...");
	g_message ("  Path:'%s'", full_path);
	g_message ("  Object Type:'%s'", G_OBJECT_TYPE_NAME (miner));

	priv->registration_id =
		g_dbus_connection_register_object (priv->d_connection,
		                                   full_path,
		                                   priv->introspection_data->interfaces[0],
		                                   &interface_vtable,
		                                   miner,
		                                   NULL,
		                                   &error);

	if (error) {
		g_critical ("Could not register the D-Bus object %s, %s",
		            full_path,
		            error ? error->message : "no error given.");
		g_clear_error (&error);
		return;
	}

	priv->full_path = full_path;
}

static void
tracker_miner_files_index_init (TrackerMinerFilesIndex *object)
{
}

TrackerMinerFilesIndex *
tracker_miner_files_index_new (TrackerMinerFiles *miner_files)
{
	return g_object_new (TRACKER_TYPE_MINER_FILES_INDEX,
	                     "files-miner", miner_files,
	                     NULL);
}
