/* Tracker
 * Copyright (C) 2005, Mr Jamie McCracken (jamiemcc@gnome.org)
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
#include "tracker.h"

#define TRACKER_FILE_METADATA_SERVICE                   "org.freedesktop.file.metadata"
#define TRACKER_FILE_METADATA_OBJECT			"/org/freedesktop/file/metadata"
#define TRACKER_FILE_METADATA_INTERFACE			"org.freedesktop.file.metadata"

static DBusGProxy *proxy;

static void
tracker_search_metadata_by_text_reply (DBusGProxy *proxy, char **OUT_result, GError *error, gpointer userdata)
{

	(*(TrackerArrayReply) userdata) (OUT_result, error);
	
}

char *
tracker_get_metadata (const char *uri, const char *key, GError *error)
{
	char *str;

	if (!org_freedesktop_file_metadata_get_metadata   (proxy, uri, key, &str, &error)) {
	
		g_warning ("method failed: %s", error->message);
		g_error_free (error);

	}
	return str;

}

void
tracker_set_metadata (const char *uri, const char *key, const char *value, GError *error)
{
	if (!org_freedesktop_file_metadata_get_metadata   (proxy, uri, key, value, &error)) {
	
		g_warning ("method failed: %s", error->message);
		g_error_free (error);

	}
}

void		
tracker_register_metadata_type (const char *name, MetadataTypes type, GError *error)
{
	if (!org_freedesktop_file_metadata_register_metadata_type (proxy, name, type, &error)) {
	
		g_warning ("method failed: %s", error->message);
		g_error_free (error);

	}
}


GHashTable *	
tracker_get_metadata_for_files_in_folder (const char *uri, char **keys, GError *error)
{
	GHashTable *table;

	if (!org_freedesktop_file_metadata_get_metadata_for_files_in_folder (proxy, uri, keys, &table, &error)) {
		g_warning ("method failed: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	return table;
}



void
tracker_search_metadata_async 	(const char *query, TrackerArrayReply callback) 
{
	org_freedesktop_file_metadata_search_metadata_by_text_async (proxy, query, tracker_search_metadata_by_text_reply, callback);
}

char **
tracker_search_metadata	(const char *query,  GError *error)
{

	char **strs;

	if (!org_freedesktop_file_metadata_search_metadata_by_text  (proxy, query, &strs, &error)) {
	
		g_warning ("method failed: %s", error->message);
		g_error_free (error);
		return NULL;
	}
	return strs;
	
}


char **
tracker_search_metadata_by_query (const char *query,  GError *error)
{

	char **strs;

	if (!org_freedesktop_file_metadata_search_metadata_by_query  (proxy, query, &strs, &error)) {
	
		g_warning ("method failed: %s", error->message);
		g_error_free (error);
		return NULL;
	}
	return strs;
	
}

static void
tracker_search_metadata_by_query_reply (DBusGProxy *proxy, char **OUT_result, GError *error, gpointer userdata)
{

	(*(TrackerArrayReply) userdata) (OUT_result, error);
	
}


void
tracker_search_metadata_by_query_async 	(const char *query, TrackerArrayReply callback) 
{
	org_freedesktop_file_metadata_search_metadata_by_text_async (proxy, query, tracker_search_metadata_by_query_reply, callback);
}


gboolean
tracker_init ()
{
	DBusGConnection *connection;
	GError *error = NULL;

	g_type_init ();

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	
	if (connection == NULL)	{
		g_warning("Unable to connect to dbus: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
			TRACKER_FILE_METADATA_SERVICE,
			TRACKER_FILE_METADATA_OBJECT,
			TRACKER_FILE_METADATA_INTERFACE);

	if (!proxy) {
		return FALSE;
	}


	return TRUE;

}

void
tracker_close ()
{
	g_object_unref (proxy);
}
