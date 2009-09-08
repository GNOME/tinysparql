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

#include "config.h"

#include <string.h>
#include <poppler.h>

#include <glib.h>

#include <libtracker-common/tracker-statement-list.h>

#include "tracker-main.h"
#include "tracker-xmp.h"

#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-type-utils.h>

#define NMM_PREFIX TRACKER_NMM_PREFIX
#define DC_PREFIX TRACKER_DC_PREFIX
#define NIE_PREFIX TRACKER_NIE_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX
#define RDF_PREFIX TRACKER_RDF_PREFIX

static void extract_pdf (const gchar *uri,
			 TrackerSparqlBuilder   *metadata);

static TrackerExtractData data[] = {
	{ "application/pdf", extract_pdf },
	{ NULL, NULL }
};

typedef struct {
	gchar *author, *title, *creation_date, *subject;
} PdfData;

typedef struct {
	gchar *creator, *title, *date;
} PdfNeedsMergeData;


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


static void
extract_pdf (const gchar *uri,
	     TrackerSparqlBuilder  *metadata)
{
	PdfData pdf_data = { 0 };
	PdfNeedsMergeData merge_data = { 0 };
	TrackerXmpData xmp_data = { 0 };
	PopplerDocument *document;
	gchar		*author, *title, *subject;
	gchar		*keywords	= NULL;
	gchar		*metadata_xml	= NULL;
	GTime		 creation_date;
	GError		*error		= NULL;
	gchar		*filename = g_filename_from_uri (uri, NULL, NULL);

	g_type_init ();

	document = poppler_document_new_from_file (filename, NULL, &error);

	if (document == NULL || error) {
		g_free (filename);
		return;
	}

	tracker_statement_list_insert (metadata, uri, 
	                               RDF_PREFIX "type", 
	                               NFO_PREFIX "PaginatedTextDocument");

	g_object_get (document,
	              "title", &title,
	              "author", &author,
	              "subject", &subject,
	              "keywords", &keywords,
	              "creation-date", &creation_date,
	              NULL);

	/* metadata property not present in older poppler versions */
	if (g_object_class_find_property (G_OBJECT_GET_CLASS (document), "metadata")) {
		g_object_get (document, "metadata", &metadata_xml, NULL);
	}

	if (!tracker_is_empty_string (title)) {
		pdf_data.title = title;
	}

	if (!tracker_is_empty_string (author)) {
		pdf_data.author = author;
	}

	if (!tracker_is_empty_string (subject)) {
		pdf_data.subject = subject;
	}

	if (creation_date > 0) {
		pdf_data.creation_date = tracker_date_to_string ((time_t) creation_date);
	}

	if (metadata_xml) {

		tracker_read_xmp (metadata_xml, strlen (metadata_xml), uri, &xmp_data);

		merge_data.creator =  tracker_coalesce (2, pdf_data.author,
		                                        xmp_data.creator);

		merge_data.date =  tracker_coalesce (3, pdf_data.creation_date,
		                                     xmp_data.date,
		                                     xmp_data.DateTimeOriginal);

		merge_data.title =  tracker_coalesce (2, pdf_data.title,
		                                        xmp_data.title);


		if (pdf_data.subject) {
			insert_keywords (metadata, uri, pdf_data.subject);
			g_free (pdf_data.subject);
		}

		if (merge_data.creator) {
			tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", merge_data.creator);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "creator", ":");
			g_free (merge_data.creator);
		}

		if (merge_data.date) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "contentCreated", merge_data.date);
			g_free (merge_data.date);
		}

		if (merge_data.title) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "title", merge_data.title);
			g_free (merge_data.title);
		}

		if (xmp_data.keywords) {
			insert_keywords (metadata, uri, xmp_data.keywords);
			g_free (xmp_data.keywords);
		}

		if (xmp_data.subject) {
			insert_keywords (metadata, uri, xmp_data.subject);
			g_free (xmp_data.subject);
		}

		if (xmp_data.publisher) {
			tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", xmp_data.publisher);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "publisher", ":");
			g_free (xmp_data.publisher);
		}

		if (xmp_data.type) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "type", xmp_data.type);
			g_free (xmp_data.type);
		}

		if (xmp_data.format) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "format", xmp_data.format);
			g_free (xmp_data.format);
		}

		if (xmp_data.identifier) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "identifier", xmp_data.identifier);
			g_free (xmp_data.identifier);
		}

		if (xmp_data.source) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "source", xmp_data.source);
			g_free (xmp_data.source);
		}

		if (xmp_data.language) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "language", xmp_data.language);
			g_free (xmp_data.language);
		}

		if (xmp_data.relation) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "relation", xmp_data.relation);
			g_free (xmp_data.relation);
		}

		if (xmp_data.coverage) {
			tracker_statement_list_insert (metadata, uri, DC_PREFIX "coverage", xmp_data.coverage);
			g_free (xmp_data.coverage);
		}

		if (xmp_data.license) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "license", xmp_data.license);
			g_free (xmp_data.license);
		}

		if (xmp_data.Make || xmp_data.Model) {
			gchar *final_camera = tracker_merge (" ", 2, xmp_data.Make, xmp_data.Model); 
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "camera", final_camera);
			g_free (final_camera);
		}

		if (xmp_data.Orientation) {
			tracker_statement_list_insert (metadata, uri, NFO_PREFIX "orientation", xmp_data.Orientation);
			g_free (xmp_data.Orientation);
		}

		if (xmp_data.rights) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "copyright", xmp_data.rights);
			g_free (xmp_data.rights);
		}

		if (xmp_data.WhiteBalance) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "whiteBalance", xmp_data.WhiteBalance);
			g_free (xmp_data.WhiteBalance);
		}

		if (xmp_data.FNumber) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "fnumber", xmp_data.FNumber);
			g_free (xmp_data.FNumber);
		}

		if (xmp_data.Flash) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "flash", xmp_data.Flash);
			g_free (xmp_data.Flash);
		}

		if (xmp_data.FocalLength) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "focalLength", xmp_data.FocalLength);
			g_free (xmp_data.FocalLength);
		}

		if (xmp_data.Artist || xmp_data.contributor) {
			gchar *final_artist =  tracker_coalesce (2, xmp_data.Artist, xmp_data.contributor);
			tracker_statement_list_insert (metadata, ":", RDF_PREFIX "type", NCO_PREFIX "Contact");
			tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", final_artist);
			tracker_statement_list_insert (metadata, uri, NCO_PREFIX "contributor", ":");
			g_free (final_artist);
		}

		if (xmp_data.ExposureTime) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "exposureTime", xmp_data.ExposureTime);
			g_free (xmp_data.ExposureTime);
		}

		if (xmp_data.ISOSpeedRatings) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "isoSpeed", xmp_data.ISOSpeedRatings);
			g_free (xmp_data.ISOSpeedRatings);
		}

		if (xmp_data.description) {
			tracker_statement_list_insert (metadata, uri, NIE_PREFIX "description", xmp_data.description);
			g_free (xmp_data.description);
		}

		if (xmp_data.MeteringMode) {
			tracker_statement_list_insert (metadata, uri, NMM_PREFIX "meteringMode", xmp_data.MeteringMode);
			g_free (xmp_data.MeteringMode);
		}

	}

	if (!tracker_is_empty_string (keywords)) {
		char *lasts, *keyw;

		for (keyw = strtok_r (keywords, ",;", &lasts); keyw; 
		     keyw = strtok_r (NULL, ",;", &lasts)) {
			tracker_statement_list_insert (metadata,
					  uri, NIE_PREFIX "keyword",
					  (const gchar*) keyw);
		}
	}

	tracker_statement_list_insert_with_int (metadata, uri,
					   NFO_PREFIX "pageCount",
					   poppler_document_get_n_pages (document));

	g_free (keywords);
	g_free (metadata_xml);
	g_object_unref (document);
	g_free (filename);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
