/*
 * Copyright (C) 2009, Nokia
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

#ifndef __LIBTRACKER_EXTRACT_UTILS_H__
#define __LIBTRACKER_EXTRACT_UTILS_H__

#if !defined (__LIBTRACKER_EXTRACT_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-extract/tracker-extract.h> must be included directly."
#endif

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

gchar *  tracker_extract_guess_date                 (const gchar  *date_string);
gchar *  tracker_extract_date_format_to_iso8601     (const gchar  *date_string,
                                                     const gchar  *format);

G_END_DECLS

#endif /*  __LIBTRACKER_EXTRACT_UTILS_H__ */
