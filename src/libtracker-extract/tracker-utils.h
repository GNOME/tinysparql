/*
 * Copyright (C) 2009, Nokia
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
 *
 * Authors: Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __LIBTRACKER_EXTRACT_UTILS_H__
#define __LIBTRACKER_EXTRACT_UTILS_H__

#include <glib.h>

/* These are for convenience */
#define tracker_coalesce tracker_extract_coalesce
#define tracker_merge tracker_extract_merge

G_BEGIN_DECLS

gchar *  tracker_extract_coalesce                   (gint n_values,
                                                     ...);
gchar *  tracker_extract_merge                      (const gchar *delim, 
                                                     gint n_values,
                                                     ...);
gchar *  tracker_extract_text_normalize             (const gchar *text,
                                                     guint        max_words,
                                                     guint       *n_words);

G_END_DECLS

#endif /*  __LIBTRACKER_EXTRACT_UTILS_H__ */
