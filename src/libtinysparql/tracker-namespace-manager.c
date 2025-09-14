/*
 * Copyright (C) 2016, Sam Thursfield <sam@afuera.me.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "tracker-namespace-manager.h"
#include "tracker-ontologies.h"
#include "tracker-private.h"

#define MAX_PREFIX_LENGTH 100

typedef struct {
	const gchar *prefix;
	const gchar *uri;
	int uri_len;
} PrefixMap;

typedef struct {
	GHashTable *prefix_to_namespace;
	GHashTable *namespace_to_prefix;
	GArray *prefix_map;
	gboolean sealed;
} TrackerNamespaceManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (TrackerNamespaceManager, tracker_namespace_manager, G_TYPE_OBJECT)
#define GET_PRIVATE(object)  (tracker_namespace_manager_get_instance_private (object))

/**
 * TrackerNamespaceManager:
 *
 * `TrackerNamespaceManager` object represents a mapping between namespaces and
 * their shortened prefixes.
 *
 * This object keeps track of namespaces, and allows you to assign
 * short prefixes for them to avoid frequent use of full namespace IRIs. The syntax
 * used is that of [Compact URIs (CURIEs)](https://www.w3.org/TR/2010/NOTE-curie-20101216).
 *
 * Usually you will want to use a namespace manager obtained through
 * [method@SparqlConnection.get_namespace_manager] from the
 * [class@SparqlConnection] that manages the RDF data, as that will
 * contain all prefixes and namespaces that are pre-defined by its ontology.
 */

static void finalize     (GObject *object);

static void
tracker_namespace_manager_class_init (TrackerNamespaceManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = finalize;
}

static void
tracker_namespace_manager_init (TrackerNamespaceManager *self)
{
	TrackerNamespaceManagerPrivate *priv = GET_PRIVATE (self);

	priv->prefix_to_namespace = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->namespace_to_prefix = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->prefix_map = g_array_new (FALSE, FALSE, sizeof (PrefixMap));
}

static void
finalize (GObject *object)
{
	TrackerNamespaceManagerPrivate *priv;

	priv = GET_PRIVATE (TRACKER_NAMESPACE_MANAGER (object));

	g_hash_table_unref (priv->prefix_to_namespace);
	g_hash_table_unref (priv->namespace_to_prefix);
	g_array_unref (priv->prefix_map);

	(G_OBJECT_CLASS (tracker_namespace_manager_parent_class)->finalize)(object);
}

/**
 * tracker_namespace_manager_new:
 *
 * Creates a new, empty `TrackerNamespaceManager` instance.
 *
 * Returns: a new `TrackerNamespaceManager` instance
 */
TrackerNamespaceManager *
tracker_namespace_manager_new ()
{
	TrackerNamespaceManager *namespace_manager;

	namespace_manager = g_object_new (TRACKER_TYPE_NAMESPACE_MANAGER, NULL);

	return namespace_manager;
}

/**
 * tracker_namespace_manager_get_default:
 *
 * Returns the global `TrackerNamespaceManager` that contains a set of well-known
 * namespaces and prefixes, such as `rdf:`, `rdfs:`, `nie:`, `tracker:`, etc.
 *
 * Note that the list of prefixes and namespaces is hardcoded in
 * libtracker-sparql. It may not correspond with the installed set of
 * ontologies, if they have been modified since they were installed.
 *
 * Returns: (transfer none): a global, shared `TrackerNamespaceManager` instance
 *
 * Deprecated: 3.3: Use [method@SparqlConnection.get_namespace_manager] instead.
 */
TrackerNamespaceManager *
tracker_namespace_manager_get_default ()
{
	static TrackerNamespaceManager *default_namespace_manager = NULL;

	if (g_once_init_enter (&default_namespace_manager)) {
		TrackerNamespaceManager *manager = tracker_namespace_manager_new();

		tracker_namespace_manager_add_prefix (manager, "rdf", TRACKER_PREFIX_RDF);
		tracker_namespace_manager_add_prefix (manager, "rdfs", TRACKER_PREFIX_RDFS);
		tracker_namespace_manager_add_prefix (manager, "xsd", TRACKER_PREFIX_XSD);
		tracker_namespace_manager_add_prefix (manager, "tracker", TRACKER_PREFIX_TRACKER);
		tracker_namespace_manager_add_prefix (manager, "dc", TRACKER_PREFIX_DC);

		tracker_namespace_manager_add_prefix (manager, "nrl", TRACKER_PREFIX_NRL);
		tracker_namespace_manager_add_prefix (manager, "nie", TRACKER_PREFIX_NIE);
		tracker_namespace_manager_add_prefix (manager, "nco", TRACKER_PREFIX_NCO);
		tracker_namespace_manager_add_prefix (manager, "nao", TRACKER_PREFIX_NAO);
		tracker_namespace_manager_add_prefix (manager, "nfo", TRACKER_PREFIX_NFO);

		tracker_namespace_manager_add_prefix (manager, "slo", TRACKER_PREFIX_SLO);
		tracker_namespace_manager_add_prefix (manager, "nmm", TRACKER_PREFIX_NMM);
		tracker_namespace_manager_add_prefix (manager, "mfo", TRACKER_PREFIX_MFO);
		tracker_namespace_manager_add_prefix (manager, "osinfo", TRACKER_PREFIX_OSINFO);
		tracker_namespace_manager_add_prefix (manager, "fts",
		                                      "http://tracker.api.gnome.org/ontology/v3/fts#");

		g_once_init_leave (&default_namespace_manager, manager);
	}

	return default_namespace_manager;
}

/**
 * tracker_namespace_manager_has_prefix:
 * @self: a `TrackerNamespaceManager`
 * @prefix: a string
 *
 * Returns whether @prefix is known.
 *
 * Returns: %TRUE if the `TrackerNamespaceManager` knows about @prefix, %FALSE otherwise
 */
gboolean
tracker_namespace_manager_has_prefix (TrackerNamespaceManager *self,
                                      const char              *prefix)
{
	TrackerNamespaceManagerPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_NAMESPACE_MANAGER (self), FALSE);

	priv = GET_PRIVATE (self);

	return g_hash_table_contains (priv->prefix_to_namespace, prefix);
}

/**
 * tracker_namespace_manager_lookup_prefix:
 * @self: a `TrackerNamespaceManager`
 * @prefix: a string
 *
 * Looks up the namespace URI corresponding to @prefix, or %NULL if the prefix
 * is not known.
 *
 * Returns: (nullable): a string owned by the `TrackerNamespaceManager`, or %NULL
 */
const char *
tracker_namespace_manager_lookup_prefix (TrackerNamespaceManager *self,
                                         const char              *prefix)
{
	TrackerNamespaceManagerPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_NAMESPACE_MANAGER (self), NULL);

	priv = GET_PRIVATE (self);

	return g_hash_table_lookup (priv->prefix_to_namespace, prefix);
}

/**
 * tracker_namespace_manager_add_prefix:
 * @self: A `TrackerNamespaceManager`
 * @prefix: a short, unique prefix to identify @namespace
 * @ns: the URL of the given namespace
 *
 * Adds @prefix as the recognised abbreviation of @namespace.
 *
 * Since 3.3, The `TrackerNamespaceManager` instances obtained through
 * [method@SparqlConnection.get_namespace_manager] are "sealed",
 * this API call should not performed on those.
 */
void
tracker_namespace_manager_add_prefix (TrackerNamespaceManager *self,
                                      const char              *prefix,
                                      const char              *ns)
{
	TrackerNamespaceManagerPrivate *priv;
	gchar *prefix_copy, *ns_copy;
	PrefixMap map;

	g_return_if_fail (TRACKER_IS_NAMESPACE_MANAGER (self));
	g_return_if_fail (strlen (prefix) <= MAX_PREFIX_LENGTH);
	g_return_if_fail (prefix != NULL);
	g_return_if_fail (ns != NULL);

	priv = GET_PRIVATE (TRACKER_NAMESPACE_MANAGER (self));
	g_return_if_fail (priv->sealed == FALSE);

	prefix_copy = g_strdup (prefix);
	ns_copy = g_strdup (ns);

	g_hash_table_insert (priv->prefix_to_namespace, prefix_copy, ns_copy);
	g_hash_table_insert (priv->namespace_to_prefix, g_strdup (ns), g_strdup (prefix));

	map.prefix = prefix_copy;
	map.uri = ns_copy;
	map.uri_len = strlen (map.uri);
	g_array_append_val (priv->prefix_map, map);
}

/**
 * tracker_namespace_manager_expand_uri:
 * @self: a `TrackerNamespaceManager`
 * @compact_uri: a URI or compact URI
 *
 * If @compact_uri begins with one of the prefixes known to this
 * `TrackerNamespaceManager`, then the return value will be the
 * expanded URI. Otherwise, a copy of @compact_uri will be returned.
 *
 * Returns: The possibly expanded URI in a newly-allocated string.
 */
char *
tracker_namespace_manager_expand_uri (TrackerNamespaceManager *self,
                                      const char              *compact_uri)
{
	TrackerNamespaceManagerPrivate *priv;

	char prefix[MAX_PREFIX_LENGTH + 1] = { 0 };
	char *colon;
	char *namespace = NULL;

	g_return_val_if_fail (TRACKER_IS_NAMESPACE_MANAGER (self), NULL);
	g_return_val_if_fail (compact_uri != NULL, NULL);

	priv = GET_PRIVATE (self);

	colon = strchr (compact_uri, ':');
	if (colon != NULL) {
		int colon_pos = colon - compact_uri;
		if (colon_pos < MAX_PREFIX_LENGTH) {
			strncpy (prefix, compact_uri, colon_pos);
			prefix[colon_pos] = 0;

			namespace = g_hash_table_lookup (priv->prefix_to_namespace, prefix);
		}
	}

	if (namespace) {
		return g_strconcat (namespace, colon + 1, NULL);
	} else {
		return g_strdup (compact_uri);
	}
}

/**
 * tracker_namespace_manager_compress_uri:
 * @self: a `TrackerNamespaceManager`
 * @uri: a URI or compact URI
 *
 * If @uri begins with one of the namespaces known to this
 * `TrackerNamespaceManager`, then the return value will be the
 * compressed URI. Otherwise, %NULL will be returned.
 *
 * Returns: (transfer full): (nullable): the compressed URI
 *
 * Since: 3.3
 **/
char *
tracker_namespace_manager_compress_uri (TrackerNamespaceManager *self,
                                        const char              *uri)
{
	TrackerNamespaceManagerPrivate *priv;
	guint i;
	int len;

	g_return_val_if_fail (TRACKER_IS_NAMESPACE_MANAGER (self), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	priv = GET_PRIVATE (self);

	len = strlen (uri);

	for (i = 0; i < priv->prefix_map->len; i++) {
		PrefixMap *map;
		const char *suffix;

		map = &g_array_index (priv->prefix_map, PrefixMap, i);

		if (map->uri_len <= len &&
		    map->uri[0] == uri[0] &&
		    map->uri[map->uri_len - 1] == uri[map->uri_len - 1] &&
		    strncmp (uri, map->uri, map->uri_len) == 0) {
			suffix = &uri[map->uri_len];
			return g_strconcat (map->prefix, ":", suffix, NULL);
		}
	}

	return NULL;
}

/**
 * tracker_namespace_manager_print_turtle:
 * @self: a `TrackerNamespaceManager`
 *
 * Writes out all namespaces as `@prefix` statements in
 * the [Turtle](https://www.w3.org/TR/turtle/) RDF format.
 *
 * Returns: a newly-allocated string
 */
char *
tracker_namespace_manager_print_turtle (TrackerNamespaceManager *self)
{
	TrackerNamespaceManagerPrivate *priv;
	GString *result;
	GHashTableIter iter;
	const char *prefix;
	const char *namespace;

	g_return_val_if_fail (TRACKER_IS_NAMESPACE_MANAGER (self), NULL);

	priv = GET_PRIVATE (self);

	result = g_string_new ("");

	g_hash_table_iter_init (&iter, priv->prefix_to_namespace);
	while (g_hash_table_iter_next (&iter, (gpointer *)&prefix, (gpointer *)&namespace)) {
		g_string_append_printf (result, "@prefix %s: <%s> .\n", prefix, namespace);
	}

	return g_string_free (result, FALSE);
}

/**
 * tracker_namespace_manager_foreach:
 * @self: a `TrackerNamespaceManager`
 * @func: (scope call): the function to call for each prefix / URI pair
 * @user_data: user data to pass to the function
 *
 * Calls @func for each known prefix / URI pair.
 */
void
tracker_namespace_manager_foreach (TrackerNamespaceManager *self,
                                   GHFunc                   func,
                                   gpointer                 user_data)
{
	TrackerNamespaceManagerPrivate *priv = GET_PRIVATE (self);

	g_hash_table_foreach (priv->prefix_to_namespace, func, user_data);
}

void
tracker_namespace_manager_seal (TrackerNamespaceManager *self)
{
	TrackerNamespaceManagerPrivate *priv = GET_PRIVATE (self);

	priv->sealed = TRUE;
}
