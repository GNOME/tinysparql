/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
 */

#ifndef __LIBTRACKER_COMMON_TYPE_UTILS_H__
#define __LIBTRACKER_COMMON_TYPE_UTILS_H__

#include <glib.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

gchar *  tracker_glong_to_string               (glong         i);
gchar *  tracker_gint_to_string                        (gint          i);
gchar *  tracker_guint_to_string               (guint         i);
gchar *  tracker_gint32_to_string              (gint32        i);
gchar *  tracker_guint32_to_string             (guint32       i);
gboolean tracker_string_to_uint                        (const gchar  *s,
                                                        guint        *ret);
gint     tracker_string_in_string_list         (const gchar  *str,
                                                gchar       **strv);
gboolean tracker_string_in_gslist              (const gchar  *str,
                                                GSList       *list);
GSList * tracker_string_list_to_gslist         (gchar       **strv,
                                                gsize         length);
gchar *  tracker_string_list_to_string         (gchar       **strv,
                                                gsize         length,
                                                gchar         sep);
gchar ** tracker_string_to_string_list         (const gchar  *str);
gchar ** tracker_gslist_to_string_list         (GSList       *list);
gboolean tracker_gslist_with_string_data_equal (GSList       *list1,
                                                GSList       *list2);
GSList * tracker_gslist_copy_with_string_data  (GSList       *list);
GList *  tracker_glist_copy_with_string_data   (GList        *list);
gchar *  tracker_string_boolean_to_string_gint (const gchar  *value);

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_TYPE_UTILS_H__ */
