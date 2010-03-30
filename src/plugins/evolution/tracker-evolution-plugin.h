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

#ifndef __TRACKER_EVOLUTION_PLUGIN_H__
#define __TRACKER_EVOLUTION_PLUGIN_H__

#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libtracker-miner/tracker-miner.h>

#define TRACKER_TYPE_EVOLUTION_PLUGIN          (tracker_evolution_plugin_get_type())
#define TRACKER_EVOLUTION_PLUGIN(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_EVOLUTION_PLUGIN, TrackerEvolutionPlugin))
#define TRACKER_EVOLUTION_PLUGIN_CLASS(c)      (G_TYPE_CHECK_CLASS_CAST ((c), TRACKER_TYPE_EVOLUTION_PLUGIN, TrackerEvolutionPluginClass))
#define TRACKER_EVOLUTION_PLUGIN_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), TRACKER_TYPE_EVOLUTION_PLUGIN, TrackerEvolutionPluginClass))

G_BEGIN_DECLS

typedef struct TrackerEvolutionPlugin TrackerEvolutionPlugin;
typedef struct TrackerEvolutionPluginClass TrackerEvolutionPluginClass;

struct TrackerEvolutionPlugin {
	TrackerMiner parent;
};

struct TrackerEvolutionPluginClass {
	TrackerMinerClass parent;
};

GType tracker_evolution_plugin_get_type (void);


G_END_DECLS

#endif /* __TRACKER_EVOLUTION_PLUGIN_H__ */
