/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef __TRACKERD_DB_ACTION_H__
#define __TRACKERD_DB_ACTION_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_ACTION (tracker_db_action_get_type ())

typedef enum {
	TRACKER_DB_ACTION_IGNORE,
	TRACKER_DB_ACTION_CHECK,
	TRACKER_DB_ACTION_DELETE,
	TRACKER_DB_ACTION_DELETE_SELF,
	TRACKER_DB_ACTION_CREATE,
	TRACKER_DB_ACTION_MOVED_FROM,
	TRACKER_DB_ACTION_MOVED_TO,
	TRACKER_DB_ACTION_FILE_CHECK,
	TRACKER_DB_ACTION_FILE_CHANGED,
	TRACKER_DB_ACTION_FILE_DELETED,
	TRACKER_DB_ACTION_FILE_CREATED,
	TRACKER_DB_ACTION_FILE_MOVED_FROM,
	TRACKER_DB_ACTION_FILE_MOVED_TO,
	TRACKER_DB_ACTION_WRITABLE_FILE_CLOSED,
	TRACKER_DB_ACTION_DIRECTORY_CHECK,
	TRACKER_DB_ACTION_DIRECTORY_CREATED,
	TRACKER_DB_ACTION_DIRECTORY_UNMOUNTED,
	TRACKER_DB_ACTION_DIRECTORY_DELETED,
	TRACKER_DB_ACTION_DIRECTORY_MOVED_FROM,
	TRACKER_DB_ACTION_DIRECTORY_MOVED_TO,
	TRACKER_DB_ACTION_DIRECTORY_REFRESH,
	TRACKER_DB_ACTION_EXTRACT_METADATA,
	TRACKER_DB_ACTION_FORCE_REFRESH
} TrackerDBAction;

GType	     tracker_db_action_get_type  (void) G_GNUC_CONST;
const gchar *tracker_db_action_to_string (TrackerDBAction action);
gboolean     tracker_db_action_is_delete (TrackerDBAction action);

G_END_DECLS

#endif /* __TRACKERD_DB_ACTION_H__ */
