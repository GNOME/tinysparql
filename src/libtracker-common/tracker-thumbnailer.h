/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __TRACKER_THUMBNAILER_H__
#define __TRACKER_THUMBNAILER_H__

#include <libtracker-common/tracker-config.h>

G_BEGIN_DECLS

void tracker_thumbnailer_init               (TrackerConfig *config,
					     guint timeout_seconds);
void tracker_thumbnailer_shutdown           (void);

void tracker_thumbnailer_get_file_thumbnail (const gchar   *path,
					     const gchar   *mime);
void tracker_thumbnailer_move               (const gchar   *from_uri,
					     const gchar   *mime_type,
					     const gchar   *to_uri);
void tracker_thumbnailer_remove             (const gchar   *uri,
					     const gchar   *mime_type);
void tracker_thumbnailer_cleanup            (const gchar   *uri_prefix);

G_END_DECLS

#endif /* __TRACKER_THUMBNAILER_H__ */
