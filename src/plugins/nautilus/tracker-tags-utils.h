/*
 * Copyright (C) 2009, Debarshi Ray <debarshir@src.gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __TRACKER_TAGS_UTILS_H__
#define __TRACKER_TAGS_UTILS_H__

#include <glib-object.h>

G_BEGIN_DECLS

inline gboolean tracker_is_empty_string                         (const char  *str);
gchar **        tracker_glist_to_string_list_for_nautilus_files (GList       *list);
GList *         tracker_glist_copy_with_nautilus_files          (GList       *list);
gchar *         tracker_tags_get_filter_string                  (GStrv        files,
                                                                 const gchar *tag);
gchar *         tracker_tags_escape_sparql_string               (const gchar *str);
gchar *         tracker_tags_add_query                          (const gchar *tag_label);
gchar *         tracker_tags_remove_query                       (const gchar *tag_label);

G_END_DECLS

#endif /* __TRACKER_TAGS_UTILS_H__ */
