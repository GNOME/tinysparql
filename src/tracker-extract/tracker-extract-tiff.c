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

#include <tiff.h>
#include <tiffio.h>

#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-file-utils.h>

#include "tracker-main.h"
#include "tracker-xmp.h"
#include "tracker-iptc.h"

#define EXIF_DATE_FORMAT     "%Y:%m:%d %H:%M:%S"

#define NMM_PREFIX TRACKER_NMM_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

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
	guint          tag;
	const gchar   *name;
	TagType        type;
	PostProcessor  post;
} TiffTag;
 
static void   extract_tiff    (const gchar *filename,
			       GPtrArray   *metadata);
static gchar *date_to_iso8601 (gchar       *date);

static TrackerExtractData extract_data[] = {
	{ "image/tiff", extract_tiff },
	{ NULL, NULL }
};

/* FIXME: We are missing some */
static TiffTag tags[] = {
	{ TIFFTAG_ARTIST, NCO_PREFIX "creator", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_COPYRIGHT, NIE_PREFIX "copyright", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_DATETIME, NIE_PREFIX "contentCreated", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_DOCUMENTNAME, NIE_PREFIX "title", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_IMAGEDESCRIPTION, NIE_PREFIX "comment", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_IMAGEWIDTH, NFO_PREFIX "width", TIFF_TAGTYPE_UINT32, NULL },
	{ TIFFTAG_IMAGELENGTH, NFO_PREFIX "height", TIFF_TAGTYPE_UINT32, NULL },
	{ TIFFTAG_MAKE, "Image:CameraMake", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_MODEL, "Image:CameraModel", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_ORIENTATION, "Image:Orientation", TIFF_TAGTYPE_UINT16, NULL },
	{ TIFFTAG_SOFTWARE, "Image:Software", TIFF_TAGTYPE_STRING, NULL },
	{ -1, NULL, TIFF_TAGTYPE_UNDEFINED, NULL }
};

static TiffTag exiftags[] = {
	{ EXIFTAG_EXPOSURETIME, "Image:ExposureTime", TIFF_TAGTYPE_DOUBLE, NULL},
	{ EXIFTAG_FNUMBER, "Image:FNumber", TIFF_TAGTYPE_DOUBLE, NULL},
	{ EXIFTAG_EXPOSUREPROGRAM, "Image:ExposureProgram", TIFF_TAGTYPE_UINT16, NULL },
	{ EXIFTAG_ISOSPEEDRATINGS, "Image:ISOSpeed", TIFF_TAGTYPE_C16_UINT16, NULL},
	{ EXIFTAG_DATETIMEORIGINAL, NIE_PREFIX "contentCreated", TIFF_TAGTYPE_STRING, date_to_iso8601 },
	{ EXIFTAG_METERINGMODE, "Image:MeteringMode", TIFF_TAGTYPE_UINT16, NULL},
	{ EXIFTAG_FLASH, "Image:Flash", TIFF_TAGTYPE_UINT16, NULL},
	{ EXIFTAG_FOCALLENGTH, "Image:FocalLength", TIFF_TAGTYPE_DOUBLE, NULL},
	{ EXIFTAG_PIXELXDIMENSION, NFO_PREFIX "width", TIFF_TAGTYPE_UINT32, NULL},
	{ EXIFTAG_PIXELYDIMENSION, NFO_PREFIX "height", TIFF_TAGTYPE_UINT32, NULL},
	{ EXIFTAG_WHITEBALANCE, "Image:WhiteBalance", TIFF_TAGTYPE_UINT16, NULL},
	{ -1, NULL, TIFF_TAGTYPE_UNDEFINED, NULL }
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
extract_tiff (const gchar *uri, 
	      GPtrArray   *metadata)
{
	TIFF *image;
	glong exifOffset;
	gchar *filename = g_filename_from_uri (uri, NULL, NULL);
	TiffTag *tag;

	gchar buffer[1024];
	gchar *text;
	guint16 varui16 = 0;
	guint32 varui32 = 0;
	void *data;
	guint16 count16;

	gfloat vardouble;

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
		g_free (filename);
		return;
	}

	tracker_statement_list_insert (metadata, uri, 
	                          RDF_TYPE, 
	                          NFO_PREFIX "Image");

#ifdef HAVE_LIBIPTCDATA
	if (TIFFGetField (image, TIFFTAG_RICHTIFFIPTC, &iptcSize, &iptcOffset)) {
		if (TIFFIsByteSwapped(image) != 0) 
			TIFFSwabArrayOfLong((uint32 *) iptcOffset,(unsigned long) iptcSize);
		tracker_read_iptc (iptcOffset,
				   4*iptcSize,
				   uri, metadata);
	}
#endif /* HAVE_LIBIPTCDATA */

	/* FIXME There are problems between XMP data embedded with different tools
	   due to bugs in the original spec (type) */
#ifdef HAVE_EXEMPI
	if (TIFFGetField (image, TIFFTAG_XMLPACKET, &size, &xmpOffset)) {
		tracker_read_xmp (xmpOffset,
				  size,
				  uri,
				  metadata);
	}
#endif /* HAVE_EXEMPI */

	if (TIFFGetField (image, TIFFTAG_EXIFIFD, &exifOffset)) {
		if (TIFFReadEXIFDirectory (image, exifOffset)) {
			for (tag = exiftags; tag->name; ++tag) {
				switch (tag->type) {
				case TIFF_TAGTYPE_STRING:
					if (!TIFFGetField (image, tag->tag, &text)) {
						continue;
					}

					sprintf (buffer,"%s",text);
					break;
				case TIFF_TAGTYPE_UINT16:						
					if (!TIFFGetField (image, tag->tag, &varui16)) {
						continue;
					}

					sprintf (buffer,"%i",varui16);
					break;
				case TIFF_TAGTYPE_UINT32:
					if (!TIFFGetField (image, tag->tag, &varui32)) {
						continue;
					}

					sprintf(buffer,"%i",varui32);
					break;
				case TIFF_TAGTYPE_DOUBLE:
					if (!TIFFGetField (image, tag->tag, &vardouble)) {
						continue;
					}

					sprintf (buffer,"%f",vardouble);
					break;
				case TIFF_TAGTYPE_C16_UINT16:						
					if (!TIFFGetField (image, tag->tag, &count16, &data)) {
						continue;
					}

					/* We only take only the first for now */
					sprintf (buffer,"%i",*(guint16 *)data);
					break;	

				default:
					continue;
					break;
				}

				if (tag->post) {
					tracker_statement_list_insert (metadata, uri,
								  tag->name,
								  (*tag->post) (buffer));
				} else {
					tracker_statement_list_insert (metadata, uri,
								  tag->name,
							 	  buffer);
				}
			}
		}
	}

	/* We want to give native tags priority over XMP/Exif */
	for (tag = tags; tag->name; ++tag) {
		gchar *what_i_need;

		switch (tag->type) {
			case TIFF_TAGTYPE_STRING:
				if (!TIFFGetField (image, tag->tag, &text)) {
					continue;
				}

				sprintf (buffer,"%s", text);
				break;
			case TIFF_TAGTYPE_UINT16:
				if (!TIFFGetField (image, tag->tag, &varui16)) {
					continue;
				}

				sprintf (buffer,"%i",varui16);
				break;
			case TIFF_TAGTYPE_UINT32:
				if (!TIFFGetField (image, tag->tag, &varui32)) {
					continue;
				}

				sprintf(buffer,"%i",varui32);
				break;
			case TIFF_TAGTYPE_DOUBLE:
				if (!TIFFGetField (image, tag->tag, &vardouble)) {
					continue;
				}

				sprintf (buffer,"%f",vardouble);
				break;
			default:
				continue;
				break;
			}

		if (tag->post) {
			what_i_need = (*tag->post) (buffer);
		} else {
			what_i_need = buffer;
		}

		if (tag->tag == TIFFTAG_ARTIST) {
			tracker_statement_list_insert (metadata, ":", RDF_TYPE, NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", what_i_need);
			tracker_statement_list_insert (metadata, uri, tag->name, ":");
		} else {
			tracker_statement_list_insert (metadata, uri, tag->name, what_i_need);
		}

		if (tag->post) 
			g_free (what_i_need);
	}

	TIFFClose (image);

	g_free (filename);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return extract_data;
}
