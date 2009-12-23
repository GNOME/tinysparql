/* Tracker IPTC - Iptc helper functions
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

#ifndef _TRACKER_IPTC_H_
#define _TRACKER_IPTC_H_

#include <glib.h>

typedef struct {
	gchar *keywords, *date_created, *byline, *credit, *copyright_notice,
		*image_orientation, *bylinetitle, *city, *state, *sublocation,
		*countryname, *contact;
} TrackerIptcData;


void tracker_read_iptc (const unsigned char *buffer,
                        size_t               len,
                        const gchar         *uri,
                        TrackerIptcData     *data);

#endif
