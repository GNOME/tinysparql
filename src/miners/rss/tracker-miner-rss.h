/*
 * Copyright (C) 2009/2010, Roberto Guido <madbob@users.barberaware.org>
 *                          Michele Tameni <michele@amdplanet.it>
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

#ifndef __TRACKER_MINER_RSS_H__
#define __TRACKER_MINER_RSS_H__

#include <libtracker-miner/tracker-miner.h>

#include <libtracker-sparql/tracker-sparql.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_MINER_RSS         (tracker_miner_rss_get_type())
#define TRACKER_MINER_RSS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_RSS, TrackerMinerRSS))
#define TRACKER_MINER_RSS_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_MINER_RSS, TrackerMinerRSSClass))
#define TRACKER_IS_MINER_RSS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_RSS))
#define TRACKER_IS_MINER_RSS_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TRACKER_TYPE_MINER_RSS))
#define TRACKER_MINER_RSS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_MINER_RSS, TrackerMinerRSSClass))

typedef struct _TrackerMinerRSS TrackerMinerRSS;
typedef struct _TrackerMinerRSSClass TrackerMinerRSSClass;

struct _TrackerMinerRSS {
	TrackerMiner parent;
};

struct _TrackerMinerRSSClass {
	TrackerMinerClass parent;
};

GType    tracker_miner_rss_get_type         (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __TRACKER_MINER_RSS_H__ */
