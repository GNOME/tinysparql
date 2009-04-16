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

#include <stdio.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include <ctype.h>
#include <string.h>
#include <fcntl.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <tiff.h>
#include <tiffio.h>

#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-utils.h>

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

typedef gchar * (*PostProcessor) (const gchar *, gboolean *);

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
	const gchar  *rdf_class;
	const gchar  *rdf_property;
	const gchar  *urn_prefix;
} TiffTag;
 
static void   extract_tiff    (const gchar *filename,
			       GPtrArray   *metadata);

static gchar *date_to_iso8601	(const gchar *exif_date, gboolean *free_it);
static gchar *fix_focal_length	(const gchar *fl, gboolean *free_it);
static gchar *fix_flash		(const gchar *flash, gboolean *free_it);
static gchar *fix_fnumber	(const gchar *fn, gboolean *free_it);
static gchar *fix_exposure_time (const gchar *et, gboolean *free_it);
static gchar *fix_orientation   (const gchar *orientation, gboolean *free_it);
static gchar *fix_metering_mode (const gchar *metering_mode, gboolean *free_it);
static gchar *fix_white_balance (const gchar *white_balance, gboolean *free_it);

static TrackerExtractData extract_data[] = {
	{ "image/tiff", extract_tiff },
	{ NULL, NULL }
};



/* FIXME: We are missing some */
static TiffTag tags[] = {
	{ TIFFTAG_ARTIST, NCO_PREFIX "creator", TIFF_TAGTYPE_STRING, NULL, NCO_PREFIX "Contact", NCO_PREFIX "fullname" , ":"},
	{ TIFFTAG_COPYRIGHT, NIE_PREFIX "copyright", TIFF_TAGTYPE_STRING, NULL, NULL, NULL, NULL },
	{ TIFFTAG_DATETIME, NIE_PREFIX "contentCreated", TIFF_TAGTYPE_STRING, NULL, NULL, NULL, NULL },
	{ TIFFTAG_DOCUMENTNAME, NIE_PREFIX "title", TIFF_TAGTYPE_STRING, NULL, NULL, NULL, NULL },
	{ TIFFTAG_IMAGEDESCRIPTION, NIE_PREFIX "comment", TIFF_TAGTYPE_STRING, NULL, NULL, NULL, NULL },
	{ TIFFTAG_IMAGEWIDTH, NFO_PREFIX "width", TIFF_TAGTYPE_UINT32, NULL, NULL, NULL, NULL },
	{ TIFFTAG_IMAGELENGTH, NFO_PREFIX "height", TIFF_TAGTYPE_UINT32, NULL, NULL, NULL, NULL },
	{ TIFFTAG_MAKE, NMM_PREFIX "camera", TIFF_TAGTYPE_STRING, NULL, NULL, NULL, NULL },
	{ TIFFTAG_MODEL, NMM_PREFIX "camera", TIFF_TAGTYPE_STRING, NULL, NULL, NULL, NULL },
	{ TIFFTAG_ORIENTATION, NFO_PREFIX "orientation", TIFF_TAGTYPE_UINT16, fix_orientation, NULL, NULL, NULL },
	/* { TIFFTAG_SOFTWARE, "Image:Software", TIFF_TAGTYPE_STRING, NULL }, */
	{ -1, NULL, TIFF_TAGTYPE_UNDEFINED, NULL, NULL, NULL, NULL }
};

static TiffTag exiftags[] = {
	{ EXIFTAG_EXPOSURETIME, NMM_PREFIX "exposureTime", TIFF_TAGTYPE_DOUBLE, fix_exposure_time, NULL, NULL, NULL},
	{ EXIFTAG_FNUMBER, NMM_PREFIX "fnumber", TIFF_TAGTYPE_DOUBLE, fix_fnumber, NULL, NULL, NULL},
	/* { EXIFTAG_EXPOSUREPROGRAM, "Image:ExposureProgram", TIFF_TAGTYPE_UINT16, NULL, NULL, NULL, NULL }, */
	{ EXIFTAG_ISOSPEEDRATINGS, NMM_PREFIX "isoSpeed", TIFF_TAGTYPE_C16_UINT16, NULL, NULL, NULL, NULL},
	{ EXIFTAG_DATETIMEORIGINAL, NIE_PREFIX "contentCreated", TIFF_TAGTYPE_STRING, date_to_iso8601, NULL, NULL, NULL },
	{ EXIFTAG_METERINGMODE, NMM_PREFIX "meteringMode", TIFF_TAGTYPE_UINT16, fix_metering_mode, NULL, NULL, NULL },
	{ EXIFTAG_FLASH, NMM_PREFIX "flash", TIFF_TAGTYPE_UINT16, fix_flash, NULL, NULL, NULL },
	{ EXIFTAG_FOCALLENGTH, NMM_PREFIX "focalLength", TIFF_TAGTYPE_DOUBLE, fix_focal_length, NULL, NULL, NULL},
	{ EXIFTAG_PIXELXDIMENSION, NFO_PREFIX "width", TIFF_TAGTYPE_UINT32, NULL, NULL, NULL, NULL},
	{ EXIFTAG_PIXELYDIMENSION, NFO_PREFIX "height", TIFF_TAGTYPE_UINT32, NULL, NULL, NULL, NULL},
	{ EXIFTAG_WHITEBALANCE, NMM_PREFIX "whiteBalance", TIFF_TAGTYPE_UINT16, fix_white_balance, NULL, NULL, NULL },
	{ -1, NULL, TIFF_TAGTYPE_UNDEFINED, NULL, NULL, NULL, NULL }
};




#ifndef HAVE_STRCASESTR

static gchar *
strcasestr (const gchar *haystack, 
	    const gchar *needle)
{
	gchar *p;
	gchar *startn = NULL;
	gchar *np = NULL;

	for (p = (gchar *) haystack; *p; p++) {
		if (np) {
			if (toupper (*p) == toupper (*np)) {
				if (!*++np) {
					return startn;
				}
			} else {
				np = 0;
			}
		} else if (toupper (*p) == toupper (*needle)) {
			np = (gchar *) needle + 1;
			startn = p;
		}
	}

	return NULL;
}

#endif /* HAVE_STRCASESTR */

static gchar *
date_to_iso8601	(const gchar *exif_date, gboolean *free_it)
{
	/* ex; date "2007:04:15 15:35:58"
	 * To
	 * ex. "2007-04-15T17:35:58+0200 where +0200 is localtime
	 */
	*free_it = TRUE;
	return tracker_date_format_to_iso8601 (exif_date, EXIF_DATE_FORMAT);
}


static gchar *
fix_focal_length (const gchar *fl, gboolean *free_it)
{
	*free_it = TRUE;
	return g_strndup (fl, strstr (fl, " mm") - fl);
}

static gchar *
fix_flash (const gchar *flash, gboolean *free_it)
{
	/* Found in the field: Auto, Did not fire, Red-eye reduction */

	*free_it = FALSE;
	
	if (strcasestr (flash, "not fire")) {
		return (gchar *) "nmm:flash-off";
	} else {
		return (gchar *) "nmm:flash-on";
	}
}

static gchar *
fix_fnumber (const gchar *fn, gboolean *free_it)
{
	gchar *new_fn;

	if (!fn) {
		*free_it = FALSE;
		return NULL;
	}

	new_fn = g_strdup (fn);

	if (new_fn[0] == 'F') {
		new_fn[0] = ' ';
	} else if (fn[0] == 'f' && new_fn[1] == '/') {
		new_fn[0] = new_fn[1] = ' ';
	}

	*free_it = TRUE;
	return g_strstrip (new_fn);
}

static gchar *
fix_exposure_time (const gchar *et, gboolean *free_it)
{
	gchar *sep;

	sep = strchr (et, '/');

	*free_it = TRUE;
	
	if (sep) {
		gdouble fraction;

		fraction = g_ascii_strtod (sep + 1, NULL);

		if (fraction > 0.0) {
			gdouble val;
			gchar	buf[G_ASCII_DTOSTR_BUF_SIZE];

			val = 1.0f / fraction;
			g_ascii_dtostr (buf, sizeof(buf), val);

			return g_strdup (buf);
		}
	}

	return g_strdup (et);
}

static gchar *
fix_orientation (const gchar *orientation, gboolean *free_it)
{
	guint i;
	static const gchar *ostr[8] = {
		/* 0 */ "top - left",
		/* 1 */ "top - right",
		/* 2 */ "bottom - right",
		/* 3 */ "bottom - left",
		/* 4 */ "left - top",
		/* 5 */ "right - top",
		/* 6 */ "right - bottom",
		/* 7 */ "left - bottom"
	};

	*free_it = FALSE;
	
	for (i=0; i < 8; i++) {
		if (g_strcmp0 (orientation,ostr[i]) == 0) {
			switch (i) {
				default:
				case 0:
				return (gchar *) "nfo:orientation-top";
				case 1:
				return (gchar *) "nfo:orientation-top-mirror"; // not sure
				case 2:
				return (gchar *) "nfo:orientation-bottom-mirror"; // not sure
				case 3:
				return (gchar *) "nfo:orientation-bottom";
				case 4:
				return (gchar *) "nfo:orientation-left-mirror";
				case 5:
				return (gchar *) "nfo:orientation-right";
				case 6:
				return (gchar *) "nfo:orientation-right-mirror";
				case 7:
				return (gchar *) "nfo:orientation-left";
			}
		}
	}

	return (gchar *) "nfo:orientation-top";
}


static gchar *
fix_metering_mode (const gchar *metering_mode, gboolean *free_it)
{
	/* Found in the field: Multi-segment. These will yield as other */

	*free_it = FALSE;
	
	if (strcasestr (metering_mode, "center")) {
		return (gchar *) "nmm:meteringMode-center-weighted-average";
	}

	if (strcasestr (metering_mode, "average")) {
		return (gchar *) "nmm:meteringMode-average";
	}

	if (strcasestr (metering_mode, "spot")) {
		return (gchar *) "nmm:meteringMode-spot";
	}

	if (strcasestr (metering_mode, "multispot")) {
		return (gchar *) "nmm:meteringMode-multispot";
	}

	if (strcasestr (metering_mode, "pattern")) {
		return (gchar *) "nmm:meteringMode-pattern";
	}

	if (strcasestr (metering_mode, "partial")) {
		return (gchar *) "nmm:meteringMode-partial";
	}

	return (gchar *) "nmm:meteringMode-other";
}


static gchar *
fix_white_balance (const gchar *white_balance, gboolean *free_it)
{
	*free_it = FALSE;

	if (strcasestr (white_balance, "auto")) {
		return (gchar *) "nmm:whiteBalance-auto";
	}

	/* Found in the field: sunny, fluorescent, incandescent, cloudy. These
	 * will this way also yield as manual. */

	return (gchar *) "nmm:whiteBalance-manual";
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
					gboolean free_it = FALSE;
					gchar *value = (*tag->post) (buffer, &free_it);
					tracker_statement_list_insert (metadata, uri,
								       tag->name,
								       value);
					if (free_it)
						g_free (value);
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
		gboolean free_it = FALSE;

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
			what_i_need = (*tag->post) (buffer, &free_it);
		} else {
			what_i_need = buffer;
		}


		if (tag->urn_prefix) {
			gchar *canonical_uri;

			if (tag->urn_prefix[0] == ':')
				canonical_uri = tracker_uri_printf_escaped (tag->urn_prefix, what_i_need);
			else
				canonical_uri = (gchar *) tag->urn_prefix;

			tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, tag->rdf_class);
			tracker_statement_list_insert (metadata, canonical_uri, tag->rdf_property, what_i_need);
			tracker_statement_list_insert (metadata, uri, tag->name, canonical_uri);

			if (tag->urn_prefix[0] != ':')
				g_free (canonical_uri);
		} else {
			tracker_statement_list_insert (metadata, uri, tag->name, what_i_need);
		}


		if (free_it) 
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
