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

#include <string.h>

#include "tracker-daemon-glue.h"
#include "tracker-files-glue.h"
#include "tracker-keywords-glue.h"
#include "tracker-metadata-glue.h"
#include "tracker-search-glue.h"

#include "tracker.h"

#define TRACKER_SERVICE			"org.freedesktop.Tracker"
#define TRACKER_OBJECT			"/org/freedesktop/Tracker"
#define TRACKER_INTERFACE		"org.freedesktop.Tracker"
#define TRACKER_INTERFACE_METADATA	"org.freedesktop.Tracker.Metadata"
#define TRACKER_INTERFACE_KEYWORDS	"org.freedesktop.Tracker.Keywords"
#define TRACKER_INTERFACE_SEARCH	"org.freedesktop.Tracker.Search"
#define TRACKER_INTERFACE_FILES		"org.freedesktop.Tracker.Files"
#define TRACKER_INTERFACE_MUSIC		"org.freedesktop.Tracker.Music"
#define TRACKER_INTERFACE_PLAYLISTS	"org.freedesktop.Tracker.PlayLists"

typedef struct {
	TrackerArrayReply callback;
	gpointer	  data;
} ArrayCallBackStruct;

typedef struct {
	TrackerGPtrArrayReply callback;
	gpointer	  data;
} GPtrArrayCallBackStruct;

typedef struct {
	TrackerHashTableReply	callback;
	gpointer		data;
} HashTableCallBackStruct;


typedef struct {
	TrackerBooleanReply callback;
	gpointer	  data;
} BooleanCallBackStruct;

typedef struct {
	TrackerStringReply callback;
	gpointer	  data;
} StringCallBackStruct;

typedef struct {
	TrackerIntReply callback;
	gpointer	  data;
} IntCallBackStruct;

typedef struct {
	TrackerVoidReply callback;
	gpointer	  data;
} VoidCallBackStruct;


char *tracker_service_types[] = {
"Files",
"Folders",
"Documents",
"Images",
"Music",
"Videos",
"Text",
"Development",
"Other",
"VFS",
"VFSFolders",
"VFSDocuments",
"VFSImages",
"VFSMusic",
"VFSVideos",
"VFSText",
"VFSDevelopment",
"VFSOther",
"Conversations",
"Playlists",
"Applications",
"Contacts",
"Emails",
"EmailAttachments",
"Appointments",
"Tasks",
"Bookmarks",
"WebHistory",
"Projects",
NULL
};




char *metadata_types[] = {
	"index",
	"string",
	"numeric",
	"date",
	"blob"
};


ServiceType
tracker_service_name_to_type (const char *service)
{

	char **st;
	int i = 0;

	for (st=tracker_service_types; *st; st++) {

		if (g_ascii_strcasecmp (service, *st) == 0) {
			return i;
		}

		i++;
	}

	return SERVICE_OTHER_FILES;
}


char *
tracker_type_to_service_name (ServiceType s)
{
	return g_strdup (tracker_service_types[s]);
}



static void
tracker_array_reply (DBusGProxy *proxy, char **OUT_result, GError *error, gpointer user_data)
{

	ArrayCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerArrayReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}


static void
tracker_hashtable_reply (DBusGProxy *proxy,  GHashTable *OUT_result, GError *error, gpointer user_data)
{

	HashTableCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerHashTableReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}

static void
tracker_GPtrArray_reply (DBusGProxy *proxy,  GPtrArray *OUT_result, GError *error, gpointer user_data)
{

	GPtrArrayCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerGPtrArrayReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}



static void
tracker_string_reply (DBusGProxy *proxy, char *OUT_result, GError *error, gpointer user_data)
{

	StringCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerStringReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}


static void
tracker_int_reply (DBusGProxy *proxy, int OUT_result, GError *error, gpointer user_data)
{

	IntCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerIntReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}

/*
static void
tracker_boolean_reply (DBusGProxy *proxy, gboolean OUT_result, GError *error, gpointer user_data)
{

	BooleanCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerBooleanReply) callback_struct->callback ) (OUT_result, error, callback_struct->data);

	g_free (callback_struct);
}
*/


static void
tracker_void_reply (DBusGProxy *proxy, GError *error, gpointer user_data)
{

	VoidCallBackStruct *callback_struct;

	callback_struct = user_data;

	(*(TrackerVoidReply) callback_struct->callback ) (error, callback_struct->data);

	g_free (callback_struct);
}





TrackerClient *
tracker_connect (gboolean enable_warnings)
{
	DBusGConnection *connection;
	GError *error = NULL;
	TrackerClient *client = NULL;
	DBusGProxy *proxy;

	g_type_init ();

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (connection == NULL)	{
		if (enable_warnings) {
			g_warning("Unable to connect to dbus: %s\n", error->message);
		}
		g_error_free (error);
		return NULL;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
			TRACKER_SERVICE,
			TRACKER_OBJECT,
			TRACKER_INTERFACE);

	if (!proxy) {
		if (enable_warnings) {
			g_warning ("could not create proxy");
		}
		return NULL;
	}


	client = g_new (TrackerClient, 1);
	client->proxy = proxy;

	proxy = dbus_g_proxy_new_for_name (connection,
			TRACKER_SERVICE,
			TRACKER_OBJECT "/Metadata",
			TRACKER_INTERFACE_METADATA);

	client->proxy_metadata = proxy;

	proxy = dbus_g_proxy_new_for_name (connection,
			TRACKER_SERVICE,
			TRACKER_OBJECT "/Keywords",
			TRACKER_INTERFACE_KEYWORDS);

	client->proxy_keywords = proxy;

	proxy = dbus_g_proxy_new_for_name (connection,
			TRACKER_SERVICE,
			TRACKER_OBJECT "/Search",
			TRACKER_INTERFACE_SEARCH);

	client->proxy_search = proxy;

	proxy = dbus_g_proxy_new_for_name (connection,
			TRACKER_SERVICE,
			TRACKER_OBJECT "/Files",
			TRACKER_INTERFACE_FILES);

	client->proxy_files = proxy;



	return client;

}

void
tracker_disconnect (TrackerClient *client)
{
	g_object_unref (client->proxy);
	g_object_unref (client->proxy_metadata);
	g_object_unref (client->proxy_keywords);
	g_object_unref (client->proxy_search);
	g_object_unref (client->proxy_files);
	client->proxy = NULL;
	client->proxy_metadata = NULL;
	client->proxy_keywords = NULL;
	client->proxy_search = NULL;
	client->proxy_files = NULL;

	g_free (client);
}



void
tracker_cancel_last_call (TrackerClient *client)
{
	dbus_g_proxy_cancel_call (client->proxy, client->last_pending_call);
}



/* dbus synchronous calls */


int
tracker_get_version (TrackerClient *client, GError **error)
{
	int version;

	org_freedesktop_Tracker_get_version (client->proxy, &version, &*error);

	return version;
}

char *
tracker_get_status (TrackerClient *client, GError **error)
{
	char *status ;
	org_freedesktop_Tracker_get_status (client->proxy, &status, &*error);
	return status;
}


GHashTable *
tracker_get_services (TrackerClient *client, gboolean main_services_only,  GError **error)
{
	GHashTable *table;

	if (!org_freedesktop_Tracker_get_services (client->proxy, main_services_only, &table, &*error)) {
		return NULL;
	}

	return table;


}



GPtrArray *
tracker_get_stats (TrackerClient *client,  GError **error)
{
	GPtrArray *table;

	if (!org_freedesktop_Tracker_get_stats (client->proxy, &table, &*error)) {
		return NULL;
	}

	return table;


}


void
tracker_set_bool_option (TrackerClient *client, const char *option, gboolean value, GError **error)
{
	org_freedesktop_Tracker_set_bool_option (client->proxy, option, value,	&*error);
}

void
tracker_set_int_option (TrackerClient *client, const char *option, int value, GError **error)
{
	org_freedesktop_Tracker_set_int_option (client->proxy, option, value,  &*error);
}


void
tracker_shutdown (TrackerClient *client, gboolean reindex, GError **error)
{
	org_freedesktop_Tracker_shutdown (client->proxy, reindex,  &*error);
}


void
tracker_prompt_index_signals (TrackerClient *client, GError **error)
{
	org_freedesktop_Tracker_prompt_index_signals (client->proxy, &*error);
}



char **
tracker_metadata_get (TrackerClient *client, ServiceType service, const char *id, char **keys, GError **error)
{
	char **array = NULL;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Metadata_get  (client->proxy_metadata, service_str, id, (const char **)keys, &array, &*error)) {
		return NULL;
	}

	return array;
}


void
tracker_metadata_set (TrackerClient *client, ServiceType service, const char *id, char **keys, char **values, GError **error)
{
	char *service_str = tracker_service_types[service];

	org_freedesktop_Tracker_Metadata_set  (client->proxy_metadata, service_str, id, (const char **)keys, (const char **)values, &*error);

}



void
tracker_metadata_register_type	(TrackerClient *client, const char *name, MetadataTypes type, GError **error)
{
	/* This does nothing now, this API has been removed */
	g_warning ("%s no longer does anything", __FUNCTION__);
}

MetaDataTypeDetails *
tracker_metadata_get_type_details (TrackerClient *client, const char *name, GError **error)
{

	MetaDataTypeDetails *details = g_new (MetaDataTypeDetails, 1);

	if (!org_freedesktop_Tracker_Metadata_get_type_details (client->proxy_metadata, name, &details->type, &details->is_embedded, &details->is_writeable, &*error)) {
		g_free (details);
		return NULL;
	}

	return details;

}


char **
tracker_metadata_get_registered_types (TrackerClient *client, const char *class, GError **error)
{
	char **array = NULL;

	if (!org_freedesktop_Tracker_Metadata_get_registered_types  (client->proxy_metadata, class, &array, &*error)) {
		return NULL;
	}

	return array;
}


char **
tracker_metadata_get_writeable_types (TrackerClient *client, const char *class, GError **error)
{
	/* This does nothing now, this API has been removed */
	g_warning ("%s no longer does anything", __FUNCTION__);

	return NULL;
}



char **
tracker_metadata_get_registered_classes (TrackerClient *client, GError **error)
{
	char **array = NULL;

	if (!org_freedesktop_Tracker_Metadata_get_registered_classes  (client->proxy_metadata, &array, &*error)) {
		return NULL;
	}

	return array;
}


GPtrArray *
tracker_metadata_get_unique_values (TrackerClient *client, ServiceType service, char **meta_types, char *query, gboolean descending, int offset, int max_hits, GError **error)
{
	GPtrArray *table;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Metadata_get_unique_values (client->proxy_metadata, service_str, (const char **)meta_types, query, descending, offset, max_hits, &table, &*error)) {
		return NULL;
	}

	return table;
}

int
tracker_metadata_get_sum (TrackerClient *client, ServiceType service, char *field, char *query, GError **error)
{
	int sum;

	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Metadata_get_sum (client->proxy_metadata, service_str, field, query, &sum, &*error)) {
		return -1;
	}

	return sum;
}

int
tracker_metadata_get_count (TrackerClient *client, ServiceType service, char *field, char *query, GError **error)
{
	int count;

	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Metadata_get_count (client->proxy_metadata, service_str, field, query, &count, &*error)) {
		return -1;
	}

	return count;

}

GPtrArray *
tracker_metadata_get_unique_values_with_count (TrackerClient *client, ServiceType service, char **meta_types, char *query, char *count, gboolean descending, int offset, int max_hits, GError **error)
{
	GPtrArray *table;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Metadata_get_unique_values_with_count (client->proxy_metadata, service_str, (const char **)meta_types, query, count, descending, offset, max_hits, &table, &*error)) {
		return NULL;
	}

	return table;
}


GPtrArray *
tracker_keywords_get_list (TrackerClient *client, ServiceType service, GError **error)
{
	GPtrArray *table;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Keywords_get_list (client->proxy_keywords,service_str, &table, &*error)) {
		return NULL;
	}

	return table;


}


char **
tracker_keywords_get (TrackerClient *client, ServiceType service, const char *id, GError **error)
{
	char **array = NULL;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Keywords_get (client->proxy_keywords, service_str, id, &array, &*error)) {
		return NULL;
	}

	return array;
}



void
tracker_keywords_add (TrackerClient *client, ServiceType service, const char *id, char **values, GError **error)
{
	char *service_str = tracker_service_types[service];
	org_freedesktop_Tracker_Keywords_add (client->proxy_keywords, service_str, id, (const char **)values, &*error);
}



void
tracker_keywords_remove (TrackerClient *client, ServiceType service, const char *id, char **values, GError **error)
{
	char *service_str = tracker_service_types[service];

	org_freedesktop_Tracker_Keywords_remove (client->proxy_keywords, service_str, id, (const char **)values, &*error);
}



void
tracker_keywords_remove_all (TrackerClient *client, ServiceType service, const char *id, GError **error)
{
	char *service_str = tracker_service_types[service];

	org_freedesktop_Tracker_Keywords_remove_all (client->proxy_keywords, service_str, id, &*error);
}


char **
tracker_keywords_search	(TrackerClient *client, int live_query_id, ServiceType service, char **keywords, int offset, int max_hits, GError **error)
{
	char **array = NULL;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Keywords_search (client->proxy_keywords, live_query_id, service_str, (const char **)keywords, offset, max_hits, &array, &*error)) {
		return NULL;
	}

	return array;

}


int
tracker_search_get_hit_count (TrackerClient *client, ServiceType service, const char *search_text, GError **error)
{

	int count = 0;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Search_get_hit_count (client->proxy_search, service_str, search_text, &count, &*error)) {
		return 0;
	}

	return count;

}


GPtrArray *
tracker_search_get_hit_count_all (TrackerClient *client, const char *search_text, GError **error)
{

	GPtrArray *array;

	if (!org_freedesktop_Tracker_Search_get_hit_count_all (client->proxy_search, search_text, &array, &*error)) {
		return NULL;
	}

	return array;

}

char **
tracker_search_text (TrackerClient *client, int live_query_id, ServiceType service, const char *search_text, int offset, int max_hits, GError **error)
{
	char **array = NULL;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Search_text (client->proxy_search, live_query_id, service_str, search_text,  offset, max_hits, &array, &*error)) {
		return NULL;
	}

	return array;
}

GPtrArray *
tracker_search_text_detailed (TrackerClient *client, int live_query_id, ServiceType service, const char *search_text, int offset, int max_hits, GError **error)
{
	GPtrArray *array;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Search_text_detailed (client->proxy_search, live_query_id, service_str, search_text,  offset, max_hits, &array, &*error)) {
		return NULL;
	}

	return array;
}

char *
tracker_search_get_snippet (TrackerClient *client, ServiceType service, const char *uri, const char *search_text, GError **error)
{
	char *result;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Search_get_snippet (client->proxy_search, service_str, uri, search_text, &result, &*error)) {
		return NULL;
	}

	return result;


}



char **
tracker_search_metadata	(TrackerClient *client, ServiceType service, const char *field, const char* search_text, int offset, int max_hits, GError **error)
{
	char **array = NULL;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Search_metadata (client->proxy_search, service_str, field, search_text,  offset, max_hits, &array, &*error)) {
		return NULL;
	}

	return array;
}



GPtrArray *
tracker_search_query (TrackerClient *client, int live_query_id, ServiceType service, char **fields, const char *search_text, const char *keywords, const char *query, int offset, int max_hits, gboolean sort_by_service, char **sort_fields, gboolean sort_descending, GError **error)
{
	GPtrArray *table;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Search_query (client->proxy_search, live_query_id, service_str, (const char **)fields, search_text, keywords, query, sort_by_service, (const char **)sort_fields, sort_descending, offset, max_hits , &table, &*error)) {
		return NULL;
	}

	return table;
}

char *
tracker_search_suggest (TrackerClient *client, const char *search_term, int maxdist, GError **error)
{
	gchar *result;
	if (org_freedesktop_Tracker_Search_suggest (client->proxy_search, search_term, maxdist, &result, &*error)) {
		return result;
	}
	return NULL;
}



void
tracker_files_create (TrackerClient *client, const char *uri, gboolean is_directory, const char *mime, int size, int mtime, GError **error)
{
	org_freedesktop_Tracker_Files_create (client->proxy_files, uri, is_directory, mime, size, mtime, &*error);
}


void
tracker_files_delete (TrackerClient *client, const char *uri, GError **error)
{
	org_freedesktop_Tracker_Files_delete (client->proxy_files, uri, &*error);
}


char *
tracker_files_get_text_contents	(TrackerClient *client,  const char *uri, int offset, int max_length, GError **error)
{
	char *result;

	if (!org_freedesktop_Tracker_Files_get_text_contents (client->proxy_files, uri, offset, max_length, &result, &*error)) {
		return NULL;
	}

	return result;

}



char *
tracker_files_search_text_contents (TrackerClient *client,  const char *uri, const char *search_text, int length, GError **error)
{
	char *result;

	if (!org_freedesktop_Tracker_Files_search_text_contents (client->proxy_files, uri, search_text, length, &result, &*error)) {
		return NULL;
	}

	return result;
}



char **
tracker_files_get_by_service_type (TrackerClient *client,  int live_query_id, ServiceType service, int offset, int max_hits, GError **error)
{
	char **array = NULL;
	char *service_str = tracker_service_types[service];

	if (!org_freedesktop_Tracker_Files_get_by_service_type (client->proxy_files, live_query_id, service_str, offset, max_hits, &array, &*error)) {
		return NULL;
	}

	return array;
}



char **
tracker_files_get_by_mime_type	(TrackerClient *client,  int live_query_id, char **mimes, int offset, int max_hits, GError **error)
{
	char **array = NULL;

	if (!org_freedesktop_Tracker_Files_get_by_mime_type (client->proxy_files, live_query_id, (const char **)mimes, offset, max_hits, &array, &*error)) {
		return NULL;
	}

	return array;
}



char **
tracker_files_get_by_mime_type_vfs (TrackerClient *client,  int live_query_id, char **mimes, int offset, int max_hits, GError **error)
{
	char **array = NULL;

	if (!org_freedesktop_Tracker_Files_get_by_mime_type_vfs (client->proxy_files, live_query_id,(const char **) mimes, offset, max_hits, &array, &*error)) {
		return NULL;
	}

	return array;
}



int
tracker_files_get_mtime	(TrackerClient *client, const char *uri, GError **error)
{
	int result;

	if (!org_freedesktop_Tracker_Files_get_mtime (client->proxy_files, uri, &result, &*error)) {
		return 0;
	}

	return result;
}


GPtrArray *
tracker_files_get_metadata_for_files_in_folder	(TrackerClient *client, int live_query_id, const char *uri, char **fields, GError **error)
{
	GPtrArray *table;

	if (!org_freedesktop_Tracker_Files_get_metadata_for_files_in_folder  (client->proxy_files, live_query_id, uri, (const char **)fields, &table, &*error)) {
		return NULL;
	}

	return table;
}



char **
tracker_search_metadata_by_text (TrackerClient *client, const char *query,  GError **error)
{

	char **array = NULL;

	if (!org_freedesktop_Tracker_Search_text (client->proxy_search, -1, "Files", query,  0, 512, &array, &*error)) {
		return NULL;
	}

	return array;

}

/* asynchronous calls */



char **
tracker_search_metadata_by_text_and_mime (TrackerClient *client, const char *query, const char **mimes, GError **error)
{
	char **strs;

	if (!org_freedesktop_Tracker_Files_search_by_text_and_mime  (client->proxy_files, query,(const char **) mimes, &strs, &*error)) {
		return NULL;
	}
	return strs;

}


char **
tracker_search_metadata_by_text_and_mime_and_location (TrackerClient *client, const char *query, const char **mimes, const char *location, GError **error)
{
	char **strs;

	if (!org_freedesktop_Tracker_Files_search_by_text_and_mime_and_location (client->proxy_files, query, (const char **)mimes, location, &strs, &*error)) {
		return NULL;
	}
	return strs;

}



char **
tracker_search_metadata_by_text_and_location (TrackerClient *client, const char *query, const char *location, GError **error)
{
	char **strs;

	if (!org_freedesktop_Tracker_Files_search_by_text_and_location (client->proxy_files, query, location, &strs, &*error)) {
		return NULL;
	}
	return strs;

}



void
tracker_get_version_async (TrackerClient *client, TrackerIntReply callback, gpointer user_data)
{

	IntCallBackStruct *callback_struct;

	callback_struct = g_new (IntCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_get_version_async (client->proxy, tracker_int_reply, callback_struct);

}

void
tracker_get_status_async (TrackerClient *client, TrackerStringReply callback, gpointer user_data)
{

	StringCallBackStruct *callback_struct;

	callback_struct = g_new (StringCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_get_status_async (client->proxy, tracker_string_reply, callback_struct);

}




void
tracker_get_services_async	(TrackerClient *client, gboolean main_services_only, TrackerHashTableReply callback, gpointer user_data)
{
	HashTableCallBackStruct *callback_struct;

	callback_struct = g_new (HashTableCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_get_services_async (client->proxy, main_services_only, tracker_hashtable_reply, callback_struct);

}


void
tracker_get_stats_async	(TrackerClient *client,  TrackerGPtrArrayReply callback, gpointer user_data)
{
	GPtrArrayCallBackStruct *callback_struct;

	callback_struct = g_new (GPtrArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_get_stats_async (client->proxy, tracker_GPtrArray_reply, callback_struct);

}



void
tracker_set_bool_option_async (TrackerClient *client, const char *option, gboolean value, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_set_bool_option_async  (client->proxy, option, value, tracker_void_reply, callback_struct);
}

void
tracker_set_int_option_async (TrackerClient *client, const char *option, int value, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_set_int_option_async  (client->proxy, option, value, tracker_void_reply, callback_struct);
}


void
tracker_shutdown_async (TrackerClient *client, gboolean reindex, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_shutdown_async  (client->proxy, reindex, tracker_void_reply, callback_struct);
}


void
tracker_prompt_index_signals_async (TrackerClient *client, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_prompt_index_signals_async	(client->proxy, tracker_void_reply, callback_struct);
}






void
tracker_metadata_get_async (TrackerClient *client, ServiceType service, const char *id, char **keys, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Metadata_get_async (client->proxy_metadata, service_str, id, (const char**)keys, tracker_array_reply, callback_struct);

}


void
tracker_metadata_set_async (TrackerClient *client, ServiceType service, const char *id, char **keys, char **values, TrackerVoidReply callback, gpointer user_data)
{

	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;


	char *service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Metadata_set_async	(client->proxy_metadata, service_str, id, (const char **)keys, (const char **)values, tracker_void_reply, callback_struct);

}



void
tracker_metadata_register_type_async (TrackerClient *client, const char *name, MetadataTypes type, TrackerVoidReply callback, gpointer user_data)
{

	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	/* This does nothing now, this API has been removed */
	g_warning ("%s no longer does anything", __FUNCTION__);

	tracker_void_reply (client->proxy_metadata, NULL, callback_struct);
}




void
tracker_metadata_get_registered_types_async (TrackerClient *client, const char *class, TrackerArrayReply callback, gpointer user_data)
{

	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;


	client->last_pending_call = org_freedesktop_Tracker_Metadata_get_registered_types_async  (client->proxy_metadata, class, tracker_array_reply, callback_struct);
}


void
tracker_metadata_get_writeable_types_async (TrackerClient *client, const char *class, TrackerArrayReply callback, gpointer user_data)
{

	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;


	/* This does nothing now, this API has been removed */
	g_warning ("%s no longer does anything", __FUNCTION__);

	tracker_void_reply (client->proxy_metadata, NULL, callback_struct);
}



void
tracker_metadata_get_registered_classes_async (TrackerClient *client, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;


	client->last_pending_call = org_freedesktop_Tracker_Metadata_get_registered_classes_async  (client->proxy_metadata,  tracker_array_reply, callback_struct);

}


void
tracker_metadata_get_unique_values_async (TrackerClient *client, ServiceType service, char **meta_types, const char *query, gboolean descending, int offset, int max_hits, TrackerGPtrArrayReply callback, gpointer user_data)
{

	GPtrArrayCallBackStruct *callback_struct;
	char *service_str = tracker_service_types[service];

	callback_struct = g_new (GPtrArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Metadata_get_unique_values_async (client->proxy_metadata, service_str, (const char **) meta_types, query, descending, offset, max_hits, tracker_GPtrArray_reply, callback_struct);

}

void
tracker_metadata_get_sum_async (TrackerClient *client, ServiceType service, char *field, char *query, TrackerIntReply callback, gpointer user_data)
{
	IntCallBackStruct *callback_struct;
	char *service_str = tracker_service_types[service];

	callback_struct = g_new (IntCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Metadata_get_sum_async (client->proxy_metadata, service_str, field, query, tracker_int_reply, callback_struct);
}


void
tracker_metadata_get_count_async (TrackerClient *client, ServiceType service, char *field, char *query, TrackerIntReply callback, gpointer user_data)
{
	IntCallBackStruct *callback_struct;
	char *service_str = tracker_service_types[service];

	callback_struct = g_new (IntCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Metadata_get_count_async (client->proxy_metadata, service_str, field, query, tracker_int_reply, callback_struct);
}

void
tracker_metadata_get_unique_values_with_count_async (TrackerClient *client, ServiceType service, char **meta_types, const char *query, char *count, gboolean descending, int offset, int max_hits, TrackerGPtrArrayReply callback, gpointer user_data)
{

	GPtrArrayCallBackStruct *callback_struct;
	char *service_str = tracker_service_types[service];

	callback_struct = g_new (GPtrArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Metadata_get_unique_values_with_count_async (client->proxy_metadata, service_str, (const char **) meta_types, query, count, descending, offset, max_hits, tracker_GPtrArray_reply, callback_struct);

}


void
tracker_keywords_get_list_async (TrackerClient *client, ServiceType service, TrackerGPtrArrayReply callback, gpointer user_data)
{

	GPtrArrayCallBackStruct *callback_struct;

	callback_struct = g_new (GPtrArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;


	char *service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Keywords_get_list_async (client->proxy_keywords, service_str, tracker_GPtrArray_reply, callback_struct);


}


void
tracker_keywords_get_async (TrackerClient *client, ServiceType service, const char *id, TrackerArrayReply callback, gpointer user_data)
{

	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Keywords_get_async (client->proxy_keywords, service_str, id, tracker_array_reply, callback_struct);

}



void
tracker_keywords_add_async (TrackerClient *client, ServiceType service, const char *id, char **values, TrackerVoidReply callback, gpointer user_data)
{

	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = tracker_service_types[service];
	client->last_pending_call = org_freedesktop_Tracker_Keywords_add_async (client->proxy_keywords, service_str, id, (const char **)values, tracker_void_reply, callback_struct);
}



void
tracker_keywords_remove_async (TrackerClient *client, ServiceType service, const char *id, char **values, TrackerVoidReply callback, gpointer user_data)
{

	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Keywords_remove_async (client->proxy_keywords, service_str, id, (const char **)values, tracker_void_reply, callback_struct);
}



void
tracker_keywords_remove_all_async (TrackerClient *client, ServiceType service, const char *id, TrackerVoidReply callback, gpointer user_data)
{

	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Keywords_remove_all_async (client->proxy_keywords, service_str, id, tracker_void_reply, callback_struct);
}


void
tracker_keywords_search_async	(TrackerClient *client, int live_query_id, ServiceType service, char **keywords, int offset, int max_hits, TrackerArrayReply callback, gpointer user_data)
{

	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Keywords_search_async (client->proxy_keywords, live_query_id, service_str, (const char **)keywords, offset, max_hits, tracker_array_reply, callback_struct);

}


void
tracker_search_text_get_hit_count_async (TrackerClient *client, ServiceType service, const char *search_text, TrackerIntReply callback, gpointer user_data)
{
	char *service_str = tracker_service_types[service];

	IntCallBackStruct *callback_struct;

	callback_struct = g_new (IntCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call =  org_freedesktop_Tracker_Search_get_hit_count_async (client->proxy_search, service_str, search_text, tracker_int_reply, callback_struct);


}


void
tracker_search_text_get_hit_count_all_async (TrackerClient *client, const char *search_text, TrackerGPtrArrayReply callback, gpointer user_data)
{

	GPtrArrayCallBackStruct *callback_struct;

	callback_struct = g_new (GPtrArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Search_get_hit_count_all_async (client->proxy_search, search_text, tracker_GPtrArray_reply, callback_struct);


}

void
tracker_search_text_async (TrackerClient *client, int live_query_id, ServiceType service, const char *search_text, int offset, int max_hits, TrackerArrayReply callback, gpointer user_data)
{

	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Search_text_async (client->proxy_search, live_query_id, service_str, search_text, offset, max_hits, tracker_array_reply, callback_struct);

}


void
tracker_search_text_detailed_async (TrackerClient *client, int live_query_id, ServiceType service, const char *search_text, int offset, int max_hits, TrackerGPtrArrayReply callback, gpointer user_data)
{

	GPtrArrayCallBackStruct *callback_struct;

	callback_struct = g_new (GPtrArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Search_text_detailed_async (client->proxy_search, live_query_id, service_str, search_text, offset, max_hits, tracker_GPtrArray_reply, callback_struct);

}


void
tracker_search_get_snippet_async (TrackerClient *client, ServiceType service, const char *uri, const char *search_text, TrackerStringReply callback, gpointer user_data)
{
	StringCallBackStruct *callback_struct;

	callback_struct = g_new (StringCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Search_get_snippet_async (client->proxy_search, service_str, uri, search_text, tracker_string_reply, callback_struct);

}


void
tracker_search_metadata_async	(TrackerClient *client, ServiceType service, const char *field, const char* search_text, int offset, int max_hits, TrackerArrayReply callback, gpointer user_data)
{

	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = tracker_service_types[service];

	org_freedesktop_Tracker_Search_metadata_async (client->proxy_search, service_str, field, search_text,  offset, max_hits,  tracker_array_reply, callback_struct);

}

void
tracker_search_query_async (TrackerClient *client, int live_query_id, ServiceType service, char **fields, const char *search_text,  const char *keywords, const char *query, int offset, int max_hits, gboolean sort_by_service, char **sort_fields, gboolean sort_descending, TrackerGPtrArrayReply callback, gpointer user_data)
{
	GPtrArrayCallBackStruct *callback_struct;

	callback_struct = g_new (GPtrArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Search_query_async (client->proxy_search, live_query_id, service_str, (const char **)fields, search_text, keywords, query, sort_by_service, (const char **)sort_fields, sort_descending, offset, max_hits,	tracker_GPtrArray_reply, callback_struct);

}


void
tracker_search_suggest_async (TrackerClient *client, const char *search_text, int maxdist, TrackerStringReply callback, gpointer user_data)
{

	StringCallBackStruct *callback_struct;

	callback_struct = g_new (StringCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Search_suggest_async (client->proxy_search, search_text, maxdist,  tracker_string_reply, callback_struct);

}


void
tracker_files_create_async (TrackerClient *client, const char *uri, gboolean is_directory, const char *mime, int size, int mtime, TrackerVoidReply callback, gpointer user_data)
{
	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Files_create_async (client->proxy_files, uri, is_directory, mime, size, mtime, tracker_void_reply, callback_struct);
}


void
tracker_files_delete_async (TrackerClient *client, const char *uri, TrackerVoidReply callback, gpointer user_data)
{

	VoidCallBackStruct *callback_struct;

	callback_struct = g_new (VoidCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Files_delete_async (client->proxy_files, uri,  tracker_void_reply, callback_struct);
}


void
tracker_files_get_text_contents_async	(TrackerClient *client,  const char *uri, int offset, int max_length, TrackerStringReply callback, gpointer user_data)
{
	StringCallBackStruct *callback_struct;

	callback_struct = g_new (StringCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Files_get_text_contents_async (client->proxy_files, uri, offset, max_length,  tracker_string_reply, callback_struct);

}



void
tracker_files_search_text_contents_async (TrackerClient *client,  const char *uri, const char *search_text, int length, TrackerStringReply callback, gpointer user_data)
{

	StringCallBackStruct *callback_struct;

	callback_struct = g_new (StringCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Files_search_text_contents_async (client->proxy_files, uri, search_text, length,  tracker_string_reply, callback_struct);


}



void
tracker_files_get_by_service_type_async (TrackerClient *client,  int live_query_id, ServiceType service, int offset, int max_hits, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *service_str = tracker_service_types[service];

	client->last_pending_call = org_freedesktop_Tracker_Files_get_by_service_type_async (client->proxy_files, live_query_id, service_str, offset, max_hits,  tracker_array_reply, callback_struct);
}



void
tracker_files_get_by_mime_type_async	(TrackerClient *client,  int live_query_id, char **mimes, int offset, int max_hits, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Files_get_by_mime_type_async (client->proxy_files, live_query_id, (const char **)mimes, offset, max_hits,  tracker_array_reply, callback_struct);

}



void
tracker_files_get_by_mime_type_vfs_async (TrackerClient *client,  int live_query_id, char **mimes, int offset, int max_hits, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Files_get_by_mime_type_vfs_async (client->proxy_files, live_query_id,(const char **) mimes, offset,  max_hits,  tracker_array_reply, callback_struct);

}



void
tracker_files_get_mtime_async	(TrackerClient *client, const char *uri, TrackerIntReply callback, gpointer user_data)
{

	IntCallBackStruct *callback_struct;

	callback_struct = g_new (IntCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Files_get_mtime_async (client->proxy_files, uri,  tracker_int_reply, callback_struct);

}


void
tracker_files_get_metadata_for_files_in_folder_async	(TrackerClient *client, int live_query_id, const char *uri, char **fields, TrackerGPtrArrayReply callback, gpointer user_data)
{
	GPtrArrayCallBackStruct *callback_struct;

	callback_struct = g_new (GPtrArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Files_get_metadata_for_files_in_folder_async  (client->proxy_files, live_query_id, uri, (const char **)fields,  tracker_GPtrArray_reply, callback_struct);


}


void
tracker_search_metadata_by_text_async (TrackerClient *client, const char *query,  TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	char *metadata;
	char *keywords[2];

	if (strchr (query, ':') != NULL) {
		metadata = strtok ((char *)query, ":");
		if(strcoll(metadata,"tag") == 0){
			keywords[0] = strtok (NULL, ":");
			keywords[1] = NULL;
			client->last_pending_call = org_freedesktop_Tracker_Keywords_search_async (client->proxy_keywords, -1, "Files", (const char **)keywords, 0, 512, tracker_array_reply, callback_struct);
		}
	}else{
		client->last_pending_call = org_freedesktop_Tracker_Search_text_async (client->proxy_search, -1, "Files", query,  0, 512, tracker_array_reply, callback_struct);
	}
}

void
tracker_search_metadata_by_text_and_mime_async (TrackerClient *client, const char *query, const char **mimes, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Files_search_by_text_and_mime_async  (client->proxy_files, query,(const char **) mimes,  tracker_array_reply, callback_struct);

}


void
tracker_search_metadata_by_text_and_mime_and_location_async (TrackerClient *client, const char *query, const char **mimes, const char *location, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Files_search_by_text_and_mime_and_location_async (client->proxy_files, query, (const char **)mimes, location,  tracker_array_reply, callback_struct);

}



void
tracker_search_metadata_by_text_and_location_async (TrackerClient *client, const char *query, const char *location, TrackerArrayReply callback, gpointer user_data)
{
	ArrayCallBackStruct *callback_struct;

	callback_struct = g_new (ArrayCallBackStruct, 1);
	callback_struct->callback = callback;
	callback_struct->data = user_data;

	client->last_pending_call = org_freedesktop_Tracker_Files_search_by_text_and_location_async (client->proxy_files, query, location,  tracker_array_reply, callback_struct);

}

