/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#ifndef __TRACKER_DB_MANAGER_H__
#define __TRACKER_DB_MANAGER_H__

#include <glib-object.h>

#include "tracker-db-interface.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_DB (tracker_db_get_type ())

typedef enum {
	TRACKER_DB_UNKNOWN,
	TRACKER_DB_COMMON,
	TRACKER_DB_CACHE,
	TRACKER_DB_FILE_METADATA,
	TRACKER_DB_FILE_CONTENTS,
	TRACKER_DB_EMAIL_METADATA,
	TRACKER_DB_EMAIL_CONTENTS,
	TRACKER_DB_XESAM,
} TrackerDB;

typedef enum {
	TRACKER_DB_CONTENT_TYPE_METADATA,
	TRACKER_DB_CONTENT_TYPE_CONTENTS
} TrackerDBContentType;

typedef enum {
	TRACKER_DB_MANAGER_FORCE_REINDEX    = 1 << 1,
	TRACKER_DB_MANAGER_REMOVE_CACHE     = 1 << 2,
	TRACKER_DB_MANAGER_LOW_MEMORY_MODE  = 1 << 3,
} TrackerDBManagerFlags;

#define TRACKER_DB_FOR_FILE_SERVICE	"Files"
#define TRACKER_DB_FOR_EMAIL_SERVICE	"Emails"
#define TRACKER_DB_FOR_XESAM_SERVICE	"Xesam"

GType	     tracker_db_get_type			    (void) G_GNUC_CONST;

void	     tracker_db_manager_init			    (TrackerDBManagerFlags  flags,
							     gboolean		   *first_time);
void	     tracker_db_manager_shutdown		    (void);

void	     tracker_db_manager_remove_all		    (void);
void	     tracker_db_manager_close_all		    (void);

const gchar *tracker_db_manager_get_file		    (TrackerDB		    db);
TrackerDBInterface *
	     tracker_db_manager_get_db_interface	    (TrackerDB		    db);
TrackerDBInterface *
	     tracker_db_manager_get_db_interfaces	    (gint num, ...);
TrackerDBInterface *
	     tracker_db_manager_get_db_interface_by_service (const gchar	   *service);
TrackerDBInterface *
	     tracker_db_manager_get_db_interface_by_type    (const gchar	   *service,
							     TrackerDBContentType   content_type);
gboolean     tracker_db_manager_are_db_too_big		    (void);

G_END_DECLS

#endif /* __TRACKER_DB_MANAGER_H__ */
