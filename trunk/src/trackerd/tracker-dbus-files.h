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



#ifndef _TRACKER_DBUS_FILES_H_
#define _TRACKER_DBUS_FILES_H_

#include "tracker-dbus.h"

void		tracker_dbus_method_files_exists 		(DBusRec *rec);
void		tracker_dbus_method_files_create		(DBusRec *rec);
void		tracker_dbus_method_files_delete		(DBusRec *rec);
void		tracker_dbus_method_files_get_service_type 	(DBusRec *rec);
void		tracker_dbus_method_files_get_text_contents	(DBusRec *rec);
void		tracker_dbus_method_files_search_text_contents	(DBusRec *rec);
void		tracker_dbus_method_files_get_mtime		(DBusRec *rec);
void		tracker_dbus_method_files_get_by_service_type	(DBusRec *rec);
void		tracker_dbus_method_files_get_by_mime_type	(DBusRec *rec);
void		tracker_dbus_method_files_get_by_mime_type_vfs	(DBusRec *rec);

void		tracker_dbus_method_files_get_metadata_for_files_in_folder 	(DBusRec *rec);
void		tracker_dbus_method_files_search_by_text_mime			(DBusRec *rec);
void		tracker_dbus_method_files_search_by_text_location		(DBusRec *rec);
void		tracker_dbus_method_files_search_by_text_mime_location		(DBusRec *rec);

#endif
