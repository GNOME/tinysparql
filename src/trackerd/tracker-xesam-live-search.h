/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 * Authors: Philip Van Hoof (pvanhoof@gnome.org)
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

#ifndef __TRACKERD_XESAM_LIVE_SEARCH_H__
#define __TRACKERD_XESAM_LIVE_SEARCH_H__

#include <glib.h>
#include <glib-object.h>

#include <libtracker-db/tracker-db-interface-sqlite.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_XESAM_LIVE_SEARCH (tracker_xesam_live_search_get_type ())
#define TRACKER_XESAM_LIVE_SEARCH(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_XESAM_LIVE_SEARCH, TrackerXesamLiveSearch))
#define TRACKER_XESAM_LIVE_SEARCH_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_XESAM_LIVE_SEARCH, TrackerXesamLiveSearchClass))
#define TRACKER_IS_XESAM_LIVE_SEARCH(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_XESAM_LIVE_SEARCH))
#define TRACKER_IS_XESAM_LIVE_SEARCH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_XESAM_LIVE_SEARCH))
#define TRACKER_XESAM_LIVE_SEARCH_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_XESAM_LIVE_SEARCH, TrackerXesamLiveSearchClass))

typedef struct _TrackerXesamLiveSearch TrackerXesamLiveSearch;
typedef struct _TrackerXesamLiveSearchClass TrackerXesamLiveSearchClass;
typedef struct _TrackerXesamLiveSearchPriv TrackerXesamLiveSearchPriv;

typedef enum {
	MATCH_WITH_EVENTS_CREATES = 1<<0,
	MATCH_WITH_EVENTS_DELETES = 1<<1,
	MATCH_WITH_EVENTS_MODIFIES = 1<<2
} MatchWithEventsFlags;

#define MATCH_WITH_EVENTS_ALL_FLAGS (MATCH_WITH_EVENTS_CREATES|MATCH_WITH_EVENTS_DELETES|MATCH_WITH_EVENTS_MODIFIES)

struct _TrackerXesamLiveSearch {
	GObject parent_instance;
	TrackerXesamLiveSearchPriv * priv;
};
struct _TrackerXesamLiveSearchClass {
	GObjectClass parent_class;
};

TrackerXesamLiveSearch *
	     tracker_xesam_live_search_new		  (const gchar		   *query_xml);
GType	     tracker_xesam_live_search_get_type		  (void);
void	     tracker_xesam_live_search_set_id		  (TrackerXesamLiveSearch  *self,
							   const gchar		   *search_id);
const gchar* tracker_xesam_live_search_get_id		  (TrackerXesamLiveSearch  *self);
const gchar* tracker_xesam_live_search_get_where_query	  (TrackerXesamLiveSearch  *self);
const gchar* tracker_xesam_live_search_get_from_query	  (TrackerXesamLiveSearch  *self);
const gchar* tracker_xesam_live_search_get_join_query	  (TrackerXesamLiveSearch  *self);
const gchar* tracker_xesam_live_search_get_xml_query	  (TrackerXesamLiveSearch  *self);
void	     tracker_xesam_live_search_set_xml_query	  (TrackerXesamLiveSearch  *self,
							   const gchar		   *xml_query);
void	     tracker_xesam_live_search_set_session	  (TrackerXesamLiveSearch  *self,
							   gpointer		    session);
void	     tracker_xesam_live_search_set_session	  (TrackerXesamLiveSearch  *self,
							   gpointer		    session);
void	     tracker_xesam_live_search_activate		  (TrackerXesamLiveSearch  *self,
							   GError		  **error);
gboolean     tracker_xesam_live_search_is_active	  (TrackerXesamLiveSearch  *self);
gboolean     tracker_xesam_live_search_parse_query	  (TrackerXesamLiveSearch  *self,
							   GError		  **error);
void	     tracker_xesam_live_search_get_hit_data	  (TrackerXesamLiveSearch  *self,
							   GArray		   *hit_ids,
							   GStrv		    fields,
							   GPtrArray		  **hit_data,
							   GError		  **error);
void	     tracker_xesam_live_search_get_hits		  (TrackerXesamLiveSearch  *self,
							   guint		    count,
							   GPtrArray		  **hits,
							   GError		  **error);
void	     tracker_xesam_live_search_get_range_hit_data (TrackerXesamLiveSearch  *self,
							   guint		    a,
							   guint		    b,
							   GStrv		    fields,
							   GPtrArray		  **hit_data,
							   GError		  **error);
void	     tracker_xesam_live_search_get_range_hits	  (TrackerXesamLiveSearch  *self,
							   guint		    a,
							   guint		    b,
							   GPtrArray		  **hits,
							   GError		  **error);
void	     tracker_xesam_live_search_get_hit_count	  (TrackerXesamLiveSearch  *self,
							   guint		   *count,
							   GError		  **error);
void	     tracker_xesam_live_search_close		  (TrackerXesamLiveSearch  *self,
							   GError		  **error);
void	     tracker_xesam_live_search_emit_hits_added	  (TrackerXesamLiveSearch  *self,
							   guint		    count);
void	     tracker_xesam_live_search_emit_hits_removed  (TrackerXesamLiveSearch  *self,
							   GArray		   *hit_ids);
void	     tracker_xesam_live_search_emit_hits_modified (TrackerXesamLiveSearch  *self,
							   GArray		   *hit_ids);
void	     tracker_xesam_live_search_emit_done	  (TrackerXesamLiveSearch  *self);
void	     tracker_xesam_live_search_match_with_events  (TrackerXesamLiveSearch  *self,
							   MatchWithEventsFlags     flags,
							   GArray		  **added,
							   GArray		  **removed,
							   GArray		  **modified);

G_END_DECLS

#endif /* __TRACKER_XESAM_LIVE_SEARCH_H__ */
