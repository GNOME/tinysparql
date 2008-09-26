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

#include "tracker-extract.h"
#include "tracker-xmp.h"

#include <libtracker-common/tracker-utils.h>

static void extract_pdf (const gchar *filename,
			 GHashTable  *metadata);

static TrackerExtractorData data[] = {
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
				     g_strdup (title));
	}
	if (!tracker_is_empty_string (author)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Author"),
				     g_strdup (author));
	}
	if (!tracker_is_empty_string (subject)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Subject"),
				     g_strdup (subject));
	}
	if (!tracker_is_empty_string (keywords)) {
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Keywords"),
				     g_strdup (keywords));
	}

	g_hash_table_insert (metadata,
			     g_strdup ("Doc:PageCount"),
			     g_strdup_printf ("%d", poppler_document_get_n_pages (document)));

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

TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}
