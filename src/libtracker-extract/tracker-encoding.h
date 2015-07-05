/*
 * Copyright (C) 2011 Nokia <ivan.frade@nokia.com>
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

#ifndef __LIBTRACKER_EXTRACT_ENCODING_H__
#define __LIBTRACKER_EXTRACT_ENCODING_H__

#if !defined (__LIBTRACKER_EXTRACT_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-extract/tracker-extract.h> must be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

/* Returns TRUE if there is some method available to guess encodings */
gboolean  tracker_encoding_can_guess (void);

/* Returns NULL if it couldn't guess it */
gchar    *tracker_encoding_guess     (const gchar *buffer,
                                      gsize        size,
                                      gdouble     *confidence);

G_END_DECLS

#endif /* __LIBTRACKER_EXTRACT_ENCODING_H__ */
