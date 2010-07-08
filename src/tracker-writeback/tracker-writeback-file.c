/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <libtracker-common/tracker-file-utils.h>

#include "tracker-writeback-file.h"

static gboolean tracker_writeback_file_update_metadata (TrackerWriteback *writeback,
                                                        GPtrArray        *values,
                                                        TrackerClient    *client);

G_DEFINE_ABSTRACT_TYPE (TrackerWritebackFile, tracker_writeback_file, TRACKER_TYPE_WRITEBACK)

static void
tracker_writeback_file_class_init (TrackerWritebackFileClass *klass)
{
	TrackerWritebackClass *writeback_class = TRACKER_WRITEBACK_CLASS (klass);

	writeback_class->update_metadata = tracker_writeback_file_update_metadata;
}

static void
tracker_writeback_file_init (TrackerWritebackFile *writeback_file)
{
}

static gboolean
file_unlock_cb (gpointer user_data)
{
	GFile *file;
	gchar *path;

	file = user_data;
	path = g_file_get_path (file);
	g_message ("Unlocking file '%s'", path);
	g_free (path);

	tracker_file_unlock (file);
	g_object_unref (file);

	return FALSE;
}

static gboolean
tracker_writeback_file_update_metadata (TrackerWriteback *writeback,
                                        GPtrArray        *values,
                                        TrackerClient    *client)
{
	TrackerWritebackFileClass *writeback_file_class;
	gboolean retval;
	GFile *file;
	GFileInfo *file_info;
	const gchar *urls[2] = { NULL, NULL };
	GStrv row;
	const gchar * const *content_types;
	const gchar *mime_type;
	guint n;

	writeback_file_class = TRACKER_WRITEBACK_FILE_GET_CLASS (writeback);

	if (!writeback_file_class->update_file_metadata) {
		g_critical ("%s doesn't implement update_file_metadata()",
		            G_OBJECT_TYPE_NAME (writeback));
		return FALSE;
	}

	if (!writeback_file_class->content_types) {
		g_critical ("%s doesn't implement content_types()",
		            G_OBJECT_TYPE_NAME (writeback));
		return FALSE;
	}

	/* Get the file from the first row */
	row = g_ptr_array_index (values, 0);
	file = g_file_new_for_uri (row[0]);

	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
	                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                               NULL, NULL);

	if (!file_info) {
		if (file) {
			g_object_unref (file);
		}

		return FALSE;
	}

	mime_type = g_file_info_get_content_type (file_info);
	content_types = (writeback_file_class->content_types) (TRACKER_WRITEBACK_FILE (writeback));

	retval = FALSE;

	for (n = 0; content_types[n] != NULL; n++) {
		if (g_strcmp0 (mime_type, content_types[n]) == 0) {
			retval = TRUE;
			break;
		}
	}

	g_object_unref (file_info);

	if (retval) {
		g_message ("Locking file '%s' in order to write metadata", row[0]);

		tracker_file_lock (file);

		urls[0] = row[0];

		tracker_miner_manager_ignore_next_update (tracker_writeback_get_miner_manager (),
		                                          "org.freedesktop.Tracker1.Miner.Files",
		                                          urls);

		retval = (writeback_file_class->update_file_metadata) (TRACKER_WRITEBACK_FILE (writeback),
		                                                       file, values, client);

		g_timeout_add_seconds (3, file_unlock_cb, g_object_ref (file));
	}

	g_object_unref (file);

	return retval;
}
