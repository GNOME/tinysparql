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

#ifndef _TRACKER_DBUS_H_
#define _TRACKER_DBUS_H_

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>


#define TRACKER_SERVICE					"org.freedesktop.Tracker"
#define TRACKER_OBJECT					"/org/freedesktop/tracker"
#define TRACKER_INTERFACE				"org.freedesktop.Tracker"
#define TRACKER_INTERFACE_METADATA			"org.freedesktop.Tracker.Metadata"
#define TRACKER_INTERFACE_KEYWORDS			"org.freedesktop.Tracker.Keywords"
#define TRACKER_INTERFACE_SEARCH			"org.freedesktop.Tracker.Search"
#define TRACKER_INTERFACE_FILES				"org.freedesktop.Tracker.Files"
#define TRACKER_INTERFACE_MUSIC				"org.freedesktop.Tracker.Music"
#define TRACKER_INTERFACE_PLAYLISTS			"org.freedesktop.Tracker.PlayLists"

/* main service interface */
#define TRACKER_METHOD_PING		               	"Ping"
#define TRACKER_METHOD_GET_STATS	               	"GetStats"
#define TRACKER_METHOD_GET_SERVICES	               	"GetServices"
#define TRACKER_METHOD_GET_VERSION	               	"GetVersion"

/* metadata interface */
#define TRACKER_METHOD_METADATA_GET                  	"Get"
#define TRACKER_METHOD_METADATA_SET                  	"Set"
#define TRACKER_METHOD_METADATA_REGISTER_TYPE       	"RegisterType"
#define TRACKER_METHOD_METADATA_GET_TYPE_DETAILS       	"GetTypeDetails"
#define TRACKER_METHOD_METADATA_GET_REGISTERED_TYPES   	"GetRegisteredTypes"
#define TRACKER_METHOD_METADATA_GET_WRITEABLE_TYPES   	"GetWriteableTypes"
#define TRACKER_METHOD_METADATA_GET_REGISTERED_CLASSES 	"GetRegisteredClasses"

/* keywords interface */
#define TRACKER_METHOD_KEYWORDS_GET_LIST		"GetList"
#define TRACKER_METHOD_KEYWORDS_GET			"Get"
#define TRACKER_METHOD_KEYWORDS_ADD 			"Add"
#define TRACKER_METHOD_KEYWORDS_REMOVE			"Remove"
#define TRACKER_METHOD_KEYWORDS_REMOVE_ALL		"RemoveAll"
#define TRACKER_METHOD_KEYWORDS_SEARCH			"Search"

/* Search interface */
#define TRACKER_METHOD_SEARCH_TEXT	        	"Text"
#define TRACKER_METHOD_SEARCH_TEXT_DETAILED        	"TextDetailed"
#define TRACKER_METHOD_SEARCH_GET_SNIPPET	        "GetSnippet"
#define TRACKER_METHOD_SEARCH_FILES_BY_TEXT        	"FilesByText"
#define TRACKER_METHOD_SEARCH_METADATA	        	"Metadata"
#define TRACKER_METHOD_SEARCH_MATCHING_FIELDS        	"MatchingFields"
#define TRACKER_METHOD_SEARCH_QUERY         		"Query"

/* File Interface */
#define TRACKER_METHOD_FILES_EXISTS	        	"Exists"
#define TRACKER_METHOD_FILES_CREATE	        	"Create"
#define TRACKER_METHOD_FILES_DELETE	        	"Delete"
#define TRACKER_METHOD_FILES_GET_SERVICE_TYPE        	"GetServiceType"
#define TRACKER_METHOD_FILES_GET_TEXT_CONTENTS        	"GetTextContents"
#define TRACKER_METHOD_FILES_SEARCH_TEXT_CONTENTS      	"SearchTextContents"
#define TRACKER_METHOD_FILES_GET_BY_SERVICE_TYPE      	"GetByServiceType"
#define TRACKER_METHOD_FILES_GET_BY_MIME_TYPE      	"GetByMimeType"
#define TRACKER_METHOD_FILES_GET_BY_MIME_TYPE_VFS      	"GetByMimeTypeVFS"
#define TRACKER_METHOD_FILES_REFRESH_METADATA      	"RefreshMetadata"
#define TRACKER_METHOD_FILES_GET_MTIME   	   	"GetMTime"
#define TRACKER_METHOD_FILES_GET_METADATA_FOLDER_FILES  "GetMetadataForFilesInFolder"

/* Deprecated calls */
#define TRACKER_METHOD_FILES_SEARCH_BY_TEXT_MIME     		"SearchByTextAndMime"
#define TRACKER_METHOD_FILES_SEARCH_BY_TEXT_MIME_LOCATION	"SearchByTextAndMimeAndLocation"
#define TRACKER_METHOD_FILES_SEARCH_BY_TEXT_LOCATION	        "SearchByTextAndLocation"


/* signals */
#define TRACKER_SIGNAL_METADATA_CHANGED			"Changed"
#define TRACKER_SIGNAL_KEYWORD_CHANGED			"Changed"
#define TRACKER_SIGNAL_FILE_CREATED			"Created"
#define TRACKER_SIGNAL_FILE_DELETED			"FileDeleted"
#define TRACKER_SIGNAL_DIRECTORY_DELETED		"DirectoryDeleted"
#define TRACKER_SIGNAL_FILE_MOVED			"Moved"
#define TRACKER_SIGNAL_FILE_EDITED			"Edited"
#define TRACKER_SIGNAL_FILE_THUMBNAIL_CHANGED		"ThumbnailChanged"


typedef enum {

	DBUS_ACTION_NONE,

	DBUS_ACTION_PING,
	DBUS_ACTION_GET_SERVICES,
	DBUS_ACTION_GET_STATS,
	DBUS_ACTION_GET_VERSION,

	DBUS_ACTION_METADATA_GET,
	DBUS_ACTION_METADATA_SET,
	DBUS_ACTION_METADATA_REGISTER_TYPE,
	DBUS_ACTION_METADATA_GET_TYPE_DETAILS,
	DBUS_ACTION_METADATA_GET_REGISTERED_TYPES,
	DBUS_ACTION_METADATA_GET_WRITEABLE_TYPES,
	DBUS_ACTION_METADATA_GET_REGISTERED_CLASSES,

	DBUS_ACTION_KEYWORDS_GET_LIST,
	DBUS_ACTION_KEYWORDS_GET,
	DBUS_ACTION_KEYWORDS_ADD,
	DBUS_ACTION_KEYWORDS_REMOVE,
	DBUS_ACTION_KEYWORDS_REMOVE_ALL,
	DBUS_ACTION_KEYWORDS_SEARCH,

	DBUS_ACTION_SEARCH_TEXT,
	DBUS_ACTION_SEARCH_TEXT_DETAILED,
	DBUS_ACTION_SEARCH_GET_SNIPPET,
	DBUS_ACTION_SEARCH_FILES_BY_TEXT,
	DBUS_ACTION_SEARCH_METADATA,
	DBUS_ACTION_SEARCH_MATCHING_FIELDS,
	DBUS_ACTION_SEARCH_QUERY,

	DBUS_ACTION_FILES_EXISTS,
	DBUS_ACTION_FILES_CREATE,
	DBUS_ACTION_FILES_DELETE,
	DBUS_ACTION_FILES_GET_SERVICE_TYPE,
	DBUS_ACTION_FILES_GET_TEXT_CONTENTS,
	DBUS_ACTION_FILES_SEARCH_TEXT_CONTENTS,
	DBUS_ACTION_FILES_GET_BY_SERVICE_TYPE,
	DBUS_ACTION_FILES_GET_BY_MIME_TYPE,
	DBUS_ACTION_FILES_GET_BY_MIME_TYPE_VFS,
	DBUS_ACTION_FILES_REFRESH_METADATA,
	DBUS_ACTION_FILES_GET_MTIME,
	DBUS_ACTION_FILES_GET_METADATA_FOLDER_FILES,

	DBUS_ACTION_FILES_SEARCH_BY_TEXT_MIME,
	DBUS_ACTION_FILES_SEARCH_BY_TEXT_MIME_LOCATION,
	DBUS_ACTION_FILES_SEARCH_BY_TEXT_LOCATION

} DBusAction;


typedef struct {
	DBusConnection *connection;
	DBusMessage    *message;
	DBusAction      action;
	gpointer	user_data;
} DBusRec;


DBusConnection*  	tracker_dbus_init    	(void);
void              	tracker_dbus_shutdown   (DBusConnection *conn);

#endif
