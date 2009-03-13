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
 *
 * Authors: Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __LIBTRACKER_COMMON_ALBUMART_H__
#define __LIBTRACKER_COMMON_ALBUMART_H__

G_BEGIN_DECLS

gboolean tracker_albumart_heuristic        (const gchar *artist_,  
					    const gchar *album_, 
					    const gchar *tracks_str, 
					    const gchar *filename,
					    const gchar *local_uri,
					    gboolean    *copied);
void     tracker_albumart_copy_to_local    (const gchar *filename, 
					    const gchar *local_uri);
void     tracker_albumart_get_path         (const gchar  *a, 
					    const gchar  *b, 
					    const gchar  *prefix, 
					    const gchar  *uri,
					    gchar       **path,
					    gchar       **local);
void     tracker_albumart_request_download (const gchar *album, 
					    const gchar *artist, 
					    const gchar *local_uri, 
					    const gchar *art_path);

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_THUMBNAILER_H__ */
