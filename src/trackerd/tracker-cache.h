/* Tracker
 * routines for cacheing 
 * Copyright (C) 2007, Jamie McCracken 
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

#ifndef _TRACKER_CACHE_H_
#define _TRACKER_CACHE_H_

#include "tracker-db-sqlite.h"


typedef struct
{
	GSList 	*new_file_list;
	int	new_file_count;
	GSList 	*new_email_list;
	int	new_email_count;
	GSList 	*update_file_list;

} Cache;


void		tracker_cache_add 		(const char *word, guint32 service_id, int service_type, int score, gboolean is_new);
void		tracker_flush_all_words 	(DBConnection *db_con);
void		tracker_cache_flush 		(DBConnection *db_con);



#endif
