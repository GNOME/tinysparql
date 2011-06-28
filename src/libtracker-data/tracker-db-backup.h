/*
 * Copyright (C) 2009, Nokia
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
 *
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __TRACKER_DB_BACKUP_H__
#define __TRACKER_DB_BACKUP_H__

#include <glib.h>
#include <gio/gio.h>

#define TRACKER_DB_BACKUP_META_FILENAME		"meta-backup.db"

G_BEGIN_DECLS

#define TRACKER_DB_BACKUP_ERROR	    (tracker_db_backup_error_quark ())

typedef enum {
	TRACKER_DB_BACKUP_ERROR_UNKNOWN,
} TrackerDBBackupError;

typedef void (*TrackerDBBackupFinished)   (GError *error, gpointer user_data);

GQuark    tracker_db_backup_error_quark (void);

void      tracker_db_backup_save        (GFile                   *destination,
                                         TrackerDBBackupFinished  callback,
                                         gpointer                 user_data,
                                         GDestroyNotify           destroy);

G_END_DECLS

#endif /* __TRACKER_DB_BACKUP_H__ */
