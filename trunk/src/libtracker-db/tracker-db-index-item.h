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

#ifndef __TRACKER_DB_INDEX_ITEM_H__
#define __TRACKER_DB_INDEX_ITEM_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	/* Service ID number of the document */
	guint32 id;

	/* Amalgamation of service_type and score of the word in the
	 * document's metadata.
	 */
	gint	amalgamated;
} TrackerDBIndexItem;

typedef struct {
	guint32 service_id;	 /* Service ID of the document */
	guint32 service_type_id; /* Service type ID of the document */
	guint32 score;		 /* Ranking score */
} TrackerDBIndexItemRank;

guint32 tracker_db_index_item_calc_amalgamated (gint		    service_type,
						gint		    score);
guint8	tracker_db_index_item_get_service_type (TrackerDBIndexItem *details);
gint16	tracker_db_index_item_get_score        (TrackerDBIndexItem *details);
guint32 tracker_db_index_item_get_id	       (TrackerDBIndexItem *details);

G_END_DECLS

#endif /* __TRACKER_DB_INDEX_ITEM_H__ */
