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

#include <stdio.h>
#include <string.h>
#include <glib.h>

void
tracker_extract_imagemagick (gchar *filename, GHashTable *metadata)
{
	gchar         *argv[6];
	gchar         *identify;
	gchar        **lines;

	argv[0] = g_strdup ("identify");
	argv[1] = g_strdup ("-format");
	argv[2] = g_strdup ("%w;\\n%h;\\n%c;\\n");
	argv[3] = g_strdup ("-ping");
	argv[4] = g_strdup (filename);
	argv[5] = NULL;

	if(g_spawn_sync (NULL,
	                 argv,
	                 NULL,
	                 G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
	                 NULL,
	                 NULL,
	                 &identify,
	                 NULL,
	                 NULL,
	                 NULL)) {

		lines = g_strsplit (identify, ";\n", 4);
		g_hash_table_insert (metadata, g_strdup ("Image:Width"), g_strdup (lines[0]));
		g_hash_table_insert (metadata, g_strdup ("Image:Height"), g_strdup (lines[1]));
		g_hash_table_insert (metadata, g_strdup ("Image:Comments"), g_strdup (g_strescape (lines[2], "")));
	}
}

