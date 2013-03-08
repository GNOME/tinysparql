/*
 * Copyright (C) 2012, Red Hat, Inc.
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

#include <glib.h>
#include <gmodule.h>

#include <libgxps/gxps.h>

#include <libtracker-extract/tracker-extract.h>

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerSparqlBuilder *metadata;
	GXPSDocument *document;
	GXPSFile *xps_file;
	GFile *file;
	gchar *filename;
	GError *error = NULL;

	metadata = tracker_extract_info_get_metadata_builder (info);
	file = tracker_extract_info_get_file (info);
	xps_file = gxps_file_new (file, &error);
	filename = g_file_get_path (file);

	if (error != NULL) {
		g_warning ("Unable to open '%s': %s", filename, error->message);
		g_error_free (error);
		g_free (filename);
		return FALSE;
	}

	document = gxps_file_get_document (xps_file, 0, &error);
	g_object_unref (xps_file);

	if (error != NULL) {
		g_warning ("Unable to read '%s': %s", filename, error->message);
		g_error_free (error);
		g_free (filename);
		return FALSE;
	}

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	tracker_sparql_builder_predicate (metadata, "nfo:pageCount");
	tracker_sparql_builder_object_int64 (metadata, gxps_document_get_n_pages (document));

	g_object_unref (document);
	g_free (filename);

	return TRUE;
}
