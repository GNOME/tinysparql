/* Tracker
 * Copyright (C) 2005, Mr Jamie McCracken
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#define MAX_FILE_WATCHES 400

gboolean 	tracker_start_watching 		(void);
void     	tracker_end_watching 		(void);

gboolean 	tracker_add_watch_dir 		(const char *dir);
void     	tracker_remove_watch_dir 	(const char *dir, gboolean delete_subdirs);  

gboolean 	tracker_is_directory_watched 	(const char * dir);
int		tracker_count_watch_dirs 	();


