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

#ifndef __LIBTRACKER_COMMON_FILE_UTILS_H__
#define __LIBTRACKER_COMMON_FILE_UTILS_H__

#include <glib.h>

gint	 tracker_file_open			   (const gchar *uri,
						    gboolean	 readahead);
void	 tracker_file_close			   (gint	 fd,
						    gboolean	 no_longer_needed);
gboolean tracker_file_unlink			   (const gchar *uri);
gboolean tracker_file_is_valid			   (const gchar *uri);
gboolean tracker_file_is_directory		   (const gchar *uri);
gboolean tracker_file_is_indexable		   (const gchar *uri);
guint32  tracker_file_get_size			   (const gchar *uri);
gint32	 tracker_file_get_mtime			   (const gchar *uri);
gchar *  tracker_file_get_mime_type		   (const gchar *uri);
void	 tracker_file_get_path_and_name		   (const gchar *uri,
						    gchar **path,
						    gchar **name);
void	 tracker_path_remove			   (const gchar *uri);
gboolean tracker_path_is_in_path		   (const gchar *path,
						    const gchar *in_path);
void	 tracker_path_hash_table_filter_duplicates (GHashTable	*roots);
GSList * tracker_path_list_filter_duplicates	   (GSList	*roots);
gchar *  tracker_path_evaluate_name		   (const gchar *uri);
gboolean tracker_env_check_xdg_dirs		   (void);

#endif /* __LIBTRACKER_COMMON_FILE_UTILS_H__ */
