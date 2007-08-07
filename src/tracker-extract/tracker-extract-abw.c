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

#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>

void tracker_extract_abw (gchar *filename, GHashTable *metadata)
{
	FILE *f;

   	if ((f = g_fopen (filename, "r"))) {
                gchar  *line;
                gsize  length;
                gssize read_char;

		line = NULL;
                length = 0;

                while ((read_char = getline (&line, &length, f)) != -1) {
			if (g_str_has_suffix (line, "</m>\n")) {
				line[read_char - 5] = '\0';
			}
			if (g_str_has_prefix (line, "<m key=\"dc.title\">")) {
				g_hash_table_insert (metadata,
                                                     g_strdup ("Doc:Title"), g_strdup (line + 18));
			}
			else if (g_str_has_prefix (line, "<m key=\"dc.subject\">")) {
				g_hash_table_insert (metadata,
                                                     g_strdup ("Doc:Subject"), g_strdup (line + 20));
			}
			else if (g_str_has_prefix (line, "<m key=\"dc.creator\">")) {
				g_hash_table_insert (metadata,
                                                     g_strdup ("Doc:Author"), g_strdup (line + 20));
			}
			else if (g_str_has_prefix (line, "<m key=\"abiword.keywords\">")) {
				g_hash_table_insert (metadata,
                                                     g_strdup ("Doc:Keywords"), g_strdup (line + 26));
			}
			else if (g_str_has_prefix (line, "<m key=\"dc.description\">")) {
				g_hash_table_insert (metadata,
                                                     g_strdup ("Doc:Comments"), g_strdup (line + 24));
			}

			g_free (line);
			line = NULL;
                        length = 0;
		}

                if (line) {
                        g_free (line);
                }

                fclose (f);
        }
}

