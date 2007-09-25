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

#ifndef _TRACKER_DB_H_
#define _TRACKER_DB_H_

#include <glib.h>

#include "tracker-utils.h"

#include "config.h"

#include "tracker-db-sqlite.h"

gboolean	tracker_db_is_file_up_to_date 	(DBConnection *db_con, const char *uri, guint32 *id);
FileInfo *	tracker_db_get_file_info	(DBConnection *db_con, FileInfo *info);
gboolean	tracker_is_valid_service	(DBConnection *db_con, const char *service);
char *		tracker_db_get_id		(DBConnection *db_con, const char *service, const char *uri);
GHashTable *	tracker_db_save_metadata	(DBConnection *db_con, GHashTable *table, GHashTable *index_table, const char *service, guint32 file_id, gboolean new_file);
void		tracker_db_save_thumbs		(DBConnection *db_con, const char *small_thumb, const char *large_thumb, guint32 file_id);
char **		tracker_db_get_files_in_folder	(DBConnection *db_con, const char *folder_uri);
FieldDef *	tracker_db_get_field_def	(DBConnection *db_con, const char *field_name);
void		tracker_db_free_field_def	(FieldDef *def);
gboolean	tracker_metadata_is_date 	(DBConnection *db_con, const char *meta);
FileInfo *	tracker_db_get_pending_file	(DBConnection *db_con, const char *uri);
void		tracker_db_update_pending_file	(DBConnection *db_con, const char *uri, int counter, TrackerChangeAction action);
void		tracker_db_add_to_extract_queue	(DBConnection *db_con, FileInfo *info);
gboolean	tracker_db_has_pending_files	(DBConnection *db_con);
gboolean	tracker_db_has_pending_metadata	(DBConnection *db_con);

void		tracker_db_index_service 	(DBConnection *db_con, FileInfo *info, const char *service, GHashTable *meta_table, const char *attachment_uri, const char *attachment_service, 
						 gboolean get_embedded, gboolean get_full_text, gboolean get_thumbs);

void		tracker_db_index_file 		(DBConnection *db_con, FileInfo *info, const char *attachment_uri, const char *attachment_service);
void		tracker_db_index_conversation 	(DBConnection *db_con, FileInfo *info);
void		tracker_db_index_application 	(DBConnection *db_con, FileInfo *info);

#endif
