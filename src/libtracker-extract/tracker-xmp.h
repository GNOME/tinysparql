/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#ifndef __LIBTRACKER_EXTRACT_XMP_H__
#define __LIBTRACKER_EXTRACT_XMP_H__

#if !defined (__LIBTRACKER_EXTRACT_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-extract/tracker-extract.h> must be included directly."
#endif

#include <libtracker-sparql/tracker-sparql.h>

G_BEGIN_DECLS

/**
 * TrackerXmpData:
 * @title: Value for the dc:title property.
 * @rights: Value for the dc:rights property.
 * @creator: Value for the dc:creator property.
 * @description: Value for the dc:description property.
 * @date: Value for the dc:date property.
 * @keywords: Value for the dc:keywords property.
 * @subject: Value for the dc:subject property.
 * @publisher: Value for the dc:publisher property.
 * @contributor: Value for the dc:contributor property.
 * @type: Value for the dc:type property.
 * @format: Value for the dc:format property.
 * @identifier: Value for the dc:identifier property.
 * @source: Value for the dc:source property.
 * @language: Value for the dc:language property.
 * @relation: Value for the dc:relation property.
 * @coverage: Value for the dc:coverage property.
 * @license: Value for the nie:license property.
 * @pdf_title: Title of the PDF.
 * @pdf_keywords: Keywords in the PDF.
 * @title2: Title from the EXIF data.
 * @time_original: Original time from the EXIF data.
 * @artist: Artist from the EXIF data.
 * @make: Make info from the EXIF data.
 * @model: Model from the EXIF data.
 * @orientation: Orientation info from the EXIF data.
 * @flash: Flash info from the EXIF data.
 * @metering_mode: Metering mode from the EXIF data.
 * @exposure_time: Exposure time from the EXIF data.
 * @fnumber: Focal ratio from the EXIF data.
 * @focal_length: Focal length from the EXIF data.
 * @iso_speed_ratings: ISO speed ratings from the EXIF data.
 * @white_balance: White balance info from the EXIF data.
 * @copyright: Copyright info from the EXIF data.
 * @rating: Rating.
 * @address: Address.
 * @country: Country.
 * @state: State.
 * @city: City.
 * @gps_altitude: GPS altitude.
 * @gps_altitude_ref: GPS altitude reference.
 * @gps_latitude: GPS latitude.
 * @gps_longitude: GPS longitude.
 * @gps_direction: GPS direction information.
 * @regions: List of #TrackerXmpRegion items.
 *
 * Structure defining XMP data of a given file.
 */
typedef struct {
	/* NS_DC */
	gchar *title;
	gchar *rights;
	gchar *creator;
	gchar *description;
	gchar *date;
	gchar *keywords;
	gchar *subject;
	gchar *publisher;
	gchar *contributor;
	gchar *type;
	gchar *format;
	gchar *identifier;
	gchar *source;
	gchar *language;
	gchar *relation;
	gchar *coverage;

	/* NS_CC */
	gchar *license;

	/* NS_PDF */
	gchar *pdf_title;
	gchar *pdf_keywords;

	/* NS_EXIF */
	gchar *title2;
	gchar *time_original;
	gchar *artist;
	gchar *make;
	gchar *model;
	gchar *orientation;
	gchar *flash;
	gchar *metering_mode;
	gchar *exposure_time;
	gchar *fnumber;
	gchar *focal_length;
	gchar *iso_speed_ratings;
	gchar *white_balance;
	gchar *copyright;

	/* TODO NS_XAP*/
	gchar *rating;

	/* TODO NS_IPTC4XMP */
	/* TODO NS_PHOTOSHOP */
	gchar *address;
	gchar *country;
	gchar *state;
	gchar *city;

	/* ABI barrier (don't change things above this) */
	gchar *gps_altitude;
	gchar *gps_altitude_ref;
	gchar *gps_latitude;
	gchar *gps_longitude;
	gchar *gps_direction;

	/* List of TrackerXmpRegion */
	GSList *regions;
} TrackerXmpData;

/**
 * TrackerXmpRegion:
 * @title: Title of the region.
 * @description: Description of the region.
 * @type: Type of the region.
 * @x: X axis position.
 * @y: Y axis position.
 * @width: Width.
 * @height: Height.
 * @link_class: Link class.
 * @link_uri: Link URI.
 *
 * Structure defining data of a given region in a #TrackerXmpData.
 */
typedef struct {
	gchar *title;
	gchar *description;
	gchar *type;
	gchar *x;
	gchar *y;
	gchar *width;
	gchar *height;
	gchar *link_class;
	gchar *link_uri;
} TrackerXmpRegion;

TrackerXmpData *tracker_xmp_new           (const gchar          *buffer,
                                           gsize                 len,
                                           const gchar          *uri);
void            tracker_xmp_free          (TrackerXmpData       *data);

gboolean        tracker_xmp_apply_to_resource         (TrackerResource *resource,
                                                       TrackerXmpData  *data);
gboolean        tracker_xmp_apply_regions_to_resource (TrackerResource *resource,
                                                       TrackerXmpData  *data);

#ifndef TRACKER_DISABLE_DEPRECATED

gboolean         tracker_xmp_read  (const gchar          *buffer,
                                    size_t                len,
                                    const gchar          *uri,
                                    TrackerXmpData       *data) G_GNUC_DEPRECATED;

#endif /* TRACKER_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __LIBTRACKER_EXTRACT_XMP_H__ */
