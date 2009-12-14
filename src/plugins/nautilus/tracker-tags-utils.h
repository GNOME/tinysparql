/*
 * Copyright (C) 2009  Debarshi Ray <debarshir@src.gnome.org>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __TRACKER_TAGS_UTILS_H__
#define __TRACKER_TAGS_UTILS_H__

#include <glib.h>

G_BEGIN_DECLS

gchar *tracker_tags_escape_sparql_string (const gchar *str);

gchar *tracker_tags_add_query            (const gchar *tag_label);
gchar *tracker_tags_remove_query         (const gchar *tag_label);

G_END_DECLS

#endif /* __TRACKER_TAGS_UTILS_H__ */
