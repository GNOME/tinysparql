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

typedef void (*TrackerArrayReply) (char **result, GError *error);
typedef void (*TrackerHashTableReply) (GHashTable *result, GError *error);
typedef void (*TrackerBooleanReply) (gboolean *result, GError *error);
typedef void (*TrackerStringReply) (char *result, GError *error);
typedef void (*TrackerVoidReply) (GError *error);


typedef enum {
	DATA_STRING,
	DATA_INTEGER,
	DATA_DATE,
	DATA_STRING_INDEXABLE
} MetadataTypes;

/* you must call this before making any other calls in libtracker */
gboolean	tracker_init 	();

/* you must call this when you have finished using libtracker */
void		tracker_close 	();

/* synchronous calls */

char *		tracker_get_metadata				(const char *uri, const char *key, GError *error);
void		tracker_set_metadata				(const char *uri, const char *key, const char *value, GError *error);
void		tracker_register_metadata_type			(const char *name, MetadataTypes type, GError *error);
gboolean 	tracker_metadata_type_exists			(const char *name, GError *error);
GHashTable *	tracker_get_multiple_metadata			(const char *uri, char **keys, GError *error);
void		tracker_set_multiple_metadata			(const char *uri, GHashTable *values, GError *error);
GHashTable *	tracker_get_metadata_for_files_in_folder	(const char *uri, char **keys, GError *error);
char **		tracker_get_keywords				(const char *uri, GError *error);
void		tracker_add_Keyword				(const char *uri, const char *value, GError *error);
void		tracker_remove_keyword				(const char *uri, const char *value, GError *error);
void		tracker_refresh_metadata			(const char *uri, GError *error);
gboolean	tracker_is_metadata_up_to_date			(const char *uri, int mtime, GError *error);
char ** 	tracker_search_metadata_by_query		(const char *query, GError *error);
char **		tracker_search_metadata_by_text			(const char *query, GError *error);
GHashTable *	tracker_search_metadata_detailed		(const char *query, char** keys, GError *error);


/* asynchronous calls */

void 		tracker_get_metadata_async 			(const char *uri, const char *key, TrackerStringReply callback);
void		tracker_set_metadata_async 			(const char *uri, const char *key, const char *value, TrackerVoidReply callback);
void		tracker_register_metadata_type_async		(const char *name, MetadataTypes type, TrackerVoidReply callback);
void		tracker_metadata_type_exists_async		(const char *name, TrackerBooleanReply callback);
void 		tracker_get_multiple_metadata_async 		(const char *uri, char **keys, TrackerHashTableReply callback);
void		tracker_set_multiple_metadata_async 		(const char *uri, GHashTable *values, TrackerVoidReply callback);
void 		tracker_get_metadata_for_files_in_folder_async 	(const char *uri, char **keys, TrackerHashTableReply callback);
void 		tracker_get_keywords_async 			(const char *uri, TrackerArrayReply callback);
void		tracker_add_Keyword_async 			(const char *uri, const char *value, TrackerVoidReply callback);
void		tracker_remove_keyword_async 			(const char *uri, const char *value, TrackerVoidReply callback);
void		tracker_refresh_metadata_async 			(const char *uri, TrackerVoidReply callback);
void		tracker_is_metadata_up_to_date_async 		(const char *uri, int mtime, TrackerBooleanReply callback);
void		tracker_search_metadata_by_query_async 		(const char *query, TrackerArrayReply callback);
void		tracker_search_metadata_by_text_async 		(const char *query, TrackerArrayReply callback);
void		tracker_search_metadata_detailed_async 		(const char *query, char **keys, TrackerHashTableReply callback);



