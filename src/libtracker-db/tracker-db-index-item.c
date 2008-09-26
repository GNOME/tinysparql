/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "tracker-db-index-item.h"

guint32
tracker_db_index_item_calc_amalgamated (gint service_type,
					gint score)
{
	unsigned char a[4];
	gint16	      score16;
	guint8	      service_type_8;

	if (score > 30000) {
		score16 = 30000;
	} else {
		score16 = (gint16) score;
	}

	service_type_8 = (guint8) service_type;

	/* Amalgamate and combine score and service_type into a single
	 * 32-bit int for compact storage.
	 */
	a[0] = service_type_8;
	a[1] = (score16 >> 8) & 0xFF;
	a[2] = score16 & 0xFF;
	a[3] = 0;

	return (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
}

guint8
tracker_db_index_item_get_service_type (TrackerDBIndexItem *item)
{
	g_return_val_if_fail (item != NULL, 0);

	return (item->amalgamated >> 24) & 0xFF;
}

gint16
tracker_db_index_item_get_score (TrackerDBIndexItem *item)
{
	unsigned char a[2];

	g_return_val_if_fail (item != NULL, 0);

	a[0] = (item->amalgamated >> 16) & 0xFF;
	a[1] = (item->amalgamated >> 8) & 0xFF;

	return (gint16) (a[0] << 8) | (a[1]);
}

guint32
tracker_db_index_item_get_id (TrackerDBIndexItem *item)
{
	g_return_val_if_fail (item != NULL, 0);

	return item->id;
}
