/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 * Authors: Philip Van Hoof (pvanhoof@gnome.org)
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

#include "tracker-xesam-manager.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_XESAM_SESSION, struct _TrackerXesamSessionPriv))

struct _TrackerXesamSessionPriv {
	GHashTable *searches;
	GHashTable *props;
	gchar	   *session_id;
};

G_DEFINE_TYPE (TrackerXesamSession, tracker_xesam_session, G_TYPE_OBJECT)

static void
tracker_xesam_session_g_value_free (GValue *value)
{
	g_value_unset(value);
	g_free(value);
}

/**
 * tracker_xesam_session_get_props:
 * @self: A #TrackerXesamSession
 *
 * Get the properties of @self. The returned value is a hashtable with key as
 * Xesam session property key strings and value a #GValue being either a string,
 * an integer, a boolean or an array (either TRACKER_TYPE_XESAM_STRV_ARRAY or
 * G_TYPE_STRV).
 *
 * The returned value must be unreferenced using @g_hash_table_unref.
 *
 * returns (caller-owns): a read-only hash-table with Xesam properties
 **/
GHashTable *
tracker_xesam_session_get_props (TrackerXesamSession *self)
{
	return g_hash_table_ref (self->priv->props);
}

static void
tracker_xesam_session_init (TrackerXesamSession *self)
{
	TrackerXesamSessionPriv *priv;
	GValue *value;
	const gchar *hit_fields[2] = {"xesam:url", NULL};
	const gchar *hit_fields_extended[1] = {NULL};
	const gchar *fields[3] = {"xesam:url", "xesam:relevancyRating", NULL};
	const gchar *contents[2] = {"xesam:Content", NULL};
	const gchar *sources[2] = {"xesam:Source", NULL};
	const gchar *exts[1] = {NULL};
	const gchar *dummy_onto[4] = {"dummy-onto","0.1","/usr/share/xesam/ontologies/dummy-onto-0.1", NULL};
	GPtrArray *ontos = g_ptr_array_new ();

	priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,TRACKER_TYPE_XESAM_SESSION,struct _TrackerXesamSessionPriv);

	g_ptr_array_add (ontos, dummy_onto);

	priv->session_id = NULL;


	priv->searches = g_hash_table_new_full (g_str_hash, g_str_equal,
				(GDestroyNotify) g_free,
				(GDestroyNotify) g_object_unref);

	priv->props = g_hash_table_new_full (g_str_hash, g_str_equal,
				(GDestroyNotify) g_free,
				(GDestroyNotify) tracker_xesam_session_g_value_free);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_BOOLEAN);
	g_value_set_boolean (value, FALSE);
	g_hash_table_insert (priv->props, g_strdup ("search.live"), value);


	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_STRV);
	g_value_set_boxed (value, hit_fields);
	g_hash_table_insert (priv->props, g_strdup ("hit.fields"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_STRV);
	g_value_set_boxed (value, hit_fields_extended);
	g_hash_table_insert (priv->props, g_strdup ("hit.fields.extended"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_UINT);
	g_value_set_uint (value, 200);
	g_hash_table_insert (priv->props, g_strdup ("hit.snippet.length"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_STRING);
	g_value_set_string (value, "xesam:relevancyRating");
	g_hash_table_insert (priv->props, g_strdup ("sort.primary"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_STRING);
	g_value_set_string (value, "");
	g_hash_table_insert (priv->props, g_strdup ("sort.secondary"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_STRING);
	g_value_set_string (value, "descending");
	g_hash_table_insert (priv->props, g_strdup ("sort.order"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_STRING);
	g_value_set_string (value, "TrackerXesamSession");
	g_hash_table_insert (priv->props, g_strdup ("vendor.id"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_UINT);
	g_value_set_uint (value, 1);
	g_hash_table_insert (priv->props, g_strdup ("vendor.version"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_STRING);
	g_value_set_string (value, "Tracker Xesam Service");
	g_hash_table_insert (priv->props, g_strdup ("vendor.display"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_UINT);
	g_value_set_uint (value, 90);
	g_hash_table_insert (priv->props, g_strdup ("vendor.xesam"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_STRV);
	g_value_set_boxed (value, fields);
	g_hash_table_insert (priv->props, g_strdup ("vendor.ontology.fields"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_STRV);
	g_value_set_boxed (value, contents);
	g_hash_table_insert (priv->props, g_strdup ("vendor.ontology.contents"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_STRV);
	g_value_set_boxed (value, sources);
	g_hash_table_insert (priv->props, g_strdup ("vendor.ontology.sources"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_STRV);
	g_value_set_boxed (value, exts);
	g_hash_table_insert (priv->props, g_strdup ("vendor.extensions"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, TRACKER_TYPE_XESAM_STRV_ARRAY);
	g_value_set_boxed(value, ontos);
	/* Comment by Philip Van Hoof: I don't see how we are freeing this one ,
	 * up. So we are most likely just leaking the GPtrArray per session ... */
	g_hash_table_insert (priv->props, g_strdup ("vendor.ontologies"), value);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_UINT);
	g_value_set_uint (value, 50);
	g_hash_table_insert (priv->props, g_strdup ("vendor.maxhits"), value);

}

static void
tracker_xesam_session_finalize (GObject *object)
{
	TrackerXesamSession *self = (TrackerXesamSession *) object;
	TrackerXesamSessionPriv *priv = self->priv;

	g_free (priv->session_id);
	g_hash_table_destroy (priv->searches);
	g_hash_table_destroy (priv->props);

}

static void
tracker_xesam_session_class_init (TrackerXesamSessionClass *klass)
{
	GObjectClass *object_class;
	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = tracker_xesam_session_finalize;

	g_type_class_add_private( klass, sizeof(struct _TrackerXesamSessionPriv) );
}

/**
 * tracker_xesam_session_get_id:
 * @self: A #TrackerXesamSession
 * @session_id: a unique ID string for @self
 *
 * Set a read-only unique ID string for @self.
 **/
void
tracker_xesam_session_set_id (TrackerXesamSession *self,
			      const gchar	  *session_id)
{
	TrackerXesamSessionPriv *priv = self->priv;

	if (priv->session_id)
		g_free (priv->session_id);
	priv->session_id = g_strdup (session_id);
}

/**
 * tracker_xesam_session_get_id:
 * @self: A #TrackerXesamSession
 *
 * Get the read-only unique ID string for @self.
 *
 * returns: a unique id
 **/
const gchar*
tracker_xesam_session_get_id (TrackerXesamSession *self)
{
	TrackerXesamSessionPriv *priv = self->priv;

	return (const gchar*) priv->session_id;
}

/**
 * tracker_xesam_session_get_searches:
 * @self: A #TrackerXesamSession
 *
 * Get all searches in @self as a doubly linked list containing
 * #TrackerXesamLiveSearch objects.
 *
 * @returns: (caller-owns) (null-ok): all searches in @self
 **/
GList *
tracker_xesam_session_get_searches (TrackerXesamSession *self)
{
	TrackerXesamSessionPriv *priv = self->priv;

	return g_hash_table_get_values (priv->searches);
}

/**
 * tracker_xesam_session_set_property:
 * @self: A #TrackerXesamSession
 * @prop: The name or the property to set, see the list of session properties
 * for valid property names at http://xesam.org/main/XesamSearchAPI#properties
 * @val: The value to set the property to
 * @new_val: (out) (caller-owns): The actual value the search engine will use.
 * As noted above it is  not guaranteed that the requested value will be
 * respected
 * @error: (null-ok) (out): a #GError
 *
 * Set a property on the session. It is not guaranteed that the session property
 * will actually be used, the return value is the property value that will be
 * used. Search engines must respect the default property values however. For a
 * list of properties and descriptions see below.
 *
 * Calling this method after the first search has been created with
 * @tracker_xesam_session_create_search is illegal. The server will raise an
 * error if you do. Ie. once you create the first search the properties are set
 * in stone for the parent session. The search engine will also throw an error
 * if the session handle has been closed or is invalid.
 *
 * An error will also be thrown if the prop parameter is not a valid session
 * property, if it is a property marked as read-only, or if the requested value
 * is invalid.
 **/
void
tracker_xesam_session_set_property (TrackerXesamSession  *self,
				    const gchar		 *prop,
				    const GValue	 *val,
				    GValue		**new_val,
				    GError		**error)
{
	TrackerXesamSessionPriv *priv = self->priv;
	const gchar *read_only[11] = {"vendor.id", "vendor.version", "vendor.display",
		"vendor.xesam", "vendor.ontology.fields", "vendor.ontology.contents",
		"vendor.ontology.sources", "vendor.extensions", "vendor.ontologies",
		"vendor.maxhits", NULL};
	GValue *property = NULL;
	gboolean found = FALSE;
	gint i = 0;

	while (read_only[i]) {
		if (!strcmp (prop, read_only[i])) {
			found = TRUE;
			break;
		}
		i++;
	}

	if (!found)
		property = g_hash_table_lookup (priv->props, prop);

	if (!property) {
		g_set_error (error, TRACKER_XESAM_ERROR_DOMAIN,
				TRACKER_XESAM_ERROR_PROPERTY_NOT_SUPPORTED,
				"Property not supported");
		*new_val = NULL;
	} else {
		GValue *target_val = g_new0 (GValue, 1);
		g_value_init (target_val, G_VALUE_TYPE (property));
		g_value_transform (val, property);
		g_value_transform (val, target_val);
		*new_val = target_val;
	}
}

/**
 * tracker_xesam_session_get_property:
 * @self: A #TrackerXesamSession
 * @prop: The name or the property to set, see the list of session properties
 * for valid property names at http://xesam.org/main/XesamSearchAPI#properties
 * @value: (out) (caller-owns): The value of a session property
 * @error: (null-ok) (out): a #GError
 *
 * Get the value of a session property. The server should throw an error if the
 * session handle is closed or does not exist. An error should also be raised if
 * prop is not a valid session property.
 **/
void
tracker_xesam_session_get_property (TrackerXesamSession  *self,
				    const gchar		 *prop,
				    GValue		**value,
				    GError		**error)
{
	TrackerXesamSessionPriv *priv = self->priv;

	GValue *property = g_hash_table_lookup (priv->props, prop);

	if (!property) {
		g_set_error (error, TRACKER_XESAM_ERROR_DOMAIN,
				TRACKER_XESAM_ERROR_PROPERTY_NOT_SUPPORTED,
				"Property not supported");
		*value = NULL;
	} else {
		GValue *target_val = g_new0 (GValue, 1);
		g_value_init (target_val, G_VALUE_TYPE (property));
		g_value_transform (property, target_val);
		*value = target_val;
	}

	return;
}

/**
 * tracker_xesam_session_create_search:
 * @self: A #TrackerXesamSession
 * @query_xml: A string in the xesam query language
 * @search_id: (out) (caller-owns): An opaque handle for the Search object
 * @error: (null-ok) (out): a #GError
 *
 * Create a new search from @query_xml. If there are errors parsing the
 * @query_xml parameter an error will be set in @error.
 *
 * Notifications of hits can be obtained by listening to the @hits-added signal.
 * Signals will not be emitted before a call to @tracker_xesam_live_search_activate
 * has been made.
 *
 * @returns: (null-ok) (caller-owns): a new non-activated #TrackerXesamLiveSearch
 **/
TrackerXesamLiveSearch*
tracker_xesam_session_create_search (TrackerXesamSession  *self,
				     const gchar	  *query_xml,
				     gchar		 **search_id,
				     GError		 **error)
{
	TrackerXesamLiveSearch	*search;
	TrackerXesamSessionPriv *priv = self->priv;

	g_debug ("Creating search for xesam session: \n %s", query_xml);

	search = tracker_xesam_live_search_new (query_xml);

	tracker_xesam_live_search_set_session (search, self);
	tracker_xesam_live_search_set_id (search,
					  tracker_xesam_manager_generate_unique_key ());

	if (tracker_xesam_live_search_parse_query (search, error)) {

		g_debug ("Xesam live search added");
		g_hash_table_insert (priv->searches,
			g_strdup (tracker_xesam_live_search_get_id (search)),
			g_object_ref (search));

		if (search_id)
			*search_id = g_strdup (tracker_xesam_live_search_get_id (search));

	} else {
		g_message ("Xesam search parse failed");
		g_object_unref (search);
		search = NULL;
	}

	return search;
}

/**
 * tracker_xesam_session_get_search:
 * @self: A #TrackerXesamSession
 * @search_id: (in): An opaque handle for the Search object
 * @error: (null-ok) (out): a #GError
 *
 * Get the #TrackerXesamLiveSearch identified by @search_id in @self.
 *
 * @returns: (null-ok) (caller-owns): a #TrackerXesamLiveSearch or NULL
 **/
TrackerXesamLiveSearch*
tracker_xesam_session_get_search (TrackerXesamSession  *self,
				  const gchar	       *search_id,
				  GError	      **error)
{
	TrackerXesamSessionPriv *priv = self->priv;
	TrackerXesamLiveSearch *search = g_hash_table_lookup (priv->searches, search_id);

	if (search)
		g_object_ref (search);
	else {
		g_set_error (error, TRACKER_XESAM_ERROR_DOMAIN,
				TRACKER_XESAM_ERROR_SEARCH_ID_NOT_REGISTERED,
				"SearchID not registered");
	}

	return search;
}

/**
 * tracker_xesam_session_new:
 *
 * Create a new #TrackerXesamSession
 *
 * @returns: (caller-owns): a new #TrackerXesamSession
 **/
TrackerXesamSession*
tracker_xesam_session_new (void)
{
	return g_object_newv (TRACKER_TYPE_XESAM_SESSION, 0, NULL);
}


