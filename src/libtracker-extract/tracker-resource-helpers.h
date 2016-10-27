
/*
 * Copyright (C) 2016, Sam Thursfield <sam@afuera.me.uk>
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

#ifndef __LIBTRACKER_EXTRACT_RESOURCE_HELPERS_H__
#define __LIBTRACKER_EXTRACT_RESOURCE_HELPERS_H__

#if !defined (__LIBTRACKER_EXTRACT_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-extract/tracker-extract.h> must be included directly."
#endif

#include <libtracker-sparql/tracker-sparql.h>

G_BEGIN_DECLS

TrackerResource *tracker_extract_new_artist (const char *name);
TrackerResource *tracker_extract_new_contact (const char *fullname);
TrackerResource *tracker_extract_new_equipment (const char *make, const char *model);
TrackerResource *tracker_extract_new_location (const char *address, const char *state, const char *city, const char *country, const char *gps_altitude, const char *gps_latitude, const char *gps_longitude);
TrackerResource *tracker_extract_new_music_album_disc (const char *album_title, TrackerResource *album_artist, int disc_number, const char *date);
TrackerResource *tracker_extract_new_tag (const char *label);

G_END_DECLS

#endif /* __LIBTRACKER_EXTRACT_RESOURCE_HELPERS_H__ */
