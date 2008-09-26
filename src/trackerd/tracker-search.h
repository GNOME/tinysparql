/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
 *
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

#ifndef __TRACKERD_SEARCH_H__
#define __TRACKERD_SEARCH_H__

#include <glib-object.h>

#include <libtracker-common/tracker-language.h>

#include <libtracker-db/tracker-db-index.h>

#define TRACKER_SEARCH_SERVICE	       "org.freedesktop.Tracker"
#define TRACKER_SEARCH_PATH	       "/org/freedesktop/Tracker/Search"
#define TRACKER_SEARCH_INTERFACE       "org.freedesktop.Tracker.Search"

G_BEGIN_DECLS

#define TRACKER_TYPE_SEARCH	       (tracker_search_get_type ())
#define TRACKER_SEARCH(object)	       (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_SEARCH, TrackerSearch))
#define TRACKER_SEARCH_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_SEARCH, TrackerSearchClass))
#define TRACKER_IS_SEARCH(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_SEARCH))
#define TRACKER_IS_SEARCH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_SEARCH))
#define TRACKER_SEARCH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_SEARCH, TrackerSearchClass))

typedef struct TrackerSearch	  TrackerSearch;
typedef struct TrackerSearchClass TrackerSearchClass;

struct TrackerSearch {
	GObject parent;
};

struct TrackerSearchClass {
	GObjectClass parent;
};

GType	       tracker_search_get_type		(void);
TrackerSearch *tracker_search_new		(TrackerConfig		*config,
						 TrackerLanguage	*language,
						 TrackerDBIndex		*file_index,
						 TrackerDBIndex		*email_index);
void	       tracker_search_get_hit_count	(TrackerSearch		*object,
						 const gchar		*service,
						 const gchar		*search_text,
						 DBusGMethodInvocation	*context,
						 GError		       **error);
void	       tracker_search_get_hit_count_all (TrackerSearch		*object,
						 const gchar		*search_text,
						 DBusGMethodInvocation	*context,
						 GError		       **error_in);
void	       tracker_search_text		(TrackerSearch		*object,
						 gint			 live_query_id,
						 const gchar		*service,
						 const gchar		*search_text,
						 gint			 offset,
						 gint			 max_hits,
						 DBusGMethodInvocation	*context,
						 GError		       **error_in);
void	       tracker_search_text_detailed	(TrackerSearch		*object,
						 gint			 live_query_id,
						 const gchar		*service,
						 const gchar		*search_text,
						 gint			 offset,
						 gint			 max_hits,
						 DBusGMethodInvocation	*context,
						 GError		       **error);
void	       tracker_search_get_snippet	(TrackerSearch		*object,
						 const gchar		*service,
						 const gchar		*id,
						 const gchar		*search_text,
						 DBusGMethodInvocation	*context,
						 GError		       **error);
void	       tracker_search_files_by_text	(TrackerSearch		*object,
						 gint			 live_query_id,
						 const gchar		*search_text,
						 gint			 offset,
						 gint			 max_hits,
						 gboolean		 group_results,
						 DBusGMethodInvocation	*context,
						 GError		       **error);
void	       tracker_search_metadata		(TrackerSearch		*object,
						 const gchar		*service,
						 const gchar		*field,
						 const gchar		*search_text,
						 gint			 offset,
						 gint			 max_hits,
						 DBusGMethodInvocation	*context,
						 GError		       **error);
void	       tracker_search_matching_fields	(TrackerSearch		*object,
						 const gchar		*service,
						 const gchar		*id,
						 const gchar		*search_text,
						 DBusGMethodInvocation	*context,
						 GError		       **error);
void	       tracker_search_query		(TrackerSearch		*object,
						 gint			 live_query_id,
						 const gchar		*service,
						 gchar		       **fields,
						 const gchar		*search_text,
						 const gchar		*keyword,
						 const gchar		*query_condition,
						 gboolean		 sort_by_service,
						 gchar		       **sort_fields,
						 gint			 sort_field_count,
						 gint			 offset,
						 gint			 max_hits,
						 DBusGMethodInvocation	*context,
						 GError		       **error);
void	       tracker_search_suggest		(TrackerSearch		*object,
						 const gchar		*search_text,
						 gint			 max_dist,
						 DBusGMethodInvocation	*context,
						 GError		       **error);

G_END_DECLS

#endif /* __TRACKERD_SEARCH_H__ */
