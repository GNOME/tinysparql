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

#include <libtracker-common/tracker-ontologies.h>
#include <libtracker-common/tracker-utils.h>

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
                                                                GPtrArray            *values,
                                                                TrackerClient        *client);
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
		"video/mp4",   /* .mp4 files */
		"video/3gpp",  /* .3gpp files */
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
                                    GPtrArray            *values,
                                    TrackerClient        *client)
{
	gchar *path;
	guint n;
	XmpFilePtr xmp_files;
	XmpPtr xmp;
#ifdef DEBUG_XMP
	XmpStringPtr str;
#endif
	GString *keywords = NULL;

	path = g_file_get_path (file);

	xmp_files = xmp_files_open_new (path, XMP_OPEN_FORUPDATE);

	if (!xmp_files) {
		g_free (path);
		return FALSE;
	}

	xmp = xmp_files_get_new_xmp (xmp_files);

	if (!xmp) {
		xmp = xmp_new_empty ();
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

		if (g_strcmp0 (row[2], TRACKER_NIE_PREFIX "title") == 0) {
			xmp_delete_property (xmp, NS_EXIF, "Title");
			xmp_set_property (xmp, NS_EXIF, "Title", row[3], 0);
			xmp_delete_property (xmp, NS_DC, "title");
			xmp_set_property (xmp, NS_DC, "title", row[3], 0);
		}

		if (g_strcmp0 (row[2], TRACKER_NCO_PREFIX "creator") == 0) {
			GPtrArray *name_array;
			GError *error = NULL;
			gchar *query;

			query = g_strdup_printf ("SELECT ?fullname { "
			                         "  <%s> nco:fullname ?fullname "
			                         "}", row[3]);

			name_array = tracker_resources_sparql_query (client, query, &error);

			g_free (query);

			if (name_array && name_array->len > 0) {
				GStrv name_row;

				name_row = g_ptr_array_index (name_array, 0);

				if (name_row[0]) {
					xmp_delete_property (xmp, NS_DC, "creator");
					xmp_set_property (xmp, NS_DC, "creator", name_row[0], 0);
				}
			}

			if (name_array) {
				g_ptr_array_foreach (name_array, (GFunc) g_strfreev, NULL);
				g_ptr_array_free (name_array, TRUE);
			}

			g_clear_error (&error);
		}

		if (g_strcmp0 (row[2], TRACKER_NCO_PREFIX "contributor") == 0) {
			GPtrArray *name_array;
			GError *error = NULL;
			gchar *query;

			query = g_strdup_printf ("SELECT ?fullname { "
			                         "  <%s> nco:fullname ?fullname "
			                         "}", row[3]);

			name_array = tracker_resources_sparql_query (client, query, &error);

			g_free (query);

			if (name_array && name_array->len > 0) {
				GStrv name_row;

				name_row = g_ptr_array_index (name_array, 0);

				if (name_row[0]) {
					xmp_delete_property (xmp, NS_DC, "contributor");
					xmp_set_property (xmp, NS_DC, "contributor", name_row[0], 0);
				}
			}

			if (name_array) {
				g_ptr_array_foreach (name_array, (GFunc) g_strfreev, NULL);
				g_ptr_array_free (name_array, TRUE);
			}

			g_clear_error (&error);
		}

		if (g_strcmp0 (row[2], TRACKER_NIE_PREFIX "description") == 0) {
			xmp_delete_property (xmp, NS_DC, "description");
			xmp_set_property (xmp, NS_DC, "description", row[3], 0);
		}

		if (g_strcmp0 (row[2], TRACKER_NIE_PREFIX "copyright") == 0) {
			xmp_delete_property (xmp, NS_EXIF, "Copyright");
			xmp_set_property (xmp, NS_EXIF, "Copyright", row[3], 0);
		}

		if (g_strcmp0 (row[2], TRACKER_NIE_PREFIX "comment") == 0) {
			xmp_delete_property (xmp, NS_EXIF, "UserComment");
			xmp_set_property (xmp, NS_EXIF, "UserComment", row[3], 0);
		}

		if (g_strcmp0 (row[2], TRACKER_NIE_PREFIX "keyword") == 0) {
			if (!keywords) {
				keywords = g_string_new (row[3]);
			} else {
				g_string_append_printf (keywords, ", %s", row[3]);
			}
		}

		if (g_strcmp0 (row[2], TRACKER_NIE_PREFIX "contentCreated") == 0) {
			xmp_delete_property (xmp, NS_EXIF, "Date");
			xmp_set_property (xmp, NS_EXIF, "Date", row[3], 0);
			xmp_delete_property (xmp,  NS_DC, "date");
			xmp_set_property (xmp,  NS_DC, "date", row[3], 0);
		}

		if (g_strcmp0 (row[2], TRACKER_NFO_PREFIX "orientation") == 0) {

			xmp_delete_property (xmp, NS_EXIF, "Orientation");

			if        (g_strcmp0 (row[3], TRACKER_NFO_PREFIX "orientation-top") == 0) {
				xmp_set_property (xmp, NS_EXIF, "Orientation", "top - left", 0);
			} else if (g_strcmp0 (row[3], TRACKER_NFO_PREFIX "orientation-top-mirror") == 0) {
				xmp_set_property (xmp, NS_EXIF, "Orientation", "top - right", 0);
			} else if (g_strcmp0 (row[3], TRACKER_NFO_PREFIX "orientation-bottom") == 0) {
				xmp_set_property (xmp, NS_EXIF, "Orientation", "bottom - left", 0);
			} else if (g_strcmp0 (row[3], TRACKER_NFO_PREFIX "orientation-bottom-mirror") == 0) {
				xmp_set_property (xmp, NS_EXIF, "Orientation", "bottom - right", 0);
			} else if (g_strcmp0 (row[3], TRACKER_NFO_PREFIX "orientation-left-mirror") == 0) {
				xmp_set_property (xmp, NS_EXIF, "Orientation", "left - top", 0);
			} else if (g_strcmp0 (row[3], TRACKER_NFO_PREFIX "orientation-right") == 0) {
				xmp_set_property (xmp, NS_EXIF, "Orientation", "right - top", 0);
			} else if (g_strcmp0 (row[3], TRACKER_NFO_PREFIX "orientation-right-mirror") == 0) {
					xmp_set_property (xmp, NS_EXIF, "Orientation", "right - bottom", 0);
			} else if (g_strcmp0 (row[3], TRACKER_NFO_PREFIX "orientation-left") == 0) {
				xmp_set_property (xmp, NS_EXIF, "Orientation", "left - bottom", 0);
			}
		}

#ifdef SET_TYPICAL_CAMERA_FIELDS
		/* Default we don't do this, we shouldn't overwrite fields that are 
		 * typically set by the camera itself. What do we know (better) than
		 * the actual camera did, anyway? Even if the user overwrites them in
		 * the RDF store ... (does he know what he's doing anyway?) */

		if (g_strcmp0 (row[2], TRACKER_NMM_PREFIX "meteringMode") == 0) {

			xmp_delete_property (xmp, NS_EXIF, "MeteringMode");

			/* 0 = Unknown
			   1 = Average
			   2 = CenterWeightedAverage
			   3 = Spot
			   4 = MultiSpot
			   5 = Pattern
			   6 = Partial
			   255 = other  */

			if        (g_strcmp0 (row[3], TRACKER_NMM_PREFIX "metering-mode-center-weighted-average") == 0) {
				xmp_set_property (xmp, NS_EXIF, "MeteringMode", "0", 0);
			} else if (g_strcmp0 (row[3], TRACKER_NMM_PREFIX "metering-mode-average") == 0) {
				xmp_set_property (xmp, NS_EXIF, "MeteringMode", "1", 0);
			} else if (g_strcmp0 (row[3], TRACKER_NMM_PREFIX "metering-mode-spot") == 0) {
				xmp_set_property (xmp, NS_EXIF, "MeteringMode", "3", 0);
			} else if (g_strcmp0 (row[3], TRACKER_NMM_PREFIX "metering-mode-multispot") == 0) {
				xmp_set_property (xmp, NS_EXIF, "MeteringMode", "4", 0);
			} else if (g_strcmp0 (row[3], TRACKER_NMM_PREFIX "metering-mode-pattern") == 0) {
				xmp_set_property (xmp, NS_EXIF, "MeteringMode", "5", 0);
			} else if (g_strcmp0 (row[3], TRACKER_NMM_PREFIX "metering-mode-partial") == 0) {
				xmp_set_property (xmp, NS_EXIF, "MeteringMode", "6", 0);
			} else {
				xmp_set_property (xmp, NS_EXIF, "MeteringMode", "255", 0);
			}
		}

		if (g_strcmp0 (row[2], TRACKER_NMM_PREFIX "whiteBalance") == 0) {

			xmp_delete_property (xmp, NS_EXIF, "WhiteBalance");

			if (g_strcmp0 (row[3], TRACKER_NMM_PREFIX "white-balance-auto") == 0) {
				/* 0 = Auto white balance
				 * 1 = Manual white balance */
				xmp_set_property (xmp, NS_EXIF, "WhiteBalance", "0", 0);
			} else {
				xmp_set_property (xmp, NS_EXIF, "WhiteBalance", "1", 0);
			}
		}

		if (g_strcmp0 (row[2], TRACKER_NMM_PREFIX "flash") == 0) {

			xmp_delete_property (xmp, NS_EXIF, "Flash");

			if (g_strcmp0 (row[3], TRACKER_NMM_PREFIX "flash-on") == 0) {
				/* 0 = Flash did not fire
				 * 1 = Flash fired */
				xmp_set_property (xmp, NS_EXIF, "Flash", "1", 0);
			} else {
				xmp_set_property (xmp, NS_EXIF, "Flash", "0", 0);
			}
		}


		/* TODO: Don't write row[3] as-is here, read xmp_specification.pdf,
		   page 84 (bottom). */

		if (g_strcmp0 (row[2], TRACKER_NMM_PREFIX "focalLength") == 0) {
			xmp_delete_property (xmp, NS_EXIF, "FocalLength");
			xmp_set_property (xmp, NS_EXIF, "FocalLength", row[3], 0);
		}

		if (g_strcmp0 (row[2], TRACKER_NMM_PREFIX "exposureTime") == 0) {
			xmp_delete_property (xmp, NS_EXIF, "ExposureTime");
			xmp_set_property (xmp, NS_EXIF, "ExposureTime", row[3], 0);
		}

		if (g_strcmp0 (row[2], TRACKER_NMM_PREFIX "isoSpeed") == 0) {
			xmp_delete_property (xmp, NS_EXIF, "ISOSpeedRatings");
			xmp_set_property (xmp, NS_EXIF, "ISOSpeedRatings", row[3], 0);
		}

		if (g_strcmp0 (row[2], TRACKER_NMM_PREFIX "fnumber") == 0) {
			xmp_delete_property (xmp, NS_EXIF, "FNumber");
			xmp_set_property (xmp, NS_EXIF, "FNumber", row[3], 0);
		}


		if (g_strcmp0 (row[2], TRACKER_NMM_PREFIX "camera") == 0) {
			gchar *work_on = g_strdup (row[3]);
			gchar *ptr = strchr (work_on, ' ');

			if (ptr) {

				*ptr = '\0';
				ptr++;

				xmp_delete_property (xmp, NS_EXIF, "Make");
				xmp_set_property (xmp, NS_EXIF, "Make", work_on, 0);
				xmp_delete_property (xmp, NS_EXIF, "Model");
				xmp_set_property (xmp, NS_EXIF, "Model", ptr, 0);
			} else {
				xmp_delete_property (xmp, NS_EXIF, "Make");
				xmp_delete_property (xmp, NS_EXIF, "Model");
				xmp_set_property (xmp, NS_EXIF, "Model", work_on, 0);
			}

			g_free (work_on);
		}
#endif /* SET_TYPICAL_CAMERA_FIELDS */

		if (g_strcmp0 (row[2], TRACKER_MLO_PREFIX "location") == 0 ||
		    g_strcmp0 (row[2], TRACKER_MLO_PREFIX "city") == 0     ||
		    g_strcmp0 (row[2], TRACKER_MLO_PREFIX "country") == 0  ||
		    g_strcmp0 (row[2], TRACKER_MLO_PREFIX "state") == 0    ||
		    g_strcmp0 (row[2], TRACKER_MLO_PREFIX "address") == 0)
		{
			GPtrArray *array;
			GError *error = NULL;
			gchar *query;

			query = g_strdup_printf ("SELECT mlo:city (?location) mlo:state (?location) "
			                               " mlo:address (?location) "
			                               " mlo:country (?location) "
			                         "WHERE { <%s> mlo:location ?location }",
			                         row[1]); /* The urn is at 1 */

			array = tracker_resources_sparql_query (client, query, &error);

			g_free (query);

			if (array && array->len > 0) {
				GStrv qrow;

				qrow = g_ptr_array_index (array, 0);

				/* TODO: A lot of these location fields are pretty vague and ambigious.
				 * We should go through them one by one and ensure that all of them are
				 * used sanely */

				if (!tracker_is_blank_string (qrow[0])) {
					xmp_delete_property (xmp, NS_IPTC4XMP, "City");
					xmp_set_property (xmp, NS_IPTC4XMP, "City", qrow[0], 0);
					xmp_delete_property (xmp, NS_PHOTOSHOP, "City");
					xmp_set_property (xmp, NS_PHOTOSHOP, "City", qrow[0], 0);
				}
				g_free (qrow[0]);

				if (!tracker_is_blank_string (qrow[1])) {
					xmp_delete_property (xmp, NS_IPTC4XMP, "State");
					xmp_set_property (xmp, NS_IPTC4XMP, "State", qrow[1], 0);
					xmp_delete_property (xmp, NS_IPTC4XMP, "Province");
					xmp_set_property (xmp, NS_IPTC4XMP, "Province", qrow[1], 0);
					xmp_delete_property (xmp, NS_PHOTOSHOP, "State");
					xmp_set_property (xmp, NS_PHOTOSHOP, "State", qrow[1], 0);
				}
				g_free (qrow[1]);

				if (!tracker_is_blank_string (qrow[2])) {
					xmp_delete_property (xmp, NS_IPTC4XMP, "SubLocation");
					xmp_set_property (xmp, NS_IPTC4XMP, "SubLocation", qrow[2], 0);
					xmp_delete_property (xmp, NS_PHOTOSHOP, "Location");
					xmp_set_property (xmp, NS_PHOTOSHOP, "Location", qrow[2], 0);
				}
				g_free (qrow[2]);

				if (!tracker_is_blank_string (qrow[3])) {
					xmp_delete_property (xmp, NS_PHOTOSHOP, "Country");
					xmp_set_property (xmp, NS_PHOTOSHOP, "Country", qrow[3], 0);
					xmp_delete_property (xmp, NS_IPTC4XMP, "Country");
					xmp_set_property (xmp, NS_IPTC4XMP, "Country", qrow[3], 0);
					xmp_delete_property (xmp, NS_IPTC4XMP, "PrimaryLocationName");
					xmp_set_property (xmp, NS_IPTC4XMP, "PrimaryLocationName", qrow[3], 0);
					xmp_delete_property (xmp, NS_IPTC4XMP, "CountryName");
					xmp_set_property (xmp, NS_IPTC4XMP, "CountryName", qrow[3], 0);
				}
				g_free (qrow[3]);

			}

			if (array) {
				g_ptr_array_free (array, TRUE);
			}

			g_clear_error (&error);
		}

		/* TODO: When a photo contains a known face
		 *
		 * if (g_strcmp0 (row[2], PHOTO_HAS "contact") == 0) {
		  xmp_delete_property (xmp, FACE, "contact");
		  Fetch full name of the contact?
		  xmp_set_array_item (xmp, FACE, "contact", 1, fetched, 0);
		  }
		*/

	}

	if (keywords) {
		xmp_delete_property (xmp, NS_DC, "subject");
		xmp_set_property (xmp, NS_DC, "subject", keywords->str, 0);
		g_string_free (keywords, TRUE);
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
