/*
 * Copyright (C) 2010, Nokia
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
} MimeTypesData;

G_DEFINE_TYPE(TrackerMinerFilesReindex, tracker_miner_files_reindex, G_TYPE_OBJECT)

static void
tracker_miner_files_reindex_class_init (TrackerMinerFilesReindexClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
}

static void
tracker_miner_files_reindex_init (TrackerMinerFilesReindex *object)
{
}

static MimeTypesData *
mime_types_data_new (guint                  request_id,
                     DBusGMethodInvocation *context,
                     TrackerClient         *client)
{
	MimeTypesData *mtd;

	mtd = g_slice_new0 (MimeTypesData);

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

	g_object_unref (mtd->client);

	g_slice_free (MimeTypesData, mtd);
}

static void
mime_types_cb (GPtrArray *result,
               GError    *error,
               gpointer   user_data)
{
	MimeTypesData *mtd = user_data;

	tracker_dbus_request_comment (mtd->request_id, mtd->context,
	                              "Found %d files that will need reindexing",
	                              result ? result->len : 0);

	tracker_dbus_request_success (mtd->request_id, mtd->context);
	dbus_g_method_return (mtd->context);

	mime_types_data_destroy (user_data);
}

TrackerMinerFilesReindex *
tracker_miner_files_reindex_new (void)
{
	return g_object_new (TRACKER_TYPE_MINER_FILES_REINDEX, NULL);
}

void
tracker_miner_files_reindex_mime_types (TrackerMinerFilesReindex  *object,
                                        gchar                    **mime_types,
                                        DBusGMethodInvocation     *context,
                                        GError                   **error)
{
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

	/* FIXME: save last call id */
	tracker_resources_sparql_query_async (client, 
	                                      query->str, 
	                                      mime_types_cb, 
	                                      mime_types_data_new (request_id, context, client));
	g_string_free (query, TRUE);
	g_object_unref (client);
}
