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

#ifndef _TRACKER_SQLITE_DB_H_
#define _TRACKER_SQLITE_DB_H_

#include <glib.h>

typedef struct {

	GHashTable *statements;

} DBConnection;
 


DBConnection * 	tracker_db_connect 		();
gboolean	tracker_update_db 		();
char *		tracker_escape_string 		(DBConnection *db_con, const char *in);
void		tracker_db_prepare_queries 	(DBConnection *db_con);
char ***	tracker_exec_proc 		(DBConnection *db_con, const char *procedure, int param_count, ...);
char ***	tracker_exec_sql   		(DBConnection *db_con, const char *query);
void		tracker_log_sql	   		(DBConnection *db_con, const char *query);
void		tracker_create_db  		();
void		tracker_db_load_stored_procs 	(DBConnection *db_con);
void		tracker_db_save_file_contents	(DBConnection *db_con, const char *file_name, long file_id);

 
#endif
