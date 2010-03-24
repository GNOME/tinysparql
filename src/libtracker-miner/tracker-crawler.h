/*
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __LIBTRACKER_MINER_CRAWLER_H__
#define __LIBTRACKER_MINER_CRAWLER_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_CRAWLER            (tracker_crawler_get_type ())
#define TRACKER_CRAWLER(object)                 (G_TYPE_CHECK_INSTANCE_CAST ((object), TRACKER_TYPE_CRAWLER, TrackerCrawler))
#define TRACKER_CRAWLER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_CRAWLER, TrackerCrawlerClass))
#define TRACKER_IS_CRAWLER(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TRACKER_TYPE_CRAWLER))
#define TRACKER_IS_CRAWLER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_CRAWLER))
#define TRACKER_CRAWLER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRACKER_TYPE_CRAWLER, TrackerCrawlerClass))

/* Max timeouts time (in msec) */
#define MAX_TIMEOUT_INTERVAL 1000

typedef struct TrackerCrawler         TrackerCrawler;
typedef struct TrackerCrawlerClass    TrackerCrawlerClass;
typedef struct TrackerCrawlerPrivate  TrackerCrawlerPrivate;

struct TrackerCrawler {
	GObject parent;
	TrackerCrawlerPrivate *private;
};

struct TrackerCrawlerClass {
	GObjectClass parent;

	gboolean (* check_directory)          (TrackerCrawler *crawler,
	                                       GFile          *file);
	gboolean (* check_file)               (TrackerCrawler *crawler,
	                                       GFile          *file);
	gboolean (* check_directory_contents) (TrackerCrawler *crawler,
	                                       GFile          *file,
	                                       GList          *contents);
	void     (* directory_crawled)        (TrackerCrawler *crawler,
	                                       GFile          *directory,
	                                       GNode          *tree,
	                                       guint           directories_found,
	                                       guint           directories_ignored,
	                                       guint           files_found,
	                                       guint           files_ignored);
	void     (* finished)                 (TrackerCrawler *crawler,
	                                       gboolean        interrupted);
};

GType           tracker_crawler_get_type     (void);
TrackerCrawler *tracker_crawler_new          (void);
gboolean        tracker_crawler_start        (TrackerCrawler *crawler,
                                              GFile          *file,
                                              gboolean        recurse);
void            tracker_crawler_stop         (TrackerCrawler *crawler);
void            tracker_crawler_pause        (TrackerCrawler *crawler);
void            tracker_crawler_resume       (TrackerCrawler *crawler);
void            tracker_crawler_set_throttle (TrackerCrawler *crawler,
                                              gdouble         throttle);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_CRAWLER_H__ */
