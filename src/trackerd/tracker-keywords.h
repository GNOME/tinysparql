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

#ifndef __TRACKERD_KEYWORDS_H__
#define __TRACKERD_KEYWORDS_H__

#include <glib-object.h>

#define TRACKER_KEYWORDS_SERVICE	 "org.freedesktop.Tracker"
#define TRACKER_KEYWORDS_PATH		 "/org/freedesktop/Tracker/Keywords"
#define TRACKER_KEYWORDS_INTERFACE	 "org.freedesktop.Tracker.Keywords"

G_BEGIN_DECLS

#define TRACKER_TYPE_KEYWORDS		 (tracker_keywords_get_type ())
#define TRACKER_KEYWORDS(object)	 (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_KEYWORDS, TrackerKeywords))
#define TRACKER_KEYWORDS_CLASS(klass)	 (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_KEYWORDS, TrackerKeywordsClass))
#define TRACKER_IS_KEYWORDS(object)	 (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_KEYWORDS))
#define TRACKER_IS_KEYWORDS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_KEYWORDS))
#define TRACKER_KEYWORDS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_KEYWORDS, TrackerKeywordsClass))

typedef struct TrackerKeywords	    TrackerKeywords;
typedef struct TrackerKeywordsClass TrackerKeywordsClass;

struct TrackerKeywords {
	GObject parent;
};

struct TrackerKeywordsClass {
	GObjectClass parent;
};

GType		 tracker_keywords_get_type   (void);
TrackerKeywords *tracker_keywords_new	     (void);
void		 tracker_keywords_get_list   (TrackerKeywords	     *object,
					      const gchar	     *service_type,
					      DBusGMethodInvocation  *context,
					      GError		    **error);
void		 tracker_keywords_get	     (TrackerKeywords	     *object,
					      const gchar	     *service_type,
					      const gchar	     *uri,
					      DBusGMethodInvocation  *context,
					      GError		    **error);
void		 tracker_keywords_add	     (TrackerKeywords	     *object,
					      const gchar	     *service_type,
					      const gchar	     *uri,
					      gchar		    **keywords,
					      DBusGMethodInvocation  *context,
					      GError		    **error);
void		 tracker_keywords_remove     (TrackerKeywords	     *object,
					      const gchar	     *service_type,
					      const gchar	     *uri,
					      gchar		    **keywords,
					      DBusGMethodInvocation  *context,
					      GError		    **error);
void		 tracker_keywords_remove_all (TrackerKeywords	     *object,
					      const gchar	     *service_type,
					      const gchar	     *uri,
					      DBusGMethodInvocation  *context,
					      GError		    **error);
void		 tracker_keywords_search     (TrackerKeywords	     *object,
					      gint		      live_query_id,
					      const gchar	     *service_type,
					      const gchar	    **keywords,
					      gint		      offset,
					      gint		      max_hits,
					      DBusGMethodInvocation  *context,
					      GError		    **error);


G_END_DECLS

#endif /* __TRACKERD_KEYWORDS_H__ */
