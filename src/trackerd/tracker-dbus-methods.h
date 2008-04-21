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



#ifndef _TRACKER_DBUS_METHODS_H_
#define _TRACKER_DBUS_METHODS_H_

#include "tracker-dbus.h"
#include "tracker-db.h"
#include "tracker-db-interface.h"


void	tracker_set_error			(DBusRec *rec, const char *fmt, ...);

char *	tracker_get_metadata			(DBConnection *db_con, const char *service, const char *id, const char *key);

void	tracker_set_metadata			(DBConnection *db_con, const char *service, const char *id, const char *key, const char *value, gboolean overwrite);

guint32	tracker_get_file_id 			(DBConnection *db_con, const char *uri, gboolean create_record);

void	tracker_dbus_reply_with_query_result 	(DBusRec *rec, TrackerDBResultSet *result_set);

void	tracker_add_query_result_to_dict 	(TrackerDBResultSet *result_set, DBusMessageIter *iter_dict);

char *	tracker_format_search_terms 		(const char *str, gboolean *do_bool_search);


char **	tracker_get_query_result_as_array 	(TrackerDBResultSet *result_set, int *row_count);

void	tracker_dbus_method_get_stats	 	(DBusRec *rec);
void	tracker_dbus_method_get_status 		(DBusRec *rec);
void	tracker_dbus_method_get_services 	(DBusRec *rec);
void	tracker_dbus_method_get_version 	(DBusRec *rec);

void	tracker_dbus_method_set_bool_option		(DBusRec *rec);
void	tracker_dbus_method_set_int_option		(DBusRec *rec);
void	tracker_dbus_method_shutdown 			(DBusRec *rec);	
void	tracker_dbus_method_prompt_index_signals 	(DBusRec *rec);


#endif
