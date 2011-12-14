/*
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

#ifndef __TRACKER_EXTRACT_MEDIA_ART_H__
#define __TRACKER_EXTRACT_MEDIA_ART_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	TRACKER_MEDIA_ART_NONE,
	TRACKER_MEDIA_ART_ALBUM,
	TRACKER_MEDIA_ART_VIDEO,
	TRACKER_MEDIA_ART_TYPE_COUNT
} TrackerMediaArtType;

gboolean tracker_media_art_init     (void);
void     tracker_media_art_shutdown (void);

gboolean tracker_media_art_process  (const unsigned char *buffer,
                                     size_t               len,
                                     const gchar         *mime,
                                     TrackerMediaArtType  type,
                                     const gchar         *artist,
                                     const gchar         *title,
                                     const gchar         *uri);

G_END_DECLS

#endif /* __TRACKER_EXTRACT_MEDIA_ART_H__ */
