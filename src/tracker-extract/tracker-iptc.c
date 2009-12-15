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
	if (strcmp(orientation, "P")==0) {
		return "nfo:orientation-left";
	}

	return "nfo:orientation-top"; /* We take this as default */
}

#endif


void
tracker_read_iptc (const unsigned char *buffer,
                   size_t               len,
                   const gchar         *uri,
                   TrackerIptcData     *data)
{
#ifdef HAVE_LIBIPTCDATA
	guint i;
	IptcData *iptc = NULL;
	IptcTag   p[6] = { IPTC_TAG_KEYWORDS, 
	                   /* 01 */  IPTC_TAG_DATE_CREATED,
	                   /* 02 */  IPTC_TAG_BYLINE,
	                   /* 03 */  IPTC_TAG_CREDIT,
	                   /* 04 */  IPTC_TAG_COPYRIGHT_NOTICE,
	                   /* 05 */  IPTC_TAG_IMAGE_ORIENTATION};

	/* FIXME According to valgrind this is leaking (together with the unref).
	 * Problem in libiptc */

	iptc = iptc_data_new ();

	if (!iptc)
		return;

	if (iptc_data_load (iptc, buffer, len) < 0) {
		iptc_data_unref (iptc);
		return;
	}

	for (i = 0; i < 6; i++) {
		IptcDataSet *dataset = NULL;

		while ((dataset = iptc_data_get_next_dataset (iptc, dataset, 2, p[i]))) {
			gchar mbuffer[1024];

			iptc_dataset_get_as_str (dataset, mbuffer, 1024);

			switch (p[i]) {
			case IPTC_TAG_KEYWORDS:
				if (!data->keywords)
					data->keywords = g_strdup (mbuffer);
				break;
			case IPTC_TAG_DATE_CREATED:
				if (!data->date_created) {
					/* From: ex; date "2007:04:15 15:35:58"
					 * To : ex. "2007-04-15T17:35:58+0200 where +0200 is localtime */
					data->date_created = tracker_date_format_to_iso8601 (mbuffer, IPTC_DATE_FORMAT);
				}
				break;
			case IPTC_TAG_BYLINE:
				if (!data->byline)
					data->byline = g_strdup (mbuffer);
				break;
			case IPTC_TAG_CREDIT:
				if (!data->credit)
					data->credit = g_strdup (mbuffer);
				break;
			case IPTC_TAG_COPYRIGHT_NOTICE:
				if (!data->copyright_notice)
					data->copyright_notice = g_strdup (mbuffer);
				break;
			case IPTC_TAG_IMAGE_ORIENTATION:
				if (!data->image_orientation)
					data->image_orientation = g_strdup (fix_iptc_orientation (mbuffer));
				break;
			default:
				break;
			}
		}
	}

	iptc_data_unref (iptc);

#endif
}
