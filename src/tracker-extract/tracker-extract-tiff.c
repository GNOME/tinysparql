/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Tracker Extract - extracts embedded metadata from files
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

#include <tiff.h>
#include <tiffio.h>

#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-main.h"
#include "tracker-xmp.h"
#include "tracker-iptc.h"

#define EXIF_DATE_FORMAT     "%Y:%m:%d %H:%M:%S"

typedef gchar * (*PostProcessor) (gchar *);

typedef enum {
	TIFF_TAGTYPE_UNDEFINED = 0,
	TIFF_TAGTYPE_STRING,
	TIFF_TAGTYPE_UINT16,
	TIFF_TAGTYPE_UINT32,
	TIFF_TAGTYPE_DOUBLE,
	TIFF_TAGTYPE_C16_UINT16
} TagType;

typedef struct {
	guint	       tag;
	gchar	      *name;
	TagType        type;
	gboolean       multi;
	PostProcessor  post;
} TiffTag;
 
static void   extract_tiff    (const gchar *filename,
			       GHashTable  *metadata);
static gchar *date_to_iso8601 (gchar       *date);

static TrackerExtractData data[] = {
	{ "image/tiff", extract_tiff },
	{ NULL, NULL }
};

/* FIXME: We are missing some */
static TiffTag tags[] = {
	{ TIFFTAG_ARTIST, "Image:Creator", TIFF_TAGTYPE_STRING, FALSE, NULL },
	{ TIFFTAG_COPYRIGHT, "File:Copyright", TIFF_TAGTYPE_STRING, FALSE, NULL },
	{ TIFFTAG_DATETIME, "Image:Date", TIFF_TAGTYPE_STRING, FALSE, NULL },
	{ TIFFTAG_DOCUMENTNAME, "Image:Title", TIFF_TAGTYPE_STRING, FALSE, NULL },
	{ TIFFTAG_IMAGEDESCRIPTION, "Image:Comments", TIFF_TAGTYPE_STRING, FALSE, NULL },
	{ TIFFTAG_IMAGEWIDTH, "Image:Width", TIFF_TAGTYPE_UINT32, FALSE, NULL },
	{ TIFFTAG_IMAGELENGTH, "Image:Height", TIFF_TAGTYPE_UINT32, FALSE, NULL },
	{ TIFFTAG_ORIENTATION, "Image:Orientation", TIFF_TAGTYPE_UINT16, FALSE, NULL },
#ifdef ENABLE_DETAILED_METADATA
	{ TIFFTAG_MAKE, "Image:CameraMake", TIFF_TAGTYPE_STRING, FALSE, NULL },
	{ TIFFTAG_MODEL, "Image:CameraModel", TIFF_TAGTYPE_STRING, FALSE, NULL },
	{ TIFFTAG_SOFTWARE, "Image:Software", TIFF_TAGTYPE_STRING, FALSE, NULL },
#endif /* ENABLE_DETAILED_METADATA */
	{ -1, NULL, TIFF_TAGTYPE_UNDEFINED, FALSE, NULL }
};

static TiffTag exiftags[] = {
	{ EXIFTAG_ISOSPEEDRATINGS, "Image:ISOSpeed", TIFF_TAGTYPE_C16_UINT16, FALSE, NULL},
	{ EXIFTAG_DATETIMEORIGINAL, "Image:Date", TIFF_TAGTYPE_STRING, FALSE, date_to_iso8601 },
	{ EXIFTAG_FLASH, "Image:Flash", TIFF_TAGTYPE_UINT16, FALSE, NULL},
	{ EXIFTAG_PIXELXDIMENSION, "Image:Width", TIFF_TAGTYPE_UINT32, FALSE, NULL},
	{ EXIFTAG_PIXELYDIMENSION, "Image:Height", TIFF_TAGTYPE_UINT32, FALSE, NULL},
#ifdef ENABLE_DETAILED_METADATA
	{ EXIFTAG_EXPOSURETIME, "Image:ExposureTime", TIFF_TAGTYPE_DOUBLE, FALSE, NULL},
	{ EXIFTAG_FNUMBER, "Image:FNumber", TIFF_TAGTYPE_DOUBLE, FALSE, NULL},
	{ EXIFTAG_EXPOSUREPROGRAM, "Image:ExposureProgram", TIFF_TAGTYPE_UINT16, FALSE, NULL },
 	{ EXIFTAG_METERINGMODE, "Image:MeteringMode", TIFF_TAGTYPE_UINT16, FALSE, NULL},
	{ EXIFTAG_FOCALLENGTH, "Image:FocalLength", TIFF_TAGTYPE_DOUBLE, FALSE, NULL},
	{ EXIFTAG_WHITEBALANCE, "Image:WhiteBalance", TIFF_TAGTYPE_UINT16, FALSE, NULL},
#endif /* ENABLE_DETAILED_METADATA */
	{ -1, NULL, TIFF_TAGTYPE_UNDEFINED, FALSE, NULL }
};

static gchar *
date_to_iso8601 (gchar *date)
{
	/* ex; date "2007:04:15 15:35:58"
	 * To
	 * ex. "2007-04-15T17:35:58+0200 where +0200 is localtime
	 */
	return tracker_date_format_to_iso8601 (date, EXIF_DATE_FORMAT);
}

static void
metadata_append (GHashTable *metadata, gchar *key, gchar *value, gboolean append)
{
	gchar   *new_value;
	gchar   *orig;
	gchar  **list;
	gboolean found = FALSE;
	guint    i;

	if (append && (orig = g_hash_table_lookup (metadata, key))) {
		gchar *escaped;
		
		escaped = tracker_escape_metadata (value);

		list = g_strsplit (orig, "|", -1);			
		for (i=0; list[i]; i++) {
			if (strcmp (list[i], escaped) == 0) {
				found = TRUE;
				break;
			}
		}			
		g_strfreev(list);

		if (!found) {
			new_value = g_strconcat (orig, "|", escaped, NULL);
			g_hash_table_insert (metadata, g_strdup (key), new_value);
		}

		g_free (escaped);		
	} else {
		new_value = tracker_escape_metadata (value);
		if (!tracker_is_empty_string (new_value)) {
			g_hash_table_insert (metadata, g_strdup (key), new_value);

			/* FIXME Postprocessing is evil and should be elsewhere */
			if (strcmp (key, "Image:Keywords") == 0) {
				g_hash_table_insert (metadata,
						     g_strdup ("Image:HasKeywords"),
						     tracker_escape_metadata ("1"));			
			}
			
			/* Adding certain fields also to HasKeywords FIXME Postprocessing is evil */
			if ((strcmp (key, "Image:Title") == 0) ||
			    (strcmp (key, "Image:Description") == 0) ) {
				g_hash_table_insert (metadata,
						     g_strdup ("Image:HasKeywords"),
						     tracker_escape_metadata ("1"));
			}
		}
	}
}

static void
extract_tiff (const gchar *filename, 
	      GHashTable  *metadata)
{
	TIFF *image;
	glong exifOffset;

	TiffTag *tag;

#ifdef HAVE_LIBIPTCDATA
	gchar   *iptcOffset;
	guint32  iptcSize;
#endif

#ifdef HAVE_EXEMPI
	gchar *xmpOffset;
	guint32 size;
#endif /* HAVE_EXEMPI */

	if ((image = TIFFOpen (filename, "r")) == NULL){
		g_critical ("Could not open image:'%s'\n", filename);
		goto fail;
	}

#ifdef HAVE_LIBIPTCDATA
	if (TIFFGetField (image, TIFFTAG_RICHTIFFIPTC, &iptcSize, &iptcOffset)) {
		if (TIFFIsByteSwapped(image) != 0) 
			TIFFSwabArrayOfLong((uint32 *) iptcOffset,(unsigned long) iptcSize);
		tracker_read_iptc (iptcOffset,
				   4*iptcSize,
				   metadata);
	}
#endif /* HAVE_LIBIPTCDATA */

	/* FIXME There are problems between XMP data embedded with different tools
	   due to bugs in the original spec (type) */
#ifdef HAVE_EXEMPI
	if (TIFFGetField (image, TIFFTAG_XMLPACKET, &size, &xmpOffset)) {
		tracker_read_xmp (xmpOffset,
				  size,
				  metadata);
	}
#endif /* HAVE_EXEMPI */

	if (TIFFGetField (image, TIFFTAG_EXIFIFD, &exifOffset) &&
	    TIFFReadEXIFDirectory (image, exifOffset)) {
		for (tag = exiftags; tag->name; ++tag) {
			gchar *str = NULL;
			
			switch (tag->type) {
			case TIFF_TAGTYPE_STRING: {
				gchar *var = NULL;

				if (TIFFGetField (image, tag->tag, &var) == 1) {
					str = g_strdup_printf ("%s", var);
				}				
				break;
			}

			case TIFF_TAGTYPE_UINT16: {
				guint16 var = 0;

				if (TIFFGetField (image, tag->tag, &var) == 1) {
					str = g_strdup_printf ("%i", var);
				}				
				break;
			}

			case TIFF_TAGTYPE_UINT32: {
				guint32 var = 0;

				if (TIFFGetField (image, tag->tag, &var) == 1) {
					str = g_strdup_printf ("%i", var);
				}				
				break;
			}

			case TIFF_TAGTYPE_DOUBLE: {
				gfloat var = 0.0;

				if (TIFFGetField (image, tag->tag, &var) == 1) {
					str = g_strdup_printf ("%f", var);
				}
				break;
			}

			case TIFF_TAGTYPE_C16_UINT16: {
				void *var = NULL;
				guint16 count;

				if (TIFFGetField (image, tag->tag, &count, &var) == 1) {
					/* We only take only the first for now */
					str = g_strdup_printf ("%i", * (guint16*) var);
				}
 				break;
			}
				
			default:
				break;
			}

			if (!str) {
				continue;
			}
			
			if (tag->post) {
				metadata_append (metadata,
						 g_strdup (tag->name),
						 tracker_escape_metadata ((*tag->post) (str)),
						 tag->multi);
			} else {
				metadata_append (metadata, 
						 g_strdup (tag->name),
						 tracker_escape_metadata (str),
						 tag->multi);
			}
			
			g_free (str);
		}
	}

	/* We want to give native tags priority over XMP/Exif */
	for (tag = tags; tag->name; ++tag) {
		gchar *str = NULL;
		
		switch (tag->type) {
		case TIFF_TAGTYPE_STRING: {
			gchar *var = NULL;

			if (TIFFGetField (image, tag->tag, &var) == 1) {
				str = g_strdup_printf ("%s", var);
			}
			break;
		}

		case TIFF_TAGTYPE_UINT16: {
			guint16 var = 0;

			if (TIFFGetField (image, tag->tag, &var) == 1) {
				str = g_strdup_printf ("%i", var);
			}
			break;
		}

		case TIFF_TAGTYPE_UINT32: {
			guint32 var = 0;

			if (TIFFGetField (image, tag->tag, &var) == 1) {
				str = g_strdup_printf ("%i", var);
			}
			break;
		}

		case TIFF_TAGTYPE_DOUBLE: {
			gfloat var = 0.0;

			if (TIFFGetField (image, tag->tag, &var) == 1) {
				str = g_strdup_printf ("%f", var);
			}
			break;
		}

		default:
			break;
		}

		if (!str) {
			continue;
		}
		
		if (tag->post) {
			metadata_append (metadata, 
					 g_strdup (tag->name),
					 tracker_escape_metadata ((*tag->post) (str)),
					 tag->multi);
		} else {
			metadata_append (metadata, 
					 g_strdup (tag->name),
					 tracker_escape_metadata (str),
					 tag->multi);
		}
		
		g_free (str);
	}

	TIFFClose (image);

fail:
	/* We fallback to the file's modified time for the
	 * "Image:Date" metadata if it doesn't exist.
	 *
	 * FIXME: This shouldn't be necessary.
	 */
	if (!g_hash_table_lookup (metadata, "Image:Date")) {
		gchar *date;
		guint64 mtime;
		
		mtime = tracker_file_get_mtime (filename);
		date = tracker_date_to_string ((time_t) mtime);
		
		g_hash_table_insert (metadata,
				     g_strdup ("Image:Date"),
				     tracker_escape_metadata (date));
		g_free (date);
	}
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
