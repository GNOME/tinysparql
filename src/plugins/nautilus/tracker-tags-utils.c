/*
 * Copyright (C) 2009  Debarshi Ray <debarshir@src.gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <string.h>
#include "tracker-tags-utils.h"

/* Copied from src/tracker-utils/tracker-tags.c */
static const gchar *
get_escaped_sparql_string (const gchar *str)
{
	GString *sparql;

	sparql = g_string_new ("");
        g_string_append_c (sparql, '"');

        while (*str != '\0') {
                gsize len = strcspn (str, "\t\n\r\"\\");
                g_string_append_len (sparql, str, len);
                str += len;
                switch (*str) {
                case '\t':
                        g_string_append (sparql, "\\t");
                        break;
                case '\n':
                        g_string_append (sparql, "\\n");
                        break;
                case '\r':
                        g_string_append (sparql, "\\r");
                        break;
                case '"':
                        g_string_append (sparql, "\\\"");
                        break;
                case '\\':
                        g_string_append (sparql, "\\\\");
                        break;
                default:
                        continue;
                }
                str++;
        }

        g_string_append_c (sparql, '"');

	return g_string_free (sparql, FALSE);
}

const gchar *
tracker_tags_utils_add_query (const gchar *tag_label)
{
	const gchar *query;
	const gchar *tag_label_escaped;

	tag_label_escaped = get_escaped_sparql_string (tag_label);
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
				 tag_label_escaped, tag_label_escaped);

	g_free ((gpointer) tag_label_escaped);
	return query;
}

const gchar *
tracker_tags_utils_remove_query (const gchar *tag_label)
{
	const gchar *query;
	const gchar *tag_label_escaped;

	tag_label_escaped = get_escaped_sparql_string (tag_label);
	query = g_strdup_printf ("DELETE { "
				 "  ?tag a nao:Tag "
				 "} "
				 "WHERE {"
				 "  ?tag nao:prefLabel %s "
				 "}",
				 tag_label_escaped);

	g_free ((gpointer) tag_label_escaped);
	return query;
}
