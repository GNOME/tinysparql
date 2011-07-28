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

#include <stdio.h>
#include <fcntl.h> /* O_WRONLY */

#include <gio/gunixoutputstream.h>

#include <libtracker-common/tracker-file-utils.h>

#include "tracker-writeback-file.h"

static gboolean tracker_writeback_file_update_metadata (TrackerWriteback        *writeback,
                                                        GPtrArray               *values,
                                                        TrackerSparqlConnection *connection,
                                                        GCancellable            *cancellable);

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

static GFile *
create_temporary_file (GFile     *file,
                       GFileInfo *file_info)
{
	GInputStream *input_stream;
	GOutputStream *output_stream;
	GFile *tmp_file, *parent;
	gchar *dir, *name, *tmp_path;
	guint32 mode;
	gint fd;
	GError *error = NULL;

	if (!g_file_is_native (file)) {
		gchar *uri;

		uri = g_file_get_uri (file);
		g_warning ("Could not create temporary file, file is not native: '%s'", uri);
		g_free (uri);

		return NULL;
	}

	/* Create input stream */
	input_stream = G_INPUT_STREAM (g_file_read (file, NULL, &error));

	if (error) {
		g_critical ("Could not create temporary file, %s", error->message);
		g_error_free (error);
		return NULL;
	}

	/* Create output stream in a tmp file */
	parent = g_file_get_parent (file);
	dir = g_file_get_path (parent);
	g_object_unref (parent);

	name = g_file_get_basename (file);
	tmp_path = g_strdup_printf ("%s" G_DIR_SEPARATOR_S ".tracker-XXXXXX.%s",
	                            dir, name);
	g_free (dir);
	g_free (name);

	mode = g_file_info_get_attribute_uint32 (file_info,
	                                         G_FILE_ATTRIBUTE_UNIX_MODE);
	fd = g_mkstemp_full (tmp_path, O_WRONLY, mode);

	output_stream = g_unix_output_stream_new (fd, TRUE);

	/* Splice the original file into the tmp file */
	g_output_stream_splice (output_stream,
	                        input_stream,
	                        G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
	                        G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
	                        NULL, &error);

	g_object_unref (output_stream);
	g_object_unref (input_stream);

	tmp_file = g_file_new_for_path (tmp_path);
	g_free (tmp_path);

	if (error) {
		g_critical ("Could not copy temporary file, %s", error->message);
		g_error_free (error);

		g_file_delete (tmp_file, NULL, NULL);
		g_object_unref (tmp_file);

		return NULL;
	}

	return tmp_file;
}

static gboolean
tracker_writeback_file_update_metadata (TrackerWriteback        *writeback,
                                        GPtrArray               *values,
                                        TrackerSparqlConnection *connection,
                                        GCancellable            *cancellable)
{
	TrackerWritebackFileClass *writeback_file_class;
	gboolean retval;
	GFile *file, *tmp_file;
	GFileInfo *file_info;
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

	/* Get the file from the row */
	row = g_ptr_array_index (values, 0);
	file = g_file_new_for_uri (row[0]);

	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_UNIX_MODE ","
	                               G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
	                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                               NULL, NULL);

	if (!file_info) {
		g_object_unref (file);
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

	if (!retval) {
		/* module does not support writeback for this file */
		g_object_unref (file);
		g_object_unref (file_info);

		return FALSE;
	}

	/* Copy to a temporary file so we can perform an atomic write on move */
	tmp_file = create_temporary_file (file, file_info);

	if (!tmp_file) {
		g_object_unref (file);
		g_object_unref (file_info);

		return FALSE;
	}

	retval = (writeback_file_class->update_file_metadata) (TRACKER_WRITEBACK_FILE (writeback),
	                                                       tmp_file,
	                                                       values,
	                                                       connection,
	                                                       cancellable);

	if (!retval) {
		/* Delete the temporary file and preserve original */
		g_file_delete (tmp_file, NULL, NULL);
	} else {
		/* Move back the modified file to the original location */
		g_file_move (tmp_file, file,
			     G_FILE_COPY_OVERWRITE,
			     NULL, NULL, NULL, NULL);
	}

	g_object_unref (tmp_file);
	g_object_unref (file_info);
	g_object_unref (file);

	return retval;
}
