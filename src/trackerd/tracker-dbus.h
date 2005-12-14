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

#define DBUS_API_SUBJECT_TO_CHANGE

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

#define TRACKER_FILE_METADATA_SERVICE                   "org.freedesktop.file.metadata"
#define TRACKER_FILE_METADATA_OBJECT			"/org/freedesktop/file/metadata"
#define TRACKER_FILE_METADATA_INTERFACE			"org.freedesktop.file.metadata"

#define TRACKER_METHOD_GET_METADATA                  	"GetMetadata"
#define TRACKER_METHOD_SET_METADATA                  	"SetMetadata"
#define TRACKER_METHOD_REGISTER_METADATA_TYPE       	"RegisterMetadataType"
#define TRACKER_METHOD_METADATA_TYPE_EXISTS       	"MetadataTypeExists"
#define TRACKER_METHOD_GET_MULTPLE_METADATA           	"GetMultipleMetadata"
#define TRACKER_METHOD_SET_MULTIPLE_METADATA         	"SetMultipleMetadata"
#define TRACKER_METHOD_GET_METADATA_FOR_FILES           "GetMetadataForFilesInFolder"
#define TRACKER_METHOD_REFRESH_METADATA                 "RefreshMetadata"
#define TRACKER_METHOD_IS_FILE_UP_TO_DATE		"IsMetadataUpToDate"
#define TRACKER_METHOD_SEARCH_METADATA_BY_QUERY         "SearchMetadataByQuery"
#define TRACKER_METHOD_SEARCH_METADATA_DETAILED         "SearchMetadataDetailed"
#define TRACKER_METHOD_SEARCH_METADATA_BY_TEXT	        "SearchMetadataByText"
#define TRACKER_METHOD_ADD_KEYWORD 			"AddKeyword"
#define TRACKER_METHOD_REMOVE_KEYWORD			"RemoveKeyword"
#define TRACKER_METHOD_GET_KEYWORDS			"GetKeywords"

#define TRACKER_SIGNAL_BASIC_METADATA_CHANGED		"FileBasicMetaDataChanged"
#define TRACKER_SIGNAL_EMBEDDED_METADATA_CHANGED	"FileEmbeddedMetaDataChanged"
#define TRACKER_SIGNAL_EDITABLE_METADATA_CHANGED	"FileEditableMetaDataChanged"
#define TRACKER_SIGNAL_THUMBNAIL_CHANGED		"FileThumbnailChanged"

typedef enum {
	DBUS_ACTION_NONE,
	DBUS_ACTION_GET_METADATA,
	DBUS_ACTION_SET_METADATA,
	DBUS_ACTION_REGISTER_METADATA,
	DBUS_ACTION_METADATA_EXISTS,
	DBUS_ACTION_GET_MULTIPLE_METADATA,
	DBUS_ACTION_SET_MULTIPLE_METADATA,
	DBUS_ACTION_GET_METADATA_FOR_FILES,
	DBUS_ACTION_REFRESH_METADATA,
	DBUS_ACTION_METADATA_UPTODATE,
	DBUS_ACTION_SEARCH_BY_QUERY,
	DBUS_ACTION_SEARCH_DETAILED,
	DBUS_ACTION_SEARCH_BY_TEXT,
	DBUS_ACTION_ADD_KEYWORD,
	DBUS_ACTION_REMOVE_KEYWORD,
	DBUS_ACTION_GET_KEYWORDS

} DBusAction;




typedef struct {
	DBusConnection *connection;
	DBusMessage    *message;
	DBusAction      action;
	gpointer	user_data;
} DBusRec;


DBusConnection*  	tracker_dbus_init    	(void);
void              	tracker_dbus_shutdown   (DBusConnection *conn);



