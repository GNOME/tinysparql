/* Tracker Xmp - Xmp helper functions
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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
 */

#ifndef _TRACKER_XMP_H_
#define _TRACKER_XMP_H_

#include <glib.h>

typedef struct {
	/* NS_DC */
	gchar *title, *rights, *creator, *description, *date, *keywords, *subject,
	      *publisher, *contributor, *type, *format, *identifier, *source,
	      *language, *relation, *coverage;

	/* NS_CC */
	gchar *license;

	/* NS_EXIF */
	gchar *Title, *DateTimeOriginal, *Artist, *Make, *Model, *Orientation,
	      *Flash, *MeteringMode, *ExposureTime, *FNumber, *FocalLength,
	      *ISOSpeedRatings, *WhiteBalance, *Copyright;

	/* TODO NS_XAP*/
	/* TODO NS_IPTC4XMP */
	/* TODO NS_PHOTOSHOP */

} TrackerXmpData;


void tracker_read_xmp (const gchar          *buffer, 
                       size_t                len, 
                       const gchar          *uri,
                       TrackerXmpData       *data);

void tracker_apply_xmp (TrackerSparqlBuilder *metadata,
                        const gchar *uri,
                        TrackerXmpData *xmp_data);

#endif /* _TRACKER_XMP_H_ */
