/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * GNOME Search Tool
 *
 *  File:  tracker_search-support.h
 *
 *  (C) 2002 the Free Software Foundation
 *
 *  Authors:	Dennis Cranston  <dennis_cranston@yahoo.com>
 *		George Lebl
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef _GSEARCHTOOL_SUPPORT_H_
#define _GSEARCHTOOL_SUPPORT_H_

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

#include "tracker-search-tool.h"

#define ICON_SIZE 40
#define ICON_THEME_EXECUTABLE_ICON "application-x-executable"
#define ICON_THEME_REGULAR_ICON    "gnome-fs-regular"
#define ICON_THEME_CHAR_DEVICE	   "gnome-fs-chardev"
#define ICON_THEME_BLOCK_DEVICE    "gnome-fs-blockdev"
#define ICON_THEME_SOCKET	   "gnome-fs-socket"
#define ICON_THEME_FIFO		   "gnome-fs-fifo"


gboolean
tracker_is_empty_string (const gchar *s);

gboolean
tracker_search_gconf_get_boolean (const gchar * key);

void
tracker_search_gconf_set_boolean (const gchar * key,
				  const gboolean flag);

gint
tracker_search_gconf_get_int (const gchar * key);

void
tracker_search_gconf_set_int (const gchar * key,
			      const gint value);

gchar *
tracker_search_gconf_get_string (const gchar * key);

GSList *
tracker_search_gconf_get_list (const gchar * key,
			       GConfValueType list_type);

void
tracker_search_gconf_set_list (const gchar * key,
			       GSList * list,
			       GConfValueType list_type);

void
tracker_search_gconf_add_dir (const gchar * dir);

void
tracker_search_gconf_watch_key (const gchar * dir,
				const gchar * key,
				GConfClientNotifyFunc callback,
				gpointer user_data);


gboolean
is_path_hidden (const gchar * path);

gboolean
is_quick_search_excluded_path (const gchar * path);

gboolean
is_second_scan_excluded_path (const gchar * path);

gboolean
compare_regex (const gchar * regex,
	       const gchar * string);

gboolean
limit_string_to_x_lines (GString * string,
			 gint x);

gchar *
tracker_string_replace (const gchar * haystack,
			gchar * needle,
			gchar * replacement);

gchar *
escape_single_quotes (const gchar * string);

gchar *
backslash_special_characters (const gchar * string);

gchar *
remove_mnemonic_character (const gchar * string);


gchar *
get_readable_date (const gchar * format_string,
		   const time_t file_time_raw);

gchar *
tracker_search_strdup_strftime (const gchar * format,
				struct tm * time_pieces);

gchar *
get_file_type_description (const gchar * file,
			   const char *mime,
			   GnomeVFSFileInfo * file_info);

GdkPixbuf *
get_file_pixbuf (GSearchWindow * gsearch,
		 const gchar * file,
		 const char * mime,
		 GnomeVFSFileInfo * file_info);


gboolean
open_file_with_xdg_open (GtkWidget * window,
			 const gchar * file);

gboolean
open_file_with_nautilus (GtkWidget * window,
			 const gchar * file);

gboolean
open_file_with_application (GtkWidget * window,
			    const gchar * file);

gboolean
launch_file (const gchar * file);

gchar *
tracker_search_get_unique_filename (const gchar * path,
				    const gchar * suffix);

GtkWidget *
tracker_search_button_new_with_stock_icon (const gchar * string,
					   const gchar * stock_id);

GSList *
tracker_search_get_columns_order (GtkTreeView * treeview);

void
tracker_search_set_columns_order (GtkTreeView * treeview);

gint
tracker_get_stored_separator_position (void);

void
tracker_set_stored_separator_position (gint pos);

void
tracker_search_get_stored_window_geometry (gint * width,
					   gint * height);

void
tracker_set_atk_relationship (GtkWidget *obj1,
			      int relation_type,
			      GtkWidget *obj2);

#ifdef __cplusplus
}
#endif

#endif /* _GSEARCHTOOL_SUPPORT_H */
