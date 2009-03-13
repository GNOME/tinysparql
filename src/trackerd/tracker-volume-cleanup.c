/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include <stdlib.h>

#include <gio/gio.h>

#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-data/tracker-data-update.h>
#include <libtracker-common/tracker-thumbnailer.h>

#include "tracker-volume-cleanup.h"

/* Deals with cleaning up resident data after longer timeouts (days,
 * sessions).
 */
#define SECONDS_PER_DAY (60 * 60 * 24)

typedef struct {
	guint timeout_id;
} TrackerCleanupPrivate;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void
private_free (gpointer data)
{
	TrackerCleanupPrivate *private;

	private = data;

	if (private->timeout_id) {
		g_source_remove (private->timeout_id);
	}

	g_free (private);
}

static gboolean
check_for_volumes_to_cleanup (gpointer user_data)
{
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;

	g_message ("Checking for stale volumes in the database...");

	iface = tracker_db_manager_get_db_interface (TRACKER_DB_COMMON);

	/* The stored statements of volume management have the "every
	 * one that is older than three days since their last unmount
	 * time" logic embedded in the SQL statements. Take a look at
	 * sqlite-stored-procs.sql.
	 */
	result_set = 
		tracker_db_interface_execute_procedure (iface, NULL,
							"GetVolumesToClean",
							NULL);
	
	if (result_set) {
		gboolean is_valid = TRUE;

		while (is_valid) {
			GValue       value = { 0, };
			const gchar *mount_point_path;
			gint         volume_id;

			_tracker_db_result_set_get_value (result_set, 0, &value);

			mount_point_path = g_value_get_string (&value);

			/* Add cleanup items here */
			if (mount_point_path) {
				GFile *file;
				gchar *mount_point_uri;

				file = g_file_new_for_path (mount_point_path);
				mount_point_uri = g_file_get_uri (file);

				g_message ("  Cleaning up volumes with mount point:'%s'", 
					   mount_point_uri);
				tracker_thumbnailer_cleanup (mount_point_uri);

				g_free (mount_point_uri);
				g_object_unref (file);
			}

			g_value_unset (&value);

			/* Reset volume date */
			_tracker_db_result_set_get_value (result_set, 1, &value);

			volume_id = g_value_get_int (&value); 
			tracker_data_update_reset_volume (volume_id);

			g_value_unset (&value);

			is_valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	} else {
		g_message ("  No volumes to clean up");
	}

	return TRUE;
}

void 
tracker_volume_cleanup_init (void)
{
	TrackerCleanupPrivate *private;
	gchar                 *str;

	private = g_new0 (TrackerCleanupPrivate, 1);

	g_static_private_set (&private_key,
			      private,
			      private_free);

	check_for_volumes_to_cleanup (private);

	/* We use +1 because we want to make sure we trigger this
	 * into the 4th day, not on the 3rd day. This guarantees at
	 * least 3 days worth.
	 */
	str = tracker_seconds_to_string (SECONDS_PER_DAY + 1, FALSE);
	g_message ("Scheduling volume check in %s", str);
	g_free (str);

	private->timeout_id =
		g_timeout_add_seconds (SECONDS_PER_DAY + 1, 
				       check_for_volumes_to_cleanup,
				       private);
}

void 
tracker_volume_cleanup_shutdown (void)
{
	g_static_private_set (&private_key, NULL, NULL);
}
