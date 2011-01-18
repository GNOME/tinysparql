/*
 * Copyright (C) 2010 Nokia <ivan.frade@nokia.com>
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
#include <string.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-locale.h>
#include <libtracker-data/tracker-data-manager.h>
#include <libtracker-data/tracker-db-dbus.h>

#include "tracker-store.h"
#include "tracker-events.h"
#include "tracker-locale-change.h"

typedef struct {
	gpointer resources;
	TrackerNotifyClassGetter getter;
} TrackerLocaleChangeContext;

/* Private */
static gpointer locale_notification_id;
static gboolean locale_change_notified;

static void
locale_change_process_cb (GObject      *source,
                          GAsyncResult *res,
                          gpointer      user_data)
{
	TrackerStatus *notifier;
	TrackerBusyCallback busy_callback;
	gpointer busy_user_data;
	GDestroyNotify busy_destroy_notify;
	TrackerLocaleChangeContext *ctxt = user_data;

	notifier = TRACKER_STATUS (tracker_dbus_get_object (TRACKER_TYPE_STATUS));

	busy_callback = tracker_status_get_callback (notifier,
	                                             &busy_user_data,
	                                             &busy_destroy_notify);

	g_message ("Processing locale change...");
	/* Reload! This will regenerate indexes with the new locale */
	tracker_data_manager_reload (busy_callback,
	                             busy_user_data,
	                             "Changing locale");

	busy_destroy_notify (busy_user_data);

	if (ctxt->resources) {
		tracker_events_init (ctxt->getter);
		tracker_resources_enable_signals (ctxt->resources);
		g_object_unref (ctxt->resources);
	}
	g_free (ctxt);

	tracker_store_resume ();

	locale_change_notified = FALSE;
}

static gboolean
locale_change_process_idle_cb (gpointer data)
{
	TrackerLocaleChangeContext *ctxt;

	ctxt = g_new0 (TrackerLocaleChangeContext, 1);
	ctxt->resources = tracker_dbus_get_object (TRACKER_TYPE_RESOURCES);
	if (ctxt->resources) {
		g_object_ref (ctxt->resources);
		tracker_resources_disable_signals (ctxt->resources);
		ctxt->getter = tracker_events_get_class_getter ();
		tracker_events_shutdown ();
	}

	/* Note: Right now, the passed callback may be called instantly and not
	 * in an idle. */
	g_message ("Setting tracker-store as inactive...");
	tracker_store_pause (locale_change_process_cb, ctxt);

	return FALSE;
}

static void
locale_notify_cb (TrackerLocaleID id,
                  gpointer        user_data)
{
	if (locale_change_notified) {
		g_message ("Locale change was already notified, not doing it again");
	} else {
		locale_change_notified = TRUE;
		/* Set an idle callback to process the locale change.
		 * NOTE: We cannot process it right here because we will be
		 * closing and opening new connections to the DB while doing it,
		 * and the DB connections are also part of the subscriber list
		 * for locale changes, so we may end up waiting to acquire an
		 * already locked mutex.
		 */
		g_message ("Locale change notified, preparing to rebuild indexes...");
		g_idle_add (locale_change_process_idle_cb, NULL);
	}
}

void
tracker_locale_change_initialize_subscription (void)
{
	gchar *collation_locale;

	collation_locale = tracker_locale_get (TRACKER_LOCALE_COLLATE);

	g_debug ("Initial collation locale is '%s', subscribing for updates...",
	         collation_locale);

	locale_notification_id = tracker_locale_notify_add (TRACKER_LOCALE_COLLATE,
	                                                    locale_notify_cb,
	                                                    NULL,
	                                                    NULL);
}

void
tracker_locale_change_shutdown_subscription (void)
{
	tracker_locale_notify_remove (locale_notification_id);
}
