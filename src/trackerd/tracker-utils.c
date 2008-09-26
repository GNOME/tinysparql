/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Michal Pryc (Michal.Pryc@Sun.Com)
 * Copyright (C) 2008, Nokia
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#include "config.h"

#include <sys/statvfs.h>

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-config.h>

#include "tracker-utils.h"
#include "tracker-main.h"
#include "tracker-xesam-manager.h"

gchar *
tracker_get_radix_by_suffix (const gchar *str,
			     const gchar *suffix)
{
	g_return_val_if_fail (str, NULL);
	g_return_val_if_fail (suffix, NULL);

	if (g_str_has_suffix (str, suffix)) {
		return g_strndup (str, g_strrstr (str, suffix) - str);
	} else {
		return NULL;
	}
}

void
tracker_add_metadata_to_table (GHashTable  *meta_table,
			       const gchar *key,
			       const gchar *value)
{
	GSList *list;

	list = g_hash_table_lookup (meta_table, (gchar*) key);
	list = g_slist_prepend (list, (gchar*) value);
	g_hash_table_steal (meta_table, key);
	g_hash_table_insert (meta_table, (gchar*) key, list);
}

