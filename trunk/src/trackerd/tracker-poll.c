/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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
#include <string.h>
#include <unistd.h>
#include <glib.h>

#include "tracker-watch.h"

extern Tracker  *tracker;

gboolean 	tracker_start_watching 		(void){tracker->watch_limit = 0; return TRUE;}
void     	tracker_end_watching 		(void){return;}

gboolean 	tracker_add_watch_dir 		(const gchar *dir, DBConnection *db_con){return FALSE;}
void     	tracker_remove_watch_dir 	(const gchar *dir, gboolean delete_subdirs, DBConnection *db_con) {return;}
gboolean 	tracker_is_directory_watched 	(const gchar *dir, DBConnection *db_con) {return FALSE;}
int		tracker_count_watch_dirs 	(void) {return 0;}

