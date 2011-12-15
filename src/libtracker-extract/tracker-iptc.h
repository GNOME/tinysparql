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

#ifndef __LIBTRACKER_EXTRACT_IPTC_H__
#define __LIBTRACKER_EXTRACT_IPTC_H__

#if !defined (__LIBTRACKER_EXTRACT_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-extract/tracker-extract.h> must be included directly."
#endif

#include <glib.h>

/* IPTC Information Interchange Model */

G_BEGIN_DECLS

/**
 * TrackerIptcData:
 * @keywords: Keywords.
 * @date_created: Date created.
 * @byline: Byline.
 * @credit: Credits.
 * @copyright_notice: Copyright.
 * @image_orientation: Image orientation.
 * @byline_title: Byline title.
 * @city: City.
 * @state: State.
 * @sublocation: Sublocation.
 * @country_name: Country.
 * @contact: Contact info.
 *
 * Structure defining IPTC data.
 */
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

TrackerIptcData *tracker_iptc_new   (const guchar    *buffer,
				     gsize            len,
				     const gchar     *uri);
void             tracker_iptc_free  (TrackerIptcData *data);

#ifndef TRACKER_DISABLE_DEPRECATED

gboolean         tracker_iptc_read  (const unsigned char *buffer,
				     size_t               len,
				     const gchar         *uri,
				     TrackerIptcData     *data) G_GNUC_DEPRECATED;

#endif /* TRACKER_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __LIBTRACKER_EXTRACT_IPTC_H__ */
