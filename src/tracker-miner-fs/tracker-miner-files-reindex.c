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

#include "tracker-miner-files-reindex.h"
#include "tracker-dbus.h"
#include "tracker-marshal.h"

#define TRACKER_MINER_FILES_REINDEX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_MINER_FILES_REINDEX, TrackerMinerFilesReindexPrivate))

typedef struct {
	GHashTable *mime_types;
} TrackerMinerFilesReindexPrivate;

G_DEFINE_TYPE(TrackerMinerFilesReindex, tracker_miner_files_reindex, G_TYPE_OBJECT)

static void
tracker_miner_files_reindex_class_init (TrackerMinerFilesReindexClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (object_class, sizeof (TrackerMinerFilesReindexPrivate));
}

static void
tracker_miner_files_reindex_init (TrackerMinerFilesReindex *object)
{
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
	guint request_id;
	gchar **p;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (mime_types != NULL, context);
	tracker_dbus_async_return_if_fail (g_strv_length (mime_types) > 0, context);

	tracker_dbus_request_new (request_id, context, "%s()", __FUNCTION__);

	tracker_dbus_request_comment (request_id, context,
	                              "Attempting to reindex the following mime types:");

	for (p = mime_types; *p; p++) {
		tracker_dbus_request_comment (request_id, context, "  %s", *p);
	}

	tracker_dbus_request_success (request_id, context);
	dbus_g_method_return (context);
}
