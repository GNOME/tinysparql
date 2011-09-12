/*
 * Copyright (C) 2009, Debarshi Ray <debarshir@src.gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <string.h>

#include <libnautilus-extension/nautilus-file-info.h>

#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-tags-utils.h"

/* Copied from src/libtracker-common/tracker-utils.c */
inline gboolean
tracker_is_empty_string (const char *str)
{
	return str == NULL || str[0] == '\0';
}

/* Copied from src/libtracker-common/tracker-type-utils.c */
gchar **
tracker_glist_to_string_list_for_nautilus_files (GList *list)
{
	GList  *l;
	gchar **strv;
	gint    i;

	strv = g_new0 (gchar *, g_list_length (list) + 1);

	for (l = list, i = 0; l; l = l->next) {
		if (!l->data) {
			continue;
		}

		strv[i++] = nautilus_file_info_get_uri (NAUTILUS_FILE_INFO (l->data));
	}

	strv[i] = NULL;

	return strv;
}

/* Copied from src/tracker-utils/tracker-tags.c */
gchar *
tracker_tags_get_filter_string (GStrv        files,
                                const gchar *tag)
{
	GString *filter;
	gint i, len;

	if (!files) {
		return NULL;
	}

	len = g_strv_length (files);

	if (len < 1) {
		return NULL;
	}

	filter = g_string_new ("");

	g_string_append_printf (filter, "FILTER (");

	if (tag) {
		g_string_append (filter, "(");
	}

	for (i = 0; i < len; i++) {
		g_string_append_printf (filter, "?f = \"%s\"", files[i]);

		if (i < len - 1) {
			g_string_append (filter, " || ");
		}
	}

	if (tag) {
		g_string_append_printf (filter, ") && ?t = <%s>", tag);
	}

	g_string_append (filter, ")");

	return g_string_free (filter, FALSE);
}

gchar *
tracker_tags_escape_sparql_string (const gchar *str)
{
	gchar *escaped, *retval;

	escaped = tracker_sparql_escape_string (str);
	retval = g_strdup_printf ("\"%s\"", escaped);
	g_free (escaped);

	return retval;
}

gchar *
tracker_tags_add_query (const gchar *tag_label)
{
	gchar *query;
	gchar *tag_label_escaped;

	tag_label_escaped = tracker_tags_escape_sparql_string (tag_label);
	query = g_strdup_printf ("INSERT { "
	                         "  _:tag a nao:Tag ;"
	                         "  nao:prefLabel %s ."
	                         "} "
	                         "WHERE {"
	                         "  OPTIONAL {"
	                         "     ?tag a nao:Tag ;"
	                         "     nao:prefLabel %s"
	                         "  } ."
	                         "  FILTER (!bound(?tag)) "
	                         "}",
	                         tag_label_escaped,
	                         tag_label_escaped);
	g_free (tag_label_escaped);

	return query;
}

gchar *
tracker_tags_remove_query (const gchar *tag_label)
{
	gchar *query;
	gchar *tag_label_escaped;

	tag_label_escaped = tracker_tags_escape_sparql_string (tag_label);
	query = g_strdup_printf ("DELETE { "
	                         "  ?tag a rdfs:Resource "
	                         "} "
	                         "WHERE {"
	                         "  ?tag a nao:Tag ;"
	                         "  nao:prefLabel %s "
	                         "}",
	                         tag_label_escaped);
	g_free (tag_label_escaped);

	return query;
}
