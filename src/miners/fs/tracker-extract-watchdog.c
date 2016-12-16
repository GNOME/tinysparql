/*
 * Copyright (C) 2016, Red Hat Inc.
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

#include "config.h"

#include "tracker-extract-watchdog.h"

#include <libtracker-common/tracker-common.h>
#include <libtracker-miner/tracker-miner.h>

struct _TrackerExtractWatchdog {
	GObject parent_class;
	guint extractor_watchdog_id;
	guint timeout_id;
	gboolean initializing;
};

static void extract_watchdog_start (TrackerExtractWatchdog *watchdog,
				    gboolean                autostart);

G_DEFINE_TYPE (TrackerExtractWatchdog, tracker_extract_watchdog, G_TYPE_OBJECT)

static void
extract_watchdog_stop (TrackerExtractWatchdog *watchdog)
{
	if (watchdog->extractor_watchdog_id) {
		g_bus_unwatch_name (watchdog->extractor_watchdog_id);
		watchdog->extractor_watchdog_id = 0;
	}
}

static gboolean
extract_watchdog_name_vanished_timeout (gpointer user_data)
{
	TrackerExtractWatchdog *watchdog = user_data;

	watchdog->timeout_id = 0;
	extract_watchdog_start (watchdog, TRUE);

	return G_SOURCE_REMOVE;
}

static void
extract_watchdog_name_appeared (GDBusConnection *conn,
				const gchar     *name,
				const gchar     *name_owner,
				gpointer         user_data)
{
	TrackerExtractWatchdog *watchdog = user_data;

	if (watchdog->initializing)
		watchdog->initializing = FALSE;
}

static void
extract_watchdog_name_vanished (GDBusConnection *conn,
				const gchar     *name,
				gpointer         user_data)
{
	TrackerExtractWatchdog *watchdog = user_data;

	/* If connection is lost, there's not much we can startup */
	if (conn == NULL)
		return;

	/* We will ignore the first call after initialization, as we
	 * don't want to autostart tracker-extract in this case (useful
	 * for debugging purposes).
	 */
	if (watchdog->initializing) {
		watchdog->initializing = FALSE;
		return;
	}

	g_debug ("tracker-extract vanished, restarting after grace period.");

	/* Close the name watch, so we'll create another one that will
	 * autostart the service if it not already running.
	 */
	extract_watchdog_stop (watchdog);

	/* Give a period of grace before restarting, so we allow replacing
	 * from eg. a terminal.
	 */
	watchdog->timeout_id =
		g_timeout_add_seconds (1, extract_watchdog_name_vanished_timeout,
				       watchdog);
}

static void
extract_watchdog_start (TrackerExtractWatchdog *watchdog,
			gboolean                autostart)
{
	g_debug ("Setting up watch on tracker-extract (autostart: %s)",
		 autostart ? "yes" : "no");

	watchdog->extractor_watchdog_id =
		g_bus_watch_name (TRACKER_IPC_BUS,
				  TRACKER_MINER_DBUS_NAME_PREFIX "Extract",
				  (autostart ?
				   G_BUS_NAME_WATCHER_FLAGS_AUTO_START :
				   G_BUS_NAME_WATCHER_FLAGS_NONE),
				  extract_watchdog_name_appeared,
				  extract_watchdog_name_vanished,
				  watchdog, NULL);
}

static void
tracker_extract_watchdog_finalize (GObject *object)
{
	TrackerExtractWatchdog *watchdog = TRACKER_EXTRACT_WATCHDOG (object);

	extract_watchdog_stop (watchdog);

	if (watchdog->timeout_id) {
		g_source_remove (watchdog->timeout_id);
	}

	G_OBJECT_CLASS (tracker_extract_watchdog_parent_class)->finalize (object);
}

static void
tracker_extract_watchdog_class_init (TrackerExtractWatchdogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_extract_watchdog_finalize;
}

static void
tracker_extract_watchdog_init (TrackerExtractWatchdog *watchdog)
{
	watchdog->initializing = TRUE;
	extract_watchdog_start (watchdog, FALSE);
}

TrackerExtractWatchdog *
tracker_extract_watchdog_new (void)
{
	return g_object_new (TRACKER_TYPE_EXTRACT_WATCHDOG,
			     NULL);
}
