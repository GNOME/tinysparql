/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

/*
 * FIXME: We should try to get raw data (from libexif) to avoid processing.
 */

#include "config.h"

#include <stdio.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include <ctype.h>
#include <string.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <setjmp.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <jpeglib.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-main.h"
#include "tracker-xmp.h"
#include "tracker-iptc.h"

#define NMM_PREFIX TRACKER_NMM_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

#ifdef HAVE_EXEMPI
#define XMP_NAMESPACE	     "http://ns.adobe.com/xap/1.0/\x00"
#define XMP_NAMESPACE_LENGTH 29
#endif /* HAVE_EXEMPI */

#ifdef HAVE_LIBEXIF
#include <libexif/exif-data.h>
#define EXIF_DATE_FORMAT "%Y:%m:%d %H:%M:%S"
#endif /* HAVE_LIBEXIF */

#ifdef HAVE_LIBIPTCDATA
#define PS3_NAMESPACE	     "Photoshop 3.0\0"
#define PS3_NAMESPACE_LENGTH 14
#include <libiptcdata/iptc-jpeg.h>
#endif /* HAVE_LIBIPTCDATA */

static void extract_jpeg (const gchar *filename,
			  GPtrArray   *metadata);

static TrackerExtractData data[] = {
	{ "image/jpeg", extract_jpeg },
	{ NULL, NULL }
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

struct tej_error_mgr 
{
	struct jpeg_error_mgr jpeg;
	jmp_buf setjmp_buffer;
};

static void tracker_extract_jpeg_error_exit (j_common_ptr cinfo)
{
    struct tej_error_mgr *h = (struct tej_error_mgr *)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(h->setjmp_buffer, 1);
}

#ifdef HAVE_LIBEXIF

typedef gchar * (*PostProcessor) (const gchar*, gboolean *free_it);

typedef struct {
	ExifTag       tag;
	const gchar  *name;
	PostProcessor post;
	const gchar  *rdf_class;
	const gchar  *rdf_property;
	const gchar  *urn_prefix;
} TagType;

static gchar *date_to_iso8601	(const gchar *exif_date, gboolean *free_it);
static gchar *fix_focal_length	(const gchar *fl, gboolean *free_it);
static gchar *fix_flash		(const gchar *flash, gboolean *free_it);
static gchar *fix_fnumber	(const gchar *fn, gboolean *free_it);
static gchar *fix_exposure_time (const gchar *et, gboolean *free_it);
static gchar *fix_orientation   (const gchar *orientation, gboolean *free_it);
static gchar *fix_metering_mode (const gchar *metering_mode, gboolean *free_it);
static gchar *fix_white_balance (const gchar *white_balance, gboolean *free_it);

static TagType tags[] = {
	{ EXIF_TAG_PIXEL_Y_DIMENSION, NFO_PREFIX "height", NULL, NULL, NULL, NULL },
	{ EXIF_TAG_PIXEL_X_DIMENSION, NFO_PREFIX "width", NULL, NULL, NULL, NULL },
	{ EXIF_TAG_RELATED_IMAGE_WIDTH, NFO_PREFIX "width", NULL, NULL, NULL, NULL },
	{ EXIF_TAG_DOCUMENT_NAME, NIE_PREFIX "title", NULL, NULL, NULL, NULL },
	/* { -1, "Image:Album", NULL }, */
	{ EXIF_TAG_DATE_TIME, NIE_PREFIX "contentCreated", date_to_iso8601, NULL, NULL, NULL },
	{ EXIF_TAG_DATE_TIME_ORIGINAL, NIE_PREFIX "contentCreated", date_to_iso8601, NULL, NULL, NULL },
	/* { -1, "Image:Keywords", NULL }, */
	{ EXIF_TAG_ARTIST, NCO_PREFIX "creator", NULL, NCO_PREFIX "Contact", NCO_PREFIX "fullname", "urn:artist:%s"},
	{ EXIF_TAG_USER_COMMENT, NIE_PREFIX "comment", NULL, NULL, NULL, NULL },
	{ EXIF_TAG_IMAGE_DESCRIPTION, NIE_PREFIX "description", NULL, NULL, NULL, NULL },
	/* { EXIF_TAG_SOFTWARE, "Image:Software", NULL }, */
	{ EXIF_TAG_MAKE, NMM_PREFIX "camera", NULL, NULL, NULL, NULL },
	{ EXIF_TAG_MODEL, NMM_PREFIX "camera", NULL, NULL, NULL, NULL },
	{ EXIF_TAG_ORIENTATION, NFO_PREFIX "orientation", fix_orientation, NULL, NULL, NULL },
	/* { EXIF_TAG_EXPOSURE_PROGRAM, "Image:ExposureProgram", NULL }, */
	{ EXIF_TAG_EXPOSURE_TIME, NMM_PREFIX "exposureTime", fix_exposure_time, NULL, NULL, NULL },
	{ EXIF_TAG_FNUMBER, NMM_PREFIX "fnumber", fix_fnumber, NULL, NULL, NULL },
	{ EXIF_TAG_FLASH, NMM_PREFIX "flash", fix_flash, NULL, NULL, NULL },
	{ EXIF_TAG_FOCAL_LENGTH, NMM_PREFIX "focalLength", fix_focal_length, NULL, NULL, NULL },
	{ EXIF_TAG_ISO_SPEED_RATINGS, NMM_PREFIX "isoSpeed", NULL, NULL, NULL, NULL },
	{ EXIF_TAG_METERING_MODE, NMM_PREFIX "meteringMode", fix_metering_mode, NULL, NULL, NULL },
	{ EXIF_TAG_WHITE_BALANCE, NMM_PREFIX "whiteBalance", fix_white_balance, NULL, NULL, NULL },
	{ EXIF_TAG_COPYRIGHT, NIE_PREFIX "copyright", NULL, NULL, NULL, NULL },
	{ -1, NULL, NULL }
};

#endif /* HAVE_EXIF */

#ifdef HAVE_LIBEXIF

static gchar *
date_to_iso8601 (const gchar *date, gboolean *free_it)
{
	/* From: ex; date "2007:04:15 15:35:58"
	 * To  : ex. "2007-04-15T17:35:58+0200 where +0200 is localtime */
	*free_it = TRUE;
	return tracker_date_format_to_iso8601 (date, EXIF_DATE_FORMAT);
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
	*free_it = FALSE;
	
	if (strcasestr (flash, "flash fired")) {
		return (gchar *) "nmm:flash-on";
	} else {
		return (gchar *) "nmm:flash-off";
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
				case 0:
				return (gchar *) "nfo:orientation-top";
				case 1:
				return (gchar *) "nfo:orientation-top-mirror";
				case 2:
				return (gchar *) "nfo:orientation-bottom";
				case 3:
				return (gchar *) "nfo:orientation-bottom-mirror";
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
read_exif (const unsigned char *buffer,
	   size_t		len,
	   const gchar         *uri,
	   GPtrArray	       *metadata)
{
	ExifData *exif;
	TagType  *p;

	exif = exif_data_new_from_data ((unsigned char *) buffer, len);

	for (p = tags; p->name; ++p) {
		ExifEntry *entry = exif_data_get_entry (exif, p->tag);

		if (entry) {
			gchar buffer_[1024];
			gchar *what_i_need;
			gboolean free_it = FALSE;

			exif_entry_get_value (entry, buffer_, 1024);

			if (p->post) {
				what_i_need = (*p->post) (buffer_, &free_it);
			} else {
				what_i_need = buffer_;
			}

			if (p->urn_prefix) {
				gchar *canonical_uri = tracker_uri_printf_escaped (p->urn_prefix, what_i_need);
				tracker_statement_list_insert (metadata, canonical_uri, RDF_TYPE, p->rdf_class);
				tracker_statement_list_insert (metadata, canonical_uri, p->rdf_property, what_i_need);
				tracker_statement_list_insert (metadata, uri, p->name, canonical_uri);
				g_free (canonical_uri);
			} else {
				tracker_statement_list_insert (metadata, uri, p->name, what_i_need);
			}

			if (free_it)
				g_free (what_i_need);
		}
	}
	
	exif_data_free (exif);
}

#endif /* HAVE_LIBEXIF */


static void
extract_jpeg (const gchar *uri,
	      GPtrArray   *metadata)
{
	struct jpeg_decompress_struct  cinfo;
	struct tej_error_mgr	       tejerr;
	struct jpeg_marker_struct     *marker;
	FILE			      *f;
	goffset                        size;
	gchar                         *filename;

	filename = g_filename_from_uri (uri, NULL, NULL);

	size = tracker_file_get_size (filename);

	if (size < 18) {
		return;
	}

	f = tracker_file_open (filename, "rb", FALSE);

	if (f) {
		gchar *str;
		gsize  len;
#ifdef HAVE_LIBIPTCDATA
		gsize  offset;
		gsize  sublen;
#endif /* HAVE_LIBEXIF */

		tracker_statement_list_insert (metadata, uri, 
		                          RDF_TYPE, 
		                          NFO_PREFIX "Image");

		cinfo.err = jpeg_std_error (&tejerr.jpeg);
		tejerr.jpeg.error_exit = tracker_extract_jpeg_error_exit;
		if (setjmp(tejerr.setjmp_buffer)) {
			goto fail;
		}

		jpeg_create_decompress (&cinfo);
		
		jpeg_save_markers (&cinfo, JPEG_COM, 0xFFFF);
		jpeg_save_markers (&cinfo, JPEG_APP0 + 1, 0xFFFF);
		jpeg_save_markers (&cinfo, JPEG_APP0 + 13, 0xFFFF);
		
		jpeg_stdio_src (&cinfo, f);
		
		jpeg_read_header (&cinfo, TRUE);
		
		/* FIXME? It is possible that there are markers after SOS,
		 * but there shouldn't be. Should we decompress the whole file?
		 *
		 * jpeg_start_decompress(&cinfo);
		 * jpeg_finish_decompress(&cinfo);
		 *
		 * jpeg_calc_output_dimensions(&cinfo);
		 */
		
		marker = (struct jpeg_marker_struct *) &cinfo.marker_list;
		
		while (marker) {
			switch (marker->marker) {
			case JPEG_COM:
				len = marker->data_length;
				str = g_strndup ((gchar*) marker->data, len);

				tracker_statement_list_insert (metadata, uri,
							  NIE_PREFIX "comment",
							  str);
				g_free (str);
				break;
				
			case JPEG_APP0+1:
				str = (gchar*) marker->data;
				len = marker->data_length;

#ifdef HAVE_LIBEXIF
				if (strncmp ("Exif", (gchar*) (marker->data), 5) == 0) {
					read_exif ((unsigned char*) marker->data,
						   marker->data_length, uri,
						   metadata);
				}
#endif /* HAVE_LIBEXIF */

#ifdef HAVE_EXEMPI

				if (strncmp (XMP_NAMESPACE, str, XMP_NAMESPACE_LENGTH) == 0) {
					tracker_read_xmp (str + XMP_NAMESPACE_LENGTH,
							  len - XMP_NAMESPACE_LENGTH,
							  uri, metadata);
				}
#endif /* HAVE_EXEMPI */
				break;
			case JPEG_APP0+13:
				str = (gchar*) marker->data;
				len = marker->data_length;
#ifdef HAVE_LIBIPTCDATA
				if (strncmp (PS3_NAMESPACE, str, PS3_NAMESPACE_LENGTH) == 0) {
					offset = iptc_jpeg_ps3_find_iptc (str, len, &sublen);
					if (offset>0) {
						tracker_read_iptc (str + offset,
								   sublen,
								   uri, metadata);
					}
				}
#endif /* HAVE_LIBIPTCDATA */
				break;
			default:
				marker = marker->next;
				continue;
			}

			marker = marker->next;
		}

		/* We want native size to have priority over EXIF, XMP etc */
		tracker_statement_list_insert_with_int (metadata, uri,
						   NFO_PREFIX "width",
						   cinfo.image_width);
		tracker_statement_list_insert_with_int (metadata, uri,
						   NFO_PREFIX "height",
						    cinfo.image_height);

fail:
		tracker_file_close (f, FALSE);
	}

	g_free (filename);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
