/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-journal.h>

#include "tracker-data-backup.h"

GQuark
tracker_data_backup_error_quark (void)
{
	return g_quark_from_static_string ("tracker-data-backup-error-quark");
}

void
tracker_data_backup_save (GFile *destination,
                          GFile *journal,
                          TrackerDataBackupFinished callback,
                          gpointer user_data,
                          GDestroyNotify destroy)
{
	// TODO: Unimplemented
	g_critical ("tracker_data_backup_save unimplemented");
}

void
tracker_data_backup_restore (GFile *backup,
                             GFile *journal,
                             TrackerDataBackupFinished callback,
                             gpointer user_data,
                             GDestroyNotify destroy)
{
	// TODO: Unimplemented
	g_critical ("tracker_data_backup_restore");
}

