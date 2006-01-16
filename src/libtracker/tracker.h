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

#include "tracker-client.h"

#define TRACKER_SIGNAL_FILE_NOTIFICATION_CHANGED	"FileNotificationChanged"
#define TRACKER_SIGNAL_BASIC_METADATA_CHANGED		"FileBasicMetaDataChanged"
#define TRACKER_SIGNAL_EMBEDDED_METADATA_CHANGED	"FileEmbeddedMetaDataChanged"
#define TRACKER_SIGNAL_EDITABLE_METADATA_CHANGED	"FileEditableMetaDataChanged"
#define TRACKER_SIGNAL_THUMBNAIL_CHANGED		"FileThumbnailChanged"

typedef void (*TrackerArrayReply) (char **result, GError *error, gpointer user_data);
typedef void (*TrackerHashTableReply) (GHashTable *result, GError *error, gpointer user_data);
typedef void (*TrackerBooleanReply) (gboolean *result, GError *error, gpointer user_data);
typedef void (*TrackerStringReply) (char *result, GError *error, gpointer user_data);
typedef void (*TrackerVoidReply) (GError *error, gpointer user_data);


typedef enum {
	DATA_STRING,
	DATA_INTEGER,
	DATA_DATE,
	DATA_STRING_INDEXABLE
} MetadataTypes;

typedef struct {
	DBusGProxy 	*proxy;
	DBusGProxyCall  *last_pending_call; 
} TrackerClient;

void	tracker_cancel_last_call (TrackerClient *client);

/* you can make multiple connections with tracker_connect and free them with tracker_disconnect */
TrackerClient * tracker_connect (gboolean integrate_main_loop);
void		enable_main_loop ();
void		tracker_disconnect (TrackerClient *client);

/* synchronous calls */

char *		tracker_get_metadata					(TrackerClient *client, const char *uri, const char *key, GError *error);
void		tracker_set_metadata					(TrackerClient *client, const char *uri, const char *key, const char *value, GError *error);
void		tracker_register_metadata_type				(TrackerClient *client, const char *name, MetadataTypes type, GError *error);
gboolean 	tracker_metadata_type_exists				(TrackerClient *client, const char *name, GError *error);
GHashTable *	tracker_get_multiple_metadata				(TrackerClient *client, const char *uri, char **keys, GError *error);
void		tracker_set_multiple_metadata				(TrackerClient *client, const char *uri, GHashTable *values, GError *error);
GHashTable *	tracker_get_metadata_for_files_in_folder		(TrackerClient *client, const char *uri, const char **keys, GError *error);
char **		tracker_get_keywords					(TrackerClient *client, const char *uri, GError *error);
void		tracker_add_Keyword					(TrackerClient *client, const char *uri, const char *value, GError *error);
void		tracker_remove_keyword					(TrackerClient *client, const char *uri, const char *value, GError *error);
void		tracker_refresh_metadata				(TrackerClient *client, const char *uri, GError *error);
gboolean	tracker_is_metadata_up_to_date				(TrackerClient *client, const char *uri, int mtime, GError *error);
char ** 	tracker_search_metadata_by_query			(TrackerClient *client, const char *query, GError *error);
char **		tracker_search_metadata_by_text				(TrackerClient *client, const char *query, GError *error);
char **		tracker_search_metadata_by_text_and_mime		(TrackerClient *client, const char *query, const char **mimes, GError *error);
char **		tracker_search_metadata_by_text_and_mime_and_location	(TrackerClient *client, const char *query, const char **mimes, const char *location, GError *error);
char **		tracker_search_metadata_by_text_and_location		(TrackerClient *client, const char *query, const char *location, GError *error);
GHashTable *	tracker_search_metadata_detailed			(TrackerClient *client, const char *query, char** keys, GError *error);


/* asynchronous calls */

void tracker_get_metadata_async 			(TrackerClient *client, const char *uri, const char *key, TrackerStringReply callback, gpointer user_data);
void tracker_set_metadata_async 			(TrackerClient *client, const char *uri, const char *key, const char *value, TrackerVoidReply callback, gpointer user_data);
void tracker_register_metadata_type_async		(TrackerClient *client, const char *name, MetadataTypes type, TrackerVoidReply callback, gpointer user_data);
void tracker_metadata_type_exists_async			(TrackerClient *client, const char *name, TrackerBooleanReply callback, gpointer user_data);
void tracker_get_multiple_metadata_async 		(TrackerClient *client, const char *uri, char **keys, TrackerHashTableReply callback, gpointer user_data);
void tracker_set_multiple_metadata_async 		(TrackerClient *client, const char *uri, GHashTable *values, TrackerVoidReply callback, gpointer user_data);
void tracker_get_metadata_for_files_in_folder_async 	(TrackerClient *client, const char *uri, char **keys, TrackerHashTableReply callback, gpointer user_data);
void tracker_get_keywords_async 			(TrackerClient *client, const char *uri, TrackerArrayReply callback, gpointer user_data);
void tracker_add_Keyword_async 				(TrackerClient *client, const char *uri, const char *value, TrackerVoidReply callback, gpointer user_data);
void tracker_remove_keyword_async 			(TrackerClient *client, const char *uri, const char *value, TrackerVoidReply callback, gpointer user_data);
void tracker_refresh_metadata_async 			(TrackerClient *client, const char *uri, TrackerVoidReply callback, gpointer user_data);
void tracker_is_metadata_up_to_date_async 		(TrackerClient *client, const char *uri, int mtime, TrackerBooleanReply callback, gpointer user_data);
void tracker_search_metadata_by_query_async 		(TrackerClient *client, const char *query, TrackerArrayReply callback, gpointer user_data);

void tracker_search_metadata_by_text_async 				(TrackerClient *client, const char *query, TrackerArrayReply callback, gpointer user_data);
void tracker_search_metadata_by_text_and_mime_async			(TrackerClient *client, const char *query, const char **mimes, TrackerArrayReply callback, gpointer user_data);
void tracker_search_metadata_by_text_and_mime_and_location_async	(TrackerClient *client, const char *query, const char **mimes, const char *location, TrackerArrayReply callback, gpointer user_data);
void tracker_search_metadata_by_text_and_location_async			(TrackerClient *client, const char *query, const char *location, TrackerArrayReply callback, gpointer user_data);

void tracker_search_metadata_detailed_async 		(TrackerClient *client, const char *query, char **keys, TrackerHashTableReply callback);



