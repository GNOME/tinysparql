/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia
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

#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>

#include <glib-object.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-ontology.h>

#include "tracker-writeback-file.h"

#define TRACKER_TYPE_WRITEBACK_XMP (tracker_writeback_xmp_get_type ())

typedef struct TrackerWritebackXMP TrackerWritebackXMP;
typedef struct TrackerWritebackXMPClass TrackerWritebackXMPClass;

struct TrackerWritebackXMP {
	TrackerWritebackFile parent_instance;
};

struct TrackerWritebackXMPClass {
	TrackerWritebackFileClass parent_class;
};

static GType                tracker_writeback_xmp_get_type     (void) G_GNUC_CONST;
static gboolean             writeback_xmp_update_file_metadata (TrackerWritebackFile *writeback_file,
                                                                GFile                *file,
                                                                GPtrArray            *values);
static const gchar * const *writeback_xmp_content_types        (TrackerWritebackFile *writeback_file);

G_DEFINE_DYNAMIC_TYPE (TrackerWritebackXMP, tracker_writeback_xmp, TRACKER_TYPE_WRITEBACK_FILE);

static void
tracker_writeback_xmp_class_init (TrackerWritebackXMPClass *klass)
{
	TrackerWritebackFileClass *writeback_file_class = TRACKER_WRITEBACK_FILE_CLASS (klass);

	xmp_init ();

	writeback_file_class->update_file_metadata = writeback_xmp_update_file_metadata;
	writeback_file_class->content_types = writeback_xmp_content_types;
}

static void
tracker_writeback_xmp_class_finalize (TrackerWritebackXMPClass *klass)
{
	xmp_terminate ();
}

static void
tracker_writeback_xmp_init (TrackerWritebackXMP *wbx)
{
}

static const gchar * const *
writeback_xmp_content_types (TrackerWritebackFile *wbf)
{
	static const gchar *content_types[] = {
	        "image/png",   /* .png files */
	        "sketch/png",  /* .sketch.png files on Maemo*/
	        "image/jpeg",  /* .jpg & .jpeg files */
	        "image/tiff",  /* .tiff & .tif files */
	        NULL 
	};

	/* "image/gif"                        .gif files 
	   "application/pdf"                  .pdf files 
	   "application/rdf+xml"              .xmp files 
	   "application/postscript"           .ps files  
	   "application/x-shockwave-flash"    .swf files 
	   "video/quicktime"                  .mov files 
	   "video/mpeg"                       .mpeg & .mpg files 
	   "audio/mpeg"                       .mp3, etc files */

	return content_types;
}

static gboolean
writeback_xmp_update_file_metadata (TrackerWritebackFile *wbf,
				    GFile                *file,
				    GPtrArray            *values)
{
	gchar *path;
	guint n;
	XmpFilePtr xmp_files;
	XmpPtr xmp;
#ifdef DEBUG_XMP
	XmpStringPtr str;
#endif

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
	str = xmp_string_new ();
	g_print ("\nBEFORE: ---- \n");
	xmp_serialize_and_format (xmp, str, 0, 0, "\n", "\t", 1);
	g_print ("%s\n", xmp_string_cstr (str));
	xmp_string_free (str);
#endif

	for (n = 0; n < values->len; n++) {
		const GStrv row = g_ptr_array_index (values, n);

		if (g_strcmp0 (row[1], TRACKER_NIE_PREFIX "title") == 0) {
			xmp_delete_property (xmp, NS_EXIF, "Title");
			xmp_set_property (xmp, NS_EXIF, "Title", row[2], 0);

			/* I have no idea why I have to set this, but without
			 * it seems that exiftool doesn't see the change */
			 xmp_set_array_item (xmp, NS_DC, "title", 1, row[2], 0); 

		}

		/* TODO: Add more */
	}

#ifdef DEBUG_XMP
	g_print ("\nAFTER: ---- \n");
	str = xmp_string_new ();
	xmp_serialize_and_format (xmp, str, 0, 0, "\n", "\t", 1);
	g_print ("%s\n", xmp_string_cstr (str));
	xmp_string_free (str);
	g_print ("\n --------- \n");
#endif

	if (xmp_files_can_put_xmp (xmp_files, xmp)) {
		xmp_files_put_xmp (xmp_files, xmp);
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

const gchar * const *
writeback_module_get_rdf_types (void)
{
	static const gchar *rdf_types[] = { 
	        TRACKER_NFO_PREFIX "Image",
	        TRACKER_NFO_PREFIX "Audio",
	        TRACKER_NFO_PREFIX "Video",
	        NULL 
	};

	return rdf_types;
}
