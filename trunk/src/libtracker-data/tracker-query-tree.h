/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#ifndef __TRACKERD_QUERY_TREE_H__
#define __TRACKERD_QUERY_TREE_H__

#include <glib.h>

G_BEGIN_DECLS

#include <glib-object.h>

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-language.h>

#define TRACKER_TYPE_QUERY_TREE		(tracker_query_tree_get_type())
#define TRACKER_QUERY_TREE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_QUERY_TREE, TrackerQueryTree))
#define TRACKER_QUERY_TREE_CLASS(c)	(G_TYPE_CHECK_CLASS_CAST ((c),	  TRACKER_TYPE_QUERY_TREE, TrackerQueryTreeClass))
#define TRACKER_IS_QUERY_TREE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_QUERY_TREE))
#define TRACKER_IS_QUERY_TREE_CLASS(c)	(G_TYPE_CHECK_CLASS_TYPE ((c),	  TRACKER_TYPE_QUERY_TREE))
#define TRACKER_QUERY_TREE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_QUERY_TREE, TrackerQueryTreeClass))

typedef struct TrackerQueryTree TrackerQueryTree;
typedef struct TrackerQueryTreeClass TrackerQueryTreeClass;
typedef struct TrackerHitCount TrackerHitCount;

struct TrackerQueryTree {
	GObject parent;
};

struct TrackerQueryTreeClass {
	GObjectClass parent_class;
};

struct TrackerHitCount {
	guint service_type_id;
	guint count;
};

GType		      tracker_query_tree_get_type	(void);
TrackerQueryTree *    tracker_query_tree_new		(const gchar	  *query_str,
							 TrackerConfig	  *config,
							 TrackerLanguage  *language,
							 GArray		  *services);
G_CONST_RETURN gchar *tracker_query_tree_get_query	(TrackerQueryTree *tree);
void		      tracker_query_tree_set_query	(TrackerQueryTree *tree,
							 const gchar	  *query_str);

GArray *	      tracker_query_tree_get_services	(TrackerQueryTree *tree);
void		      tracker_query_tree_set_services	(TrackerQueryTree *tree,
							 GArray		  *services);
GSList *	      tracker_query_tree_get_words	(TrackerQueryTree *tree);
GArray *	      tracker_query_tree_get_hits	(TrackerQueryTree *tree,
							 guint		   offset,
							 guint		   limit);
gint		      tracker_query_tree_get_hit_count	(TrackerQueryTree *tree);
GArray *	      tracker_query_tree_get_hit_counts (TrackerQueryTree *tree);

G_END_DECLS

#endif /* __TRACKERD_QUERY_TREE_H__ */
