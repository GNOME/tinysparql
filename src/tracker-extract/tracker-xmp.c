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

#include "tracker-main.h"
#include "tracker-xmp.h"

#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-ontology.h>

#define NMM_PREFIX TRACKER_NMM_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX
#define RDF_PREFIX TRACKER_RDF_PREFIX

#ifdef HAVE_EXEMPI

#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>

static void tracker_xmp_iter        (XmpPtr                xmp,
				     XmpIteratorPtr        iter,
				     const gchar          *uri,
				     TrackerXmpData       *data,
				     gboolean              append);
static void tracker_xmp_iter_simple (const gchar          *uri,
				     TrackerXmpData       *data,
				     const gchar          *schema,
				     const gchar          *path,
				     const gchar          *value,
				     gboolean              append);

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

/* We have an array, now recursively iterate over it's children.  Set
 * 'append' to true so that all values of the array are added under
 * one entry. 
 */
static void 
tracker_xmp_iter_array (XmpPtr                xmp,
			const gchar          *uri,
			TrackerXmpData       *data, 
			const gchar          *schema, 
			const gchar          *path)
{
	XmpIteratorPtr iter;

	iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN);
	tracker_xmp_iter (xmp, iter, uri, data, TRUE);
	xmp_iterator_free (iter);
}


/* We have an array, now recursively iterate over it's children.  Set 'append' to false so that only one item is used. */
static void
tracker_xmp_iter_alt_text (XmpPtr                xmp, 
			   const gchar          *uri,
			   TrackerXmpData       *data, 
			   const gchar          *schema, 
			   const gchar          *path)
{
	XmpIteratorPtr iter;

	iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN);
	tracker_xmp_iter (xmp, iter, uri, data, FALSE);
	xmp_iterator_free (iter);
}

/* We have a simple element, but need to iterate over the qualifiers */
static void
tracker_xmp_iter_simple_qual (XmpPtr                xmp, 
			      const gchar          *uri,
			      TrackerXmpData       *data,
			      const gchar          *schema, 
			      const gchar          *path, 
			      const gchar          *value, 
			      gboolean              append)
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
		tracker_xmp_iter_simple (uri, data, schema, path, value, append);
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


/* We have a simple element. Add any data we know about to the
 * hash table.
 */
static void
tracker_xmp_iter_simple (const gchar          *uri,
			 TrackerXmpData       *data,
			 const gchar          *schema, 
			 const gchar          *path, 
			 const gchar          *value, 
			 gboolean              append)
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
		if (strcmp (name, "title") == 0 && !data->title) {
			data->title = g_strdup (value);
		}
		else if (strcmp (name, "rights") == 0 && !data->rights) {
			data->rights = g_strdup (value);
		}
		else if (strcmp (name, "creator") == 0 && !data->creator) {
			data->creator = g_strdup (value);
		}
		else if (strcmp (name, "description") == 0 && !data->description) {
			data->description = g_strdup (value);
		}
		else if (strcmp (name, "date") == 0 && !data->date) {
			data->date = g_strdup (value);
		}
		else if (strcmp (name, "keywords") == 0 && !data->keywords) {
			 data->keywords = g_strdup (value);
		}
		else if (strcmp (name, "subject") == 0 && !data->subject) {
			data->subject = g_strdup (value);
		}
		else if (strcmp (name, "publisher") == 0 && !data->publisher) {
			data->publisher = g_strdup (value);
		}
		else if (strcmp (name, "contributor") == 0 && !data->contributor) {
			data->contributor = g_strdup (value);
		}
		else if (strcmp (name, "type") == 0 && !data->type) {
			data->type = g_strdup (value);
		}
		else if (strcmp (name, "format") == 0 && !data->format) {
			data->format = g_strdup (value);
		}
		else if (strcmp (name, "identifier") == 0 && !data->identifier) {
			data->identifier = g_strdup (value);
		}
		else if (strcmp (name, "source") == 0 && !data->source) {
			data->source = g_strdup (value);
		}
		else if (strcmp (name, "language") == 0 && !data->language) {
			data->language = g_strdup (value);
		}
		else if (strcmp (name, "relation") == 0 && !data->relation) {
			data->relation = g_strdup (value);
		}
		else if (strcmp (name, "coverage") == 0 && !data->coverage) {
			data->coverage = g_strdup (value);
		}

	}
	/* Creative Commons */
	else if (strcmp (schema, NS_CC) == 0) {
		if (strcmp (name, "license") == 0 && !data->license) {
			data->license = g_strdup (value);
		}
	}
	/* Exif basic scheme */
	else if (strcmp (schema, NS_EXIF) == 0) {
		if (strcmp (name, "Title") == 0 && !data->Title) {
			data->Title = g_strdup (value);
		}
		else if (strcmp (name, "DateTimeOriginal") == 0 && !data->DateTimeOriginal) {
			data->DateTimeOriginal = g_strdup (value);
		}
		else if (strcmp (name, "Artist") == 0 && !data->Artist) {
			data->Artist = g_strdup (value);
		}
/*		else if (strcmp (name, "Software") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:Software", value);
		}*/
		else if (strcmp (name, "Make") == 0 && !data->Make) {
			data->Make = g_strdup (value);
		}
		else if (strcmp (name, "Model") == 0 && !data->Model) {
			data->Model = g_strdup (value);
		}
		else if (strcmp (name, "Orientation") == 0 && !data->Orientation) {
			data->Orientation = g_strdup (fix_orientation (value));
		}
		else if (strcmp (name, "Flash") == 0 && !data->Flash) {
			data->Flash = g_strdup (fix_flash (value));
		}
		else if (strcmp (name, "MeteringMode") == 0 && !data->MeteringMode) {
			data->MeteringMode = g_strdup (fix_metering_mode (value));
		}
		/*else if (strcmp (name, "ExposureProgram") == 0) {
			tracker_statement_list_insert (metadata, uri,
						  "Image:ExposureProgram", value);
		}*/
		else if (strcmp (name, "ExposureTime") == 0 && !data->ExposureTime) {
			data->ExposureTime = g_strdup (value);
		}
		else if (strcmp (name, "FNumber") == 0 && !data->FNumber) {
			data->FNumber = g_strdup (value);
		}
		else if (strcmp (name, "FocalLength") == 0 && !data->FocalLength) {
			data->FocalLength = g_strdup (value);
		}
		else if (strcmp (name, "ISOSpeedRatings") == 0 && !data->ISOSpeedRatings) {
			data->ISOSpeedRatings = g_strdup (value);
		}
		else if (strcmp (name, "WhiteBalance") == 0 && !data->WhiteBalance) {
			data->WhiteBalance = g_strdup (fix_white_balance (value));
		}
		else if (strcmp (name, "Copyright") == 0 && !data->Copyright) {
			data->Copyright = g_strdup (value);
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
tracker_xmp_iter (XmpPtr                xmp, 
		  XmpIteratorPtr        iter, 
		  const gchar          *uri,
		  TrackerXmpData       *data, 
		  gboolean              append)
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
					tracker_xmp_iter_simple_qual (xmp, uri, data, schema, path, value, append);
				} else {
					tracker_xmp_iter_simple (uri, data, schema, path, value, append);
				}
			}
		}
		else if (XMP_IS_PROP_ARRAY (opt)) {
			if (XMP_IS_ARRAY_ALTTEXT (opt)) {
				tracker_xmp_iter_alt_text (xmp, uri, data, schema, path);
				xmp_iterator_skip (iter, XMP_ITER_SKIPSUBTREE);
			} else {
				tracker_xmp_iter_array (xmp, uri, data, schema, path);
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
tracker_read_xmp (const gchar          *buffer, 
		  size_t                len, 
		  const gchar          *uri,
		  TrackerXmpData       *data)
{
#ifdef HAVE_EXEMPI
	XmpPtr xmp;

	xmp_init ();

	xmp = xmp_new_empty ();
	xmp_parse (xmp, buffer, len);

	if (xmp != NULL) {
		XmpIteratorPtr iter;

		iter = xmp_iterator_new (xmp, NULL, NULL, XMP_ITER_PROPERTIES);
		tracker_xmp_iter (xmp, iter, uri, data, FALSE);
		xmp_iterator_free (iter);
		xmp_free (xmp);
	}

	xmp_terminate ();
#endif
}


static void
insert_keywords (TrackerSparqlBuilder *metadata, const gchar *uri, gchar *keywords)
{
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
		tracker_statement_list_insert (metadata, uri, 
		                               NIE_PREFIX "keyword", 
		                               (const gchar*) keyw);
	}
}

void
tracker_apply_xmp (TrackerSparqlBuilder *metadata, const gchar *uri, TrackerXmpData *xmp_data)
{
	if (xmp_data->keywords) {
		insert_keywords (metadata, uri, xmp_data->keywords);
		g_free (xmp_data->keywords);
	}

	if (xmp_data->subject) {
		insert_keywords (metadata, uri, xmp_data->subject);
		g_free (xmp_data->subject);
	}

	if (xmp_data->publisher) {
		tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
		tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", xmp_data->publisher);
		tracker_statement_list_insert (metadata, uri, NCO_PREFIX "publisher", ":");
		g_free (xmp_data->publisher);
	}

	if (xmp_data->type) {
		tracker_statement_list_insert (metadata, uri, DC_PREFIX "type", xmp_data->type);
		g_free (xmp_data->type);
	}

	if (xmp_data->format) {
		tracker_statement_list_insert (metadata, uri, DC_PREFIX "format", xmp_data->format);
		g_free (xmp_data->format);
	}

	if (xmp_data->identifier) {
		tracker_statement_list_insert (metadata, uri, DC_PREFIX "identifier", xmp_data->identifier);
		g_free (xmp_data->identifier);
	}

	if (xmp_data->source) {
		tracker_statement_list_insert (metadata, uri, DC_PREFIX "source", xmp_data->source);
		g_free (xmp_data->source);
	}

	if (xmp_data->language) {
		tracker_statement_list_insert (metadata, uri, DC_PREFIX "language", xmp_data->language);
		g_free (xmp_data->language);
	}

	if (xmp_data->relation) {
		tracker_statement_list_insert (metadata, uri, DC_PREFIX "relation", xmp_data->relation);
		g_free (xmp_data->relation);
	}

	if (xmp_data->coverage) {
		tracker_statement_list_insert (metadata, uri, DC_PREFIX "coverage", xmp_data->coverage);
		g_free (xmp_data->coverage);
	}

	if (xmp_data->license) {
		tracker_statement_list_insert (metadata, uri, NIE_PREFIX "license", xmp_data->license);
		g_free (xmp_data->license);
	}

	if (xmp_data->Make || xmp_data->Model) {
		gchar *final_camera = tracker_coalesce (2, xmp_data->Make, xmp_data->Model); 
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "camera", final_camera);
		g_free (final_camera);
	}

	if (xmp_data->title) {
		tracker_statement_list_insert (metadata, uri, NIE_PREFIX "title", xmp_data->title);
		g_free (xmp_data->title);
	}

	if (xmp_data->Orientation) {
		tracker_statement_list_insert (metadata, uri, NFO_PREFIX "orientation", xmp_data->Orientation);
		g_free (xmp_data->Orientation);
	}

	if (xmp_data->rights) {
		tracker_statement_list_insert (metadata, uri, NIE_PREFIX "copyright", xmp_data->rights);
		g_free (xmp_data->rights);
	}

	if (xmp_data->WhiteBalance) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "whiteBalance", xmp_data->WhiteBalance);
		g_free (xmp_data->WhiteBalance);
	}

	if (xmp_data->FNumber) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "fnumber", xmp_data->FNumber);
		g_free (xmp_data->FNumber);
	}

	if (xmp_data->Flash) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "flash", xmp_data->Flash);
		g_free (xmp_data->Flash);
	}

	if (xmp_data->FocalLength) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "focalLength", xmp_data->FocalLength);
		g_free (xmp_data->FocalLength);
	}

	if (xmp_data->Artist || xmp_data->contributor) {
		gchar *final_artist =  tracker_coalesce (2, xmp_data->Artist, xmp_data->contributor);
		tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
		tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", final_artist);
		tracker_statement_list_insert (metadata, uri, NCO_PREFIX "contributor", ":");
		g_free (final_artist);
	}

	if (xmp_data->ExposureTime) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "exposureTime", xmp_data->ExposureTime);
		g_free (xmp_data->ExposureTime);
	}

	if (xmp_data->ISOSpeedRatings) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "isoSpeed", xmp_data->ISOSpeedRatings);
		g_free (xmp_data->ISOSpeedRatings);
	}

	if (xmp_data->date || xmp_data->DateTimeOriginal) {
		gchar *final_date =  tracker_coalesce (2, xmp_data->date, xmp_data->DateTimeOriginal);
		tracker_statement_list_insert (metadata, uri, NIE_PREFIX "contentCreated", final_date);
		g_free (final_date);
	}

	if (xmp_data->description) {
		tracker_statement_list_insert (metadata, uri, NIE_PREFIX "description", xmp_data->description);
		g_free (xmp_data->description);
	}

	if (xmp_data->MeteringMode) {
		tracker_statement_list_insert (metadata, uri, NMM_PREFIX "meteringMode", xmp_data->MeteringMode);
		g_free (xmp_data->MeteringMode);
	}

	if (xmp_data->creator) {
		tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
		tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", xmp_data->creator);
		tracker_statement_list_insert (metadata, uri, NCO_PREFIX "creator", ":");
		g_free (xmp_data->creator);
	}
}
