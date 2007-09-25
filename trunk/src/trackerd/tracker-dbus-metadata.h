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


#ifndef _TRACKER_DBUS_METADATA_H_
#define _TRACKER_DBUS_METADATA_H_

#include "tracker-dbus.h"

void	tracker_dbus_method_metadata_get 			(DBusRec *rec);
void	tracker_dbus_method_metadata_set 			(DBusRec *rec);

void	tracker_dbus_method_metadata_register_type 		(DBusRec *rec);

void	tracker_dbus_method_metadata_get_type_details 		(DBusRec *rec);
void	tracker_dbus_method_metadata_get_registered_types	(DBusRec *rec);
void	tracker_dbus_method_metadata_get_writeable_types	(DBusRec *rec);
void	tracker_dbus_method_metadata_get_registered_classes	(DBusRec *rec);

#endif
