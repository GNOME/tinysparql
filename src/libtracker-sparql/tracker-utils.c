/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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
/**
 * SECTION: tracker-misc
 * @short_description: miscellaneous functionality
 * @title: Utility Functions
 * @stability: Stable
 * @include: tracker-sparql.h
 *
 * Collection of Tracker utility functions.
 */

#include "config.h"

#include "tracker-utils.h"

#include <libtracker-sparql/core/tracker-uuid.h>

/**
 * tracker_sparql_escape_string:
 * @literal: a string to escape
 *
 * Escapes @literal so it is suitable for insertion in
 * SPARQL queries as string literals. Manual construction
 * of query strings based user input is best avoided at
 * all cost, use of #TrackerSparqlStatement is recommended
 * instead.
 *
 * Returns: (transfer full): the escaped string
 **/
gchar *
tracker_sparql_escape_string (const gchar* literal)
{
	GString *str;
	const gchar *p;

	str = g_string_new (NULL);
	p = literal;

	while (*p != '\0') {
		size_t len;

		len = strcspn (p, "\t\n\r\b\f\'\"\\");
		g_string_append_len (str, p, len);
		p += len;

		switch (*p) {
		case '\t':
			g_string_append (str, "\\t");
			break;
		case '\n':
			g_string_append (str, "\\n");
			break;
		case '\r':
			g_string_append (str, "\\r");
			break;
		case '\b':
			g_string_append (str, "\\b");
			break;
		case '\f':
			g_string_append (str, "\\f");
			break;
		case '"':
			g_string_append (str, "\\\"");
			break;
		case '\'':
			g_string_append (str, "\\'");
			break;
		case '\\':
			g_string_append (str, "\\\\");
			break;
		default:
			continue;
		}

		p++;
	}

	return g_string_free (str, FALSE);
}

/**
 * tracker_sparql_get_uuid_urn:
 *
 * Creates a fresh UUID-based URN.
 *
 * Returns: (transfer full): A newly generated UUID URN.
 **/
gchar *
tracker_sparql_get_uuid_urn (void)
{
	return tracker_generate_uuid ("urn:uuid");
}

/**
 * tracker_sparql_get_ontology_nepomuk:
 *
 * Returns a path to the built-in Nepomuk ontologies.
 *
 * Returns: (transfer full): a #GFile instance.
 */
GFile *
tracker_sparql_get_ontology_nepomuk (void)
{
	return g_file_new_for_path (SHAREDIR "/tracker3/ontologies/nepomuk");
}
