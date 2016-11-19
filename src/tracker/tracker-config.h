/*
 * Copyright (C) 2014, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2014, Lanedo <martyn@lanedo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <glib.h>
#include <gio/gio.h>

#ifndef __TRACKER_CONFIG_H__
#define __TRACKER_CONFIG_H__

typedef struct {
	gchar *name;
	GSettingsSchema *schema;
	GSettings *settings;
	gboolean is_miner;
} ComponentGSettings;

GSList   *tracker_gsettings_get_all (gint *longest_name_length);
gboolean  tracker_gsettings_set_all (GSList           *all,
                                     TrackerVerbosity  verbosity);
void      tracker_gsettings_free    (GSList *all);

#endif /* __TRACKER_CONFIG_H__ */
