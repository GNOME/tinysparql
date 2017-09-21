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

#include "tracker-resource-helpers.h"
#include "tracker-xmp.h"
#include "tracker-utils.h"

#ifdef HAVE_EXEMPI

#define NS_XMP_REGIONS "http://www.metadataworkinggroup.com/schemas/regions/"
#define NS_ST_DIM "http://ns.adobe.com/xap/1.0/sType/Dimensions#"
#define NS_ST_AREA "http://ns.adobe.com/xmp/sType/Area#"

#define REGION_LIST_REGEX "^mwg-rs:Regions/mwg-rs:RegionList\\[(\\d+)\\]"

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

static gchar *
gps_coordinate_dup (const gchar *coordinates)
{
	static GRegex *reg = NULL;
	GMatchInfo *info = NULL;

	if (!reg) {
		reg = g_regex_new ("([0-9]+),([0-9]+.[0-9]+)([A-Z])", 0, 0, NULL);
	}

	if (g_regex_match (reg, coordinates, 0, &info)) {
		gchar *deg,*min,*ref;
		gdouble r,d,m;

		deg = g_match_info_fetch (info, 1);
		min = g_match_info_fetch (info, 2);
		ref = g_match_info_fetch (info, 3);

		d = atof (deg);
		m = atof (min);

		r = d + m/60;

		if ( (ref[0] == 'S') || (ref[0] == 'W')) {
			r = r * -1;
		}

		g_free (deg);
		g_free (min);
		g_free (ref);
                g_match_info_free (info);

		return g_strdup_printf ("%f", r);
	} else {
                g_match_info_free (info);
		return NULL;
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

		if (b != 0) {
			ret = g_strdup_printf ("%G", ((gdouble)((gdouble) a / (gdouble) b)));
		} else {
			ret = NULL;
		}

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
	if (orientation && g_ascii_strcasecmp (orientation, "1") == 0) {
		return "nfo:orientation-top";
	} else if (orientation && g_ascii_strcasecmp (orientation, "2") == 0) {
		return  "nfo:orientation-top-mirror";
	} else if (orientation && g_ascii_strcasecmp (orientation, "3") == 0) {
		return "nfo:orientation-bottom";
	} else if (orientation && g_ascii_strcasecmp (orientation, "4") == 0) {
		return  "nfo:orientation-bottom-mirror";
	} else if (orientation && g_ascii_strcasecmp (orientation, "5") == 0) {
		return  "nfo:orientation-left-mirror";
	} else if (orientation && g_ascii_strcasecmp (orientation, "6") == 0) {
		return  "nfo:orientation-right";
	} else if (orientation && g_ascii_strcasecmp (orientation, "7") == 0) {
		return "nfo:orientation-right-mirror";
	} else if (orientation && g_ascii_strcasecmp (orientation, "8") == 0) {
		return  "nfo:orientation-left";
	}

	return  "nfo:orientation-top";
}

/*
 * In a path like: mwg-rs:Regions/mwg-rs:RegionList[2]/mwg-rs:Area/stArea:x
 * this function returns the "2" from RegionsList[2] 
 *  Note: The first element from a list is 1
 */
static gint
get_region_counter (const gchar *path) 
{
        static GRegex *regex = NULL;
        GMatchInfo    *match_info = NULL;
        gchar         *match;
        gint           result;
        
        if (!regex) {
                regex = g_regex_new (REGION_LIST_REGEX, 0, 0, NULL);
        }

        if (!g_regex_match (regex, path, 0, &match_info)) {
                g_match_info_free (match_info);
                return -1;
        }

        match = g_match_info_fetch (match_info, 1);
        result =  g_strtod (match, NULL);

        g_free (match);
        g_match_info_free (match_info);

        return result;
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
	const gchar *p;
	gchar *propname;

	p = strchr (path, ':');
	if (!p) {
		return;
	}

	name = g_strdup (p + 1);

	/* For 'dc:subject[1]' the name will be 'subject'.
	 * This rule doesn't work for RegionLists
	 */
	p = strrchr (name, '[');
	if (p) {
		name[p - name] = '\0';
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
		} else if (!data->gps_altitude && g_ascii_strcasecmp (name, "GPSAltitude") == 0) {
			data->gps_altitude = div_str_dup (value);
		} else if (!data->gps_altitude_ref && g_ascii_strcasecmp (name, "GPSAltitudeRef") == 0) {
			data->gps_altitude_ref = g_strdup (value);
		} else if (!data->gps_latitude && g_ascii_strcasecmp (name, "GPSLatitude") == 0) {
			data->gps_latitude = gps_coordinate_dup (value);
		} else if (!data->gps_longitude && g_ascii_strcasecmp (name, "GPSLongitude") == 0) {
			data->gps_longitude = gps_coordinate_dup (value);
		} else if (!data->gps_direction && g_ascii_strcasecmp (name, "GPSImgDirection") == 0) {
			data->gps_direction = div_str_dup (value);
		}
		/* TIFF */
	} else if (g_ascii_strcasecmp (schema, NS_TIFF) == 0) {
		if (!data->orientation && g_ascii_strcasecmp (name, "Orientation") == 0) {
			data->orientation = g_strdup (fix_orientation (value));
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
	} else if (g_ascii_strcasecmp (schema, NS_XMP_REGIONS) == 0) {
                if (g_str_has_prefix (path, "mwg-rs:Regions/mwg-rs:RegionList")) {
	                TrackerXmpRegion *current_region;
                        gint              position = get_region_counter (path);

                        if (position == -1) {
                                g_free (name);
                                return;
                        }

                        /* First time a property appear for a region, we create the region */
                        current_region = g_slist_nth_data (data->regions, position-1);
                        if (current_region == NULL) {
                                current_region = g_slice_new0 (TrackerXmpRegion);
                                data->regions = g_slist_append (data->regions, current_region);
                        }

                        propname = g_strdup (strrchr (path, '/') + 1);

                        if (!current_region->title && g_ascii_strcasecmp (propname, "mwg-rs:Name") == 0) {
                                current_region->title = g_strdup (value);
                        } else if (!current_region->description && g_ascii_strcasecmp (propname, "mwg-rs:Description") == 0) {
                                current_region->description = g_strdup (value);
                        } else if (!current_region->x && g_ascii_strcasecmp (propname, "stArea:x") == 0) {
                                current_region->x = g_strdup (value);
                        } else if (!current_region->y && g_ascii_strcasecmp (propname, "stArea:y") == 0) {
                                current_region->y = g_strdup (value);
                        } else if (!current_region->width && g_ascii_strcasecmp (propname, "stArea:w") == 0) {
                                current_region->width = g_strdup (value);
                        } else if (!current_region->height && g_ascii_strcasecmp (propname, "stArea:h") == 0) {
                                current_region->height = g_strdup (value);

                                /* Spec not clear about units
                                 *  } else if (!current_region->unit
                                 *             && g_ascii_strcasecmp (propname, "stArea:unit") == 0) {
                                 *      current_region->unit = g_strdup (value);
                                 *
                                 *  we consider it always comes normalized
                                */
                        } else if (!current_region->type && g_ascii_strcasecmp (propname, "mwg-rs:Type") == 0) {
                                current_region->type = g_strdup (value);
                        } else if (!current_region->link_class && !current_region->link_uri &&
                                   g_str_has_prefix (strrchr (path, ']') + 2, "mwg-rs:Extensions")) {
                                current_region->link_class = g_strdup (propname);
                                current_region->link_uri = g_strdup (value);
                        }

                        g_free (propname);
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
		} else if (XMP_IS_PROP_ARRAY (opt)) {
			if (XMP_IS_ARRAY_ALTTEXT (opt)) {
				iterate_alt_text (xmp, uri, data, schema, path);
				xmp_iterator_skip (iter, XMP_ITER_SKIPSUBTREE);
			} else {
				iterate_array (xmp, uri, data, schema, path);

                                /* Some dc: elements are handled as arrays by exempi.
                                 * In those cases, to avoid duplicated values, is easier
                                 * to skip the subtree.
                                 */
                                if (g_ascii_strcasecmp (schema, NS_DC) == 0) {
                                        xmp_iterator_skip (iter, XMP_ITER_SKIPSUBTREE);
                                }
			}
		}
	}

	xmp_string_free (the_prop);
	xmp_string_free (the_path);
	xmp_string_free (the_schema);
}

static void
register_namespace (const gchar *ns_uri,
                    const gchar *suggested_prefix)
{
        if (!xmp_namespace_prefix (ns_uri, NULL)) {
                xmp_register_namespace (ns_uri, suggested_prefix, NULL);
        }
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

        register_namespace (NS_XMP_REGIONS, "mwg-rs");
        register_namespace (NS_ST_DIM, "stDim");
        register_namespace (NS_ST_AREA, "stArea");

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

// LCOV_EXCL_START

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

// LCOV_EXCL_STOP

#endif /* TRACKER_DISABLE_DEPRECATED */

static void
xmp_region_free (gpointer data)
{
        TrackerXmpRegion *region = (TrackerXmpRegion *) data;

        g_free (region->title);
        g_free (region->description);
        g_free (region->type);
        g_free (region->x);
        g_free (region->y);
        g_free (region->width);
        g_free (region->height);
        g_free (region->link_class);
        g_free (region->link_uri);

        g_slice_free (TrackerXmpRegion, region);
}


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
 * Since: 0.10
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
 * Since: 0.10
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
	g_free (data->gps_altitude);
	g_free (data->gps_altitude_ref);
	g_free (data->gps_latitude);
	g_free (data->gps_longitude);
	g_free (data->gps_direction);

        g_slist_free_full (data->regions, xmp_region_free);
	g_free (data);
}


static const gchar *
fix_region_type (const gchar *region_type)
{
        if (region_type == NULL) {
                return "nfo:region-content-undefined";
        }

        if (g_ascii_strncasecmp (region_type, "Face", 4) == 0) {
                return "nfo:roi-content-face";
        } else if (g_ascii_strncasecmp (region_type, "Pet", 3) == 0) {
                return "nfo:roi-content-pet";
        } else if (g_ascii_strncasecmp (region_type, "Focus", 5) == 0) {
                return "nfo:roi-content-focus";
        } else if (g_ascii_strncasecmp (region_type, "BarCode", 7) == 0) {
                return "nfo:roi-content-barcode";
        }

        return "nfo:roi-content-undefined";
}


/**
 * tracker_xmp_apply_to_resource:
 * @resource: the #TrackerResource to apply XMP data to.
 * @data: the data to push into @resource.
 *
 * This function applies all data in @data to @resource.
 *
 * This function also calls tracker_xmp_apply_regions_to_resource(), so there
 * is no need to call both functions.
 *
 * Returns: %TRUE if the @data was applied to @resource successfully,
 * otherwise %FALSE is returned.
 *
 * Since: 1.10
 **/
gboolean
tracker_xmp_apply_to_resource (TrackerResource *resource,
                               TrackerXmpData  *data)
{
	GPtrArray *keywords;
	guint i;

	g_return_val_if_fail (TRACKER_IS_RESOURCE (resource), FALSE);
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
		TrackerResource *label;
		gchar *p;

		p = g_ptr_array_index (keywords, i);
		label = tracker_extract_new_tag (p);

		tracker_resource_set_relation (resource, "nao:hasTag", label);

		g_free (p);
		g_object_unref (label);
	}
	g_ptr_array_free (keywords, TRUE);

	if (data->publisher) {
		TrackerResource *publisher;

		publisher = tracker_extract_new_contact (data->publisher);
		tracker_resource_set_relation (resource, "nco:publisher", publisher);
		g_object_unref (publisher);
	}

	if (data->type) {
		tracker_resource_set_string (resource, "dc:type", data->type);
	}

	if (data->format) {
		tracker_resource_set_string (resource, "dc:format", data->format);
	}

	if (data->identifier) {
		tracker_resource_set_string (resource, "dc:identifier", data->identifier);
	}

	if (data->source) {
		tracker_resource_set_string (resource, "dc:source", data->source);
	}

	if (data->language) {
		tracker_resource_set_string (resource, "dc:language", data->language);
	}

	if (data->relation) {
		tracker_resource_set_string (resource, "dc:relation", data->relation);
	}

	if (data->coverage) {
		tracker_resource_set_string (resource, "dc:coverage", data->coverage);
	}

	if (data->license) {
		tracker_resource_set_string (resource, "dc:license", data->license);
	}

	if (data->make || data->model) {
		TrackerResource *equipment;

		equipment = tracker_extract_new_equipment (data->make, data->model);
		tracker_resource_set_relation (resource, "nfo:equipment", equipment);
		g_object_unref (equipment);
	}

	if (data->title || data->title2 || data->pdf_title) {
		const gchar *final_title = tracker_coalesce_strip (3, data->title,
		                                                   data->title2,
		                                                   data->pdf_title);

		tracker_resource_set_string (resource, "nie:title", final_title);
	}

	if (data->orientation) {
		TrackerResource *orientation;

		orientation = tracker_resource_new (data->orientation);
		tracker_resource_set_relation (resource, "nfo:orientation", orientation);
		g_object_unref (orientation);
	}

	if (data->rights || data->copyright) {
		const gchar *final_rights = tracker_coalesce_strip (2, data->copyright, data->rights);

		tracker_resource_set_string (resource, "nie:copyright", final_rights);
	}

	if (data->white_balance) {
		TrackerResource *white_balance;

		white_balance = tracker_resource_new (data->white_balance);
		tracker_resource_set_relation (resource, "nmm:whiteBalance", white_balance);
		g_object_unref (white_balance);
	}

	if (data->fnumber) {
		tracker_resource_set_string (resource, "nmm:fnumber", data->fnumber);
	}

	if (data->flash) {
		TrackerResource *flash;

		flash = tracker_resource_new (data->flash);
		tracker_resource_set_relation (resource, "nmm:flash", flash);
		g_object_unref (flash);
	}

	if (data->focal_length) {
		tracker_resource_set_string (resource, "nmm:focalLength", data->focal_length);
	}

	if (data->artist || data->contributor) {
		TrackerResource *contributor;
		const gchar *final_artist = tracker_coalesce_strip (2, data->artist, data->contributor);

		contributor = tracker_extract_new_contact (final_artist);
		tracker_resource_set_relation (resource, "nco:contributor", contributor);
		g_object_unref (contributor);
	}

	if (data->exposure_time) {
		tracker_resource_set_string (resource, "nmm:exposureTime", data->exposure_time);
	}

	if (data->iso_speed_ratings) {
		tracker_resource_set_string (resource, "nmm:isoSpeed", data->iso_speed_ratings);
	}

	if (data->date || data->time_original) {
		const gchar *final_date = tracker_coalesce_strip (2, data->date,
		                                                  data->time_original);

		tracker_resource_set_string (resource, "nie:contentCreated", final_date);
	}

	if (data->description) {
		tracker_resource_set_string (resource, "nie:description", data->description);
	}

	if (data->metering_mode) {
		TrackerResource *metering;

		metering = tracker_resource_new (data->metering_mode);
		tracker_resource_set_relation (resource, "nmm:meteringMode", metering);
		g_object_unref (metering);
	}

	if (data->creator) {
		TrackerResource *creator;

		creator = tracker_extract_new_contact (data->creator);
		tracker_resource_set_relation (resource, "nco:creator", creator);
		g_object_unref (creator);
	}

	if (data->address || data->state || data->country || data->city ||
	    data->gps_altitude || data->gps_latitude || data->gps_longitude) {
		TrackerResource *geopoint;

		geopoint = tracker_extract_new_location (data->address, data->state, data->city,
		        data->country, data->gps_altitude, data->gps_latitude, data->gps_longitude);
		tracker_resource_set_relation (resource, "slo:location", geopoint);
		g_object_unref (geopoint);
	}

	if (data->gps_direction) {
		tracker_resource_set_string (resource, "nfo:heading", data->gps_direction);
	}

	if (data->regions) {
		tracker_xmp_apply_regions_to_resource (resource, data);
	}

	return TRUE;
}

/**
 * tracker_xmp_apply_regions_to_resource:
 * @resource: the #TrackerResource object to apply XMP data to.
 * @data: the data to push into @resource
 *
 * This function applies all regional @data to @resource. Regional data exists
 * for image formats like JPEG, PNG, etc. where parts of the image refer to
 * areas of interest. This can be people's faces, places to focus, barcodes,
 * etc. The regional data describes the title, height, width, X, Y and can
 * occur multiple times in a given file.
 *
 * This data usually is standardized between image formats and that's
 * what makes this function different to tracker_xmp_apply_to_resource() which
 * is useful for XMP files only.
 *
 * Returns: %TRUE if the @data was applied to @resource successfully, otherwise
 * %FALSE is returned.
 *
 * Since: 1.10
 **/
gboolean
tracker_xmp_apply_regions_to_resource (TrackerResource *resource,
                                       TrackerXmpData  *data)
{
	GSList *iter;

	g_return_val_if_fail (TRACKER_IS_RESOURCE (resource), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	if (!data->regions) {
		return TRUE;
	}

	for (iter = data->regions; iter != NULL; iter = iter->next) {
		TrackerResource *region_resource;
		TrackerXmpRegion *region;
		gchar *uuid;

		region = (TrackerXmpRegion *) iter->data;
		uuid = tracker_sparql_get_uuid_urn ();

		region_resource = tracker_resource_new (uuid);
		tracker_resource_set_uri (region_resource, "rdf:type", "nfo:RegionOfInterest");

		g_free (uuid);

		if (region->title) {
			tracker_resource_set_string (region_resource, "nie:title", region->title);
		}

		if (region->description) {
			tracker_resource_set_string (region_resource, "nie:description", region->description);
		}

		if (region->type) {
			tracker_resource_set_string (region_resource, "nfo:regionOfInterestType", fix_region_type (region->type));
		}

		if (region->x) {
			tracker_resource_set_string (region_resource, "nfo:regionOfInterestX", region->x);
		}

		if (region->y) {
			tracker_resource_set_string (region_resource, "nfo:regionOfInterestY", region->y);
		}

		if (region->width) {
			tracker_resource_set_string (region_resource, "nfo:regionOfInterestWidth", region->width);
		}

		if (region->height) {
			tracker_resource_set_string (region_resource, "nfo:regionOfInterestHeight", region->height);
		}

		if (region->link_uri && region->link_class) {
			tracker_resource_set_string (region_resource, "nfo:roiRefersTo", region->link_uri);
		}

		tracker_resource_add_relation (resource, "nfo:hasRegionOfInterest", region_resource);
		g_object_unref (region_resource);
	}

	return TRUE;
}
