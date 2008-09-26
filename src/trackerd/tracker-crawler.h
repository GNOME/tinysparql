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

#ifndef __TRACKERD_CRAWLER_H__
#define __TRACKERD_CRAWLER_H__

#include <glib-object.h>

#include <libtracker-common/tracker-config.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_CRAWLER		(tracker_crawler_get_type ())
#define TRACKER_CRAWLER(object)		(G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_CRAWLER, TrackerCrawler))
#define TRACKER_CRAWLER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_CRAWLER, TrackerCrawlerClass))
#define TRACKER_IS_CRAWLER(object)	(G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_CRAWLER))
#define TRACKER_IS_CRAWLER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_CRAWLER))
#define TRACKER_CRAWLER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_CRAWLER, TrackerCrawlerClass))

typedef struct _TrackerCrawler	       TrackerCrawler;
typedef struct _TrackerCrawlerClass    TrackerCrawlerClass;
typedef struct _TrackerCrawlerPrivate  TrackerCrawlerPrivate;

struct _TrackerCrawler {
	GObject		       parent;
	TrackerCrawlerPrivate *private;
};

struct _TrackerCrawlerClass {
	GObjectClass	       parent;
};

GType		tracker_crawler_get_type	     (void);
TrackerCrawler *tracker_crawler_new		     (TrackerConfig  *config,
						      const gchar    *module_name);
gboolean	tracker_crawler_start		     (TrackerCrawler *crawler);
void		tracker_crawler_stop		     (TrackerCrawler *crawler);
gboolean	tracker_crawler_is_path_ignored      (TrackerCrawler *crawler,
						      const gchar    *path,
						      gboolean	      is_directory);

/* Convenience API for old .cfg file */
void		tracker_crawler_add_path	     (TrackerCrawler *crawler,
						      const gchar    *path);
void		tracker_crawler_set_use_module_paths (TrackerCrawler *crawler,
						      gboolean	      use_paths);

G_END_DECLS

#endif /* __TRACKERD_CRAWLER_H__ */
