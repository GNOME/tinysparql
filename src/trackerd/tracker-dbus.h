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

#define TRACKER_SERVICE                 "org.freedesktop.Tracker"
#define TRACKER_OBJECT			"/org/freedesktop/tracker"
#define TRACKER_INTERFACE		"org.freedesktop.Tracker"
#define TRACKER_INTERFACE_METADATA	"org.freedesktop.Tracker.Metadata"
#define TRACKER_INTERFACE_KEYWORDS	"org.freedesktop.Tracker.Keywords"
#define TRACKER_INTERFACE_SEARCH	"org.freedesktop.Tracker.Search"
#define TRACKER_INTERFACE_FILES		"org.freedesktop.Tracker.Files"
#define TRACKER_INTERFACE_PLAYLISTS	"org.freedesktop.Tracker.PlayLists"


#define TRACKER_METHOD_PING		               	"Ping"

#define TRACKER_METHOD_METADATA_GET                  	"GetMetadata"
#define TRACKER_METHOD_METADATA_SET                  	"SetMetadata"

#define TRACKER_METHOD_METADAT_REGISTER_TYPE       	"RegisterMetadataType"
#define TRACKER_METHOD_METADATA_TYPE_EXISTS       	"MetadataTypeExists"
#define TRACKER_METHOD_METADATA_GET_TYPES       	"GetMetadataTypes"

#define TRACKER_METHOD_KEYWORDS_ADD 			"AddKeywords"
#define TRACKER_METHOD_KEYWORDS_REMOVE			"RemoveKeywords"
#define TRACKER_METHOD_KEYWORDS_GET			"GetKeywords"

#define TRACKER_METHOD_SEARCH_QUERY         		"SearchMetadataQuery"
#define TRACKER_METHOD_SEARCH_TEXT	        	"SearchMetadataText"

/* File specific calls */
#define TRACKER_METHOD_METADATA_GET_FOR_FILES           "GetMetadataForFilesInFolder"
#define TRACKER_METHOD_REFRESH_FILE_METADATA            "RefreshFileMetadata"
#define TRACKER_METHOD_IS_FILE_METADATA_UP_TO_DATE	"IsFileMetadataUpToDate"

/* Deprecated calls */
#define TRACKER_METHOD_SEARCH_FILES_BY_TEXT_MIME     		"SearchFilesByTextAndMime"
#define TRACKER_METHOD_SEARCH_FILES_BY_TEXT_MIME_LOCATION	"SearchFilesByTextAndMimeAndLocation"
#define TRACKER_METHOD_SEARCH_FILES_BY_TEXT_LOCATION	        "SearchFilesByTextAndLocation"
#define TRACKER_METHOD_SEARCH_FILES_QUERY			"SearchFileQuery"

/* signals */
#define TRACKER_SIGNAL_FILE_CHANGED			"FileChanged"
#define TRACKER_SIGNAL_FILE_METADATA_CHANGED		"FileMetadataChanged"
#define TRACKER_SIGNAL_THUMBNAIL_CHANGED		"FileThumbnailChanged"

typedef enum {
	DBUS_ACTION_NONE,
	DBUS_ACTION_PING,
	DBUS_ACTION_METADATA_GET,
	DBUS_ACTION_METADATA_SET,
	DBUS_ACTION_REGISTER_METADATA,
	DBUS_ACTION_METADATA_TYPE_EXISTS,
	DBUS_ACTION_METADATA_GET_TYPES,
	DBUS_ACTION_KEYWORDS_ADD,
	DBUS_ACTION_KEYWORDS_REMOVE,
	DBUS_ACTION_KEYWORDS_GET,
	DBUS_ACTION_SEARCH_METADATA_QUERY,
	DBUS_ACTION_SEARCH_METADATA_TEXT,
	DBUS_ACTION_METADATA_GET_FOR_FILES,
	DBUS_ACTION_REFRESH_FILE_METADATA,
	DBUS_ACTION_IS_FILE_METADATA_UP_TO_DATE,
	DBUS_ACTION_SEARCH_FILES_BY_TEXT_MIME,
	DBUS_ACTION_SEARCH_FILES_BY_TEXT_MIME_LOCATION,
	DBUS_ACTION_SEARCH_FILES_BY_TEXT_LOCATION,
	DBUS_ACTION_SEARCH_FILES_QUERY
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
