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

#include <libtracker-common/tracker-date-time.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-data-query.h>

#include "tracker-thumbnailer.h"
#include "tracker-volume-cleanup.h"

/* Deals with cleaning up resident data after longer timeouts (days,
 * sessions).
 */
#define SECONDS_PER_DAY (60 * 60 * 24)
#define THREE_DAYS_OF_SECONDS (SECONDS_PER_DAY * 3)

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
#if 0
	TrackerDBInterface *iface;
	TrackerDBResultSet *result_set;
	gchar *query;
	time_t three_days_ago;
	gchar *three_days_ago_as_string;

	three_days_ago = (time (NULL) - THREE_DAYS_OF_SECONDS);
	three_days_ago_as_string = tracker_date_to_string (three_days_ago);

	g_message ("Checking for stale volumes in the database...");

	iface = tracker_db_manager_get_db_interface ();

	query = g_strdup_printf ("SELECT ?o ?m WHERE { "
	                         "?o a tracker:Volume ; "
	                         "tracker:mountPoint ?m ; "
	                         "tracker:unmountDate ?z ; "
	                         "tracker:isMounted false . "
	                         "FILTER (?z < \"%s\") }",
	                         three_days_ago_as_string);

	result_set = tracker_data_query_sparql (query, NULL);

	if (result_set) {
		gboolean is_valid = TRUE;

		while (is_valid) {
			GValue       value = { 0, };
			const gchar *mount_point_uri;
			const gchar *volume_uri;

			_tracker_db_result_set_get_value (result_set, 1, &value);

			mount_point_uri = g_value_get_string (&value);

			/* mount_point_uri is indeed different than volume_uri,
			 * volume_uri is the datasource URN built using the UDI,
			 * mount_point_uri is like <file:///media/USBStick> */

			if (mount_point_uri) {
				g_message ("  Cleaning up volumes with mount point:'%s'",
				           mount_point_uri);

				/* Add cleanup items here */
				tracker_thumbnailer_cleanup (mount_point_uri);
			}

			g_value_unset (&value);

			/* Reset volume date */
			_tracker_db_result_set_get_value (result_set, 0, &value);

			volume_uri = g_value_get_string (&value);
			tracker_data_update_reset_volume (volume_uri);

			g_value_unset (&value);

			is_valid = tracker_db_result_set_iter_next (result_set);
		}

		g_object_unref (result_set);
	} else {
		g_message ("  No volumes to clean up");
	}

	g_free (three_days_ago_as_string);
	g_free (query);
#endif

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
