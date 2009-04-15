/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Jason Kivlighn (jkivlighn@gmail.com)
 * Copyright (C) 2007, Creative Commons (http://creativecommons.org)
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

#include <string.h>
#include <stdlib.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-db/tracker-db-index.h>
#include <libtracker-db/tracker-db-interface-sqlite.h>
#include <libtracker-db/tracker-db-index-manager.h>
#include <libtracker-db/tracker-db-manager.h>

#include "tracker-data-manager.h"
#include "tracker-data-query.h"
#include "tracker-data-search.h"
#include "tracker-query-tree.h"

#define DEFAULT_METADATA_MAX_HITS 1024

gint *
tracker_data_search_get_matches (const gchar       *search_string,
				 gint		   *result_length)
{
	GArray		    *hits;
	TrackerQueryTree    *tree;
	gint		    *result;
	gint                 i;

	g_return_val_if_fail (search_string != NULL, NULL);
	g_return_val_if_fail (result_length != NULL, NULL);

	tree = tracker_query_tree_new (search_string,
				       tracker_data_manager_get_config (),
				       tracker_data_manager_get_language ());
	hits = tracker_query_tree_get_hits (tree, 0, 0);

	result = g_new0 (gint, hits->len);
	*result_length = hits->len;

	for (i = 0; i < hits->len; i++) {
		TrackerDBIndexItemRank	rank;

		rank = g_array_index (hits, TrackerDBIndexItemRank, i);

		result[i] = rank.service_id;
	}

	return result;
}


