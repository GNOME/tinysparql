/* Tracker Xmp - Xmp helper functions
 * Copyright (C) 2008, Nokia
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

#ifndef _TRACKER_EXIF_H_
#define _TRACKER_EXIF_H_

#include <glib.h>

typedef struct {
	gchar *y_dimension, *x_dimension, *image_width, *document_name, *time, *time_original,
		*artist, *user_comment, *description, *make, *model, *orientation,
		*exposure_time, *fnumber, *flash, *focal_length, *iso_speed_ratings,
		*metering_mode, *white_balance, *copyright;
} TrackerExifData;

void tracker_read_exif (const unsigned char *buffer,
                        size_t               len,
                        const gchar         *uri,
                        TrackerExifData     *data);

#endif /* _TRACKER_EXIF_H_ */

