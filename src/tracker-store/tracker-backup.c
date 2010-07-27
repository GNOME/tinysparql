/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <glib-object.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-dbus.h>

#include <libtracker-data/tracker-data-backup.h>
#include <libtracker-data/tracker-data-update.h>

#include "tracker-dbus.h"
#include "tracker-backup.h"
#include "tracker-store.h"
#include "tracker-resources.h"

typedef struct {
	DBusGMethodInvocation *context;
	guint request_id;
	gchar *journal_uri;
} TrackerDBusMethodInfo;

G_DEFINE_TYPE (TrackerBackup, tracker_backup, G_TYPE_OBJECT)

static void
tracker_backup_class_init (TrackerBackupClass *klass)
{
}

static void
tracker_backup_init (TrackerBackup *backup)
{
}

TrackerBackup *
tracker_backup_new (void)
{
	return g_object_new (TRACKER_TYPE_BACKUP, NULL);
}

static void
backup_callback (GError *error, gpointer user_data)
{
	TrackerDBusMethodInfo *info = user_data;

	if (error) {
		tracker_dbus_request_failed (info->request_id,
		                             info->context,
		                             &error,
		                             NULL);
		dbus_g_method_return_error (info->context, error);
		return;
	}

	dbus_g_method_return (info->context);

	tracker_dbus_request_success (info->request_id,
	                              info->context);
}

void
tracker_backup_save (TrackerBackup          *object,
                     const gchar            *destination_uri,
                     DBusGMethodInvocation  *context,
                     GError                **error)
{
	guint request_id;
	TrackerDBusMethodInfo *info;
	GFile *destination;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
	                          context,
	                          "D-Bus request to save backup into '%s'",
	                          destination_uri);

	info = g_new0 (TrackerDBusMethodInfo, 1);

	info->request_id = request_id;
	info->context = context;
	destination = g_file_new_for_uri (destination_uri);

	tracker_data_backup_save (destination,
	                          backup_callback,
	                          info, 
	                          (GDestroyNotify) g_free);

	g_object_unref (destination);
}

static gboolean
backup_idle (gpointer user_data)
{
	TrackerDBusMethodInfo *info = user_data;
	GFile *journal;
	TrackerStatus *notifier;
	TrackerBusyCallback busy_callback;
	gpointer busy_user_data;

	journal = g_file_new_for_uri (info->journal_uri);

	tracker_store_set_active (FALSE);

	notifier = TRACKER_STATUS (tracker_dbus_get_object (TRACKER_TYPE_STATUS));

	busy_callback = tracker_status_get_callback (notifier, 
	                                            &busy_user_data);

	g_free (info->journal_uri);

	tracker_data_backup_restore (journal,
	                             backup_callback,
	                             info, 
	                             (GDestroyNotify) g_free,
	                             NULL,
	                             busy_callback,
	                             busy_user_data);

	tracker_store_set_active (TRUE);

	g_object_unref (journal);

	return FALSE;
}

void
tracker_backup_restore (TrackerBackup          *object,
                        const gchar            *journal_uri,
                        DBusGMethodInvocation  *context,
                        GError                **error)
{
	guint request_id;
	TrackerDBusMethodInfo *info;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
	                          context,
	                          "D-Bus request to restore backup from '%s'",
	                          journal_uri);

	info = g_new0 (TrackerDBusMethodInfo, 1);
	info->request_id = request_id;
	info->context = context;
	info->journal_uri = g_strdup (journal_uri);

	g_idle_add (backup_idle, info);

}

