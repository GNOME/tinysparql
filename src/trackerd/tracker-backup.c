/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#include "tracker-dbus.h"
#include "tracker-indexer-client.h"
#include "tracker-backup.h"
#include "tracker-status.h"

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

void
tracker_backup_save (TrackerBackup          *object,
		     const gchar            *path,
		     DBusGMethodInvocation  *context,
		     GError                **error)
{
	guint request_id;
	GError *err = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
				  "DBus request to save backup into '%s'",
				  path);

	g_message ("Backing up metadata");
	tracker_data_backup_save (path, &err);

	if (err) {
		GError *actual_error = NULL;

		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     err->message);

		dbus_g_method_return_error (context, actual_error);

		g_error_free (actual_error);
		g_error_free (err);
	} else {
		dbus_g_method_return (context);
		tracker_dbus_request_success (request_id);
	}
}

void
tracker_backup_restore (TrackerBackup          *object,
			const gchar            *path,
			DBusGMethodInvocation  *context,
			GError                **error)
{
	guint request_id;
	GError *actual_error = NULL;
	GError *restore_error = NULL;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_request_new (request_id,
				  "DBus request to restore backup from '%s'",
				  path);

	/* First check we have disk space, we do this with ALL our
	 * indexer commands.
	 */
	if (tracker_status_get_is_paused_for_space ()) {
		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     "No disk space left to write to the databases");
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		return;
	}

	org_freedesktop_Tracker_Indexer_restore_backup (tracker_dbus_indexer_get_proxy (),
							path, 
							&restore_error);

	if (restore_error) {
		GError *actual_error = NULL;

		tracker_dbus_request_failed (request_id,
					     &actual_error,
					     restore_error->message);

		dbus_g_method_return_error (context, actual_error);

		g_error_free (actual_error);
		g_error_free (restore_error);
	} else {
		dbus_g_method_return (context);
		tracker_dbus_request_success (request_id);
	}
}
