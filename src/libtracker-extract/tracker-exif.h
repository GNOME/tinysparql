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
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __TRACKER_EXTRACT_EXIF_H__
#define __TRACKER_EXTRACT_EXIF_H__

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
} TrackerExifData;

gboolean tracker_exif_read (const unsigned char *buffer,
                            size_t               len,
                            const gchar         *uri,
                            TrackerExifData     *data);

G_END_DECLS

#endif /* _TRACKER_EXTRACT_EXIF_H_ */

