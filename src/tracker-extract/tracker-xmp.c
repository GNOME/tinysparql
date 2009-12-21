/*
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

		if (g_ascii_strcasecmp (qual_path, "xml:lang") == 0) {
			/* Is this a language we should ignore? */
			if (g_ascii_strcasecmp (qual_value, "x-default") != 0 &&
			    g_ascii_strcasecmp (qual_value, "x-repair") != 0 &&
			    g_ascii_strcasecmp (qual_value, locale) != 0) {
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
	if (orientation && g_ascii_strcasecmp (orientation, "top - left") == 0) {
		return "nfo:orientation-top";
	} else
	if (orientation && g_ascii_strcasecmp (orientation, "top - right") == 0) {
		return  "nfo:orientation-top-mirror";
	} else
	if (orientation && g_ascii_strcasecmp (orientation, "bottom - right") == 0) {
		return "nfo:orientation-bottom-mirror";
	} else
	if (orientation && g_ascii_strcasecmp (orientation, "bottom - left") == 0) {
		return  "nfo:orientation-bottom";
	} else
	if (orientation && g_ascii_strcasecmp (orientation, "left - top") == 0) {
		return  "nfo:orientation-left-mirror";
	} else
	if (orientation && g_ascii_strcasecmp (orientation, "right - top") == 0) {
		return  "nfo:orientation-right";
	} else
	if (orientation && g_ascii_strcasecmp (orientation, "right - bottom") == 0) {
		return "nfo:orientation-right-mirror";
	} else
	if (orientation && g_ascii_strcasecmp (orientation, "left - bottom") == 0) {
		return  "nfo:orientation-left";
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

	/* Exif basic scheme */
	if (g_ascii_strcasecmp (schema, NS_EXIF) == 0) {
		if (!data->Title && g_ascii_strcasecmp (name, "Title") == 0) {
			data->Title = g_strdup (value);
		} else
		if (g_ascii_strcasecmp (name, "DateTimeOriginal") == 0 && !data->DateTimeOriginal) {
			data->DateTimeOriginal = g_strdup (value);
		} else
		if (!data->Artist && g_ascii_strcasecmp (name, "Artist") == 0) {
			data->Artist = g_strdup (value);
		} else /*
		if (g_ascii_strcasecmp (name, "Software") == 0) {
			tracker_statement_list_insert (metadata, uri,
			                               "Image:Software", value);
		} else */
		if (!data->Make && g_ascii_strcasecmp (name, "Make") == 0) {
			data->Make = g_strdup (value);
		} else
		if (!data->Model && g_ascii_strcasecmp (name, "Model") == 0) {
			data->Model = g_strdup (value);
		} else
		if (!data->Orientation && g_ascii_strcasecmp (name, "Orientation") == 0) {
			data->Orientation = g_strdup (fix_orientation (value));
		} else
		if (!data->Flash && g_ascii_strcasecmp (name, "Flash") == 0) {
			data->Flash = g_strdup (fix_flash (value));
		} else
		if (!data->MeteringMode && g_ascii_strcasecmp (name, "MeteringMode") == 0) {
			data->MeteringMode = g_strdup (fix_metering_mode (value));
		} else /*
		if (g_ascii_strcasecmp (name, "ExposureProgram") == 0) {
			tracker_statement_list_insert (metadata, uri,
			                              "Image:ExposureProgram", value);
		} else*/
		if (!data->ExposureTime && g_ascii_strcasecmp (name, "ExposureTime") == 0) {
			data->ExposureTime = g_strdup (value);
		} else
		if (!data->FNumber && g_ascii_strcasecmp (name, "FNumber") == 0) {
			data->FNumber = g_strdup (value);
		} else
		if (!data->FocalLength && g_ascii_strcasecmp (name, "FocalLength") == 0) {
			data->FocalLength = g_strdup (value);
		} else
		if (!data->ISOSpeedRatings && g_ascii_strcasecmp (name, "ISOSpeedRatings") == 0) {
			data->ISOSpeedRatings = g_strdup (value);
		} else
		if (!data->WhiteBalance && g_ascii_strcasecmp (name, "WhiteBalance") == 0) {
			data->WhiteBalance = g_strdup (fix_white_balance (value));
		} else
		if (!data->Copyright && g_ascii_strcasecmp (name, "Copyright") == 0) {
			data->Copyright = g_strdup (value);
		}
	} else
	/* PDF*/ 
	if (g_ascii_strcasecmp (schema, NS_PDF) == 0) {
		if (!data->PDFkeywords && g_ascii_strcasecmp (name, "keywords") == 0) {
			data->PDFkeywords = g_strdup (value);
		} else
		if (!data->PDFtitle && g_ascii_strcasecmp (name, "title") == 0) {
			data->PDFtitle = g_strdup (value);
		}
	} else
	/* Dublin Core */
	if (g_ascii_strcasecmp (schema, NS_DC) == 0) {
		if (!data->title && g_ascii_strcasecmp (name, "title") == 0) {
			data->title = g_strdup (value);
		} else
		if (!data->rights && g_ascii_strcasecmp (name, "rights") == 0) {
			data->rights = g_strdup (value);
		} else
		if (!data->creator && g_ascii_strcasecmp (name, "creator") == 0) {
			data->creator = g_strdup (value);
		} else
		if (!data->description && g_ascii_strcasecmp (name, "description") == 0) {
			data->description = g_strdup (value);
		} else
		if (!data->date && g_ascii_strcasecmp (name, "date") == 0) {
			data->date = g_strdup (value);
		} else
		if (!data->keywords && g_ascii_strcasecmp (name, "keywords") == 0) {
			data->keywords = g_strdup (value);
		} else
		if (!data->subject && g_ascii_strcasecmp (name, "subject") == 0) {
			data->subject = g_strdup (value);
		} else
		if (!data->publisher && g_ascii_strcasecmp (name, "publisher") == 0) {
			data->publisher = g_strdup (value);
		} else
		if (!data->contributor && g_ascii_strcasecmp (name, "contributor") == 0) {
			data->contributor = g_strdup (value);
		} else
		if (!data->type && g_ascii_strcasecmp (name, "type") == 0) {
			data->type = g_strdup (value);
		} else
		if (!data->format && g_ascii_strcasecmp (name, "format") == 0) {
			data->format = g_strdup (value);
		} else
		if (!data->identifier && g_ascii_strcasecmp (name, "identifier") == 0) {
			data->identifier = g_strdup (value);
		} else
		if (!data->source && g_ascii_strcasecmp (name, "source") == 0) {
			data->source = g_strdup (value);
		} else
		if (!data->language && g_ascii_strcasecmp (name, "language") == 0) {
			data->language = g_strdup (value);
		} else
		if (!data->relation && g_ascii_strcasecmp (name, "relation") == 0) {
			data->relation = g_strdup (value);
		} else
		if (!data->coverage && g_ascii_strcasecmp (name, "coverage") == 0) {
			data->coverage = g_strdup (value);
		}
	} else
	/* Creative Commons */
	if (g_ascii_strcasecmp (schema, NS_CC) == 0) {
		if (!data->license && g_ascii_strcasecmp (name, "license") == 0) {
			data->license = g_strdup (value);
		}
	} else
	/* TODO: A lot of these location fields are pretty vague and ambigious.
	 * We should go through them one by one and ensure that all of them are
	 * used sanely */

	/* Photoshop TODO: is this needed anyway? */
	if (g_ascii_strcasecmp (schema,  NS_PHOTOSHOP) == 0) {
		if (data->City && g_ascii_strcasecmp (name, "City") == 0) {
			data->City = g_strdup (value);
		} else
		if (!data->Country && g_ascii_strcasecmp (name, "Country") == 0) {
			data->Country = g_strdup (value);
		} else
		if (!data->State && g_ascii_strcasecmp (name, "State") == 0) {
			data->State = g_strdup (value);
		} else
		if (!data->Address && g_ascii_strcasecmp (name, "Location") == 0) {
			data->Address = g_strdup (value);
		}
	} else
	/* IPTC4XMP scheme - GeoClue / location stuff, TODO */
	if (g_ascii_strcasecmp (schema,  NS_IPTC4XMP) == 0) {
		if (!data->City && g_ascii_strcasecmp (name, "City") == 0) {
			data->City = g_strdup (value);
		} else 
		if (!data->Country && g_ascii_strcasecmp (name, "Country") == 0) {
			data->Country = g_strdup (value);
		} else 
		if (!data->Country && g_ascii_strcasecmp (name, "PrimaryLocationName") == 0) {
			data->Country = g_strdup (value);
		} else
		if (!data->State && g_ascii_strcasecmp (name, "State") == 0) {
			data->State = g_strdup (value);
		} else 
		if (!data->State && g_ascii_strcasecmp (name, "Province") == 0) {
			data->State = g_strdup (value);
		} else 
		if (!data->Address && g_ascii_strcasecmp (name, "Sublocation") == 0) {
			data->Address = g_strdup (value);
		}
	} 
	 /* else
	* XAP (XMP)scheme *
	if (g_ascii_strcasecmp (schema, NS_XAP) == 0) {
		if (g_ascii_strcasecmp (name, "Rating") == 0) {
			tracker_statement_list_insert (metadata, uri,
			                               "Image:Rating", value);
		}
		if (g_ascii_strcasecmp (name, "MetadataDate") == 0) {
			tracker_statement_list_insert (metadata, uri,
			                              "Image:Date", value);
		}
	} else
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
		tracker_sparql_builder_predicate (metadata, "nao:hasTag");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nao:Tag");

		tracker_sparql_builder_predicate (metadata, "nao:prefLabel");
		tracker_sparql_builder_object_unvalidated (metadata, keyw);

		tracker_sparql_builder_object_blank_close (metadata);
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

	if (xmp_data->PDFkeywords) {
		insert_keywords (metadata, uri, xmp_data->PDFkeywords);
		g_free (xmp_data->PDFkeywords);
	}

	if (xmp_data->publisher) {
		tracker_sparql_builder_predicate (metadata, "nco:publisher");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");

		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->publisher);
		tracker_sparql_builder_object_blank_close (metadata);
		g_free (xmp_data->publisher);
	}

	if (xmp_data->type) {
		tracker_sparql_builder_predicate (metadata, "dc:type");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->type);
		g_free (xmp_data->type);
	}

	if (xmp_data->format) {
		tracker_sparql_builder_predicate (metadata, "dc:format");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->format);
		g_free (xmp_data->format);
	}

	if (xmp_data->identifier) {
		tracker_sparql_builder_predicate (metadata, "dc:identifier");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->identifier);
		g_free (xmp_data->identifier);
	}

	if (xmp_data->source) {
		tracker_sparql_builder_predicate (metadata, "dc:source");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->source);
		g_free (xmp_data->source);
	}

	if (xmp_data->language) {
		tracker_sparql_builder_predicate (metadata, "dc:language");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->language);
		g_free (xmp_data->language);
	}

	if (xmp_data->relation) {
		tracker_sparql_builder_predicate (metadata, "dc:relation");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->relation);
		g_free (xmp_data->relation);
	}

	if (xmp_data->coverage) {
		tracker_sparql_builder_predicate (metadata, "dc:coverage");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->coverage);
		g_free (xmp_data->coverage);
	}

	if (xmp_data->license) {
		tracker_sparql_builder_predicate (metadata, "dc:license");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->license);
		g_free (xmp_data->license);
	}

	if (xmp_data->Make || xmp_data->Model) {
		gchar *final_camera = tracker_merge (" ", 2, xmp_data->Make, xmp_data->Model);

		tracker_sparql_builder_predicate (metadata, "nmm:camera");
		tracker_sparql_builder_object_unvalidated (metadata, final_camera);
		g_free (final_camera);
	}

	if (xmp_data->title || xmp_data->Title || xmp_data->PDFtitle) {
		gchar *final_title = tracker_coalesce (3, xmp_data->title, 
		                                       xmp_data->Title, 
		                                       xmp_data->PDFtitle);
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, final_title);
		g_free (final_title);
	}

	if (xmp_data->Orientation) {
		tracker_sparql_builder_predicate (metadata, "nfo:orientation");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->Orientation);
		g_free (xmp_data->Orientation);
	}
	
	if (xmp_data->rights || xmp_data->Copyright) {
		gchar *final_rights = tracker_coalesce (2, xmp_data->Copyright, xmp_data->rights);
		tracker_sparql_builder_predicate (metadata, "nie:copyright");
		tracker_sparql_builder_object_unvalidated (metadata, final_rights);
		g_free (final_rights);
	}

	if (xmp_data->WhiteBalance) {
		tracker_sparql_builder_predicate (metadata, "nmm:whiteBalance");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->WhiteBalance);
		g_free (xmp_data->WhiteBalance);
	}

	if (xmp_data->FNumber) {
		tracker_sparql_builder_predicate (metadata, "nmm:fnumber");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->FNumber);
		g_free (xmp_data->FNumber);
	}

	if (xmp_data->Flash) {
		tracker_sparql_builder_predicate (metadata, "nmm:flash");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->Flash);
		g_free (xmp_data->Flash);
	}

	if (xmp_data->FocalLength) {
		tracker_sparql_builder_predicate (metadata, "nmm:focalLength");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->FocalLength);
		g_free (xmp_data->FocalLength);
	}

	if (xmp_data->Artist || xmp_data->contributor) {
		gchar *final_artist = tracker_coalesce (2, xmp_data->Artist, xmp_data->contributor);

		tracker_sparql_builder_predicate (metadata, "nco:contributor");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");

		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, final_artist);
		tracker_sparql_builder_object_blank_close (metadata);
		g_free (final_artist);
	}

	if (xmp_data->ExposureTime) {
		tracker_sparql_builder_predicate (metadata, "nmm:exposureTime");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->ExposureTime);
		g_free (xmp_data->ExposureTime);
	}

	if (xmp_data->ISOSpeedRatings) {
		tracker_sparql_builder_predicate (metadata, "nmm:isoSpeed");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->ISOSpeedRatings);
		g_free (xmp_data->ISOSpeedRatings);
	}

	if (xmp_data->date || xmp_data->DateTimeOriginal) {
		gchar *final_date =  tracker_coalesce (2, xmp_data->date, xmp_data->DateTimeOriginal);
		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, final_date);
		g_free (final_date);
	}

	if (xmp_data->description) {
		tracker_sparql_builder_predicate (metadata, "nie:description");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->description);
		g_free (xmp_data->description);
	}

	if (xmp_data->MeteringMode) {
		tracker_sparql_builder_predicate (metadata, "nmm:meteringMode");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->MeteringMode);
		g_free (xmp_data->MeteringMode);
	}

	if (xmp_data->creator) {
		tracker_sparql_builder_predicate (metadata, "nco:creator");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");

		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, xmp_data->creator);
		tracker_sparql_builder_object_blank_close (metadata);
		g_free (xmp_data->creator);
	}

	if (xmp_data->Address || xmp_data->Country || xmp_data->City) {
		tracker_sparql_builder_predicate (metadata, "mlo:location");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "mlo:GeoPoint");

		if (xmp_data->Address) {
			tracker_sparql_builder_predicate (metadata, "mlo:address");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data->Address);
			g_free (xmp_data->Address);
		}

		if (xmp_data->State) {
			tracker_sparql_builder_predicate (metadata, "mlo:state");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data->State);
			g_free (xmp_data->State);
		}

		if (xmp_data->City) {
			tracker_sparql_builder_predicate (metadata, "mlo:city");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data->City);
			g_free (xmp_data->City);
		}

		if (xmp_data->Country) {
			tracker_sparql_builder_predicate (metadata, "mlo:country");
			tracker_sparql_builder_object_unvalidated (metadata, xmp_data->Country);
			g_free (xmp_data->Country);
		}
	
		tracker_sparql_builder_object_blank_close (metadata);
	}
	
}
