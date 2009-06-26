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

#define NIE_PREFIX TRACKER_NIE_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

static void extract_pdf (const gchar *uri,
			 TrackerSparqlBuilder   *metadata);

static TrackerExtractData data[] = {
	{ "application/pdf", extract_pdf },
	{ NULL, NULL }
};

static void
extract_pdf (const gchar *uri,
	     TrackerSparqlBuilder  *metadata)
{
	PopplerDocument *document;
	gchar		*title		= NULL;
	gchar		*author		= NULL;
	gchar		*subject	= NULL;
	gchar		*keywords	= NULL;
	gchar		*metadata_xml	= NULL;
	GTime		 creation_date;
	GError		*error		= NULL;
	gchar           *filename = g_filename_from_uri (uri, NULL, NULL);

	g_type_init ();

	document = poppler_document_new_from_file (filename, NULL, &error);

	if (document == NULL || error) {
		g_free (filename);
		return;
	}

	tracker_statement_list_insert (metadata, uri, 
	                          RDF_TYPE, 
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
		tracker_statement_list_insert (metadata, uri,
					  NIE_PREFIX "title",
					  title);
	}
	if (!tracker_is_empty_string (author)) {

		tracker_statement_list_insert (metadata, ":", RDF_TYPE, NCO_PREFIX "Contact");
		tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", author);
		tracker_statement_list_insert (metadata, uri, NCO_PREFIX "creator", ":");

	}
	if (!tracker_is_empty_string (subject)) {
		tracker_statement_list_insert (metadata, uri,
					  NIE_PREFIX "subject",
					  subject);
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

	if (creation_date > 0) {
		gchar *date_string = tracker_date_to_string ((time_t) creation_date);
		tracker_statement_list_insert (metadata, uri,
				          NIE_PREFIX "created",
				          date_string);
		g_free (date_string);
	}

	tracker_statement_list_insert_with_int (metadata, uri,
					   NFO_PREFIX "pageCount",
					   poppler_document_get_n_pages (document));

	if ( metadata_xml ) {
		tracker_read_xmp (metadata_xml, strlen (metadata_xml), uri, metadata);
	}

	g_free (title);
	g_free (author);
	g_free (subject);
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
