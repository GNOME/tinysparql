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

#ifndef __LIBTRACKER_EXTRACT_EXIF_H__
#define __LIBTRACKER_EXTRACT_EXIF_H__

#if !defined (__LIBTRACKER_EXTRACT_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-extract/tracker-extract.h> must be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	gchar *y_dimension;
	gchar *x_dimension;
	gchar *image_width;
	gchar *document_name;
	gchar *time;
	gchar *time_original;
	gchar *artist;
	gchar *user_comment;
	gchar *description;
	gchar *make;
	gchar *model;
	gchar *orientation;
	gchar *exposure_time;
	gchar *fnumber;
	gchar *flash;
	gchar *focal_length;
	gchar *iso_speed_ratings;
	gchar *metering_mode;
	gchar *white_balance;
	gchar *copyright;
	gchar *software;
	gchar *x_resolution;
	gchar *y_resolution;
        gchar *gps_altitude;
        gchar *gps_latitude;
        gchar *gps_longitude;
	gint resolution_unit;
} TrackerExifData;

TrackerExifData * tracker_exif_new   (const guchar *buffer,
                                      size_t        len,
                                      const gchar  *uri);
void              tracker_exif_free  (TrackerExifData *data);

#ifndef TRACKER_DISABLE_DEPRECATED

gboolean          tracker_exif_read  (const unsigned char *buffer,
                                      size_t               len,
                                      const gchar         *uri,
                                      TrackerExifData     *data) G_GNUC_DEPRECATED;

#endif /* TRACKER_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __LIBTRACKER_EXTRACT_EXIF_H__ */
