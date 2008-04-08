/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "tracker-service-manager.h"

typedef struct {
	gchar *prefix;
	gint   service;
} ServiceMimePrefixes;

/* Hash (gint service_type_id, TrackerService *service) */ 
static GHashTable *service_id_table;   

/* Hash (gchar *service_name, TrackerService *service) */
static GHashTable *service_table;      

/* Hash (gchar *mime, gint service_type_id) */
static GHashTable *mime_service;       

/* List of ServiceMimePrefixes */
static GSList *mime_prefix_service; 

static void
service_manager_mime_prefix_foreach (gpointer data, 
				     gpointer user_data) 
{
	ServiceMimePrefixes *mime_prefix;

	mime_prefix = (ServiceMimePrefixes*) data;

	g_free (mime_prefix->prefix);
	g_free (mime_prefix);
}

gpointer
service_manager_hash_lookup_by_str (GHashTable  *hash_table, 
				    const gchar *str)
{
	gpointer *data;
	gchar    *str_lower;

	str_lower = g_utf8_strdown (str, -1);
	data = g_hash_table_lookup (hash_table, str_lower);
	g_free (str_lower);

	return data;
}

gpointer
service_manager_hash_lookup_by_id (GHashTable  *hash_table, 
				   gint         id)
{
	gpointer *data;
	gchar    *str;

	str = g_strdup_printf ("%d", id);
	data = g_hash_table_lookup (hash_table, str);
	g_free (str);

	return data;
}

void
tracker_service_manager_init (void)
{

	g_return_if_fail (service_id_table == NULL 
			  && service_table == NULL
			  && mime_service == NULL);

	service_id_table = g_hash_table_new_full (g_str_hash, 
						  g_str_equal, 
						  g_free, 
						  g_object_unref);
	
	service_table = g_hash_table_new_full (g_str_hash, 
					       g_str_equal,
					       g_free, 
					       g_object_unref);
	
	mime_service = g_hash_table_new_full (g_str_hash, 
					      g_str_equal, 
					      NULL, 
					      NULL);
}

void
tracker_service_manager_term (void)
{
	g_hash_table_remove_all (service_id_table);
	g_hash_table_remove_all (service_table);
	g_hash_table_remove_all (mime_service);

	if (mime_prefix_service) {
		g_slist_foreach (mime_prefix_service, 
				 service_manager_mime_prefix_foreach, 
				 NULL); 
		g_slist_free (mime_prefix_service);
	}
}

void 
tracker_service_manager_add_service (TrackerService *service,
				     GSList         *mimes,
				     GSList         *mime_prefixes)
{

	GSList              *mime, *prefix;
	ServiceMimePrefixes *service_mime_prefix;
	gint                 id;
	const gchar         *name;

	g_return_if_fail (TRACKER_IS_SERVICE (service));

	id = tracker_service_get_id (service);
	name = tracker_service_get_name (service);

	g_hash_table_insert (service_table, 
			     g_utf8_strdown (name, -1), 
			     g_object_ref (service));
	g_hash_table_insert (service_id_table, 
			     g_strdup_printf ("%d", id), 
			     g_object_ref (service));

	for (mime = mimes; mime != NULL; mime = mime->next) {
		g_hash_table_insert (mime_service, 
				     mime->data, 
				     GINT_TO_POINTER (id));
	}

	for (prefix = mime_prefixes; prefix != NULL; prefix = prefix->next) {
		service_mime_prefix = g_new0 (ServiceMimePrefixes, 1);
		service_mime_prefix->prefix = prefix->data;
		service_mime_prefix->service = id;
		mime_prefix_service = g_slist_prepend (mime_prefix_service, 
						       service_mime_prefix);
	}
}

TrackerService *
tracker_service_manager_get_service (const gchar *service_str)
{
	return service_manager_hash_lookup_by_str (service_table, service_str);
}

gchar *
tracker_service_manager_get_service_by_id (gint id)
{
	TrackerService *service;

	service = service_manager_hash_lookup_by_id (service_id_table, id);

	if (!service) {
		return NULL;
	}

	return g_strdup (tracker_service_get_name (service));
}

gchar *
tracker_service_manager_get_service_type_for_mime (const gchar *mime) 
{
	gpointer            *id;
	ServiceMimePrefixes *item;
	GSList              *prefix_service;

	/* Try a complete mime */
	id = g_hash_table_lookup (mime_service, mime);
	if (id) {
		return tracker_service_manager_get_service_by_id (GPOINTER_TO_INT (id));
	}

	/* Try in prefixes */
	for (prefix_service = mime_prefix_service; 
	     prefix_service != NULL; 
	     prefix_service = prefix_service->next) {
		item = prefix_service->data;
		if (g_str_has_prefix (mime, item->prefix)) {
			return tracker_service_manager_get_service_by_id (item->service);
		}
	}
	
	/* Default option */
	return g_strdup ("Other");
}

gint
tracker_service_manager_get_id_for_service (const char *service_str)
{
	TrackerService *service;

	service = service_manager_hash_lookup_by_str (service_table, service_str);

	if (!service) {
		return -1;
	}

	return tracker_service_get_id (service);
}

gchar *
tracker_service_manager_get_parent_service_by_id (gint id)
{
	TrackerService *service;

	service = service_manager_hash_lookup_by_id (service_id_table, id);

	if (!service) {
		return NULL;
	}

	return g_strdup (tracker_service_get_parent (service));
}

gint
tracker_service_manager_get_parent_id_for_service_id (gint id)
{
	TrackerService *service;
	const gchar    *parent = NULL;

	service = service_manager_hash_lookup_by_id (service_id_table, id);

	if (service) {
		parent = tracker_service_get_parent (service);
	}

	if (!parent) {
		return -1;
	}
	
	service = service_manager_hash_lookup_by_str (service_table, parent);

	if (!service) {
		return -1;
	}

	return tracker_service_get_id (service);
}

gint
tracker_service_manager_get_id_for_parent_service (const gchar *service_str)
{
	TrackerService *service;
	const gchar    *parent = NULL;

	service = service_manager_hash_lookup_by_str (service_table, service_str);

	if (service) {
		parent = tracker_service_get_parent (service);
	}

	if (!parent) {
		return -1;
	}

	return tracker_service_manager_get_id_for_service (parent);
}

gchar *
tracker_service_manager_get_parent_service (const gchar *service_str)
{
	TrackerService *service;
	const gchar    *parent = NULL;

	service = service_manager_hash_lookup_by_str (service_table, service_str);
	
	if (service) {
		parent = tracker_service_get_parent (service);
	}

	return g_strdup (parent);
}

TrackerDBType
tracker_service_manager_get_db_for_service (const gchar *service_str)
{
	TrackerDBType  type;
	gchar         *str;

	type = TRACKER_DB_TYPE_DATA;
	str = g_utf8_strdown (service_str, -1);

	if (g_str_has_prefix (str, "emails") || 
	    g_str_has_prefix (str, "attachments")) {
		type = TRACKER_DB_TYPE_EMAIL;
	}

	g_free (str);

	return type;
}

gboolean
tracker_service_manager_is_service_embedded (const gchar *service_str)
{
	TrackerService *service;

	service = service_manager_hash_lookup_by_str (service_table, service_str);

	if (!service) {
		return FALSE;
	}

	return tracker_service_get_embedded (service);
}

gboolean
tracker_service_manager_is_valid_service (const gchar *service_str)
{
	return tracker_service_manager_get_id_for_service (service_str) != -1;
}

gint
tracker_service_manager_metadata_in_service (const gchar *service_str, 
					     const gchar *meta_name)
{
	TrackerService *service;
	gint            i;
	const GSList   *l;

	service = service_manager_hash_lookup_by_str (service_table, service_str);

	if (!service) {
		return 0;
	}

	for (l = tracker_service_get_key_metadata (service), i = 0; 
	     l; 
	     l = l->next, i++) {
		if (!l->data) {
			continue;
		}

		if (strcasecmp (l->data, meta_name) == 0) {
			return i;
		}
	}

	return 0;
}

gboolean
tracker_service_manager_has_metadata (const gchar *service_str) 
{
	TrackerService *service;

	service = service_manager_hash_lookup_by_str (service_table, service_str);

	if (!service) {
		return FALSE;
	}

	return tracker_service_get_has_metadata (service);
}

gboolean
tracker_service_manager_has_thumbnails (const gchar *service_str)
{
	TrackerService *service;

	service = service_manager_hash_lookup_by_str (service_table, service_str);

	if (!service) {
		return FALSE;
	}

	return tracker_service_get_has_thumbs (service);
}

gboolean 
tracker_service_manager_has_text (const char *service_str) 
{
	TrackerService *service;

	service = service_manager_hash_lookup_by_str (service_table, service_str);

	if (!service) {
		return FALSE;
	}

	return tracker_service_get_has_full_text (service);
}

gboolean
tracker_service_manager_show_service_files (const gchar *service_str) 
{
	TrackerService *service;

	service = service_manager_hash_lookup_by_str (service_table, service_str);

	if (!service) {
		return FALSE;
	}

	return tracker_service_get_show_service_files (service);
}

gboolean
tracker_service_manager_show_service_directories (const gchar *service_str) 
{
	TrackerService *service;

	service = service_manager_hash_lookup_by_str (service_table, service_str);

	if (!service) {
		return FALSE;
	}

	return tracker_service_get_show_service_directories (service);
}
