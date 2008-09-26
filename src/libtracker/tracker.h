/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef TRACKER_H
#define TRACKER_H

G_BEGIN_DECLS

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

typedef void (*TrackerArrayReply) (char **result, GError *error, gpointer user_data);
typedef void (*TrackerHashTableReply) (GHashTable *result, GError *error, gpointer user_data);
typedef void (*TrackerGPtrArrayReply) (GPtrArray *result, GError *error, gpointer user_data);
typedef void (*TrackerBooleanReply) (gboolean result, GError *error, gpointer user_data);
typedef void (*TrackerStringReply) (char *result, GError *error, gpointer user_data);
typedef void (*TrackerIntReply) (int result, GError *error, gpointer user_data);
typedef void (*TrackerVoidReply) (GError *error, gpointer user_data);



typedef enum {
	METADATA_STRING_INDEXABLE,
	METADATA_STRING,
	METADATA_NUMERIC,
	METADATA_DATE
} MetadataTypes;

typedef enum {
	SERVICE_FILES,
	SERVICE_FOLDERS,
	SERVICE_DOCUMENTS,
	SERVICE_IMAGES,
	SERVICE_MUSIC,
	SERVICE_VIDEOS,
	SERVICE_TEXT_FILES,
	SERVICE_DEVELOPMENT_FILES,
	SERVICE_OTHER_FILES,
	SERVICE_VFS_FILES,
	SERVICE_VFS_FOLDERS,
	SERVICE_VFS_DOCUMENTS,
	SERVICE_VFS_IMAGES,
	SERVICE_VFS_MUSIC,
	SERVICE_VFS_VIDEOS,
	SERVICE_VFS_TEXT_FILES,
	SERVICE_VFS_DEVELOPMENT_FILES,
	SERVICE_VFS_OTHER_FILES,
	SERVICE_CONVERSATIONS,
	SERVICE_PLAYLISTS,
	SERVICE_APPLICATIONS,
	SERVICE_CONTACTS,
	SERVICE_EMAILS,
	SERVICE_EMAILATTACHMENTS,
	SERVICE_APPOINTMENTS,
	SERVICE_TASKS,
	SERVICE_BOOKMARKS,
	SERVICE_WEBHISTORY,
	SERVICE_PROJECTS
} ServiceType;




typedef struct {
	char *		type;
	gboolean	is_embedded;
	gboolean	is_writeable;

} MetaDataTypeDetails;


typedef struct {
	DBusGProxy	*proxy;
	DBusGProxy	*proxy_metadata;
	DBusGProxy	*proxy_keywords;
	DBusGProxy	*proxy_search;
	DBusGProxy	*proxy_files;
	DBusGProxyCall	*last_pending_call;
} TrackerClient;


void	tracker_cancel_last_call (TrackerClient *client);

/* you can make multiple connections with tracker_connect and free them with tracker_disconnect */
TrackerClient * tracker_connect (gboolean enable_warnings);
void		tracker_disconnect (TrackerClient *client);


ServiceType	tracker_service_name_to_type (const char *service);
char *		tracker_type_to_service_name (ServiceType s);



/* synchronous calls */

int		tracker_get_version				(TrackerClient *client, GError **error);
char *		tracker_get_status				(TrackerClient *client, GError **error);
GHashTable *	tracker_get_services				(TrackerClient *client, gboolean main_services_only, GError **error);
GPtrArray *	tracker_get_stats				(TrackerClient *client, GError **error);

void		tracker_set_bool_option				(TrackerClient *client, const char *option, gboolean value, GError **error);
void		tracker_set_int_option				(TrackerClient *client, const char *option, int value, GError **error);
void		tracker_shutdown				(TrackerClient *client, gboolean reindex, GError **error);
void		tracker_prompt_index_signals			(TrackerClient *client, GError **error);

char **			tracker_metadata_get				(TrackerClient *client, ServiceType service, const char *id, char **keys, GError **error);
void			tracker_metadata_set				(TrackerClient *client, ServiceType service, const char *id, char **keys, char **values, GError **error);
void			tracker_metadata_register_type			(TrackerClient *client, const char *name, MetadataTypes type, GError **error);
MetaDataTypeDetails *	tracker_metadata_get_type_details		(TrackerClient *client, const char *name, GError **error);
char **			tracker_metadata_get_registered_types		(TrackerClient *client, const char *classname, GError **error);
char **			tracker_metadata_get_writeable_types		(TrackerClient *client, const char *classname, GError **error);
char **			tracker_metadata_get_registered_classes		(TrackerClient *client, GError **error);
GPtrArray *		tracker_metadata_get_unique_values		(TrackerClient *client, ServiceType service, char **meta_types, char *query, gboolean descending, int offset, int max_hits, GError **error);
int		tracker_metadata_get_sum			(TrackerClient *client, ServiceType service, char *field, char *query, GError **error);
int		tracker_metadata_get_count			(TrackerClient *client, ServiceType service, char *field, char *query, GError **error);
GPtrArray *		tracker_metadata_get_unique_values_with_count	(TrackerClient *client, ServiceType service, char **meta_types, char *query, char *count, gboolean descending, int offset, int max_hits, GError **error);

GPtrArray *	tracker_keywords_get_list			(TrackerClient *client, ServiceType service, GError **error);
char **		tracker_keywords_get				(TrackerClient *client, ServiceType service, const char *id, GError **error);
void		tracker_keywords_add				(TrackerClient *client, ServiceType service, const char *id, char **values, GError **error);
void		tracker_keywords_remove				(TrackerClient *client, ServiceType service, const char *id, char **values, GError **error);
void		tracker_keywords_remove_all			(TrackerClient *client, ServiceType service, const char *id, GError **error);
char **		tracker_keywords_search				(TrackerClient *client, int live_query_id, ServiceType service, char **keywords, int offset, int max_hits, GError **error);


int		tracker_search_get_hit_count			(TrackerClient *client, ServiceType service, const char *search_text, GError **error);
GPtrArray *	tracker_search_get_hit_count_all		(TrackerClient *client, const char *search_text, GError **error);
char **		tracker_search_text				(TrackerClient *client, int live_query_id, ServiceType service, const char *search_text, int offset, int max_hits, GError **error);
GPtrArray *	tracker_search_text_detailed			(TrackerClient *client, int live_query_id, ServiceType service, const char *search_text, int offset, int max_hits, GError **error);
char *		tracker_search_get_snippet			(TrackerClient *client, ServiceType service, const char *uri, const char *search_text, GError **error);
char **		tracker_search_metadata				(TrackerClient *client, ServiceType service, const char *field, const char* search_text, int offset, int max_hits, GError **error);
GPtrArray *	tracker_search_query				(TrackerClient *client, int live_query_id, ServiceType service, char **fields, const char *search_text, const char *keywords, const char *query, int offset, int max_hits, gboolean sort_by_service, char **sort_fields, gboolean sort_descending, GError **error);
gchar *		tracker_search_suggest				(TrackerClient *client, const char *search_text, int maxdist, GError **error);


void		tracker_files_create				(TrackerClient *client,  const char *uri, gboolean is_directory, const char *mime, int size, int mtime, GError **error);
void		tracker_files_delete				(TrackerClient *client,  const char *uri, GError **error);
char *		tracker_files_get_text_contents			(TrackerClient *client,  const char *uri, int offset, int max_length, GError **error);
char *		tracker_files_search_text_contents		(TrackerClient *client,  const char *uri, const char *search_text, int length, GError **error);
char **		tracker_files_get_by_service_type		(TrackerClient *client,  int live_query_id, ServiceType service, int offset, int max_hits, GError **error);
char **		tracker_files_get_by_mime_type			(TrackerClient *client,  int live_query_id, char **mimes, int offset, int max_hits, GError **error);
char **		tracker_files_get_by_mime_type_vfs		(TrackerClient *client,  int live_query_id, char **mimes, int offset, int max_hits, GError **error);

int		tracker_files_get_mtime				(TrackerClient *client, const char *uri, GError **error);
GPtrArray *	tracker_files_get_metadata_for_files_in_folder	(TrackerClient *client, int live_query_id, const char *uri, char **fields, GError **error);


/* Deprecated calls - Following API specific for nautilus search use only */
char **		tracker_search_metadata_by_text				(TrackerClient *client, const char *query, GError **error);
char **		tracker_search_metadata_by_text_and_mime		(TrackerClient *client, const char *query, const char **mimes, GError **error);
char **		tracker_search_metadata_by_text_and_mime_and_location	(TrackerClient *client, const char *query, const char **mimes, const char *location, GError **error);
char **		tracker_search_metadata_by_text_and_location		(TrackerClient *client, const char *query, const char *location, GError **error);
/* end deprecated call list */


/* asynchronous calls */


void		tracker_get_version_async				(TrackerClient *client,  TrackerIntReply callback, gpointer user_data);
void		tracker_get_status_async				(TrackerClient *client,  TrackerStringReply callback, gpointer user_data);
void		tracker_get_services_async				(TrackerClient *client,  gboolean main_services_only,  TrackerHashTableReply callback, gpointer user_data);
void		tracker_get_stats_async					(TrackerClient *client,  TrackerGPtrArrayReply callback, gpointer user_data);

void		tracker_set_bool_option_async				(TrackerClient *client, const char *option, gboolean value, TrackerVoidReply callback, gpointer user_data);
void		tracker_set_int_option_async				(TrackerClient *client, const char *option, int value, TrackerVoidReply callback, gpointer user_data);
void		tracker_shutdown_async					(TrackerClient *client, gboolean reindex, TrackerVoidReply callback, gpointer user_data);
void		tracker_prompt_index_signals_async			(TrackerClient *client, TrackerVoidReply callback, gpointer user_data);

void		tracker_metadata_get_async				(TrackerClient *client, ServiceType service, const char *id, char **keys, TrackerArrayReply callback, gpointer user_data);
void		tracker_metadata_set_async				(TrackerClient *client, ServiceType service, const char *id, char **keys, char **values, TrackerVoidReply callback, gpointer user_data);
void		tracker_metadata_register_type_async			(TrackerClient *client, const char *name, MetadataTypes type, TrackerVoidReply callback, gpointer user_data);
void		tracker_metadata_get_registered_types_async		(TrackerClient *client, const char *classname, TrackerArrayReply callback, gpointer user_data);
void		tracker_metadata_get_writeable_types_async		(TrackerClient *client, const char *classname, TrackerArrayReply callback, gpointer user_data);
void		tracker_metadata_get_registered_classes_async		(TrackerClient *client, TrackerArrayReply callback, gpointer user_data);
void		tracker_metadata_get_unique_values_async		(TrackerClient *client, ServiceType service, char **meta_types, const char *query, gboolean descending, int offset, int max_hits, TrackerGPtrArrayReply callback, gpointer user_data);
void		tracker_metadata_get_sum_async				(TrackerClient *client, ServiceType service, char *field, char *query, TrackerIntReply callback, gpointer user_data);
void		tracker_metadata_get_count_async			(TrackerClient *client, ServiceType service, char *field, char *query, TrackerIntReply callback, gpointer user_data);
void		tracker_metadata_get_unique_values_with_count_async	(TrackerClient *client, ServiceType service, char **meta_types, const char *query, char *count, gboolean descending, int offset, int max_hits, TrackerGPtrArrayReply callback, gpointer user_data);

void		tracker_keywords_get_list_async				(TrackerClient *client, ServiceType service, TrackerGPtrArrayReply callback, gpointer user_data);
void		tracker_keywords_get_async				(TrackerClient *client, ServiceType service, const char *id, TrackerArrayReply callback, gpointer user_data);
void		tracker_keywords_add_async				(TrackerClient *client, ServiceType service, const char *id, char **values, TrackerVoidReply callback, gpointer user_data);
void		tracker_keywords_remove_async				(TrackerClient *client, ServiceType service, const char *id, char **values, TrackerVoidReply callback, gpointer user_data);
void		tracker_keywords_remove_all_async			(TrackerClient *client, ServiceType service, const char *id, TrackerVoidReply callback, gpointer user_data);
void		tracker_keywords_search_async				(TrackerClient *client, int live_query_id, ServiceType service, char **keywords, int offset, int max_hits, TrackerArrayReply callback, gpointer user_data);

void		tracker_search_text_get_hit_count_async			(TrackerClient *client, ServiceType service, const char *search_text, TrackerIntReply callback, gpointer user_data);
void		tracker_search_text_get_hit_count_all_async		(TrackerClient *client, const char *search_text, TrackerGPtrArrayReply callback, gpointer user_data);
void		tracker_search_text_async				(TrackerClient *client, int live_query_id, ServiceType service, const char *search_text, int offset, int max_hits, TrackerArrayReply callback, gpointer user_data);
void		tracker_search_text_detailed_async			(TrackerClient *client, int live_query_id, ServiceType service, const char *search_text, int offset, int max_hits, TrackerGPtrArrayReply callback, gpointer user_data);
void		tracker_search_get_snippet_async			(TrackerClient *client, ServiceType service, const char *uri, const char *search_text, TrackerStringReply callback, gpointer user_data);
void		tracker_search_metadata_async				(TrackerClient *client, ServiceType service, const char *field, const char* search_text, int offset, int max_hits, TrackerArrayReply callback, gpointer user_data);
void		tracker_search_query_async				(TrackerClient *client, int live_query_id, ServiceType service, char **fields, const char *search_text, const char *keywords, const char *query, int offset, int max_hits, gboolean sort_by_service, char **sort_fields, gboolean sort_descending, TrackerGPtrArrayReply callback, gpointer user_data);
void		tracker_search_suggest_async				(TrackerClient *client, const char *search_text, int maxdist, TrackerStringReply callback, gpointer user_data);

void		tracker_files_create_async				(TrackerClient *client,  const char *uri, gboolean is_directory, const char *mime, int size, int mtime, TrackerVoidReply callback, gpointer user_data);
void		tracker_files_delete_async				(TrackerClient *client,  const char *uri, TrackerVoidReply callback, gpointer user_data);
void		tracker_files_get_text_contents_async			(TrackerClient *client,  const char *uri, int offset, int max_length, TrackerStringReply callback, gpointer user_data);
void		tracker_files_search_text_contents_async		(TrackerClient *client,  const char *uri, const char *search_text, int length, TrackerStringReply callback, gpointer user_data);
void		tracker_files_get_by_service_type_async			(TrackerClient *client,  int live_query_id, ServiceType service, int offset, int max_hits, TrackerArrayReply callback, gpointer user_data);
void		tracker_files_get_by_mime_type_async			(TrackerClient *client,  int live_query_id, char **mimes, int offset, int max_hits, TrackerArrayReply callback, gpointer user_data);
void		tracker_files_get_by_mime_type_vfs_async		(TrackerClient *client,  int live_query_id, char **mimes, int offset, int max_hits, TrackerArrayReply callback, gpointer user_data);

void		tracker_files_get_mtime_async				(TrackerClient *client, const char *uri, TrackerIntReply callback, gpointer user_data);
void		tracker_files_get_metadata_for_files_in_folder_async	(TrackerClient *client, int live_query_id, const char *uri, char **fields, TrackerGPtrArrayReply callback, gpointer user_data);




/* Deprecated calls - API specific for nautilus search use only. New code should use tracker_search_metadata_matching_text_async instead */
void tracker_search_metadata_by_text_async				(TrackerClient *client, const char *query, TrackerArrayReply callback, gpointer user_data);
void tracker_search_metadata_by_text_and_mime_async			(TrackerClient *client, const char *query, const char **mimes, TrackerArrayReply callback, gpointer user_data);
void tracker_search_metadata_by_text_and_mime_and_location_async	(TrackerClient *client, const char *query, const char **mimes, const char *location, TrackerArrayReply callback, gpointer user_data);
void tracker_search_metadata_by_text_and_location_async			(TrackerClient *client, const char *query, const char *location, TrackerArrayReply callback, gpointer user_data);

G_END_DECLS

#endif /* TRACKER_H */
