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

#include "tracker-main.h"
#include "tracker-xmp.h"

#include <libtracker-common/tracker-utils.h>

static void extract_pdf (const gchar *filename,
			 GHashTable  *metadata);

static TrackerExtractData data[] = {
	{ "application/pdf", extract_pdf },
	{ NULL, NULL }
};

static void
extract_pdf (const gchar *filename,
	     GHashTable  *metadata)
{
	PopplerDocument *document;
	gchar		*tmp;
	gchar		*title		= NULL;
	gchar		*author		= NULL;
	gchar		*subject	= NULL;
	gchar		*keywords	= NULL;
	gchar		*metadata_xml	= NULL;
	GTime		 creation_date;
	GError		*error		= NULL;

	g_type_init ();

	tmp = g_strconcat ("file://", filename, NULL);
	document = poppler_document_new_from_file (tmp, NULL, &error);
	g_free (tmp);

	if (document == NULL || error) {
		return;
	}

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
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Title"),
				     tracker_escape_metadata (title));
	}
	if (!tracker_is_empty_string (author)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Author"),
				     tracker_escape_metadata (author));
	}
	if (!tracker_is_empty_string (subject)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Subject"),
				     tracker_escape_metadata (subject));
	}
	if (!tracker_is_empty_string (keywords)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Keywords"),
				     tracker_escape_metadata (keywords));
	}

	if (creation_date > 0) {
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Created"),
				     tracker_date_to_string ((time_t) creation_date));
	}

	g_hash_table_insert (metadata,
			     g_strdup ("Doc:PageCount"),
			     tracker_escape_metadata_printf ("%d", poppler_document_get_n_pages (document)));

	if ( metadata_xml ) {
		tracker_read_xmp (metadata_xml, strlen (metadata_xml), metadata);
	}

	g_free (title);
	g_free (author);
	g_free (subject);
	g_free (keywords);
	g_free (metadata_xml);

	g_object_unref (document);
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
