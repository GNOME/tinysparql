/*
 * Copyright (C) 2010 Nokia <ivan.frade@nokia.com>
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

#ifndef __LIBTRACKER_COMMON_ALBUMART_H__
#define __LIBTRACKER_COMMON_ALBUMART_H__

#include <glib.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

gchar * tracker_albumart_strip_invalid_entities (const gchar  *original);
void    tracker_albumart_get_path               (const gchar  *artist,
                                                 const gchar  *album,
                                                 const gchar  *prefix,
                                                 const gchar  *uri,
                                                 gchar       **path,
                                                 gchar       **local_uri);


G_END_DECLS

#endif /* __LIBTRACKER_COMMON_LOCALE_H__ */
