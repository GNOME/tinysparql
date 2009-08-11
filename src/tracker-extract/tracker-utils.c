/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include <glib.h>

#include "tracker-utils.h"
#include "tracker-escape.h"

void
tracker_utils_default_check_filename (GHashTable  *metadata,
				      gchar       *key,
				      const gchar *filename)
{
	g_return_if_fail (key != NULL);
	g_return_if_fail (filename != NULL);

	if (!g_hash_table_lookup (metadata, key)) {
		gchar  *name = g_filename_display_basename (filename);
		gchar  *suffix = NULL;

		suffix = g_strrstr (name, ".");

		if (suffix) {
			*suffix = '\0';
		}
		
		g_strdelimit (name, "._", ' ');
		
		g_hash_table_insert (metadata,
				     g_strdup (key),
				     tracker_escape_metadata (name));
		g_free (name);
	}	
}
