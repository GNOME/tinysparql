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

#include "tracker-extract.h"

#include <glib.h>

#include <tiff.h>
#include <tiffio.h>

#include "tracker-xmp.h"

#define XMP_NAMESPACE_LENGTH 29

typedef gchar * (*PostProcessor) (gchar *);

typedef enum {
	TIFF_TAGTYPE_UNDEFINED = 0,
	TIFF_TAGTYPE_STRING,
	TIFF_TAGTYPE_UINT16,
	TIFF_TAGTYPE_UINT32,
	TIFF_TAGTYPE_DOUBLE
} TagType;

typedef struct {
	guint	       tag;
	gchar	      *name;
	TagType        type;
	PostProcessor  post;
} TiffTag;

#define EXIF_DATE_FORMAT "%Y:%m:%d %H:%M:%S"

static gchar *
date_to_iso8601 (gchar *exif_date)
{
	/* ex; date "2007:04:15 15:35:58"
	   To
	   ex. "2007-04-15T17:35:58+0200 where +0200 is localtime
	*/
	return tracker_generic_date_to_iso8601 (exif_date, EXIF_DATE_FORMAT);
}


/* FIXME We are missing some */
TiffTag tags[] = {
	{ TIFFTAG_ARTIST, "Image:Creator", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_COPYRIGHT, "File:Copyright", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_DATETIME, "Image:Date", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_DOCUMENTNAME, "Image:Title", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_IMAGEDESCRIPTION, "Image:Comments", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_IMAGEWIDTH, "Image:Width", TIFF_TAGTYPE_UINT32, NULL },
	{ TIFFTAG_IMAGELENGTH, "Image:Height", TIFF_TAGTYPE_UINT32, NULL },
	{ TIFFTAG_MAKE, "Image:CameraMake", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_MODEL, "Image:CameraModel", TIFF_TAGTYPE_STRING, NULL },
	{ TIFFTAG_ORIENTATION, "Image:Orientation", TIFF_TAGTYPE_UINT16, NULL },
	{ TIFFTAG_SOFTWARE, "Image:Software", TIFF_TAGTYPE_STRING, NULL },
	{ -1, NULL, TIFF_TAGTYPE_UNDEFINED, NULL }
};

TiffTag exiftags[] = {
	{EXIFTAG_EXPOSURETIME, "Image:ExposureTime", TIFF_TAGTYPE_DOUBLE, NULL},
	{EXIFTAG_FNUMBER, "Image:FNumber", TIFF_TAGTYPE_DOUBLE, NULL},
	{EXIFTAG_EXPOSUREPROGRAM, "Image:ExposureProgram", TIFF_TAGTYPE_UINT16 ,NULL },
	{EXIFTAG_ISOSPEEDRATINGS, "Image:ISOSpeed",TIFF_TAGTYPE_UINT32, NULL},
	{EXIFTAG_DATETIMEORIGINAL, "Image:Date", TIFF_TAGTYPE_STRING,date_to_iso8601},
	{EXIFTAG_METERINGMODE, "Image:MeteringMode", TIFF_TAGTYPE_UINT16, NULL},
	{EXIFTAG_FLASH, "Image:Flash", TIFF_TAGTYPE_UINT16, NULL},
	{EXIFTAG_FOCALLENGTH, "Image:FocalLength", TIFF_TAGTYPE_UINT16, NULL},
	{EXIFTAG_PIXELXDIMENSION, "Image:Width",TIFF_TAGTYPE_UINT32, NULL},
	{EXIFTAG_PIXELYDIMENSION, "Image:Height", TIFF_TAGTYPE_UINT32,NULL},
	{EXIFTAG_WHITEBALANCE, "Image:WhiteBalance", TIFF_TAGTYPE_UINT16,NULL},
	{ -1, NULL, TIFF_TAGTYPE_UNDEFINED, NULL }
};


static void
tracker_extract_tiff (const gchar *filename, GHashTable *metadata)
{
	TIFF	 *image;
	long	  exifOffset;

	TiffTag  *tag;

	gchar buffer[1024];
	guint16 varui16 = 0;
	guint32 varui32 = 0;
	float vardouble;

#ifdef HAVE_EXEMPI
	gchar	 *xmpOffset;
	uint32	  size;
#endif /* HAVE_EXEMPI */


	if((image = TIFFOpen(filename, "r")) == NULL){
		g_error("Could not open image\n");
		return;
	}

	/* FIXME There are problems between XMP data embedded with different tools
	   due to bugs in the original spec (type) */

#ifdef HAVE_EXEMPI

	if (TIFFGetField(image, TIFFTAG_XMLPACKET, &size, &xmpOffset)) {
		tracker_read_xmp (xmpOffset,
				  size,
				  metadata);
	}

#endif /* HAVE_EXEMPI */

	if (TIFFGetField(image, TIFFTAG_EXIFIFD, &exifOffset)) {

		if (TIFFReadEXIFDirectory(image, exifOffset)) {

			for (tag = exiftags; tag->name; ++tag) {

				switch (tag->type) {
				case TIFF_TAGTYPE_STRING:
					if (!TIFFGetField(image, tag->tag, &buffer)) {
						continue;
					}
					break;
				case TIFF_TAGTYPE_UINT16:
					if (!TIFFGetField(image, tag->tag, &varui32)) {
						continue;
					}

					sprintf(buffer,"%i",varui16);
					break;
				case TIFF_TAGTYPE_UINT32:
					if (!TIFFGetField(image, tag->tag, &varui32)) {
						continue;
					}

					sprintf(buffer,"%i",varui32);
					break;
				case TIFF_TAGTYPE_DOUBLE:
					if (!TIFFGetField(image, tag->tag, &vardouble)) {
						continue;
					}

					sprintf(buffer,"%f",vardouble);
					break;
				default:
					continue;
					break;
				}

				if (tag->post) {
					g_hash_table_insert (metadata, g_strdup (tag->name),
							     g_strdup ((*tag->post) (buffer)));
				} else {
					g_hash_table_insert (metadata, g_strdup (tag->name),
							     g_strdup (buffer));
				}
			}

		}
	}

	/* We want to give native tags priority over XMP/Exif */
	for (tag = tags; tag->name; ++tag) {
		switch (tag->type) {
			case TIFF_TAGTYPE_STRING:
				if (!TIFFGetField(image, tag->tag, &buffer)) {
					continue;
				}
				break;
			case TIFF_TAGTYPE_UINT16:
				if (!TIFFGetField(image, tag->tag, &varui32)) {
					continue;
				}

				sprintf(buffer,"%i",varui16);
				break;
			case TIFF_TAGTYPE_UINT32:
				if (!TIFFGetField(image, tag->tag, &varui32)) {
					continue;
				}

				sprintf(buffer,"%i",varui32);
				break;
			case TIFF_TAGTYPE_DOUBLE:
				if (!TIFFGetField(image, tag->tag, &vardouble)) {
					continue;
				}

				sprintf(buffer,"%f",vardouble);
				break;
			default:
				continue;
				break;
			}

		if (tag->post) {
			g_hash_table_insert (metadata, g_strdup (tag->name),
					     g_strdup ((*tag->post) (buffer)));
		} else {
			g_hash_table_insert (metadata, g_strdup (tag->name),
					     g_strdup (buffer));
		}
	}

	TIFFClose(image);

}




TrackerExtractorData data[] = {
	{ "image/tiff", tracker_extract_tiff },
	{ NULL, NULL }
};


TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}
