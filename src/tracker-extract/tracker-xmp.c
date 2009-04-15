/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Tracker Xmp - Xmp helper functions
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <locale.h>
#include <string.h>

#include <glib.h>

#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-statement-list.h>

#include "tracker-main.h"
#include "tracker-xmp.h"

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX

#ifdef HAVE_EXEMPI

#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>

static const gchar *
fix_metering_mode (const gchar *mode)
{
	gint value;
	value = atoi(mode);

	switch (value) {
	case 0:
		return "unknown";
	case 1:
		return "Average";
	case 2:
		return "CenterWeightedAverage";
	case 3:
		return "Spot";
	case 4:
		return "MultiSpot";
	case 5:
		return "Pattern";
	case 6:
		return "Partial";
	}

	return "unknown";
}

static gchar *
fix_flash (const gchar *flash)
{
	static const gint fired_mask = 0x1;
	gint value;
	value = atoi(flash);
	if (value & fired_mask) {
		return "1";
	} else {
		return "0";
	}
		
}

static const gchar *
fix_white_balance (const gchar *wb)
{
	gint value;
	value = atoi(wb);
	if (wb) {
		return "Manual white balance";
	} else {
		return "Auto white balance";
	}
}

static void tracker_xmp_iter        (XmpPtr          xmp,
				     XmpIteratorPtr  iter,
				     const gchar    *uri,
				     GPtrArray      *metadata,
				     gboolean        append);
static void tracker_xmp_iter_simple (const gchar    *uri,
				     GPtrArray      *metadata,
				     const gchar    *schema,
				     const gchar    *path,
				     const gchar    *value,
				     gboolean        append);


/* We have an array, now recursively iterate over it's children.  Set 'append' to true so that all values of the array are added
   under one entry. */
static void
tracker_xmp_iter_array (XmpPtr       xmp,
			const gchar *uri,
			GPtrArray   *metadata, 
			const gchar *schema, 
			const gchar *path)
{
	XmpIteratorPtr iter;

	iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN);
	tracker_xmp_iter (xmp, iter, uri, metadata, TRUE);
	xmp_iterator_free (iter);
}


/* We have an array, now recursively iterate over it's children.  Set 'append' to false so that only one item is used. */
static void
tracker_xmp_iter_alt_text (XmpPtr       xmp, 
			   const gchar *uri,
			   GPtrArray   *metadata, 
			   const gchar *schema, 
			   const gchar *path)
{
	XmpIteratorPtr iter;

	iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN);
	tracker_xmp_iter (xmp, iter, uri, metadata, FALSE);
	xmp_iterator_free (iter);
}


/* We have a simple element, but need to iterate over the qualifiers */
static void
tracker_xmp_iter_simple_qual (XmpPtr       xmp, 
			      const gchar *uri,
			      GPtrArray   *metadata,
			      const gchar *schema, 
			      const gchar *path, 
			      const gchar *value, 
			      gboolean     append)
{
	XmpIteratorPtr iter;
	XmpStringPtr the_path;
	XmpStringPtr the_prop;
	gchar *locale;
	gchar *sep;
	gboolean ignore_element = FALSE;

	iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN | XMP_ITER_JUSTLEAFNAME);

	the_path = xmp_string_new ();
	the_prop = xmp_string_new ();

	locale = setlocale (LC_ALL, NULL);
	sep = strchr (locale,'.');

	if (sep) {
		locale[sep - locale] = '\0';
	}

	sep = strchr (locale, '_');
	if (sep) {
		locale[sep - locale] = '-';
	}

	while (xmp_iterator_next (iter, NULL, the_path, the_prop, NULL)) {
		const gchar *qual_path = xmp_string_cstr (the_path);
		const gchar *qual_value = xmp_string_cstr (the_prop);

		if (strcmp (qual_path, "xml:lang") == 0) {
			/* Is this a language we should ignore? */
			if (strcmp (qual_value, "x-default") != 0 && 
			    strcmp (qual_value, "x-repair") != 0 && 
			    strcmp (qual_value, locale) != 0) {
				ignore_element = TRUE;
				break;
			}
		}
	}

	if (!ignore_element) {
		tracker_xmp_iter_simple (uri, metadata, schema, path, value, append);
	}

	xmp_string_free (the_prop);
	xmp_string_free (the_path);

	xmp_iterator_free (iter);
}

/* We have a simple element. Add any metadata we know about to the
 * hash table.
 */
static void
tracker_xmp_iter_simple (const gchar *uri,
			 GPtrArray   *metadata,
			 const gchar *schema, 
			 const gchar *path, 
			 const gchar *value, 
			 gboolean     append)
{
	gchar *name;
	const gchar *index;

	name = g_strdup (strchr (path, ':') + 1);
	index = strrchr (name, '[');

	if (index) {
		name[index-name] = '\0';
	}

	/* Dublin Core */
	if (strcmp (schema, NS_DC) == 0) {
		if (strcmp (name, "title") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  "Image:Title", value);
		}
		else if (strcmp (name, "rights") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  NIE_PREFIX "copyright", value);
		}
		else if (strcmp (name, "creator") == 0) {

			tracker_statement_list_insert (metadata, ":", RDF_TYPE, NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", value);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "creator", ":");

		}
		else if (strcmp (name, "description") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:Description", value);
		}
		else if (strcmp (name, "date") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:Date", value);
		}
		else if (strcmp (name, "keywords") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:Keywords", value);
		}
		else if (strcmp (name, "subject") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  NIE_PREFIX "subject", value);

			/* The subject field may contain keywords as well */
			tracker_statement_list_insert (metadata, uri,
						  "Image:Keywords", value);
		}
		else if (strcmp (name, "publisher") == 0) {
			tracker_statement_list_insert (metadata, ":", RDF_TYPE, NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", value);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "publisher", ":");
		}
		else if (strcmp (name, "contributor") == 0) {
			tracker_statement_list_insert (metadata, ":", RDF_TYPE, NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", value);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "contributor", ":");
		}
		else if (strcmp (name, "type") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  DC_PREFIX "type", value);
		}
		else if (strcmp (name, "format") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  DC_PREFIX "format", value);
		}
		else if (strcmp (name, "identifier") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  DC_PREFIX "identifier", value);
		}
		else if (strcmp (name, "source") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  DC_PREFIX "source", value);
		}
		else if (strcmp (name, "language") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  DC_PREFIX "language", value);
		}
		else if (strcmp (name, "relation") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  DC_PREFIX "relation", value);
		}
		else if (strcmp (name, "coverage") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  DC_PREFIX "coverage", value);
		}

	}
	/* Creative Commons */
	else if (strcmp (schema, NS_CC) == 0) {
		if (strcmp (name, "license") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  NIE_PREFIX "license", value);
		}
	}
	/* Exif basic scheme */
	else if (strcmp (schema, NS_EXIF) == 0) {
		if (strcmp (name, "Title") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:Title", value);
		}
		else if (strcmp (name, "DateTimeOriginal") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  "Image:Date", value);
		}
		else if (strcmp (name, "Artist") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:Creator", value);
		}
		else if (strcmp (name, "Software") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:Software", value);
		}
		else if (strcmp (name, "Make") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  "Image:CameraMake", value);
		}
		else if (strcmp (name, "Model") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  "Image:CameraModel", value);
		}
		else if (strcmp (name, "Orientation") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:Orientation", 
						  value);
		}
		else if (strcmp (name, "Flash") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:Flash", 
						  fix_flash (value));
		}
		else if (strcmp (name, "MeteringMode") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:MeteringMode", 
						  fix_metering_mode (value));
		}
		else if (strcmp (name, "ExposureProgram") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:ExposureProgram", value);
		}
		else if (strcmp (name, "ExposureTime") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:ExposureTime", value);
		}
		else if (strcmp (name, "FNumber") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:FNumber", value);
		}
		else if (strcmp (name, "FocalLength") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  "Image:FocalLength", value);
		}
		else if (strcmp (name, "ISOSpeedRatings") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  "Image:ISOSpeed", value);
		}
		else if (strcmp (name, "WhiteBalance") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:WhiteBalance",
						   fix_white_balance (value));
		}
		else if (strcmp (name, "Copyright") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  NIE_PREFIX "copyright", value);
		}
	}
	/* XAP (XMP)scheme */
	else if (strcmp (schema, NS_XAP) == 0) {
	        if (strcmp (name, "Rating") == 0) {
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Rating", value);
		}
		if (strcmp (name, "MetadataDate") == 0) {
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Date", value);
		}
	}
	/* IPTC4XMP scheme */
	else if (strcmp (schema,  NS_IPTC4XMP) == 0) {
	        if (strcmp (name, "Location") == 0) {
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Location", value);

			/* Added to the valid keywords */
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Keywords", value);
		}
		if (strcmp (name, "Sublocation") == 0) {
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Sublocation", value);

			/* Added to the valid keywords */
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Keywords", value);
		}
	}
	/* Photoshop scheme */
	else if (strcmp (schema,  NS_PHOTOSHOP) == 0) {
	        if (strcmp (name, "City") == 0) {
		        tracker_statement_list_insert (metadata, uri,
						  "Image:City", value);

			/* Added to the valid keywords */
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Keywords", value);
		}
		else if (strcmp (name, "Country") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:Country", value);

			/* Added to the valid keywords */
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Keywords", value);
		}
	}

	g_free (name);
}


/* Iterate over the XMP, dispatching to the appropriate element type
 * (simple, simple w/qualifiers, or an array) handler.
 */
void
tracker_xmp_iter (XmpPtr          xmp, 
		  XmpIteratorPtr  iter, 
		  const gchar    *uri,
		  GPtrArray      *metadata, 
		  gboolean        append)
{
	XmpStringPtr the_schema = xmp_string_new ();
	XmpStringPtr the_path = xmp_string_new ();
	XmpStringPtr the_prop = xmp_string_new ();
	uint32_t opt;

	while (xmp_iterator_next (iter, the_schema, the_path, the_prop, &opt)) {
		const gchar *schema = xmp_string_cstr (the_schema);
		const gchar *path = xmp_string_cstr (the_path);
		const gchar *value = xmp_string_cstr (the_prop);

		if (XMP_IS_PROP_SIMPLE (opt)) {
			if (strcmp (path,"") != 0) {
				if (XMP_HAS_PROP_QUALIFIERS (opt)) {
					tracker_xmp_iter_simple_qual (xmp, uri, metadata, schema, path, value, append);
				} else {
					tracker_xmp_iter_simple (uri, metadata, schema, path, value, append);
				}
			}
		}
		else if (XMP_IS_PROP_ARRAY (opt)) {
			if (XMP_IS_ARRAY_ALTTEXT (opt)) {
				tracker_xmp_iter_alt_text (xmp, uri, metadata, schema, path);
				xmp_iterator_skip (iter, XMP_ITER_SKIPSUBTREE);
			} else {
				tracker_xmp_iter_array (xmp, uri, metadata, schema, path);
				xmp_iterator_skip (iter, XMP_ITER_SKIPSUBTREE);
			}
		}
	}

	xmp_string_free (the_prop);
	xmp_string_free (the_path);
	xmp_string_free (the_schema);
}

#endif /* HAVE_EXEMPI */

void
tracker_read_xmp (const gchar *buffer, 
		  size_t       len, 
		  const gchar *uri,
		  GPtrArray   *metadata)
{
#ifdef HAVE_EXEMPI
	XmpPtr xmp;

	xmp_init ();

	xmp = xmp_new_empty ();
	xmp_parse (xmp, buffer, len);

	if (xmp != NULL) {
		XmpIteratorPtr iter;

		iter = xmp_iterator_new (xmp, NULL, NULL, XMP_ITER_PROPERTIES);
		tracker_xmp_iter (xmp, iter, uri, metadata, FALSE);
		xmp_iterator_free (iter);
		xmp_free (xmp);
	}

	xmp_terminate ();
#endif
}
