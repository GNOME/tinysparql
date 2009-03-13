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
#ifndef __TRACKER_DATA_BACKUP_H__
#define __TRACKER_DATA_BACKUP_H__

#include <glib.h>

G_BEGIN_DECLS

typedef void (* TrackerDataBackupRestoreFunc) (const gchar *subject,
					       const gchar *predicate,
					       const gchar *object,
					       gpointer     user_data);

gboolean tracker_data_backup_save    (const gchar                   *turtle_filename,
				      GError                       **error);
gboolean tracker_data_backup_restore (const gchar                   *turtle_filename,
				      TrackerDataBackupRestoreFunc   restore_func,
				      gpointer                       user_data,
				      GError                       **error);

G_END_DECLS

#endif /* __TRACKER_DATA_BACKUP_H__ */
