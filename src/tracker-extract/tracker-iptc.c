/* Tracker Iptc - Iptc helper functions
 * Copyright (C) 2009, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include "tracker-iptc.h"
#include "tracker-main.h"

#include <glib.h>
#include <string.h>

#include <libtracker-common/tracker-type-utils.h>

#ifdef HAVE_LIBIPTCDATA

#include <libiptcdata/iptc-data.h>
#include <libiptcdata/iptc-dataset.h>

#define IPTC_DATE_FORMAT "%Y %m %d"

static const gchar *
fix_iptc_orientation (const gchar *orientation)
{
	if (g_strcmp0 (orientation, "P") == 0) {
		return "nfo:orientation-left";
	}

	return "nfo:orientation-top"; /* We take this as default */
}


static void
foreach_dataset (IptcDataSet *dataset, void *user_data)
{
	TrackerIptcData *data = user_data;
	gchar mbuffer[1024];

	switch (dataset->tag) {
			case IPTC_TAG_KEYWORDS:
			if (!data->keywords) {
				iptc_dataset_get_as_str (dataset, mbuffer, 1024);
				data->keywords = g_strdup (mbuffer);
			}
			break;

			case IPTC_TAG_DATE_CREATED:
			if (!data->date_created) {
				iptc_dataset_get_as_str (dataset, mbuffer, 1024);
				/* From: ex; date "2007:04:15 15:35:58"
				 * To : ex. "2007-04-15T17:35:58+0200 where +0200 is localtime */
				data->date_created = tracker_date_format_to_iso8601 (mbuffer, IPTC_DATE_FORMAT);
			}
			break;

			case IPTC_TAG_BYLINE:
			if (!data->byline) {
				iptc_dataset_get_as_str (dataset, mbuffer, 1024);
				data->byline = g_strdup (mbuffer);
			}
			break;

			case IPTC_TAG_CREDIT:
			if (!data->credit) {
				iptc_dataset_get_as_str (dataset, mbuffer, 1024);
				data->credit = g_strdup (mbuffer);
			}
			break;

			case IPTC_TAG_COPYRIGHT_NOTICE:
			if (!data->copyright_notice) {
				iptc_dataset_get_as_str (dataset, mbuffer, 1024);
				data->copyright_notice = g_strdup (mbuffer);
			}
			break;

			case IPTC_TAG_IMAGE_ORIENTATION:
			if (!data->image_orientation) {
				iptc_dataset_get_as_str (dataset, mbuffer, 1024);
				data->image_orientation = g_strdup (fix_iptc_orientation (mbuffer));
			}
			break;

			case IPTC_TAG_BYLINE_TITLE:
			if (!data->bylinetitle) {
				iptc_dataset_get_as_str (dataset, mbuffer, 1024);
				data->bylinetitle = g_strdup (mbuffer);
			}
			break;

			case IPTC_TAG_CITY:
			if (!data->city) {
				iptc_dataset_get_as_str (dataset, mbuffer, 1024);
				data->city = g_strdup (mbuffer);
			}
			break;

			case IPTC_TAG_STATE:
			if (!data->state) {
				iptc_dataset_get_as_str (dataset, mbuffer, 1024);
				data->state = g_strdup (mbuffer);
			}
			break;

			case IPTC_TAG_SUBLOCATION:
			if (!data->sublocation) {
				iptc_dataset_get_as_str (dataset, mbuffer, 1024);
				data->sublocation = g_strdup (mbuffer);
			}
			break;

			case IPTC_TAG_COUNTRY_NAME:
			if (!data->countryname) {
				iptc_dataset_get_as_str (dataset, mbuffer, 1024);
				data->countryname = g_strdup (mbuffer);
			}
			break;

			case IPTC_TAG_CONTACT:
			if (!data->contact) {
				iptc_dataset_get_as_str (dataset, mbuffer, 1024);
				data->contact = g_strdup (mbuffer);
			}
			break;

			default:
			break;
	}
}


#endif


void
tracker_read_iptc (const unsigned char *buffer,
                   size_t               len,
                   const gchar         *uri,
                   TrackerIptcData     *data)
{
#ifdef HAVE_LIBIPTCDATA
	IptcData *iptc = NULL;

	/* FIXME According to valgrind this is leaking (together with the unref).
	 * Problem in libiptc (I replaced this with the _free equivalent) */

	iptc = iptc_data_new ();

	if (!iptc)
		return;

	if (iptc_data_load (iptc, buffer, len) < 0) {
		iptc_data_free (iptc);
		return;
	}

	iptc_data_foreach_dataset (iptc, foreach_dataset, data);

	iptc_data_free (iptc);

#endif
}
