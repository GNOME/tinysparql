/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
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
 *
 * Authors: Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <locale.h>
#include <string.h>

#include <glib.h>

#include <glib-object.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-ontology.h>

#include "tracker-writeback-file.h"

#ifdef HAVE_EXEMPI

#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>

#define TRACKER_TYPE_WRITEBACK_XMP    (tracker_writeback_xmp_get_type ())

typedef struct TrackerWritebackXMP TrackerWritebackXMP;
typedef struct TrackerWritebackXMPClass TrackerWritebackXMPClass;

struct TrackerWritebackXMP {
	TrackerWritebackFile parent_instance;
};

struct TrackerWritebackXMPClass {
	TrackerWritebackFileClass parent_class;
};


static GType    tracker_writeback_xmp_get_type             (void) G_GNUC_CONST;
static gboolean tracker_writeback_xmp_update_file_metadata (TrackerWritebackFile *writeback_file,
                                                            GFile                *file,
                                                            GPtrArray            *values);

G_DEFINE_DYNAMIC_TYPE (TrackerWritebackXMP, tracker_writeback_xmp, TRACKER_TYPE_WRITEBACK_FILE);

static void
tracker_writeback_xmp_class_init (TrackerWritebackXMPClass *klass)
{
	TrackerWritebackFileClass *writeback_file_class = TRACKER_WRITEBACK_FILE_CLASS (klass);

	xmp_init ();
	writeback_file_class->update_file_metadata = tracker_writeback_xmp_update_file_metadata;
}

static void
tracker_writeback_xmp_class_finalize (TrackerWritebackXMPClass *klass)
{
	xmp_terminate ();
}

static void
tracker_writeback_xmp_init (TrackerWritebackXMP *xmp)
{
}

static gboolean
tracker_writeback_xmp_update_file_metadata (TrackerWritebackFile *writeback_file,
                                            GFile                *file,
                                            GPtrArray            *values)
{
	GFileInfo *file_info;
	const gchar *mime_type;
	gchar *path;
	guint n;
	XmpFilePtr xmp_files;
	XmpPtr xmp;
#ifdef DEBUG_XMP
	XmpStringPtr str;
#endif

	file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
	                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                               NULL, NULL);

	if (!file_info) {
		return FALSE;
	}

	mime_type = g_file_info_get_content_type (file_info);

	if (g_strcmp0 (mime_type, "image/png") != 0                     && /* .png files */
	    g_strcmp0 (mime_type, "sketch/png") != 0                    && /* .sketch.png files on Maemo*/
	    g_strcmp0 (mime_type, "image/jpeg") != 0                    && /* .jpg & .jpeg files */
	    g_strcmp0 (mime_type, "image/tiff") != 0                   ) { /* .tiff & .tif files */

	/*  g_strcmp0 (mime_type, "image/gif") != 0                     && * .gif files *
	    g_strcmp0 (mime_type, "application/pdf") != 0               && * .pdf files *
	    g_strcmp0 (mime_type, "application/rdf+xml") != 0           && * .xmp files *
	    g_strcmp0 (mime_type, "application/postscript") != 0        && * .ps files *
	    g_strcmp0 (mime_type, "application/x-shockwave-flash") != 0 && * .swf files *
	    g_strcmp0 (mime_type, "video/quicktime") != 0               && * .mov files *
	    g_strcmp0 (mime_type, "video/mpeg") != 0                    && * .mpeg & .mpg files *
	    g_strcmp0 (mime_type, "audio/mpeg") != 0 ) {                   * .mp3, etc files */

		g_object_unref (file_info);
		return FALSE;
	}

	g_object_unref (file_info);
	path = g_file_get_path (file);

	xmp_files = xmp_files_open_new (path, XMP_OPEN_FORUPDATE);

	if (!xmp_files) {
		g_free (path);
		return FALSE;
	}

	xmp = xmp_files_get_new_xmp (xmp_files);

	if (!xmp) {
		g_free (path);
		return FALSE;
	}

#ifdef DEBUG_XMP
	str = xmp_string_new();
	g_print ("\nBEFORE: ---- \n");
	xmp_serialize_and_format(xmp, str, 0, 0, "\n", "\t", 1);
	printf ("%s\n", xmp_string_cstr (str));
	xmp_string_free (str);
#endif

	for (n = 0; n < values->len; n++) {
		const GStrv row = g_ptr_array_index (values, n);

		if (g_strcmp0 (row[1], TRACKER_NIE_PREFIX "title") == 0) {
			xmp_delete_property (xmp, NS_EXIF, "Title");
			xmp_set_property(xmp, NS_EXIF, "Title", row[2], 0);

			/* I have no idea why I have to set this, but without
			 * it seems that exiftool doesn't see the change */
			 xmp_set_array_item(xmp, NS_DC, "title", 1, row[2], 0); 

		}

		/* TODO: Add more */
	}

#ifdef DEBUG_XMP
	g_print ("\nAFTER: ---- \n");
	str = xmp_string_new();
	xmp_serialize_and_format(xmp, str, 0, 0, "\n", "\t", 1);
	printf ("%s\n", xmp_string_cstr (str));
	xmp_string_free (str);
	g_print ("\n --------- \n");
#endif

	if (xmp_files_can_put_xmp(xmp_files, xmp)) {
		xmp_files_put_xmp(xmp_files, xmp);
	}

	xmp_files_close (xmp_files, XMP_CLOSE_SAFEUPDATE);

	xmp_free (xmp);
	xmp_files_free (xmp_files);
	g_free (path);

	return TRUE;
}

TrackerWriteback *
writeback_module_create (GTypeModule *module)
{
	tracker_writeback_xmp_register_type (module);

	return g_object_new (TRACKER_TYPE_WRITEBACK_XMP, NULL);
}

const gchar**
writeback_module_get_rdftypes (void)
{
	static const gchar *rdftypes[] = { TRACKER_NFO_PREFIX "Image",
	                                   TRACKER_NFO_PREFIX "Audio",
	                                   TRACKER_NFO_PREFIX "Video",
	                                   NULL };

	return rdftypes;
}

#endif /* HAVE_EXEMPI */
