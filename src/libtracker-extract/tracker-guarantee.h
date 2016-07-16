/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <stdio.h>

#ifndef __LIBTRACKER_EXTRACT_GUARANTEE__
#define __LIBTRACKER_EXTRACT_GUARANTEE_H__

#if !defined (__LIBTRACKER_EXTRACT_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-extract/tracker-extract.h> must be included directly."
#endif

#include "tracker-data.h"

G_BEGIN_DECLS

gboolean tracker_guarantee_resource_title_from_file      (TrackerResource  *resource,
                                                          const gchar      *key,
                                                          const gchar      *current_value,
                                                          const gchar      *uri,
                                                          gchar           **p_new_value);
gboolean tracker_guarantee_resource_date_from_file_mtime (TrackerResource  *resource,
                                                          const gchar      *key,
                                                          const gchar      *current_value,
                                                          const gchar      *uri);
gboolean tracker_guarantee_resource_utf8_string (TrackerResource *resource,
                                                 const gchar     *key,
                                                 const gchar     *value);

G_END_DECLS

#endif /*  __LIBTRACKER_EXTRACT_GUARANTEE_H__ */
