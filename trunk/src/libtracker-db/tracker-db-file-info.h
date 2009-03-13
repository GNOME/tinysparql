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

#ifndef __TRACKER_DB_FILE_INFO_H__
#define __TRACKER_DB_FILE_INFO_H__

#include <glib-object.h>

#include "tracker-db-action.h"

/*
 * Watch types
 */
#define TRACKER_TYPE_DB_WATCH (tracker_db_watch_get_type ())

typedef enum {
	TRACKER_DB_WATCH_ROOT,
	TRACKER_DB_WATCH_SUBFOLDER,
	TRACKER_DB_WATCH_SPECIAL_FOLDER,
	TRACKER_DB_WATCH_SPECIAL_FILE,
	TRACKER_DB_WATCH_NO_INDEX,
	TRACKER_DB_WATCH_OTHER
} TrackerDBWatch;

GType		   tracker_db_watch_get_type  (void) G_GNUC_CONST;
const gchar *	   tracker_db_watch_to_string (TrackerDBWatch	  watch);

/*
 * File Information
 */
typedef struct _TrackerDBFileInfo TrackerDBFileInfo;

struct _TrackerDBFileInfo {
	/* File name/path related info */
	gchar		*uri;
	guint32		 file_id;

	TrackerDBAction  action;
	guint32		 cookie;
	gint		 counter;
	TrackerDBWatch	 watch_type;

	/* Symlink info - File ID of link might not be in DB so need
	 * to store path/filename too.
	 */
	gint32		 link_id;
	gchar		*link_path;
	gchar		*link_name;

	gchar		*mime;
	gint		 service_type_id;
	guint32		 file_size;
	gchar		*permissions;
	gint32		 mtime;
	gint32		 atime;
	gint32		 indextime;
	gint32		 offset;

	/* Options */
	gchar		*moved_to_uri;

	gint		 aux_id;

	guint		 is_new : 1;
	guint		 is_directory : 1;
	guint		 is_link : 1;
	guint		 extract_embedded : 1;
	guint		 extract_contents : 1;
	guint		 extract_thumbs : 1;
	guint		 is_hidden : 1;

	/* We ref count FileInfo as it has a complex lifespan and is
	 * tossed between various threads, lists, queues and hash
	 * tables.
	 */
	gint		 ref_count;
};

TrackerDBFileInfo *tracker_db_file_info_new	 (const gchar	    *uri,
						  TrackerDBAction    action,
						  gint		     counter,
						  TrackerDBWatch     watch);
void		   tracker_db_file_info_free	 (TrackerDBFileInfo *info);
TrackerDBFileInfo *tracker_db_file_info_ref	 (TrackerDBFileInfo *info);
TrackerDBFileInfo *tracker_db_file_info_unref	 (TrackerDBFileInfo *info);
TrackerDBFileInfo *tracker_db_file_info_get	 (TrackerDBFileInfo *info);
gboolean	   tracker_db_file_info_is_valid (TrackerDBFileInfo *info);


#endif /* __TRACKER_DB_FILE_INFO_H__ */
