/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#ifndef __TRACKER_READ_H__
#define __TRACKER_READ_H__

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

gchar *tracker_read_text_from_stream (GInputStream *stream,
                                      gsize         max_bytes,
                                      gboolean      try_locale_if_not_utf8);

gchar *tracker_read_text_from_fd (gint     fd,
                                  gsize    max_bytes,
                                  gboolean try_locale_if_not_utf8);

G_END_DECLS

#endif /* __TRACKER_READ_H__ */

