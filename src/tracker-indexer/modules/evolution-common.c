/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

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

#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include <tracker-indexer/tracker-module-metadata-utils.h>

#include "evolution-common.h"

GMimeStream *
evolution_common_get_stream (const gchar *path,
                             gint         flags,
                             off_t        start)
{
	GMimeStream *stream;
	gint fd;

	fd = g_open (path, flags, S_IRUSR | S_IWUSR);

	if (fd == -1) {
		return NULL;
	}

	stream = g_mime_stream_fs_new_with_bounds (fd, start, -1);

	if (!stream) {
		close (fd);
	}

	return stream;
}

TrackerModuleMetadata *
evolution_common_get_wrapper_metadata (GMimeDataWrapper *wrapper)
{
	TrackerModuleMetadata *metadata;
	GMimeStream *stream;
	gchar *path;
	gint fd;

	path = g_build_filename (g_get_tmp_dir (), "tracker-evolution-module-XXXXXX", NULL);
	fd = g_mkstemp (path);
	metadata = NULL;

	stream = g_mime_stream_fs_new (fd);

	if (g_mime_data_wrapper_write_to_stream (wrapper, stream) != -1) {
                GFile *file;

                file = g_file_new_for_path (path);
		g_mime_stream_flush (stream);

		metadata = tracker_module_metadata_utils_get_data (file);

                g_object_unref (file);
		g_unlink (path);
	}

	g_mime_stream_close (stream);
	g_object_unref (stream);
	g_free (path);

	return metadata;
}

