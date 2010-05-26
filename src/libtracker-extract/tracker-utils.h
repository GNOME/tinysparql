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

#ifndef __LIBTRACKER_EXTRACT_UTILS_H__
#define __LIBTRACKER_EXTRACT_UTILS_H__

#if !defined (__LIBTRACKER_EXTRACT_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-extract/tracker-extract.h> must be included directly."
#endif

G_BEGIN_DECLS

#ifndef TRACKER_DISABLE_DEPRECATED
gchar*       tracker_coalesce               (gint         n_values,
                                                          ...) G_GNUC_DEPRECATED;
gchar*       tracker_merge                  (const gchar *delimiter,
                                             gint         n_values,
                                                          ...) G_GNUC_DEPRECATED;
gchar*       tracker_text_normalize         (const gchar *text,
                                             guint        max_words,
                                             guint       *n_words) G_GNUC_DEPRECATED;
#endif /* TRACKER_DISABLE_DEPRECATED */

gboolean     tracker_text_validate_utf8     (const gchar  *text,
                                             gssize        text_len,
                                             GString     **str,
                                             gsize        *valid_len);
gchar*       tracker_date_guess             (const gchar *date_string);
gchar*       tracker_date_format_to_iso8601 (const gchar *date_string,
                                             const gchar *format);
const gchar* tracker_coalesce_strip         (gint         n_values,
                                                          ...);
gchar*       tracker_merge_const            (const gchar *delimiter,
                                             gint         n_values,
                                                          ...);
gssize       tracker_getline                (gchar      **lineptr,
                                             gsize       *n,
                                             FILE        *stream);
void         tracker_keywords_parse         (GPtrArray   *store,
                                             const gchar *keywords);

G_END_DECLS

#endif /*  __LIBTRACKER_EXTRACT_UTILS_H__ */
