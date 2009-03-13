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

#ifndef __TRACKERD_FILES_H__
#define __TRACKERD_FILES_H__

#include <glib-object.h>

#define TRACKER_FILES_SERVICE	      "org.freedesktop.Tracker"
#define TRACKER_FILES_PATH	      "/org/freedesktop/Tracker/Files"
#define TRACKER_FILES_INTERFACE       "org.freedesktop.Tracker.Files"

G_BEGIN_DECLS

#define TRACKER_TYPE_FILES	      (tracker_files_get_type ())
#define TRACKER_FILES(object)	      (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_FILES, TrackerFiles))
#define TRACKER_FILES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_FILES, TrackerFilesClass))
#define TRACKER_IS_FILES(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_FILES))
#define TRACKER_IS_FILES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_FILES))
#define TRACKER_FILES_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_FILES, TrackerFilesClass))

typedef struct TrackerFiles	 TrackerFiles;
typedef struct TrackerFilesClass TrackerFilesClass;

struct TrackerFiles {
	GObject parent;
};

struct TrackerFilesClass {
	GObjectClass parent;
};

GType	      tracker_files_get_type				 (void);
TrackerFiles *tracker_files_new					 (TrackerProcessor	 *processor);
void	      tracker_files_exist				 (TrackerFiles		 *object,
								  const gchar		 *uri,
								  gboolean		  auto_create,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_create				 (TrackerFiles		 *object,
								  const gchar		 *uri,
								  gboolean		  is_directory,
								  const gchar		 *mime,
								  gint			  size,
								  gint			  mtime,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_delete				 (TrackerFiles		 *object,
								  const gchar		 *uri,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_get_service_type			 (TrackerFiles		 *object,
								  const gchar		 *uri,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_get_text_contents			 (TrackerFiles		 *object,
								  const gchar		 *uri,
								  gint			  offset,
								  gint			  max_length,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_search_text_contents		 (TrackerFiles		 *object,
								  const gchar		 *uri,
								  const gchar		 *text,
								  gint			  max_length,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_get_by_service_type			 (TrackerFiles		 *object,
								  gint			  live_query_id,
								  const gchar		 *service,
								  gint			  offset,
								  gint			  max_hits,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_get_by_mime_type			 (TrackerFiles		 *object,
								  gint			  live_query_id,
								  gchar			**mime_types,
								  gint			  offset,
								  gint			  max_hits,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_get_by_mime_type_vfs		 (TrackerFiles		 *object,
								  gint			  live_query_id,
								  gchar			**mime_types,
								  gint			  offset,
								  gint			  max_hits,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_get_mtime				 (TrackerFiles		 *object,
								  const gchar		 *uri,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_get_metadata_for_files_in_folder	 (TrackerFiles		 *object,
								  gint			  live_query_id,
								  const gchar		 *uri,
								  gchar			**fields,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_search_by_text_and_mime		 (TrackerFiles		 *object,
								  const gchar		 *text,
								  gchar			**mime_types,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_search_by_text_and_location		 (TrackerFiles		 *object,
								  const gchar		 *text,
								  const gchar		 *uri,
								  DBusGMethodInvocation  *context,
								  GError		**error);
void	      tracker_files_search_by_text_and_mime_and_location (TrackerFiles		 *object,
								  const gchar		 *text,
								  gchar			**mime_types,
								  const gchar		 *uri,
								  DBusGMethodInvocation  *context,
								  GError		**error);



G_END_DECLS

#endif /* __TRACKERD_FILES_H__ */
