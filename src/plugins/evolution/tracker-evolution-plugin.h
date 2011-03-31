/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __TRACKER_MINER_EVOLUTION_H__
#define __TRACKER_MINER_EVOLUTION_H__

#include <libtracker-miner/tracker-miner.h>

#define TRACKER_TYPE_MINER_EVOLUTION          (tracker_miner_evolution_get_type())
#define TRACKER_MINER_EVOLUTION(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_EVOLUTION, TrackerMinerEvolution))
#define TRACKER_MINER_EVOLUTION_CLASS(c)      (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_MINER_EVOLUTION, TrackerMinerEvolutionClass))
#define TRACKER_MINER_EVOLUTION_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_MINER_EVOLUTION, TrackerMinerEvolutionClass))

G_BEGIN_DECLS

typedef struct TrackerMinerEvolution TrackerMinerEvolution;
typedef struct TrackerMinerEvolutionClass TrackerMinerEvolutionClass;

struct TrackerMinerEvolution {
	TrackerMiner parent;
};

struct TrackerMinerEvolutionClass {
	TrackerMinerClass parent;
};

GType tracker_miner_evolution_get_type (void);


G_END_DECLS

#endif /* __TRACKER_MINER_EVOLUTION_H__ */
