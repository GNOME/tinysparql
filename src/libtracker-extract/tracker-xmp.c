/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <locale.h>

#include <libtracker-common/tracker-utils.h>

#include "tracker-xmp.h"
#include "tracker-utils.h"

#ifdef HAVE_EXEMPI

#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>

/**
 * SECTION:tracker-xmp
 * @title: XMP
 * @short_description: Extensible Metadata Platform (XMP)
 * @stability: Stable
 * @include: libtracker-extract/tracker-extract.h
 *
 * The Adobe Extensible Metadata Platform (XMP) is a standard, created
 * by Adobe Systems Inc., for processing and storing standardized and
 * proprietary information relating to the contents of a file.
 *
 * XMP standardizes the definition, creation, and processing of
 * extensible metadata. Serialized XMP can be embedded into a
 * significant number of popular file formats, without breaking their
 * readability by non-XMP-aware applications. Embedding metadata ("the
 * truth is in the file") avoids many problems that occur when
 * metadata is stored separately. XMP is used in PDF, photography and
 * photo editing applications.
 *
 * This API is provided to remove code duplication between extractors
 * using these standards.
 **/

static void iterate        (XmpPtr                xmp,
                            XmpIteratorPtr        iter,
                            const gchar          *uri,
                            TrackerXmpData       *data,
                            gboolean              append);
static void iterate_simple (const gchar          *uri,
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
	case 0:
		return "nmm:metering-mode-other";
	case 1:
		return "nmm:metering-mode-average";
	case 2:
		return "nmm:metering-mode-center-weighted-average";
	case 3:
		return "nmm:metering-mode-spot";
	case 4:
		return "nmm:metering-mode-multispot";
	case 5:
		return "nmm:metering-mode-pattern";
	case 6:
		return "nmm:metering-mode-partial";
	}

	return "nmm:metering-mode-other";
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
	if (g_strcmp0 (wb, "1") == 0) {
		return "nmm:white-balance-manual";
	} else {
		return "nmm:white-balance-auto";
	}
}

/* We have an array, now recursively iterate over it's children.  Set
 * 'append' to true so that all values of the array are added under
 * one entry.
 */
static void
iterate_array (XmpPtr          xmp,
               const gchar    *uri,
               TrackerXmpData *data,
               const gchar    *schema,
               const gchar    *path)
{
	XmpIteratorPtr iter;

	iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN);
	iterate (xmp, iter, uri, data, TRUE);
	xmp_iterator_free (iter);
}

/* We have an array, now recursively iterate over it's children.  Set
 * 'append' to false so that only one item is used.
 */
static void
iterate_alt_text (XmpPtr          xmp,
                  const gchar    *uri,
                  TrackerXmpData *data,
                  const gchar    *schema,
                  const gchar    *path)
{
	XmpIteratorPtr iter;

	iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN);
	iterate (xmp, iter, uri, data, FALSE);
	xmp_iterator_free (iter);
}

static gchar *
div_str_dup (const gchar *value)
{
	gchar *ret;
	gchar *ptr = strchr (value, '/');
	if (ptr) {
		gchar *cpy = g_strdup (value);
		gint a, b;
		cpy [ptr - value] = '\0';
		a = atoi (cpy);
		b = atoi (cpy + (ptr - value) + 1);
		if (b != 0)
			ret = g_strdup_printf ("%G", ((gdouble)((gdouble) a / (gdouble) b)));
		else
			ret = NULL;
		g_free (cpy);
	} else {
		ret = g_strdup (value);
	}

	return ret;
}

/* We have a simple element, but need to iterate over the qualifiers */
static void
iterate_simple_qual (XmpPtr          xmp,
                     const gchar    *uri,
                     TrackerXmpData *data,
                     const gchar    *schema,
                     const gchar    *path,
                     const gchar    *value,
                     gboolean        append)
{
	XmpIteratorPtr iter;
	XmpStringPtr the_path;
	XmpStringPtr the_prop;
	static gchar *locale = NULL;
	gboolean ignore_element = FALSE;

	iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN | XMP_ITER_JUSTLEAFNAME);

	the_path = xmp_string_new ();
	the_prop = xmp_string_new ();

	if (G_UNLIKELY (!locale)) {
		locale = g_strdup (setlocale (LC_ALL, NULL));

		if (!locale) {
			locale = g_strdup ("C");
		} else {
			gchar *sep;

			sep = strchr (locale, '.');

			if (sep) {
				locale[sep - locale] = '\0';
			}

			sep = strchr (locale, '_');

			if (sep) {
				locale[sep - locale] = '-';
			}
		}
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
		iterate_simple (uri, data, schema, path, value, append);
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
	} else if (orientation && g_ascii_strcasecmp (orientation, "top - right") == 0) {
			return  "nfo:orientation-top-mirror";
	} else if (orientation && g_ascii_strcasecmp (orientation, "bottom - right") == 0) {
		return "nfo:orientation-bottom-mirror";
	} else if (orientation && g_ascii_strcasecmp (orientation, "bottom - left") == 0) {
		return  "nfo:orientation-bottom";
	} else if (orientation && g_ascii_strcasecmp (orientation, "left - top") == 0) {
		return  "nfo:orientation-left-mirror";
	} else if (orientation && g_ascii_strcasecmp (orientation, "right - top") == 0) {
		return  "nfo:orientation-right";
	} else if (orientation && g_ascii_strcasecmp (orientation, "right - bottom") == 0) {
		return "nfo:orientation-right-mirror";
	} else if (orientation && g_ascii_strcasecmp (orientation, "left - bottom") == 0) {
		return  "nfo:orientation-left";
	}

	return  "nfo:orientation-top";
}

/* We have a simple element. Add any data we know about to the
 * hash table.
 */
static void
iterate_simple (const gchar    *uri,
                TrackerXmpData *data,
                const gchar    *schema,
                const gchar    *path,
                const gchar    *value,
                gboolean        append)
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
		if (!data->title2 && g_ascii_strcasecmp (name, "Title") == 0) {
			data->title2 = g_strdup (value);
		} else if (g_ascii_strcasecmp (name, "DateTimeOriginal") == 0 && !data->time_original) {
			data->time_original = tracker_date_guess (value);
		} else if (!data->artist && g_ascii_strcasecmp (name, "Artist") == 0) {
			data->artist = g_strdup (value);
		/* } else if (g_ascii_strcasecmp (name, "Software") == 0) {
			   tracker_statement_list_insert (metadata, uri,
			   "Image:Software", value);*/
		} else if (!data->make && g_ascii_strcasecmp (name, "Make") == 0) {
			data->make = g_strdup (value);
		} else if (!data->model && g_ascii_strcasecmp (name, "Model") == 0) {
			data->model = g_strdup (value);
		} else if (!data->orientation && g_ascii_strcasecmp (name, "Orientation") == 0) {
			data->orientation = g_strdup (fix_orientation (value));
		} else if (!data->flash && g_ascii_strcasecmp (name, "Flash") == 0) {
			data->flash = g_strdup (fix_flash (value));
		} else if (!data->metering_mode && g_ascii_strcasecmp (name, "MeteringMode") == 0) {
			data->metering_mode = g_strdup (fix_metering_mode (value));
		/* } else if (g_ascii_strcasecmp (name, "ExposureProgram") == 0) {
			   tracker_statement_list_insert (metadata, uri,
			   "Image:ExposureProgram", value);*/
		} else if (!data->exposure_time && g_ascii_strcasecmp (name, "ExposureTime") == 0) {
			data->exposure_time = div_str_dup (value);
		} else if (!data->fnumber && g_ascii_strcasecmp (name, "FNumber") == 0) {
			data->fnumber = div_str_dup (value);
		} else if (!data->focal_length && g_ascii_strcasecmp (name, "FocalLength") == 0) {
			data->focal_length = div_str_dup (value);
		} else if (!data->iso_speed_ratings && g_ascii_strcasecmp (name, "ISOSpeedRatings") == 0) {
			data->iso_speed_ratings = div_str_dup (value);
		} else if (!data->white_balance && g_ascii_strcasecmp (name, "WhiteBalance") == 0) {
			data->white_balance = g_strdup (fix_white_balance (value));
		} else if (!data->copyright && g_ascii_strcasecmp (name, "Copyright") == 0) {
			data->copyright = g_strdup (value);
		}
		/* PDF*/
	} else if (g_ascii_strcasecmp (schema, NS_PDF) == 0) {
		if (g_ascii_strcasecmp (name, "keywords") == 0) {
			if (data->pdf_keywords) {
				gchar *temp = g_strdup_printf ("%s, %s", value, data->pdf_keywords);
				g_free (data->pdf_keywords);
				data->pdf_keywords = temp;
			} else {
				data->pdf_keywords = g_strdup (value);
			}
		} else
			if (!data->pdf_title && g_ascii_strcasecmp (name, "title") == 0) {
				data->pdf_title = g_strdup (value);
			}
		/* Dublin Core */
	} else if (g_ascii_strcasecmp (schema, NS_DC) == 0) {
		if (!data->title && g_ascii_strcasecmp (name, "title") == 0) {
			data->title = g_strdup (value);
		} else if (!data->rights && g_ascii_strcasecmp (name, "rights") == 0) {
			data->rights = g_strdup (value);
		} else if (!data->creator && g_ascii_strcasecmp (name, "creator") == 0) {
			data->creator = g_strdup (value);
		} else if (!data->description && g_ascii_strcasecmp (name, "description") == 0) {
			data->description = g_strdup (value);
		} else if (!data->date && g_ascii_strcasecmp (name, "date") == 0) {
			data->date = tracker_date_guess (value);
		} else if (g_ascii_strcasecmp (name, "keywords") == 0) {
			if (data->keywords) {
				gchar *temp = g_strdup_printf ("%s, %s", value, data->keywords);
				g_free (data->keywords);
				data->keywords = temp;
			} else {
				data->keywords = g_strdup (value);
			}
		} else if (g_ascii_strcasecmp (name, "subject") == 0) {
			if (data->subject) {
				gchar *temp = g_strdup_printf ("%s, %s", value, data->subject);
				g_free (data->subject);
				data->subject = temp;
			} else {
				data->subject = g_strdup (value);
			}
		} else if (!data->publisher && g_ascii_strcasecmp (name, "publisher") == 0) {
			data->publisher = g_strdup (value);
		} else if (!data->contributor && g_ascii_strcasecmp (name, "contributor") == 0) {
			data->contributor = g_strdup (value);
		} else if (!data->type && g_ascii_strcasecmp (name, "type") == 0) {
			data->type = g_strdup (value);
		} else if (!data->format && g_ascii_strcasecmp (name, "format") == 0) {
			data->format = g_strdup (value);
		} else if (!data->identifier && g_ascii_strcasecmp (name, "identifier") == 0) {
			data->identifier = g_strdup (value);
		} else if (!data->source && g_ascii_strcasecmp (name, "source") == 0) {
			data->source = g_strdup (value);
		} else if (!data->language && g_ascii_strcasecmp (name, "language") == 0) {
			data->language = g_strdup (value);
		} else if (!data->relation && g_ascii_strcasecmp (name, "relation") == 0) {
			data->relation = g_strdup (value);
		} else if (!data->coverage && g_ascii_strcasecmp (name, "coverage") == 0) {
			data->coverage = g_strdup (value);
		}
		/* Creative Commons */
	} else if (g_ascii_strcasecmp (schema, NS_CC) == 0) {
		if (!data->license && g_ascii_strcasecmp (name, "license") == 0) {
			data->license = g_strdup (value);
		}
		/* TODO: A lot of these location fields are pretty vague and ambigious.
		 * We should go through them one by one and ensure that all of them are
		 * used sanely */

		/* Photoshop TODO: is this needed anyway? */
	} else if (g_ascii_strcasecmp (schema, NS_PHOTOSHOP) == 0) {
		if (!data->city && g_ascii_strcasecmp (name, "City") == 0) {
			data->city = g_strdup (value);
		} else if (!data->country && g_ascii_strcasecmp (name, "Country") == 0) {
			data->country = g_strdup (value);
		} else if (!data->state && g_ascii_strcasecmp (name, "State") == 0) {
			data->state = g_strdup (value);
		} else if (!data->address && g_ascii_strcasecmp (name, "Location") == 0) {
			data->address = g_strdup (value);
		}
		/* IPTC4XMP scheme - GeoClue / location stuff, TODO */
	} else if (g_ascii_strcasecmp (schema, NS_IPTC4XMP) == 0) {
		if (!data->city && g_ascii_strcasecmp (name, "City") == 0) {
			data->city = g_strdup (value);
		} else if (!data->country && g_ascii_strcasecmp (name, "Country") == 0) {
			data->country = g_strdup (value);
		} else if (!data->country && g_ascii_strcasecmp (name, "CountryName") == 0) {
			data->country = g_strdup (value);
		} else if (!data->country && g_ascii_strcasecmp (name, "PrimaryLocationName") == 0) {
			data->country = g_strdup (value);
		} else if (!data->state && g_ascii_strcasecmp (name, "State") == 0) {
			data->state = g_strdup (value);
		} else if (!data->state && g_ascii_strcasecmp (name, "Province") == 0) {
			data->state = g_strdup (value);
		} else if (!data->address && g_ascii_strcasecmp (name, "Sublocation") == 0) {
			data->address = g_strdup (value);
		}
	} else if (g_ascii_strcasecmp (schema, NS_XAP) == 0) {
		if (!data->rating && g_ascii_strcasecmp (name, "Rating") == 0) {
			data->rating = g_strdup (value);
		}
	}


	g_free (name);
}

/* Iterate over the XMP, dispatching to the appropriate element type
 * (simple, simple w/qualifiers, or an array) handler.
 */
static void
iterate (XmpPtr          xmp,
         XmpIteratorPtr  iter,
         const gchar    *uri,
         TrackerXmpData *data,
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
					iterate_simple_qual (xmp, uri, data, schema, path, value, append);
				} else {
					iterate_simple (uri, data, schema, path, value, append);
				}
			}
		}
		else if (XMP_IS_PROP_ARRAY (opt)) {
			if (XMP_IS_ARRAY_ALTTEXT (opt)) {
				iterate_alt_text (xmp, uri, data, schema, path);
				xmp_iterator_skip (iter, XMP_ITER_SKIPSUBTREE);
			} else {
				iterate_array (xmp, uri, data, schema, path);
				xmp_iterator_skip (iter, XMP_ITER_SKIPSUBTREE);
			}
		}
	}

	xmp_string_free (the_prop);
	xmp_string_free (the_path);
	xmp_string_free (the_schema);
}

#endif /* HAVE_EXEMPI */

static gboolean
parse_xmp (const gchar    *buffer,
           size_t          len,
           const gchar    *uri,
           TrackerXmpData *data)
{
#ifdef HAVE_EXEMPI
	XmpPtr xmp;
#endif /* HAVE_EXEMPI */

	memset (data, 0, sizeof (TrackerXmpData));

#ifdef HAVE_EXEMPI

	xmp_init ();

	xmp = xmp_new_empty ();
	xmp_parse (xmp, buffer, len);

	if (xmp != NULL) {
		XmpIteratorPtr iter;

		iter = xmp_iterator_new (xmp, NULL, NULL, XMP_ITER_PROPERTIES);
		iterate (xmp, iter, uri, data, FALSE);
		xmp_iterator_free (iter);
		xmp_free (xmp);
	}

	xmp_terminate ();
#endif /* HAVE_EXEMPI */

	return TRUE;
}

#ifndef TRACKER_DISABLE_DEPRECATED

/**
 * tracker_xmp_read:
 * @buffer: a chunk of data with xmp data in it.
 * @len: the size of @buffer.
 * @uri: the URI this is related to.
 * @data: a pointer to a TrackerXmpData structure to populate.
 *
 * This function takes @len bytes of @buffer and runs it through the
 * XMP library. The result is that @data is populated with the XMP
 * data found in @uri.
 *
 * Returns: %TRUE if the @data was populated successfully, otherwise
 * %FALSE is returned.
 *
 * Since: 0.8
 *
 * Deprecated: 0.9. Use tracker_xmp_new() instead.
 **/
gboolean
tracker_xmp_read (const gchar    *buffer,
                  size_t          len,
                  const gchar    *uri,
                  TrackerXmpData *data)
{
	g_return_val_if_fail (buffer != NULL, FALSE);
	g_return_val_if_fail (len > 0, FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	return parse_xmp (buffer, len, uri, data);
}

#endif /* TRACKER_DISABLE_DEPRECATED */

/**
 * tracker_xmp_new:
 * @buffer: a chunk of data with xmp data in it.
 * @len: the size of @buffer.
 * @uri: the URI this is related to.
 *
 * This function takes @len bytes of @buffer and runs it through the
 * XMP library.
 *
 * Returns: a newly allocated #TrackerXmpData struct if XMP data was
 * found, %NULL otherwise. Free the returned struct with tracker_xmp_free().
 *
 * Since: 0.9
 **/
TrackerXmpData *
tracker_xmp_new (const gchar *buffer,
                 gsize        len,
                 const gchar *uri)
{
	TrackerXmpData *data;

	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (len > 0, NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	data = g_new0 (TrackerXmpData, 1);

	if (!parse_xmp (buffer, len, uri, data)) {
		tracker_xmp_free (data);
		return NULL;
	}

	return data;
}

/**
 * tracker_xmp_free:
 * @data: a #TrackerXmpData struct
 *
 * Frees @data and all #TrackerXmpData members. %NULL will produce a
 * a warning.
 *
 * Since: 0.9
 **/
void
tracker_xmp_free (TrackerXmpData *data)
{
	g_return_if_fail (data != NULL);

	g_free (data->title);
	g_free (data->rights);
	g_free (data->creator);
	g_free (data->description);
	g_free (data->date);
	g_free (data->keywords);
	g_free (data->subject);
	g_free (data->publisher);
	g_free (data->contributor);
	g_free (data->type);
	g_free (data->format);
	g_free (data->identifier);
	g_free (data->source);
	g_free (data->language);
	g_free (data->relation);
	g_free (data->coverage);
	g_free (data->license);
	g_free (data->pdf_title);
	g_free (data->pdf_keywords);
	g_free (data->title2);
	g_free (data->time_original);
	g_free (data->artist);
	g_free (data->make);
	g_free (data->model);
	g_free (data->orientation);
	g_free (data->flash);
	g_free (data->metering_mode);
	g_free (data->exposure_time);
	g_free (data->fnumber);
	g_free (data->focal_length);
	g_free (data->iso_speed_ratings);
	g_free (data->white_balance);
	g_free (data->copyright);
	g_free (data->rating);
	g_free (data->address);
	g_free (data->country);
	g_free (data->state);
	g_free (data->city);

	g_free (data);
}

/**
 * tracker_xmp_apply:
 * @metadata: the metadata object to apply XMP data to.
 * @uri: the URI this is related to.
 * @data: the data to push into @metadata.
 *
 * This function applies all data in @data to @metadata.
 *
 * Returns: %TRUE if the @data was applied to @metadata successfully,
 * otherwise %FALSE is returned.
 *
 * Since: 0.8
 **/
gboolean
tracker_xmp_apply (TrackerSparqlBuilder *metadata,
                   const gchar          *uri,
                   TrackerXmpData       *data)
{
	GPtrArray *keywords;
	guint i;

	g_return_val_if_fail (TRACKER_IS_SPARQL_BUILDER (metadata), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	keywords = g_ptr_array_new ();

	if (data->keywords) {
		tracker_keywords_parse (keywords, data->keywords);
	}

	if (data->subject) {
		tracker_keywords_parse (keywords, data->subject);
	}

	if (data->pdf_keywords) {
		tracker_keywords_parse (keywords, data->pdf_keywords);
	}

	for (i = 0; i < keywords->len; i++) {
		gchar *p;

		p = g_ptr_array_index (keywords, i);

		tracker_sparql_builder_predicate (metadata, "nao:hasTag");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nao:Tag");
		tracker_sparql_builder_predicate (metadata, "nao:prefLabel");
		tracker_sparql_builder_object_unvalidated (metadata, p);
		tracker_sparql_builder_object_blank_close (metadata);
		g_free (p);
	}
	g_ptr_array_free (keywords, TRUE);

	if (data->publisher) {
		tracker_sparql_builder_predicate (metadata, "nco:publisher");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");

		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, data->publisher);
		tracker_sparql_builder_object_blank_close (metadata);
	}

	if (data->type) {
		tracker_sparql_builder_predicate (metadata, "dc:type");
		tracker_sparql_builder_object_unvalidated (metadata, data->type);
	}

	if (data->format) {
		tracker_sparql_builder_predicate (metadata, "dc:format");
		tracker_sparql_builder_object_unvalidated (metadata, data->format);
	}

	if (data->identifier) {
		tracker_sparql_builder_predicate (metadata, "dc:identifier");
		tracker_sparql_builder_object_unvalidated (metadata, data->identifier);
	}

	if (data->source) {
		tracker_sparql_builder_predicate (metadata, "dc:source");
		tracker_sparql_builder_object_unvalidated (metadata, data->source);
	}

	if (data->language) {
		tracker_sparql_builder_predicate (metadata, "dc:language");
		tracker_sparql_builder_object_unvalidated (metadata, data->language);
	}

	if (data->relation) {
		tracker_sparql_builder_predicate (metadata, "dc:relation");
		tracker_sparql_builder_object_unvalidated (metadata, data->relation);
	}

	if (data->coverage) {
		tracker_sparql_builder_predicate (metadata, "dc:coverage");
		tracker_sparql_builder_object_unvalidated (metadata, data->coverage);
	}

	if (data->license) {
		tracker_sparql_builder_predicate (metadata, "dc:license");
		tracker_sparql_builder_object_unvalidated (metadata, data->license);
	}

	if (data->make || data->model) {
		gchar *final_camera = tracker_merge_const (" ", 2, data->make, data->model);

		tracker_sparql_builder_predicate (metadata, "nfo:device");
		tracker_sparql_builder_object_unvalidated (metadata, final_camera);
		g_free (final_camera);
	}

	if (data->title || data->title2 || data->pdf_title) {
		const gchar *final_title = tracker_coalesce_strip (3, data->title,
		                                                   data->title2,
		                                                   data->pdf_title);

		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, final_title);
	}

	if (data->orientation) {
		tracker_sparql_builder_predicate (metadata, "nfo:orientation");
		tracker_sparql_builder_object_unvalidated (metadata, data->orientation);
	}

	if (data->rights || data->copyright) {
		const gchar *final_rights = tracker_coalesce_strip (2, data->copyright, data->rights);

		tracker_sparql_builder_predicate (metadata, "nie:copyright");
		tracker_sparql_builder_object_unvalidated (metadata, final_rights);
	}

	if (data->white_balance) {
		tracker_sparql_builder_predicate (metadata, "nmm:whiteBalance");
		tracker_sparql_builder_object_unvalidated (metadata, data->white_balance);
	}

	if (data->fnumber) {
		tracker_sparql_builder_predicate (metadata, "nmm:fnumber");
		tracker_sparql_builder_object_unvalidated (metadata, data->fnumber);
	}

	if (data->flash) {
		tracker_sparql_builder_predicate (metadata, "nmm:flash");
		tracker_sparql_builder_object_unvalidated (metadata, data->flash);
	}

	if (data->focal_length) {
		tracker_sparql_builder_predicate (metadata, "nmm:focalLength");
		tracker_sparql_builder_object_unvalidated (metadata, data->focal_length);
	}

	if (data->artist || data->contributor) {
		const gchar *final_artist = tracker_coalesce_strip (2, data->artist, data->contributor);

		tracker_sparql_builder_predicate (metadata, "nco:contributor");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");

		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, final_artist);
		tracker_sparql_builder_object_blank_close (metadata);
	}

	if (data->exposure_time) {
		tracker_sparql_builder_predicate (metadata, "nmm:exposureTime");
		tracker_sparql_builder_object_unvalidated (metadata, data->exposure_time);
	}

	if (data->iso_speed_ratings) {
		tracker_sparql_builder_predicate (metadata, "nmm:isoSpeed");
		tracker_sparql_builder_object_unvalidated (metadata, data->iso_speed_ratings);
	}

	if (data->date || data->time_original) {
		const gchar *final_date = tracker_coalesce_strip (2, data->date,
		                                                  data->time_original);

		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, final_date);
	}

	if (data->description) {
		tracker_sparql_builder_predicate (metadata, "nie:description");
		tracker_sparql_builder_object_unvalidated (metadata, data->description);
	}

	if (data->metering_mode) {
		tracker_sparql_builder_predicate (metadata, "nmm:meteringMode");
		tracker_sparql_builder_object_unvalidated (metadata, data->metering_mode);
	}

	if (data->creator) {
		tracker_sparql_builder_predicate (metadata, "nco:creator");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");

		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, data->creator);
		tracker_sparql_builder_object_blank_close (metadata);
	}

	if (data->address || data->country || data->city) {
		tracker_sparql_builder_predicate (metadata, "mlo:location");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "mlo:GeoPoint");

		if (data->address) {
			tracker_sparql_builder_predicate (metadata, "mlo:address");
			tracker_sparql_builder_object_unvalidated (metadata, data->address);
		}

		if (data->state) {
			tracker_sparql_builder_predicate (metadata, "mlo:state");
			tracker_sparql_builder_object_unvalidated (metadata, data->state);
		}

		if (data->city) {
			tracker_sparql_builder_predicate (metadata, "mlo:city");
			tracker_sparql_builder_object_unvalidated (metadata, data->city);
		}

		if (data->country) {
			tracker_sparql_builder_predicate (metadata, "mlo:country");
			tracker_sparql_builder_object_unvalidated (metadata, data->country);
		}
	
		tracker_sparql_builder_object_blank_close (metadata);
	}

	return TRUE;
}
