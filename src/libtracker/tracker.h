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

#include <dbus/dbus-glib-bindings.h>

G_BEGIN_DECLS

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
	DBusGProxy	*proxy_search;
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
GPtrArray *	tracker_get_stats				(TrackerClient *client, GError **error);

void		tracker_set_bool_option				(TrackerClient *client, const char *option, gboolean value, GError **error);
void		tracker_set_int_option				(TrackerClient *client, const char *option, int value, GError **error);
void		tracker_shutdown				(TrackerClient *client, gboolean reindex, GError **error);
void		tracker_prompt_index_signals			(TrackerClient *client, GError **error);

char *		tracker_search_get_snippet			(TrackerClient *client, ServiceType service, const char *uri, const char *search_text, GError **error);
gchar *		tracker_search_suggest				(TrackerClient *client, const char *search_text, int maxdist, GError **error);

/* asynchronous calls */

void		tracker_get_version_async				(TrackerClient *client,  TrackerIntReply callback, gpointer user_data);
void		tracker_get_status_async				(TrackerClient *client,  TrackerStringReply callback, gpointer user_data);
void		tracker_get_stats_async					(TrackerClient *client,  TrackerGPtrArrayReply callback, gpointer user_data);

void		tracker_set_bool_option_async				(TrackerClient *client, const char *option, gboolean value, TrackerVoidReply callback, gpointer user_data);
void		tracker_set_int_option_async				(TrackerClient *client, const char *option, int value, TrackerVoidReply callback, gpointer user_data);
void		tracker_shutdown_async					(TrackerClient *client, gboolean reindex, TrackerVoidReply callback, gpointer user_data);
void		tracker_prompt_index_signals_async			(TrackerClient *client, TrackerVoidReply callback, gpointer user_data);

void		tracker_search_get_snippet_async			(TrackerClient *client, ServiceType service, const char *uri, const char *search_text, TrackerStringReply callback, gpointer user_data);
void		tracker_search_suggest_async				(TrackerClient *client, const char *search_text, int maxdist, TrackerStringReply callback, gpointer user_data);

G_END_DECLS

#endif /* TRACKER_H */
