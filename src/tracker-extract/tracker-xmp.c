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
#define NMM_PREFIX TRACKER_NMM_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX

#include <libtracker-common/tracker-utils.h>

#ifdef HAVE_EXEMPI

#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>

static const gchar *
fix_metering_mode (const gchar *mode)
{
	gint value;
	value = atoi(mode);

	switch (value) {
	default:
	case 0:
		return "nmm:meteringMode-other";
	case 1:
		return "nmm:meteringMode-average";
	case 2:
		return "nmm:meteringMode-center-weighted-average";
	case 3:
		return "nmm:meteringMode-spot";
	case 4:
		return "nmm:meteringMode-multispot";
	case 5:
		return "nmm:meteringMode-pattern";
	case 6:
		return "nmm:meteringMode-partial";
	}

	return "nmm:meteringMode-other";
}

static const gchar *
fix_flash (const gchar *flash)
{
	static const gint fired_mask = 0x1;
	gint value;

	value = atoi (flash);

	if (value & fired_mask) {
		return "nmm:flash-on";
	} else {
		return "nmm:flash-off";
	}
}

static const gchar *
fix_white_balance (const gchar *wb)
{
	if (wb) {
		return "nmm:whiteBalance-manual";
	} else {
		return "nmm:whiteBalance-auto";
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


static const gchar *
fix_orientation (const gchar *orientation)
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

	for (i=0; i < 8; i++) {
		if (g_strcmp0 (orientation,ostr[i]) == 0) {
			switch (i) {
				default:
				case 0:
				return  "nfo:orientation-top";
				case 1:
				return  "nfo:orientation-top-mirror"; // not sure
				case 2:
				return  "nfo:orientation-bottom-mirror"; // not sure
				case 3:
				return  "nfo:orientation-bottom";
				case 4:
				return  "nfo:orientation-left-mirror";
				case 5:
				return  "nfo:orientation-right";
				case 6:
				return  "nfo:orientation-right-mirror";
				case 7:
				return  "nfo:orientation-left";
			}
		}
	}

	return  "nfo:orientation-top";
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
	const gchar *index_;

	name = g_strdup (strchr (path, ':') + 1);
	index_ = strrchr (name, '[');

	if (index_) {
		name[index_ - name] = '\0';
	}

	/* Dublin Core */
	if (strcmp (schema, NS_DC) == 0) {
		if (strcmp (name, "title") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  NIE_PREFIX "title", value);
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
						  NIE_PREFIX "description", value);
		}
		else if (strcmp (name, "date") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  NIE_PREFIX "contentCreated", value);
		}
		else if (strcmp (name, "keywords") == 0) {
			gchar *keywords = g_strdup (value);
			char *lasts, *keyw;
			size_t len;

			keyw = keywords;
			keywords = strchr (keywords, '"');
			if (keywords)
				keywords++;
			else 
				keywords = keyw;

			len = strlen (keywords);
			if (keywords[len - 1] == '"')
				keywords[len - 1] = '\0';

			for (keyw = strtok_r (keywords, ",; ", &lasts); keyw; 
			     keyw = strtok_r (NULL, ",; ", &lasts)) {
				tracker_statement_list_insert (metadata,
						  uri, NIE_PREFIX "keyword",
						  (const gchar*) keyw);
			}

			g_free (keyw);
		}
		else if (strcmp (name, "subject") == 0) {
			gchar *keywords = g_strdup (value);
			char *lasts, *keyw;
			size_t len;

			tracker_statement_list_insert (metadata, uri, 
						  NIE_PREFIX "subject", value);

			/* The subject field may contain keywords as well */

			keyw = keywords;
			keywords = strchr (keywords, '"');
			if (keywords)
				keywords++;
			else 
				keywords = keyw;

			len = strlen (keywords);
			if (keywords[len - 1] == '"')
				keywords[len - 1] = '\0';

			for (keyw = strtok_r (keywords, ",; ", &lasts); keyw; 
			     keyw = strtok_r (NULL, ",; ", &lasts)) {
				tracker_statement_list_insert (metadata,
						  uri, NIE_PREFIX "keyword",
						  (const gchar*) keyw);
			}

			g_free (keyw);

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
						  NFO_PREFIX "title", value);
		}
		else if (strcmp (name, "DateTimeOriginal") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  NIE_PREFIX "contentCreated", value);
		}
		else if (strcmp (name, "Artist") == 0) {
			tracker_statement_list_insert (metadata, ":", RDF_TYPE, NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", value);
			/* contributor is OK here? */
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "contributor", ":");
		}
/*		else if (strcmp (name, "Software") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:Software", value);
		}*/
		else if (strcmp (name, "Make") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  NMM_PREFIX "camera", value);
		}
		else if (strcmp (name, "Model") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  NMM_PREFIX "camera", value);
		}
		else if (strcmp (name, "Orientation") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  NFO_PREFIX "orientation", 
						  fix_orientation (value));
		}
		else if (strcmp (name, "Flash") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  NMM_PREFIX "flash", 
						  fix_flash (value));
		}
		else if (strcmp (name, "MeteringMode") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  NMM_PREFIX "meteringMode", 
						  fix_metering_mode (value));
		}
		/*else if (strcmp (name, "ExposureProgram") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:ExposureProgram", value);
		}*/
		else if (strcmp (name, "ExposureTime") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  NMM_PREFIX "exposureTime", value);
		}
		else if (strcmp (name, "FNumber") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  NMM_PREFIX "fnumber", value);
		}
		else if (strcmp (name, "FocalLength") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  NMM_PREFIX "focalLength", value);
		}
		else if (strcmp (name, "ISOSpeedRatings") == 0) {
			tracker_statement_list_insert (metadata, uri, 
						  NMM_PREFIX "isoSpeed", value);
		}
		else if (strcmp (name, "WhiteBalance") == 0) {
			tracker_statement_list_insert (metadata, uri,
						   NMM_PREFIX "whiteBalance",
						   fix_white_balance (value));
		}
		else if (strcmp (name, "Copyright") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  NIE_PREFIX "copyright", value);
		}
	}
	/* XAP (XMP)scheme */
	/*else if (strcmp (schema, NS_XAP) == 0) {
	        if (strcmp (name, "Rating") == 0) {
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Rating", value);
		}
		if (strcmp (name, "MetadataDate") == 0) {
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Date", value);
		}
	}*/
	/* IPTC4XMP scheme */

	/*
	 GeoClue / location stuff, TODO
	 else if (strcmp (schema,  NS_IPTC4XMP) == 0) {
	        if (strcmp (name, "Location") == 0) {
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Location", value);

			/ Added to the valid keywords *
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Keywords", value);
		}
		if (strcmp (name, "Sublocation") == 0) {
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Sublocation", value);

			/ Added to the valid keywords *
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Keywords", value);
		}
	}
	/ Photoshop scheme *
	else if (strcmp (schema,  NS_PHOTOSHOP) == 0) {
	        if (strcmp (name, "City") == 0) {
		        tracker_statement_list_insert (metadata, uri,
						  "Image:City", value);

			/ Added to the valid keywords *
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Keywords", value);
		}
		else if (strcmp (name, "Country") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:Country", value);

			/ Added to the valid keywords *
		        tracker_statement_list_insert (metadata, uri,
						  "Image:Keywords", value);
		}
	}
	*/

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
			if (!tracker_is_empty_string (path)) {
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
