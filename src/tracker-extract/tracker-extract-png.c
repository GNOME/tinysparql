/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <png.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-date-time.h>
#include <libtracker-extract/tracker-extract.h>

#define RFC1123_DATE_FORMAT "%d %B %Y %H:%M:%S %z"
#define CM_TO_INCH          0.393700787

typedef struct {
	const gchar *title;
	const gchar *copyright;
	const gchar *creator;
	const gchar *description;
	const gchar *date;
	const gchar *license;
	const gchar *artist;
	const gchar *make;
	const gchar *model;
	const gchar *orientation;
	const gchar *white_balance;
	const gchar *fnumber;
	const gchar *flash;
	const gchar *focal_length;
	const gchar *exposure_time;
	const gchar *iso_speed_ratings;
	const gchar *metering_mode;
	const gchar *comment;
	const gchar *city;
	const gchar *state;
	const gchar *address;
	const gchar *country;
	const gchar *gps_direction;
} MergeData;

typedef struct {
	const gchar *author;
	const gchar *creator;
	const gchar *description;
	const gchar *comment;
	const gchar *copyright;
	gchar *creation_time;
	const gchar *title;
	const gchar *disclaimer;
	const gchar *software;
} PngData;

static gchar *
rfc1123_to_iso8601_date (const gchar *date)
{
	/* From: ex. RFC1123 date: "22 May 1997 18:07:10 -0600"
	 * To  : ex. ISO8601 date: "2007-05-22T18:07:10-0600"
	 */
	return tracker_date_format_to_iso8601 (date, RFC1123_DATE_FORMAT);
}

#if defined(PNG_iTXt_SUPPORTED) && (defined(HAVE_EXEMPI) || defined(HAVE_LIBEXIF))

/* Handle raw profiles by Imagemagick (at least). Hex encoded with
 * line-changes and other (undocumented/unofficial) twists.
 */
static gchar *
raw_profile_new (const gchar *input,
                 const guint  input_length,
                 guint       *output_length)
{
	static const gchar* const lut = "0123456789abcdef";
	gchar *output;
	const gchar *ptr;
	const gchar *length_ptr;
	gsize size;
	gchar *length_str;
	guint length;

	size_t len;
	size_t i;
	size_t o;
	char *p;
	char *q;

	ptr = input;

	if (*ptr != '\n') {
		return NULL;
	}

	ptr++;

	if (!g_ascii_isalpha (*ptr)) {
		return NULL;
	}

	/* Skip the type string */
	do {
		ptr++;
	} while (g_ascii_isalpha (*ptr));

	if (*ptr != '\n') {
		return NULL;
	}

	/* Hop over whitespaces */
	do {
		ptr++;
	} while (*ptr == ' ');

	if (!g_ascii_isdigit (*ptr)) {
		return NULL;
	}

	/* Get the length string */
	length_ptr = ptr;
	size = 1;

	do {
		ptr++;
		size++;
	} while (g_ascii_isdigit (*ptr));

	length_str = g_strndup (length_ptr, size - 1);

	if (*ptr != '\n') {
		return NULL;
	}

	ptr++;

	length = atoi (length_str);
	g_free (length_str);

	len = length;
	i = 0;
	o = 0;

	output = malloc (length + 1); /* A bit less with non-valid */

	o = 0;
	while (o < len) {
		do {
			gchar a = ptr[i];
			p = strchr (lut, a);
			i++;
		} while (p == 0);

		do {
			gchar b = ptr[i];
			q = strchr (lut, b);
			i++;
		} while (q == 0);

		output[o] = (((p - lut) << 4) | (q - lut));
		o++;
	}

	output[o] = '\0';
	*output_length = o;

	return output;
}

#endif /* defined(PNG_iTXt_SUPPORTED) && (defined(HAVE_EXEMPI) || defined(HAVE_LIBEXIF)) */

static void
read_metadata (TrackerSparqlBuilder *preupdate,
               TrackerSparqlBuilder *metadata,
               GString              *where,
               png_structp           png_ptr,
               png_infop             info_ptr,
               png_infop             end_ptr,
               const gchar          *uri,
               const gchar          *graph)
{
	MergeData md = { 0 };
	PngData pd = { 0 };
	TrackerExifData *ed = NULL;
	TrackerXmpData *xd = NULL;
	png_infop info_ptrs[2];
	png_textp text_ptr;
	gint info_index;
	gint num_text;
	gint i;
	gint found;
	GPtrArray *keywords;

	info_ptrs[0] = info_ptr;
	info_ptrs[1] = end_ptr;

	for (info_index = 0; info_index < 2; info_index++) {
		if ((found = png_get_text (png_ptr, info_ptrs[info_index], &text_ptr, &num_text)) < 1) {
			g_debug ("Calling png_get_text() returned %d (< 1)", found);
			continue;
		}

		for (i = 0; i < num_text; i++) {
			if (!text_ptr[i].key || !text_ptr[i].text || text_ptr[i].text[0] == '\0') {
				continue;
			}

#if defined(HAVE_EXEMPI) && defined(PNG_iTXt_SUPPORTED)
			if (g_strcmp0 ("XML:com.adobe.xmp", text_ptr[i].key) == 0) {
				/* ATM tracker_extract_xmp_read supports setting xd
				 * multiple times, keep it that way as here it's
				 * theoretically possible that the function gets
				 * called multiple times
				 */
				xd = tracker_xmp_new (text_ptr[i].text,
				                      text_ptr[i].itxt_length,
				                      uri);

				continue;
			}

			if (g_strcmp0 ("Raw profile type xmp", text_ptr[i].key) == 0) {
				gchar *xmp_buffer;
				guint xmp_buffer_length = 0;
				guint input_len;

				if (text_ptr[i].text_length) {
					input_len = text_ptr[i].text_length;
				} else {
					input_len = text_ptr[i].itxt_length;
				}

				xmp_buffer = raw_profile_new (text_ptr[i].text,
				                              input_len,
				                              &xmp_buffer_length);

				if (xmp_buffer) {
					xd = tracker_xmp_new (xmp_buffer,
					                      xmp_buffer_length,
					                      uri);
				}

				g_free (xmp_buffer);

				continue;
			}
#endif /*HAVE_EXEMPI && PNG_iTXt_SUPPORTED */

#if defined(HAVE_LIBEXIF) && defined(PNG_iTXt_SUPPORTED)
			if (g_strcmp0 ("Raw profile type exif", text_ptr[i].key) == 0) {
				gchar *exif_buffer;
				guint exif_buffer_length = 0;
				guint input_len;

				if (text_ptr[i].text_length) {
					input_len = text_ptr[i].text_length;
				} else {
					input_len = text_ptr[i].itxt_length;
				}

				exif_buffer = raw_profile_new (text_ptr[i].text,
				                               input_len,
				                               &exif_buffer_length);

				if (exif_buffer) {
					ed = tracker_exif_new (exif_buffer,
					                       exif_buffer_length,
					                       uri);
				}

				g_free (exif_buffer);

				continue;
			}
#endif /* HAVE_LIBEXIF && PNG_iTXt_SUPPORTED */

			if (g_strcmp0 (text_ptr[i].key, "Author") == 0) {
				pd.author = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Creator") == 0) {
				pd.creator = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Description") == 0) {
				pd.description = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Comment") == 0) {
				pd.comment = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Copyright") == 0) {
				pd.copyright = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Creation Time") == 0) {
				pd.creation_time = rfc1123_to_iso8601_date (text_ptr[i].text);
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Title") == 0) {
				pd.title = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0 (text_ptr[i].key, "Disclaimer") == 0) {
				pd.disclaimer = text_ptr[i].text;
				continue;
			}

			if (g_strcmp0(text_ptr[i].key, "Software") == 0) {
				pd.software = text_ptr[i].text;
				continue;
			}
		}
	}

	if (!ed) {
		ed = g_new0 (TrackerExifData, 1);
	}

	if (!xd) {
		xd = g_new0 (TrackerXmpData, 1);
	}

	md.creator = tracker_coalesce_strip (3, xd->creator, pd.creator, pd.author);
	md.title = tracker_coalesce_strip (5, xd->title, pd.title, ed->document_name, xd->title2, xd->pdf_title);
	md.copyright = tracker_coalesce_strip (3, xd->rights, pd.copyright, ed->copyright);
	md.license = tracker_coalesce_strip (2, xd->license, pd.disclaimer);
	md.description = tracker_coalesce_strip (3, xd->description, pd.description, ed->description);
	md.date = tracker_coalesce_strip (5, xd->date, xd->time_original, pd.creation_time, ed->time, ed->time_original);
	md.comment = tracker_coalesce_strip (2, pd.comment, ed->user_comment);
	md.artist = tracker_coalesce_strip (3, xd->artist, ed->artist, xd->contributor);
	md.orientation = tracker_coalesce_strip (2, xd->orientation, ed->orientation);
	md.exposure_time = tracker_coalesce_strip (2, xd->exposure_time, ed->exposure_time);
	md.iso_speed_ratings = tracker_coalesce_strip (2, xd->iso_speed_ratings, ed->iso_speed_ratings);
	md.fnumber = tracker_coalesce_strip (2, xd->fnumber, ed->fnumber);
	md.flash = tracker_coalesce_strip (2, xd->flash, ed->flash);
	md.focal_length = tracker_coalesce_strip (2, xd->focal_length, ed->focal_length);
	md.metering_mode = tracker_coalesce_strip (2, xd->metering_mode, ed->metering_mode);
	md.white_balance = tracker_coalesce_strip (2, xd->white_balance, ed->white_balance);
	md.make = tracker_coalesce_strip (2, xd->make, ed->make);
	md.model = tracker_coalesce_strip (2, xd->model, ed->model);

	keywords = g_ptr_array_new ();

	if (md.comment) {
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, md.comment);
	}

	if (md.license) {
		tracker_sparql_builder_predicate (metadata, "nie:license");
		tracker_sparql_builder_object_unvalidated (metadata, md.license);
	}

	/* TODO: add ontology and store this ed->software */

	if (md.creator) {
		gchar *uri = tracker_sparql_escape_uri_printf ("urn:contact:%s", md.creator);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		if (graph) {
			tracker_sparql_builder_graph_open (preupdate, graph);
		}

		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, md.creator);

		if (graph) {
			tracker_sparql_builder_graph_close (preupdate);
		}
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "nco:creator");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	tracker_guarantee_date_from_file_mtime (metadata,
	                                        "nie:contentCreated",
	                                        md.date,
	                                        uri);

	if (md.description) {
		tracker_sparql_builder_predicate (metadata, "nie:description");
		tracker_sparql_builder_object_unvalidated (metadata, md.description);
	}

	if (md.copyright) {
		tracker_sparql_builder_predicate (metadata, "nie:copyright");
		tracker_sparql_builder_object_unvalidated (metadata, md.copyright);
	}

	tracker_guarantee_title_from_file (metadata,
	                                   "nie:title",
	                                   md.title,
	                                   uri,
	                                   NULL);

	if (md.make || md.model) {
		gchar *equip_uri;

		equip_uri = tracker_sparql_escape_uri_printf ("urn:equipment:%s:%s:",
		                                              md.make ? md.make : "",
		                                              md.model ? md.model : "");

		tracker_sparql_builder_insert_open (preupdate, NULL);
		if (graph) {
			tracker_sparql_builder_graph_open (preupdate, graph);
		}

		tracker_sparql_builder_subject_iri (preupdate, equip_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nfo:Equipment");

		if (md.make) {
			tracker_sparql_builder_predicate (preupdate, "nfo:manufacturer");
			tracker_sparql_builder_object_unvalidated (preupdate, md.make);
		}
		if (md.model) {
			tracker_sparql_builder_predicate (preupdate, "nfo:model");
			tracker_sparql_builder_object_unvalidated (preupdate, md.model);
		}

		if (graph) {
			tracker_sparql_builder_graph_close (preupdate);
		}
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "nfo:equipment");
		tracker_sparql_builder_object_iri (metadata, equip_uri);
		g_free (equip_uri);
	}

	if (md.artist) {
		gchar *uri = tracker_sparql_escape_uri_printf ("urn:contact:%s", md.artist);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		if (graph) {
			tracker_sparql_builder_graph_open (preupdate, graph);
		}

		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, md.artist);

		if (graph) {
			tracker_sparql_builder_graph_close (preupdate);
		}
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "nco:contributor");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (md.orientation) {
		tracker_sparql_builder_predicate (metadata, "nfo:orientation");
		tracker_sparql_builder_object_unvalidated (metadata, md.orientation);
	}

	if (md.exposure_time) {
		tracker_sparql_builder_predicate (metadata, "nmm:exposureTime");
		tracker_sparql_builder_object_unvalidated (metadata, md.exposure_time);
	}

	if (md.iso_speed_ratings) {
		tracker_sparql_builder_predicate (metadata, "nmm:isoSpeed");
		tracker_sparql_builder_object_unvalidated (metadata, md.iso_speed_ratings);
	}

	if (md.white_balance) {
		tracker_sparql_builder_predicate (metadata, "nmm:whiteBalance");
		tracker_sparql_builder_object_unvalidated (metadata, md.white_balance);
	}

	if (md.fnumber) {
		tracker_sparql_builder_predicate (metadata, "nmm:fnumber");
		tracker_sparql_builder_object_unvalidated (metadata, md.fnumber);
	}

	if (md.flash) {
		tracker_sparql_builder_predicate (metadata, "nmm:flash");
		tracker_sparql_builder_object_unvalidated (metadata, md.flash);
	}

	if (md.focal_length) {
		tracker_sparql_builder_predicate (metadata, "nmm:focalLength");
		tracker_sparql_builder_object_unvalidated (metadata, md.focal_length);
	}

	if (md.metering_mode) {
		tracker_sparql_builder_predicate (metadata, "nmm:meteringMode");
		tracker_sparql_builder_object_unvalidated (metadata, md.metering_mode);
	}


	if (xd->keywords) {
		tracker_keywords_parse (keywords, xd->keywords);
	}

	if (xd->pdf_keywords) {
		tracker_keywords_parse (keywords, xd->pdf_keywords);
	}

	if (xd->rating) {
		tracker_sparql_builder_predicate (metadata, "nao:numericRating");
		tracker_sparql_builder_object_unvalidated (metadata, xd->rating);
	}

	if (xd->subject) {
		tracker_keywords_parse (keywords, xd->subject);
	}

	if (xd->publisher) {
		gchar *uri = tracker_sparql_escape_uri_printf ("urn:contact:%s", xd->publisher);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		if (graph) {
			tracker_sparql_builder_graph_open (preupdate, graph);
		}

		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "nco:Contact");
		tracker_sparql_builder_predicate (preupdate, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (preupdate, xd->publisher);

		if (graph) {
			tracker_sparql_builder_graph_close (preupdate);
		}
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "nco:creator");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (xd->type) {
		tracker_sparql_builder_predicate (metadata, "dc:type");
		tracker_sparql_builder_object_unvalidated (metadata, xd->type);
	}

	if (xd->format) {
		tracker_sparql_builder_predicate (metadata, "dc:format");
		tracker_sparql_builder_object_unvalidated (metadata, xd->format);
	}

	if (xd->identifier) {
		tracker_sparql_builder_predicate (metadata, "dc:identifier");
		tracker_sparql_builder_object_unvalidated (metadata, xd->identifier);
	}

	if (xd->source) {
		tracker_sparql_builder_predicate (metadata, "dc:source");
		tracker_sparql_builder_object_unvalidated (metadata, xd->source);
	}

	if (xd->language) {
		tracker_sparql_builder_predicate (metadata, "dc:language");
		tracker_sparql_builder_object_unvalidated (metadata, xd->language);
	}

	if (xd->relation) {
		tracker_sparql_builder_predicate (metadata, "dc:relation");
		tracker_sparql_builder_object_unvalidated (metadata, xd->relation);
	}

	if (xd->coverage) {
		tracker_sparql_builder_predicate (metadata, "dc:coverage");
		tracker_sparql_builder_object_unvalidated (metadata, xd->coverage);
	}

	if (xd->address || xd->state || xd->country || xd->city ||
	    xd->gps_altitude || xd->gps_latitude || xd-> gps_longitude) {

		tracker_sparql_builder_predicate (metadata, "slo:location");

		tracker_sparql_builder_object_blank_open (metadata); /* GeoLocation */
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "slo:GeoLocation");

		if (xd->address || xd->state || xd->country || xd->city)  {
			gchar *addruri;

			addruri = tracker_sparql_get_uuid_urn ();

			tracker_sparql_builder_predicate (metadata, "slo:postalAddress");
			tracker_sparql_builder_object_iri (metadata, addruri);

			tracker_sparql_builder_insert_open (preupdate, NULL);
			if (graph) {
				tracker_sparql_builder_graph_open (preupdate, graph);
			}

			tracker_sparql_builder_subject_iri (preupdate, addruri);

			g_free (addruri);

			tracker_sparql_builder_predicate (preupdate, "a");
			tracker_sparql_builder_object (preupdate, "nco:PostalAddress");

			if (xd->address) {
			  tracker_sparql_builder_predicate (preupdate, "nco:streetAddress");
			  tracker_sparql_builder_object_unvalidated (preupdate, xd->address);
			}

			if (xd->state) {
			  tracker_sparql_builder_predicate (preupdate, "nco:region");
			  tracker_sparql_builder_object_unvalidated (preupdate, xd->state);
			}

			if (xd->city) {
			  tracker_sparql_builder_predicate (preupdate, "nco:locality");
			  tracker_sparql_builder_object_unvalidated (preupdate, xd->city);
			}

			if (xd->country) {
			  tracker_sparql_builder_predicate (preupdate, "nco:country");
			  tracker_sparql_builder_object_unvalidated (preupdate, xd->country);
			}

			if (graph) {
				tracker_sparql_builder_graph_close (preupdate);
			}
			tracker_sparql_builder_insert_close (preupdate);
		}

		if (xd->gps_altitude) {
			tracker_sparql_builder_predicate (metadata, "slo:altitude");
			tracker_sparql_builder_object_unvalidated (metadata, xd->gps_altitude);
		}

		if (xd->gps_latitude) {
			tracker_sparql_builder_predicate (metadata, "slo:latitude");
			tracker_sparql_builder_object_unvalidated (metadata, xd->gps_latitude);
		}

		if (xd->gps_longitude) {
			tracker_sparql_builder_predicate (metadata, "slo:longitude");
			tracker_sparql_builder_object_unvalidated (metadata, xd->gps_longitude);
		}

		tracker_sparql_builder_object_blank_close (metadata); /* GeoLocation */
	}

	if (xd->gps_direction) {
		tracker_sparql_builder_predicate (metadata, "nfo:heading");
		tracker_sparql_builder_object_unvalidated (metadata, xd->gps_direction);
	}

	if (ed->x_resolution) {
		gdouble value;

		value = ed->resolution_unit != 3 ? g_strtod (ed->x_resolution, NULL) : g_strtod (ed->x_resolution, NULL) * CM_TO_INCH;
		tracker_sparql_builder_predicate (metadata, "nfo:horizontalResolution");
		tracker_sparql_builder_object_double (metadata, value);
	}

	if (ed->y_resolution) {
		gdouble value;

		value = ed->resolution_unit != 3 ? g_strtod (ed->y_resolution, NULL) : g_strtod (ed->y_resolution, NULL) * CM_TO_INCH;
		tracker_sparql_builder_predicate (metadata, "nfo:verticalResolution");
		tracker_sparql_builder_object_double (metadata, value);
	}

	if (xd->regions) {
		tracker_xmp_apply_regions (preupdate, metadata, graph, xd);
	}

	for (i = 0; i < keywords->len; i++) {
		gchar *p, *escaped, *var;

		p = g_ptr_array_index (keywords, i);
		escaped = tracker_sparql_escape_string (p);
		var = g_strdup_printf ("tag%d", i + 1);

		/* ensure tag with specified label exists */
		tracker_sparql_builder_append (preupdate, "INSERT { ");

		if (graph) {
			tracker_sparql_builder_append (preupdate, "GRAPH <");
			tracker_sparql_builder_append (preupdate, graph);
			tracker_sparql_builder_append (preupdate, "> { ");
		}

		tracker_sparql_builder_append (preupdate,
		                               "_:tag a nao:Tag ; nao:prefLabel \"");
		tracker_sparql_builder_append (preupdate, escaped);
		tracker_sparql_builder_append (preupdate, "\"");

		if (graph) {
			tracker_sparql_builder_append (preupdate, " } ");
		}

		tracker_sparql_builder_append (preupdate, " }\n");
		tracker_sparql_builder_append (preupdate,
		                               "WHERE { FILTER (NOT EXISTS { "
		                               "?tag a nao:Tag ; nao:prefLabel \"");
		tracker_sparql_builder_append (preupdate, escaped);
		tracker_sparql_builder_append (preupdate,
		                               "\" }) }\n");

		/* associate file with tag */
		tracker_sparql_builder_predicate (metadata, "nao:hasTag");
		tracker_sparql_builder_object_variable (metadata, var);

		g_string_append_printf (where, "?%s a nao:Tag ; nao:prefLabel \"%s\" .\n", var, escaped);

		g_free (var);
		g_free (escaped);
		g_free (p);
	}
	g_ptr_array_free (keywords, TRUE);

	if (g_strcmp0(pd.software, "gnome-screenshot") == 0) {
		tracker_sparql_builder_predicate (metadata, "nie:isPartOf");
		tracker_sparql_builder_object (metadata, "nfo:image-category-screenshot");
	}

	tracker_exif_free (ed);
	tracker_xmp_free (xd);
	g_free (pd.creation_time);
}

static gboolean
guess_dlna_profile (gint          depth,
                    gint          width,
                    gint          height,
                    const gchar **dlna_profile,
                    const gchar **dlna_mimetype)
{
	const gchar *profile = NULL;

	if (dlna_profile) {
		*dlna_profile = NULL;
	}

	if (dlna_mimetype) {
		*dlna_mimetype = NULL;
	}

	profile = "PNG_LRG";

	if (profile) {
		if (dlna_profile) {
			*dlna_profile = profile;
		}

		if (dlna_mimetype) {
			*dlna_mimetype = "image/png";
		}

		return TRUE;
	}

	return FALSE;
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	goffset size;
	FILE *f;
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_ptr;
	png_bytepp row_pointers;
	guint row;
	png_uint_32 width, height;
	gint bit_depth, color_type;
	gint interlace_type, compression_type, filter_type;
	const gchar *dlna_profile, *dlna_mimetype, *graph;
	TrackerSparqlBuilder *preupdate, *metadata;
	gchar *filename, *uri;
	GString *where;
	GFile *file;

	file = tracker_extract_info_get_file (info);
	filename = g_file_get_path (file);
	size = tracker_file_get_size (filename);

	preupdate = tracker_extract_info_get_preupdate_builder (info);
	metadata = tracker_extract_info_get_metadata_builder (info);
	graph = tracker_extract_info_get_graph (info);

	if (size < 64) {
		return FALSE;
	}

	f = tracker_file_open (filename);
	g_free (filename);

	if (!f) {
		return FALSE;
	}

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
	                                  NULL,
	                                  NULL,
	                                  NULL);
	if (!png_ptr) {
		tracker_file_close (f, FALSE);
		return FALSE;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		tracker_file_close (f, FALSE);
		return FALSE;
	}

	end_ptr = png_create_info_struct (png_ptr);
	if (!end_ptr) {
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		tracker_file_close (f, FALSE);
		return FALSE;
	}

	if (setjmp (png_jmpbuf (png_ptr))) {
		png_destroy_read_struct (&png_ptr, &info_ptr, &end_ptr);
		tracker_file_close (f, FALSE);
		return FALSE;
	}

	png_init_io (png_ptr, f);
	png_read_info (png_ptr, info_ptr);

	if (!png_get_IHDR (png_ptr,
	                   info_ptr,
	                   &width,
	                   &height,
	                   &bit_depth,
	                   &color_type,
	                   &interlace_type,
	                   &compression_type,
	                   &filter_type)) {
		png_destroy_read_struct (&png_ptr, &info_ptr, &end_ptr);
		tracker_file_close (f, FALSE);
		return FALSE;
	}

	/* Read the image. FIXME We should be able to skip this step and
	 * just get the info from the end. This causes some errors atm.
	 */
	row_pointers = g_new0 (png_bytep, height);

	for (row = 0; row < height; row++) {
		row_pointers[row] = png_malloc (png_ptr,
		                                png_get_rowbytes (png_ptr,info_ptr));
	}

	png_read_image (png_ptr, row_pointers);

	for (row = 0; row < height; row++) {
		png_free (png_ptr, row_pointers[row]);
	}

	g_free (row_pointers);

	png_read_end (png_ptr, end_ptr);

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:Image");
	tracker_sparql_builder_object (metadata, "nmm:Photo");

	uri = g_file_get_uri (file);
	where = g_string_new ("");

	read_metadata (preupdate, metadata, where, png_ptr, info_ptr, end_ptr, uri, graph);
	tracker_extract_info_set_where_clause (info, where->str);
	g_string_free (where, TRUE);
	g_free (uri);

	tracker_sparql_builder_predicate (metadata, "nfo:width");
	tracker_sparql_builder_object_int64 (metadata, width);

	tracker_sparql_builder_predicate (metadata, "nfo:height");
	tracker_sparql_builder_object_int64 (metadata, height);

	if (guess_dlna_profile (bit_depth, width, height, &dlna_profile, &dlna_mimetype)) {
		tracker_sparql_builder_predicate (metadata, "nmm:dlnaProfile");
		tracker_sparql_builder_object_string (metadata, dlna_profile);
		tracker_sparql_builder_predicate (metadata, "nmm:dlnaMime");
		tracker_sparql_builder_object_string (metadata, dlna_mimetype);
	}

	png_destroy_read_struct (&png_ptr, &info_ptr, &end_ptr);
	tracker_file_close (f, FALSE);

	return TRUE;
}
