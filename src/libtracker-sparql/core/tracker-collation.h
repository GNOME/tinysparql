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

#ifndef __LIBTRACKER_COMMON_COLLATION_H__
#define __LIBTRACKER_COMMON_COLLATION_H__

G_BEGIN_DECLS

gpointer tracker_collation_init     (void);
void     tracker_collation_shutdown (gpointer      collator);
gint     tracker_collation_utf8     (gpointer      collator,
                                     gint          len1,
                                     gconstpointer str1,
                                     gint          len2,
                                     gconstpointer str2);

gint     tracker_collation_utf8_title (gpointer      collator,
                                       gint          len1,
                                       gconstpointer str1,
                                       gint          len2,
                                       gconstpointer str2);

#ifdef HAVE_LIBICU
#define TRACKER_COLLATION_LAST_CHAR ((gunichar) 0x10fffd)
#else
/* glibc-based collators do not properly sort private use characters */
#define TRACKER_COLLATION_LAST_CHAR ((gunichar) 0x9fa5)
#endif

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_COLLATION_H__ */
