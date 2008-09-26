/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * tracker-gtk/keyword-store.h - A derived GtkListStore that maintians a
 * DBus connection to tracker such that when a new keyword is created it
 * is automatically inserted here.
 *
 * Copyright (C) 2007 John Stowers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef TRACKER_KEYWORD_STORE_H
#define TRACKER_KEYWORD_STORE_H

#include <gtk/gtkliststore.h>

#include <tracker.h>

G_BEGIN_DECLS

typedef enum
{
	TRACKER_KEYWORD_STORE_KEYWORD,
	TRACKER_KEYWORD_STORE_IMAGE_URI,
	TRACKER_KEYWORD_STORE_NUM_COLUMNS
} TrackerKeywordStoreColumns;


#define TRACKER_TYPE_KEYWORD_STORE	      (tracker_keyword_store_get_type ())
#define TRACKER_KEYWORD_STORE(obj)	      (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_KEYWORD_STORE, TrackerKeywordStore))
#define TRACKER_KEYWORD_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_KEYWORD_STORE, TrackerKeywordStoreClass))
#define TRACKER_IS_KEYWORD_STORE(obj)	      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_KEYWORD_STORE))
#define TRACKER_IS_KEYWORD_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_KEYWORD_STORE))
#define TRACKER_KEYWORD_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_KEYWORD_STORE, TrackerKeywordStoreClass))


typedef struct _TrackerKeywordStoreClass  TrackerKeywordStoreClass;

struct _TrackerKeywordStore
{
	GtkListStore  parent_instance;

	/*< private >*/
	GHashTable *keywords;
	TrackerClient *tracker_client;
};

struct _TrackerKeywordStoreClass
{
	GtkListStoreClass  parent_class;
};


GType		tracker_keyword_store_get_type		(void) G_GNUC_CONST;

GtkListStore *	tracker_keyword_store_new		(void);

gboolean
tracker_keyword_store_insert (GtkListStore		*store,
			      const char		*keyword,
			      const char		*stock_id );

GtkTreeIter *
tracker_keyword_store_lookup (GtkListStore		*store,
			      const char		*keyword);

gboolean
tracker_keyword_store_remove (GtkListStore		*store,
			      const char		*keyword);

G_END_DECLS

#endif	/* TRACKER_KEYWORD_STORE_H */
