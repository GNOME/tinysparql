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

#include <libtracker-client/tracker.h>

#include "tracker-miner-files-reindex.h"
#include "tracker-dbus.h"
#include "tracker-marshal.h"

typedef struct {
	guint request_id;
	DBusGMethodInvocation *context;
	TrackerClient *client;
	TrackerMinerFiles *miner_files;
} MimeTypesData;

typedef struct {
	TrackerMinerFiles *files_miner;
} TrackerMinerFilesReindexPrivate;

enum {
	PROP_0,
	PROP_FILES_MINER
};

#define TRACKER_MINER_FILES_REINDEX_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_FILES_REINDEX, TrackerMinerFilesReindexPrivate))

static void     reindex_set_property      (GObject              *object,
                                           guint                 param_id,
                                           const GValue         *value,
                                           GParamSpec           *pspec);
static void     reindex_get_property      (GObject              *object,
                                           guint                 param_id,
                                           GValue               *value,
                                           GParamSpec           *pspec);
static void     reindex_finalize          (GObject              *object);

G_DEFINE_TYPE(TrackerMinerFilesReindex, tracker_miner_files_reindex, G_TYPE_OBJECT)

static void
tracker_miner_files_reindex_class_init (TrackerMinerFilesReindexClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = reindex_finalize;
	object_class->set_property = reindex_set_property;
	object_class->get_property = reindex_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_FILES_MINER,
	                                 g_param_spec_object ("files_miner",
	                                                      "files_miner",
	                                                      "The FS Miner",
	                                                      TRACKER_TYPE_MINER_FILES,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (klass, sizeof (TrackerMinerFilesReindexPrivate));
}

static void
reindex_set_property (GObject      *object,
                      guint         param_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
	TrackerMinerFilesReindexPrivate *priv;

	priv = TRACKER_MINER_FILES_REINDEX_GET_PRIVATE (object);

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
reindex_get_property (GObject    *object,
                      guint       param_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
	TrackerMinerFilesReindexPrivate *priv;

	priv = TRACKER_MINER_FILES_REINDEX_GET_PRIVATE (object);

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
reindex_finalize (GObject *object)
{
	TrackerMinerFilesReindexPrivate *priv = TRACKER_MINER_FILES_REINDEX_GET_PRIVATE (object);
	g_object_unref (priv->files_miner);
}

static void
tracker_miner_files_reindex_init (TrackerMinerFilesReindex *object)
{
}

static MimeTypesData *
mime_types_data_new (guint                  request_id,
                     DBusGMethodInvocation *context,
                     TrackerClient         *client,
                     TrackerMinerFiles     *miner_files)
{
	MimeTypesData *mtd;

	mtd = g_slice_new0 (MimeTypesData);

	mtd->miner_files = g_object_ref (miner_files);
	mtd->request_id = request_id;
	mtd->context = context;
	mtd->client = g_object_ref (client);

	return mtd;
}

static void
mime_types_data_destroy (gpointer data)
{
	MimeTypesData *mtd;

	mtd = data;

	g_object_unref (mtd->miner_files);
	g_object_unref (mtd->client);

	g_slice_free (MimeTypesData, mtd);
}

static void
mime_types_cb (TrackerResultIterator *iterator,
               GError                *error,
               gpointer               user_data)
{
	MimeTypesData *mtd = user_data;

	if (!error) {
		tracker_dbus_request_comment (mtd->request_id, mtd->context,
		                              "Found files that will need reindexing");

		while (tracker_result_iterator_next (iterator)) {
			if (tracker_result_iterator_value (iterator, 0)) {
				const gchar *url = (const gchar *) tracker_result_iterator_value (iterator, 0);
				GFile *file = g_file_new_for_uri (url);
				tracker_miner_fs_file_add (TRACKER_MINER_FS (mtd->miner_files), file);
				g_object_unref (file);
			}
		}

		tracker_dbus_request_success (mtd->request_id, mtd->context);
		dbus_g_method_return (mtd->context);

	} else {
		tracker_dbus_request_success (mtd->request_id, mtd->context);
		dbus_g_method_return_error (mtd->context, error);
	}


	mime_types_data_destroy (user_data);
}

TrackerMinerFilesReindex *
tracker_miner_files_reindex_new (TrackerMinerFiles *miner_files)
{
	return g_object_new (TRACKER_TYPE_MINER_FILES_REINDEX, 
	                     "files-miner", miner_files, 
	                     NULL);
}

void
tracker_miner_files_reindex_mime_types (TrackerMinerFilesReindex  *object,
                                        gchar                    **mime_types,
                                        DBusGMethodInvocation     *context,
                                        GError                   **error)
{
	TrackerMinerFilesReindexPrivate *priv;
	GString *query;
	TrackerClient *client;
	guint request_id;
	gint len, i;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (mime_types != NULL, context);

	len = g_strv_length (mime_types);
	tracker_dbus_async_return_if_fail (len > 0, context);

	tracker_dbus_request_new (request_id, context, "%s()", __FUNCTION__);

	client = tracker_client_new (FALSE, G_MAXINT);
	if (!client) {
		GError *actual_error = NULL;

		tracker_dbus_request_failed (request_id,
		                             context,
		                             &actual_error,
		                             "Could not create TrackerClient");
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
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

	priv = TRACKER_MINER_FILES_REINDEX_GET_PRIVATE (object);

	/* FIXME: save last call id */
	tracker_resources_sparql_query_iterate_async (client,
	                                              query->str,
	                                              mime_types_cb,
	                                              mime_types_data_new (request_id,
	                                                                   context,
	                                                                   client,
	                                                                   priv->files_miner));

	g_string_free (query, TRUE);
	g_object_unref (client);
}
