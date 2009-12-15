/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <glib-object.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-dbus.h>

#include <libtracker-data/tracker-data-backup.h>
#include <libtracker-data/tracker-data-update.h>

#include "tracker-dbus.h"
#include "tracker-backup.h"
#include "tracker-store.h"

typedef struct {
	DBusGMethodInvocation *context;
	guint request_id;
	gboolean play_journal;
	GFile *destination, *journal;
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
destroy_method_info (gpointer user_data)
{
	TrackerDBusMethodInfo *info = user_data;

	if (info->destination) {
		g_object_unref (info->destination);
	}

	if (info->journal) {
		g_object_unref (info->journal);
	}

	g_free (info);
}

static void
backup_callback (GError *error, gpointer user_data)
{
	TrackerDBusMethodInfo *info = user_data;

	if (error) {
		tracker_dbus_request_failed (info->request_id,
		                             &error,
		                             NULL);
		dbus_g_method_return_error (info->context, error);
		return;
	}

	if (info->play_journal) {
		tracker_store_play_journal ();
	}

	dbus_g_method_return (info->context);

	tracker_dbus_request_success (info->request_id);
}

static void
on_batch_commit (gpointer user_data)
{
	TrackerDBusMethodInfo *info = user_data;

	/* At this point no transactions are left open, we can now start the
	 * sqlite3_backup API, which will run itself as a GSource within the
	 * mainloop after it got initialized (which will reopen the mainloop) */

	tracker_data_backup_save (info->destination, info->journal,
	                          backup_callback,
	                          info, destroy_method_info);
}

void
tracker_backup_save (TrackerBackup          *object,
                     const gchar            *destination_uri,
                     const gchar            *journal_uri,
                     DBusGMethodInvocation  *context,
                     GError                **error)
{
	guint request_id;
	TrackerDBusMethodInfo *info;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
	                          "D-Bus request to save backup into '%s'",
	                          destination_uri);

	info = g_new0 (TrackerDBusMethodInfo, 1);

	info->request_id = request_id;
	info->context = context;
	info->play_journal = FALSE;
	info->destination = g_file_new_for_uri (destination_uri);
	info->journal = g_file_new_for_uri (journal_uri);

	/* The sqlite3_backup API apparently doesn't much like open transactions,
	 * this queue_commit will first call the currently open transaction
	 * of the open batch (if any), and then in the callback we'll idd
	 * continue with making the backup itself (using sqlite3_backup's API) */

	tracker_store_queue_commit (on_batch_commit, NULL, info, NULL);
}

void
tracker_backup_restore (TrackerBackup          *object,
                        const gchar            *backup_uri,
                        const gchar            *journal_uri,
                        DBusGMethodInvocation  *context,
                        GError                **error)
{
	guint request_id;
	TrackerDBusMethodInfo *info;
	GFile *destination, *journal;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
	                          "D-Bus request to restore backup from '%s'",
	                          backup_uri);

	destination = g_file_new_for_uri (backup_uri);
	journal = g_file_new_for_uri (journal_uri);

	info = g_new0 (TrackerDBusMethodInfo, 1);

	info->request_id = request_id;
	info->context = context;
	info->play_journal = TRUE;

	/* This call is mostly synchronous, because we want to block the
	 * mainloop during a restore procedure (you're switching the active
	 * database here, let's not allow queries during this time)
	 *
	 * No need for commits or anything. Whatever is in the current db will
	 * be eliminated in favor of the data in `backup_uri` and `journal_uri`. */

	tracker_data_backup_restore (destination, journal,
	                             backup_callback,
	                             info, destroy_method_info);

	g_object_unref (destination);
	g_object_unref (journal);
}

