/* Tracker Extract - extracts embedded metadata from files
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifdef HAVE_POPPLER

#include <poppler.h>
#include <string.h>
#include <glib.h>

void tracker_extract_pdf (gchar *filename, GHashTable *metadata)
{
	PopplerDocument *document;
	gchar           *tmp;
	gchar           *title;
	gchar           *author;
	gchar           *subject;
	gchar           *keywords;
	GTime            creation_date;
	GError          *error = NULL;

	g_type_init ();
	tmp = g_strconcat ("file://", filename, NULL);
	document = poppler_document_new_from_file (tmp, NULL, &error);
	g_free (tmp);
	if (document == NULL || error)
		return;

	g_object_get (document,
		"title", &title,
		"author", &author,
		"subject", &subject,
		"keywords", &keywords,
		"creation-date", &creation_date,
		NULL);

	if (title && strlen (title))
		g_hash_table_insert (metadata, g_strdup ("Doc:Title"), g_strdup (title));
	if (author && strlen (author))
		g_hash_table_insert (metadata, g_strdup ("Doc:Author"), g_strdup (author));
	if (subject && strlen (subject))
		g_hash_table_insert (metadata, g_strdup ("Doc:Subject"), g_strdup (subject));
	if (keywords && strlen (keywords))
		g_hash_table_insert (metadata, g_strdup ("Doc:Keywords"), g_strdup (keywords));


#if 0
	GTimeVal creation_date_val = { creation_date, 0 };
	g_hash_table_insert (metadata, g_strdup ("Doc.Created"),
		g_time_val_to_iso8601 (creation_date_val));
#endif

	g_hash_table_insert (metadata, g_strdup ("Doc:PageCount"),
		g_strdup_printf ("%d", poppler_document_get_n_pages (document)));

	g_free (title);
	g_free (author);
	g_free (subject);
	g_free (keywords);
	g_object_unref (document);
}

#else
#warning "Not building PDF metadata extractor."
#endif  /* HAVE_POPPLER */


