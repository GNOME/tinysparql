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

#ifndef __LIBTRACKER_EXTRACT_IPTC_H__
#define __LIBTRACKER_EXTRACT_IPTC_H__

#include <glib.h>

/* IPTC Information Interchange Model */

G_BEGIN_DECLS

typedef struct {
	gchar *keywords;
	gchar *date_created;
	gchar *byline;
	gchar *credit;
	gchar *copyright_notice;
	gchar *image_orientation;
	gchar *byline_title;
	gchar *city;
	gchar *state;
	gchar *sublocation;
	gchar *country_name;
	gchar *contact;
} TrackerIptcData;

gboolean tracker_extract_iptc_read (const unsigned char *buffer,
                                    size_t               len,
                                    const gchar         *uri,
                                    TrackerIptcData     *data);

G_END_DECLS

#endif /* __LIBTRACKER_EXTRACT_IPTC_H__ */
