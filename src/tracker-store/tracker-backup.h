/*
 * Copyright (C) 2006, Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef __TRACKER_STORE_BACKUP_H__
#define __TRACKER_STORE_BACKUP_H__

#include <glib-object.h>

#define TRACKER_BACKUP_SERVICE         "org.freedesktop.Tracker1"
#define TRACKER_BACKUP_PATH            "/org/freedesktop/Tracker1/Backup"
#define TRACKER_BACKUP_INTERFACE       "org.freedesktop.Tracker1.Backup"

G_BEGIN_DECLS

#define TRACKER_TYPE_BACKUP            (tracker_backup_get_type ())
#define TRACKER_BACKUP(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_BACKUP, TrackerBackup))
#define TRACKER_BACKUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_DBUS_BACKUP, TrackerBackupClass))
#define TRACKER_IS_BACKUP(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_BACKUP))
#define TRACKER_IS_BACKUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_BACKUP))
#define TRACKER_BACKUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_BACKUP, TrackerBackupClass))

typedef struct TrackerBackup TrackerBackup;
typedef struct TrackerBackupClass TrackerBackupClass;

struct TrackerBackup {
	GObject parent;
};

struct TrackerBackupClass {
	GObjectClass parent;
};

GType          tracker_backup_get_type (void) G_GNUC_CONST;

TrackerBackup *tracker_backup_new      (void);
void           tracker_backup_save     (TrackerBackup          *object,
                                        const gchar            *destination_uri,
                                        DBusGMethodInvocation  *context,
                                        GError                **error);
void           tracker_backup_restore  (TrackerBackup          *object,
                                        const gchar            *journal_uri,
                                        DBusGMethodInvocation  *context,
                                        GError                **error);

G_END_DECLS

#endif /* __TRACKER_STORE_BACKUP_H__ */
