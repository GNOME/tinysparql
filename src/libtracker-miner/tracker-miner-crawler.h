/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#ifndef __TRACKER_MINER_CRAWLER_H__
#define __TRACKER_MINER_CRAWLER_H__

#include "config.h"

#include "tracker-miner.h"
#include <gio/gio.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER_CRAWLER         (tracker_miner_crawler_get_type())
#define TRACKER_MINER_CRAWLER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_CRAWLER, TrackerMinerCrawler))
#define TRACKER_MINER_CRAWLER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_MINER_CRAWLER, TrackerMinerCrawlerClass))
#define TRACKER_IS_MINER_CRAWLER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_CRAWLER))
#define TRACKER_IS_MINER_CRAWLER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_MINER_CRAWLER))
#define TRACKER_MINER_CRAWLER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MINER_CRAWLER, TrackerMinerCrawlerClass))

typedef struct TrackerMinerCrawler TrackerMinerCrawler;
typedef struct TrackerMinerCrawlerClass TrackerMinerCrawlerClass;

struct TrackerMinerCrawler {
        TrackerMiner parent_instance;
        gpointer _priv;
};

struct TrackerMinerCrawlerClass {
        TrackerMinerClass parent_class;

	gboolean (* check_file) (TrackerMinerCrawler *miner,
				 GFile               *file);
	gboolean (* process_file) (TrackerMinerCrawler *miner,
				   GFile               *file);
};

GType    tracker_miner_crawler_get_type    (void) G_GNUC_CONST;

void     tracker_miner_crawler_add_directory (TrackerMinerCrawler *miner,
					      const gchar         *directory_uri,
					      gboolean             monitor,
					      gboolean             recurse);

void     tracker_miner_crawler_set_ignore_directory_patterns (TrackerMinerCrawler *miner,
							      GList               *patterns);
void     tracker_miner_crawler_set_ignore_file_patterns      (TrackerMinerCrawler *miner,
							      GList               *patterns);


G_END_DECLS

#endif /* __TRACKER_MINER_CRAWLER_H__ */
