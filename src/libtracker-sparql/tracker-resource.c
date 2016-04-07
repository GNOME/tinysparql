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

#include <glib.h>
#include <json-glib/json-glib.h>

#include <string.h>

#include "config.h"
#include <tracker-uri.h>
#include <tracker-resource.h>
#include <tracker-ontologies.h>

/* Necessary to be able to use the TrackerSparqlBuilder Vala code */
#include "tracker-generated-no-checks.h"

typedef struct {
	char *identifier;
	GHashTable *properties;
	GHashTable *overwrite;
} TrackerResourcePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (TrackerResource, tracker_resource, G_TYPE_OBJECT);
#define GET_PRIVATE(object)  (tracker_resource_get_instance_private (object))

/**
 * SECTION: tracker-resource
 * @short_description: Represents a single Tracker resource
 * @title: TrackerResource
 * @stability: Stable
 * @include: tracker-resource.h
 *
 * <para>
 * #TrackerResource keeps track of a set of properties for a given resource.
 * The resulting data can be serialized in several ways.
 * </para>
 */

static char *
generate_blank_node_identifier (void)
{
	static gint64 counter = 0;

	return g_strdup_printf("_:%" G_GINT64_FORMAT, counter ++);
}

/**
 * TrackerResource:
 *
 * The <structname>TrackerResource</structname> object represents information
 * about a given resource.
 */

enum {
	PROP_0,

	PROP_IDENTIFIER,
};

static void finalize     (GObject       *object);
static void get_property (GObject       *object,
                          guint          param_id,
                          GValue        *value,
                          GParamSpec    *pspec);
static void set_property (GObject       *object,
                          guint          param_id,
                          const GValue  *value,
                          GParamSpec    *pspec);


static void
tracker_resource_class_init (TrackerResourceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = finalize;
	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property (object_class,
	                                 PROP_IDENTIFIER,
	                                 g_param_spec_string ("identifier",
	                                                      "Identifier",
	                                                      "Identifier",
	                                                      NULL,
	                                                      G_PARAM_READWRITE));
}

/* Destroy-notify function for the values stored in the hash table. */
static void
free_value (GValue *value)
{
	g_value_unset (value);
	g_slice_free (GValue, value);
}

static void
tracker_resource_init (TrackerResource *resource)
{
	TrackerResourcePrivate *priv = GET_PRIVATE(resource);

	/* Values of properties */
	priv->properties = g_hash_table_new_full (
	    g_str_hash,
	    g_str_equal,
	    g_free,
	    (GDestroyNotify) free_value);

	/* TRUE for any property where we should delete any existing values. */
	priv->overwrite = g_hash_table_new_full (
	    g_str_hash,
	    g_str_equal,
	    g_free,
	    NULL);
}

static void
finalize (GObject *object)
{
	TrackerResourcePrivate *priv;

	priv = GET_PRIVATE (TRACKER_RESOURCE (object));

	if (priv->identifier) {
		g_free (priv->identifier);
	}

	g_hash_table_unref (priv->overwrite);
	g_hash_table_unref (priv->properties);

	(G_OBJECT_CLASS (tracker_resource_parent_class)->finalize) (object);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
	TrackerResourcePrivate *priv;

	priv = GET_PRIVATE (TRACKER_RESOURCE (object));

	switch (param_id) {
		case PROP_IDENTIFIER:
			g_value_set_string (value, priv->identifier);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
	switch (param_id) {
		case PROP_IDENTIFIER:
			tracker_resource_set_identifier (TRACKER_RESOURCE (object), g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/**
 * tracker_resource_new:
 * @identifier: A string containing a URI
 *
 * Creates a TrackerResource instance.
 *
 * Returns: a newly created #TrackerResource. Free with g_object_unref() when done
 *
 * Since: 1.10
 */
TrackerResource *
tracker_resource_new (const char *identifier)
{
	TrackerResource *resource;
	GValue value = G_VALUE_INIT;

	resource = g_object_new (TRACKER_TYPE_RESOURCE, NULL);

	/* The identifier may be NULL, so I don't think we can pass it in the va_list
	 * that g_object_new() accepts.
	 */
	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, identifier);
	g_object_set_property (G_OBJECT (resource), "identifier", &value);
	g_value_unset (&value);

	return resource;
}

/* Difference between 'set' and 'add': when generating a SPARQL update, the
 * setter will generate a corresponding DELETE, the adder will not. The setter
 * will also overwrite existing values in the Resource object, while the adder
 * will make a list.
 */

/**
 * tracker_resource_set_gvalue:
 * @self: the #TrackerResource
 * @property_uri: a string identifying the property to set
 * @value: an initialised #GValue
 *
 * State that the only value for the given property is 'value'. Any existing
 * values for 'property' will be removed.
 *
 * When serialising to SPARQL, any properties that were set with this function
 * will get a corresponding DELETE statement to remove any existing values in
 * the database.
 *
 * You can pass any kind of GValue for @value, but serialization functions will
 * normally only be able to serialize URIs/relationships and fundamental value
 * types (string, int, etc.).
 *
 * Since: 1.10
 */
void
tracker_resource_set_gvalue (TrackerResource *self,
                             const char *property_uri,
                             GValue *value)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (self);
	GValue *our_value;

	our_value = g_slice_new0 (GValue);
	g_value_init (our_value, G_VALUE_TYPE (value));
	g_value_copy (value, our_value);

	g_hash_table_insert (priv->properties, g_strdup (property_uri), our_value);

	g_hash_table_insert (priv->overwrite, g_strdup (property_uri), GINT_TO_POINTER (TRUE));
};

#define SET_PROPERTY_FOR_GTYPE(name, ctype, gtype, set_function)   \
	void name (TrackerResource *self,                              \
	           const char *property_uri,                           \
	           ctype value)                                        \
	{                                                              \
		TrackerResourcePrivate *priv = GET_PRIVATE (self);         \
		GValue *our_value;                                         \
		                                                           \
		our_value = g_slice_new0 (GValue);                         \
		g_value_init (our_value, gtype);                           \
		set_function (our_value, value);                           \
		                                                           \
		g_hash_table_insert (priv->properties,                    \
		                      g_strdup (property_uri),             \
		                      our_value);                          \
		                                                           \
		g_hash_table_insert (priv->overwrite,                     \
		                      g_strdup (property_uri),             \
		                      GINT_TO_POINTER (TRUE));             \
	};

SET_PROPERTY_FOR_GTYPE (tracker_resource_set_boolean, gboolean, G_TYPE_BOOLEAN, g_value_set_boolean);
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_double, double, G_TYPE_DOUBLE, g_value_set_double);
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_int, int, G_TYPE_INT, g_value_set_int);
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_int64, gint64, G_TYPE_INT64, g_value_set_int64);
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_relation, TrackerResource *, TRACKER_TYPE_RESOURCE, g_value_set_object);
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_string, const char *, G_TYPE_STRING, g_value_set_string);
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_uri, const char *, TRACKER_TYPE_URI, g_value_set_string);

/**
 * tracker_resource_add_gvalue:
 * @self: the #TrackerResource
 * @property_uri: a string identifying the property to set
 * @value: an initialised #GValue
 *
 * Add 'value' to the list of values for given property.
 *
 * You can pass any kind of GValue for @value, but serialization functions will
 * normally only be able to serialize URIs/relationships and fundamental value
 * types (string, int, etc.).
 *
 * Since: 1.10
 */
void
tracker_resource_add_gvalue (TrackerResource *self,
                             const char *property_uri,
                             GValue *value)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (self);
	GValue *existing_value, *array_holder, *our_value;
	GPtrArray *array;

	existing_value = g_hash_table_lookup (priv->properties, property_uri);

	if (existing_value && G_VALUE_HOLDS (existing_value, G_TYPE_PTR_ARRAY)) {
		array = g_value_get_boxed (existing_value);
		array_holder = existing_value;
	} else {
		array = g_ptr_array_new_with_free_func ((GDestroyNotify)free_value);
		array_holder = g_slice_new0 (GValue);
		g_value_init (array_holder, G_TYPE_PTR_ARRAY);
		g_value_take_boxed (array_holder, array);

		if (existing_value) {
			/* The existing_value is owned by the hash table and will be freed
			 * when we overwrite it with array_holder, so we need to allocate a
			 * new value and give it to the ptrarray.
			 */
			our_value = g_slice_new0 (GValue);
			g_value_init (our_value, G_VALUE_TYPE (existing_value));
			g_value_copy (existing_value, our_value);
			g_ptr_array_add (array, our_value);
		}
	}

	our_value = g_slice_new0 (GValue);
	g_value_init (our_value, G_VALUE_TYPE (value));
	g_value_copy (value, our_value);

	g_ptr_array_add (array, our_value);

	if (array_holder != existing_value) {
		g_hash_table_insert (priv->properties, g_strdup (property_uri), array_holder);
	}
};

#define ADD_PROPERTY_FOR_GTYPE(name, ctype, gtype, set_function)         \
	void name (TrackerResource *self,                                    \
	           const char *property_uri,                                 \
	           ctype value)                                              \
	{                                                                    \
		TrackerResourcePrivate *priv = GET_PRIVATE (self);               \
		GValue *existing_value, *array_holder, *our_value;               \
		GPtrArray *array;                                                \
		                                                                 \
		existing_value = g_hash_table_lookup (priv->properties,          \
		                                      property_uri);             \
		                                                                 \
		if (existing_value && G_VALUE_HOLDS (existing_value,             \
		                                     G_TYPE_PTR_ARRAY)) {        \
			array = g_value_get_boxed (existing_value);                  \
			array_holder = existing_value;                               \
		} else {                                                         \
			array = g_ptr_array_new_with_free_func (                     \
			    (GDestroyNotify)free_value);                             \
			array_holder = g_slice_new0 (GValue);                        \
			g_value_init (array_holder, G_TYPE_PTR_ARRAY);               \
			g_value_take_boxed (array_holder, array);                    \
			                                                             \
			if (existing_value) {                                        \
				/* existing_value is owned by hash table */              \
				our_value = g_slice_new0 (GValue);                       \
				g_value_init (our_value, G_VALUE_TYPE(existing_value));  \
				g_value_copy (existing_value, our_value);                \
				g_ptr_array_add (array, our_value);                      \
			}                                                            \
		}                                                                \
		                                                                 \
		our_value = g_slice_new0 (GValue);                               \
		g_value_init (our_value, gtype);                                 \
		set_function (our_value, value);                                 \
		                                                                 \
		g_ptr_array_add (array, our_value);                              \
		                                                                 \
		if (array_holder != existing_value) {                            \
			g_hash_table_insert (priv->properties,                       \
			                     g_strdup (property_uri), array_holder); \
		}                                                                \
	};

SET_PROPERTY_FOR_GTYPE (tracker_resource_add_boolean, gboolean, G_TYPE_BOOLEAN, g_value_set_boolean);
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_double, double, G_TYPE_DOUBLE, g_value_set_double);
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_int, int, G_TYPE_INT, g_value_set_int);
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_int64, gint64, G_TYPE_INT64, g_value_set_int64);
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_relation, TrackerResource *, TRACKER_TYPE_RESOURCE, g_value_set_object);
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_string, const char *, G_TYPE_STRING, g_value_set_string);
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_uri, const char *, TRACKER_TYPE_URI, g_value_set_string);


/**
 * tracker_resource_get_values:
 * @self: the #TrackerResource
 * @property_uri: a string identifying the property to look up
 *
 * Returns the list of all known values of the given property.
 *
 * Returns: a #GList of #GValue instances, which must be freed by the caller.
 *
 * Since: 1.10
 */
GList *tracker_resource_get_values (TrackerResource *self,
                                    const char *property_uri)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (self);
	GValue *value;

	value = g_hash_table_lookup (priv->properties, property_uri);

	if (value == NULL) {
		return NULL;
	}

	if (G_VALUE_HOLDS (value, G_TYPE_PTR_ARRAY)) {
		GList *result = NULL;
		GPtrArray *array;
		int i;

		array = g_value_get_boxed (value);

		for (i = 0; i < array->len; i++) {
			value = g_ptr_array_index (array, i);
			result = g_list_prepend (result, value);
		};

		return g_list_reverse (result);
	} else {
		return g_list_append (NULL, value);
	}
}

#define GET_PROPERTY_FOR_GTYPE(name, ctype, gtype, get_function, no_value)  \
	ctype name (TrackerResource *self,                                      \
	            const char *property_uri)                                   \
	{                                                                       \
		TrackerResourcePrivate *priv = GET_PRIVATE (self);                  \
		GValue *value;                                                      \
		                                                                    \
		value = g_hash_table_lookup (priv->properties, property_uri);       \
		                                                                    \
		if (value == NULL) {                                                \
		    return no_value;                                                \
		};                                                                  \
		                                                                    \
		if (G_VALUE_HOLDS (value, G_TYPE_PTR_ARRAY)) {                      \
			GPtrArray *array;                                               \
			array = g_value_get_boxed (value);                              \
			if (array->len == 0) {                                          \
				return no_value;                                            \
			} else {                                                        \
				value = g_ptr_array_index (array, 0);                       \
			}                                                               \
		}                                                                   \
		                                                                    \
		return get_function (value);                                        \
	};

GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_boolean, gboolean, G_TYPE_BOOLEAN, g_value_get_boolean, FALSE);
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_double, double, G_TYPE_DOUBLE, g_value_get_double, 0.0);
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_int, int, G_TYPE_INT, g_value_get_int, 0);
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_int64, gint64, G_TYPE_INT64, g_value_get_int64, 0);
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_relation, TrackerResource *, TRACKER_TYPE_RESOURCE, g_value_get_object, NULL);
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_string, const char *, G_TYPE_STRING, g_value_get_string, NULL);
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_uri, const char *, TRACKER_TYPE_URI, g_value_get_string, NULL);

/**
 * tracker_resource_get_identifier:
 * @self: A #TrackerResource
 *
 * Returns the identifier of a resource.
 *
 * If the identifier was set to NULL, the identifier returned will be a unique
 * SPARQL blank node identifier, such as "_:123".
 *
 * Returns: a string owned by the resource
 *
 * Since: 1.10
 */
const char *
tracker_resource_get_identifier (TrackerResource *self)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (self);

	return priv->identifier;
}

/**
 * tracker_resource_set_identifier:
 * @self: a #TrackerResource
 * @identifier: (allow-none): a string identifying the resource
 *
 * Changes the identifier of a #TrackerResource. The identifier should be a
 * URI or compact URI, but this is not necessarily enforced. Invalid
 * identifiers may cause errors when serializing the resource or trying to
 * insert the results in a database.
 *
 * If the identifier is set to NULL, a SPARQL blank node identifier such as
 * "_:123" is assigned to the resource.
 *
 * Since: 1.10
 */
void
tracker_resource_set_identifier (TrackerResource *self,
                                 const char *identifier)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (self);

	if (priv->identifier) {
		g_free (priv->identifier);
	}

	if (identifier == NULL) {
		/* We take NULL to mean "this is a blank node", and generate a
		 * unique blank node identifier right away. This is easier than
		 * leaving it NULL and trying to generate a blank node ID at
		 * serialization time, and it means that the serialization
		 * output is stable when called multiple times.
		 */
		priv->identifier = generate_blank_node_identifier ();
	} else {
		priv->identifier = g_strdup (identifier);
	}
}

gint
tracker_resource_identifier_compare_func (TrackerResource *resource,
                                          const char *identifier)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (resource);

	return strcmp (priv->identifier, identifier);
}

/**
 * tracker_resource_compare:
 * @self: A #TrackerResource
 *
 * Compare the identifiers of two TrackerResource instances. The resources
 * are considered identical if they have the same identifier.
 *
 * Note that there can be false negatives with this simplistic approach: two
 * resources may have different identifiers that actually refer to the same
 * thing.
 *
 * Returns: 0 if the identifiers are the same, -1 or +1 otherwise
 *
 * Since: 1.10
 */
gint
tracker_resource_compare (TrackerResource *a,
                          TrackerResource *b)
{
    TrackerResourcePrivate *a_priv = GET_PRIVATE (a);
    TrackerResourcePrivate *b_priv = GET_PRIVATE (b);

    return strcmp (a_priv->identifier, b_priv->identifier);
};


/* Helper function for serialization code. This allows you to selectively
 * populate 'interned_namespaces' from 'all_namespaces' based on when a
 * particular prefix is actually used. This is quite inefficient compared
 * to just dumping all known namespaces, but it makes the serializated
 * output more readable.
 */
static void
maybe_intern_prefix_of_compact_uri (TrackerNamespaceManager *all_namespaces,
                                    TrackerNamespaceManager *interned_namespaces,
                                    const char *uri)
{
	/* The TrackerResource API doesn't distinguish between compact URIs and full
	 * URIs. This is fine as long as users don't add prefixes that can be
	 * confused with URIs. Both URIs and CURIEs can have anything following
	 * the ':', so without hardcoding knowledge of well-known protocols here,
	 * we can't really tell if the user has done something dumb like defining a
	 * "urn" prefix.
	 */
	char *prefix = g_uri_parse_scheme (uri);

	if (prefix == NULL) {
		g_warning ("Invalid URI or compact URI: %s", uri);
		return;
	}

	if (tracker_namespace_manager_has_prefix (all_namespaces, prefix)) {
		if (!tracker_namespace_manager_has_prefix (interned_namespaces, prefix)) {
			const char *namespace = tracker_namespace_manager_lookup_prefix (all_namespaces, prefix);
			tracker_namespace_manager_add_prefix (interned_namespaces, prefix, namespace);
		}
	}

	g_free (prefix);
}


typedef struct {
	TrackerNamespaceManager *all_namespaces, *our_namespaces;
	GString *string;
	GList *done_list;
} GenerateTurtleData;

void generate_turtle (TrackerResource *resource, GenerateTurtleData *data);

void
generate_nested_turtle_resource (TrackerResource *resource,
                                 GenerateTurtleData *data)
{
	if (g_list_find_custom (data->done_list, resource, (GCompareFunc) tracker_resource_compare) == NULL) {
		generate_turtle (resource, data);

		g_string_append (data->string, "\n");

		data->done_list = g_list_prepend (data->done_list, resource);
	}
}

void
generate_turtle_resources_foreach (gpointer key,
                                   gpointer value_ptr,
                                   gpointer user_data)
{
	const char *property = key;
	const GValue *value = value_ptr;
	GenerateTurtleData *data = user_data;
	TrackerResource *resource;
	int i;

	if (G_VALUE_HOLDS (value, TRACKER_TYPE_RESOURCE)) {
		resource = g_value_get_object (value);
		generate_nested_turtle_resource (resource, data);
	} else if (G_VALUE_HOLDS (value, G_TYPE_PTR_ARRAY)) {
		GPtrArray *array = g_value_get_boxed (value);
		const GValue *array_value;

		for (i = 0; i < array->len; i++) {
			array_value = g_ptr_array_index (array, i);

			if (G_VALUE_HOLDS (array_value, TRACKER_TYPE_RESOURCE)) {
				resource = g_value_get_object (array_value);
				generate_nested_turtle_resource (resource, data);
			}
		}
	}
}

static void
generate_turtle_value (const GValue *value,
                       GenerateTurtleData *data)
{
	GType type = G_VALUE_TYPE (value);
	if (type == TRACKER_TYPE_URI) {
		const char *uri = g_value_get_string (value);
		maybe_intern_prefix_of_compact_uri (data->all_namespaces, data->our_namespaces, uri);
		g_string_append_printf(data->string, "%s", uri);
	} else if (type == TRACKER_TYPE_RESOURCE) {
		TrackerResource *relation = TRACKER_RESOURCE (g_value_get_object (value));
		g_string_append_printf(data->string, "<%s>", tracker_resource_get_identifier (relation));
	} else if (type == G_TYPE_STRING) {
		g_string_append_printf(data->string, "\"%s\"", g_value_get_string (value));
	} else {
		GValue str_value = G_VALUE_INIT;
		g_value_init (&str_value, G_TYPE_STRING);
		if (g_value_transform (value, &str_value)) {
			g_string_append (data->string, g_value_get_string (&str_value));
		} else {
			g_warning ("Cannot serialize value of type %s to Turtle", G_VALUE_TYPE_NAME (value));
		}
		g_value_unset (&str_value);
	}
}

void
generate_turtle_property (const char *property,
                          const GValue *value,
                          GenerateTurtleData *data)
{
	g_string_append (data->string, property);
	g_string_append (data->string, " ");
	if (G_VALUE_HOLDS (value, G_TYPE_PTR_ARRAY)) {
		int i;
		GPtrArray *array = g_value_get_boxed (value);
		if (array->len > 0) {
			generate_turtle_value (g_ptr_array_index (array, 0), data);
			for (i = 1; i < array->len; i++) {
				g_string_append (data->string, " , ");
				generate_turtle_value (g_ptr_array_index (array, i), data);
			}
		}
	} else {
		generate_turtle_value (value, data);
	}
}

void
generate_turtle (TrackerResource *resource,
                 GenerateTurtleData *data)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (resource);
	GString *result;
	GHashTableIter iter;
	const char *property;
	const GValue *value;

	/* First we recurse to any relations that aren't already in the done list */
	g_hash_table_foreach (priv->properties, generate_turtle_resources_foreach, data);

	g_string_append_printf (data->string, "<%s> ", priv->identifier);

	g_hash_table_iter_init (&iter, priv->properties);
	if (g_hash_table_iter_next (&iter, (gpointer *)&property, (gpointer *)&value))
		while (TRUE) {
			generate_turtle_property (property, value, data);

			maybe_intern_prefix_of_compact_uri (data->all_namespaces, data->our_namespaces, property);

			if (g_hash_table_iter_next (&iter, (gpointer *)&property, (gpointer *)&value)) {
				g_string_append (data->string, " ;\n  ");
			} else {
				g_string_append (data->string, " .\n");
				break;
			}
		}
}

/**
 * tracker_resource_print_turtle:
 * @resource: a #TrackerResource
 *
 * Serialize all the information in @resource as a Turtle document.
 *
 * The generated Turtle should correspond to this standard:
 * <https://www.w3.org/TR/2014/REC-turtle-20140225/>
 *
 * Returns: a newly-allocated string
 *
 * Since: 1.10
 */
char *
tracker_resource_print_turtle (TrackerResource *self,
                               TrackerNamespaceManager *namespaces)
{
	GenerateTurtleData context;
	char *prefixes;

	context.all_namespaces = namespaces;
	context.our_namespaces = tracker_namespace_manager_new ();
	context.string = g_string_new ("");
	context.done_list = NULL;

	maybe_intern_prefix_of_compact_uri (context.all_namespaces, context.our_namespaces, tracker_resource_get_identifier(self));

	generate_turtle (self, &context);

	prefixes = tracker_namespace_manager_print_turtle (context.our_namespaces);
	g_string_prepend (context.string, "\n");
	g_string_prepend (context.string, prefixes);

	g_object_unref (context.our_namespaces);
	g_free (prefixes);

	g_list_free (context.done_list);

	return g_string_free (context.string, FALSE);
}

typedef struct {
	TrackerNamespaceManager *namespaces;
	TrackerSparqlBuilder *builder;
	const char *graph_id;
	GList *done_list;
	GHashTable *overwrite_flags;
} GenerateSparqlData;

void generate_sparql_update (TrackerResource *resource, GenerateSparqlData *data);

static void
generate_sparql_relations_foreach (gpointer key,
                                   gpointer value_ptr,
                                   gpointer user_data)
{
	const char *property = key;
	const GValue *value = value_ptr;
	GenerateSparqlData *data = user_data;
	GError *error = NULL;

	if (G_VALUE_HOLDS (value, TRACKER_TYPE_RESOURCE)) {
		TrackerResource *relation = g_value_get_object (value);

		if (g_list_find_custom (data->done_list, relation, (GCompareFunc) tracker_resource_compare) == NULL) {
			generate_sparql_update (relation, data);
			data->done_list = g_list_prepend (data->done_list, relation);
		}
	}
}

static char *
variable_name_for_property (const char *property) {
	return g_strcanon (g_strdup (property),
	                   "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890",
	                   '_');
}

static void
generate_sparql_deletes_foreach (gpointer key,
                                 gpointer value_ptr,
                                 gpointer user_data)
{
	const char *property = key;
	const GValue *value = value_ptr;
	GenerateSparqlData *data = user_data;

	/* Whether to generate the DELETE is based on whether set_value was ever
	 * called for this property. That's tracked in a hash table.
	 */
	if (g_hash_table_lookup (data->overwrite_flags, property)) {
		char *variable_name = variable_name_for_property (property);
		tracker_sparql_builder_predicate (data->builder, property);
		tracker_sparql_builder_object_variable (data->builder, variable_name);
		g_free (variable_name);
	}
}

static void
generate_sparql_uri_value (const char *uri_or_curie,
                           GenerateSparqlData *data)
{
	/* The tracker_resource_set_uri() function accepts both URIs
	 * (such as http://example.com/) and compact URIs (such as nie:DataObject).
	 * We could expand them here, but since the tracker-store can understand them
	 * as-is we leave them be and the generated SPARQL is clearer as a result.
	 * We still need to attempt to expand them in order to know whether they need
	 * <> brackets around them.
	 */
	char *prefix = g_uri_parse_scheme (uri_or_curie);

	if (prefix && tracker_namespace_manager_has_prefix (data->namespaces, prefix)) {
		/* It's a compact URI and we know the prefix */
		tracker_sparql_builder_object (data->builder, uri_or_curie);
	} else {
		/* It's a full URI (or something invalid, but we can't really tell that here) */
		tracker_sparql_builder_object_iri (data->builder, uri_or_curie);
	}
}

static void
generate_sparql_value (const GValue *value,
                       GenerateSparqlData *data)
{
	TrackerSparqlBuilder *builder = data->builder;
	GType type = G_VALUE_TYPE (value);
	if (type == G_TYPE_BOOLEAN) {
		tracker_sparql_builder_object_boolean (builder, g_value_get_boolean (value));
	} else if (type == G_TYPE_DATE) {
		/* tracker_sparql_builder_object_date() exists, but it requires a
		 * time_t, and GDate and GDateTime don't provide those conveniently.
		 */
		char literal[256];
		g_date_strftime (literal, 256,
		                 "\"%Y-%m-%d%z\"^^<http://www.w3.org/2001/XMLSchema#date>",
		                 g_value_get_boxed (value));
		tracker_sparql_builder_object (builder, literal);
	} else if (type == G_TYPE_DATE_TIME) {
		char *literal;
		literal = g_date_time_format (g_value_get_boxed (value),
		                              "\"%Y-%m-%dT%H:%M:%s%z\"^^<http://www.w3.org/2001/XMLSchema#dateTime>");
		tracker_sparql_builder_object (builder, literal);
		g_free (literal);
	} else if (type == G_TYPE_DOUBLE) {
		tracker_sparql_builder_object_double (builder, g_value_get_double (value));
	} else if (type == G_TYPE_FLOAT) {
		tracker_sparql_builder_object_double (builder, g_value_get_float (value));
	} else if (type == G_TYPE_CHAR) {
		tracker_sparql_builder_object_int64 (builder, g_value_get_schar (value));
	} else if (type == G_TYPE_INT) {
		tracker_sparql_builder_object_int64 (builder, g_value_get_int (value));
	} else if (type == G_TYPE_INT64) {
		tracker_sparql_builder_object_int64 (builder, g_value_get_int64 (value));
	} else if (type == G_TYPE_LONG) {
		tracker_sparql_builder_object_int64 (builder, g_value_get_long (value));
	} else if (type == G_TYPE_UCHAR) {
		tracker_sparql_builder_object_int64 (builder, g_value_get_uchar (value));
	} else if (type == G_TYPE_UINT) {
		tracker_sparql_builder_object_int64 (builder, g_value_get_uint (value));
	} else if (type == G_TYPE_ULONG) {
		tracker_sparql_builder_object_int64 (builder, g_value_get_ulong (value));
	} else if (type == G_TYPE_UINT64) {
		g_warning ("Cannot serialize uint64 types to SPARQL. Use int64.");
		tracker_sparql_builder_object (builder, "null");
	} else if (type == G_TYPE_STRING) {
		tracker_sparql_builder_object_string (builder, g_value_get_string (value));
	} else if (type == TRACKER_TYPE_URI) {
		generate_sparql_uri_value (g_value_get_string (value), data);
	} else if (type == TRACKER_TYPE_RESOURCE) {
		TrackerResource *relation = TRACKER_RESOURCE (g_value_get_object (value));
		tracker_sparql_builder_object_iri (builder, tracker_resource_get_identifier (relation));
	} else {
		g_warning ("Cannot serialize value of type %s to SPARQL", G_VALUE_TYPE_NAME (value));
		tracker_sparql_builder_object (builder, "null");
	}
}

static void
generate_sparql_inserts_foreach (gpointer key,
                                 gpointer value_ptr,
                                 gpointer user_data)
{
	const char *property = key;
	const GValue *value = value_ptr;
	GenerateSparqlData *data = user_data;
	char *full_property;

	full_property = tracker_namespace_manager_expand_uri (data->namespaces, property);

	/* The caller should have already set rdf:type */
	if (strcmp (full_property, TRACKER_PREFIX_RDF "type") == 0 || strcmp (property, "rdf:type") == 0) {
		g_free (full_property);
		return;
	}

	tracker_sparql_builder_predicate (data->builder, property);

	g_free (full_property);

	if (G_VALUE_TYPE (value) == G_TYPE_PTR_ARRAY) {
		g_ptr_array_foreach (g_value_get_boxed (value), (GFunc)generate_sparql_value, data);
	} else {
		generate_sparql_value (value, data);
	}
}

void
generate_sparql_update (TrackerResource *resource,
                        GenerateSparqlData *data)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (resource);
	TrackerSparqlBuilder *builder = data->builder;
	GValue *type_value;

	if (! priv->identifier) {
		g_warning ("Main resource must have an identifier.");
		return;
	}

	g_return_if_fail (tracker_sparql_builder_get_state (builder) == TRACKER_SPARQL_BUILDER_STATE_UPDATE);

	/* Delete the existing data. If we don't do this, we may get constraint
	 * violations due to trying to add a second value to a single-valued
	 * property, and we may get old metadata hanging around.
	 *
	 * We have to generate a rather awkward query here, like:
	 *
	 *     DELETE { pattern } WHERE { pattern }
	 *
	 * It would be better if we could use "DELETE DATA { pattern }". This is
	 * allowed in SPARQL update 1.1, but not yet supported by Tracker's store.
	 */
	data->overwrite_flags = priv->overwrite;

	tracker_sparql_builder_delete_open (builder, NULL);
	if (data->graph_id) {
		tracker_sparql_builder_graph_open (builder, data->graph_id);
	}
	tracker_sparql_builder_subject_iri (builder, priv->identifier);
	g_hash_table_foreach (priv->properties, generate_sparql_deletes_foreach, data);
	if (data->graph_id) {
		tracker_sparql_builder_graph_close (builder);
	}
	tracker_sparql_builder_delete_close (builder);

	tracker_sparql_builder_where_open (builder);
	if (data->graph_id) {
		tracker_sparql_builder_graph_open (builder, data->graph_id);
	}
	tracker_sparql_builder_subject_iri (builder, priv->identifier);
	g_hash_table_foreach (priv->properties, generate_sparql_deletes_foreach, data);
	if (data->graph_id) {
		tracker_sparql_builder_graph_close (builder);
	}
	tracker_sparql_builder_where_close (builder);

	/* Now emit any sub-resources. */
	g_hash_table_foreach (priv->properties, generate_sparql_relations_foreach, data);

	/* Finally insert the rest of the data */

	/* Passing the graph directly to insert_open causes it to generate a
	 * non-standard 'INSERT INTO <graph>' statement, while calling graph_open
	 * separately causes it to generate INSERT { GRAPH { .. } }. See
	 * <https://bugzilla.gnome.org/show_bug.cgi?id=658838>.
	 */
	tracker_sparql_builder_insert_open (builder, NULL);
	if (data->graph_id) {
		tracker_sparql_builder_graph_open (builder, data->graph_id);
	}

	tracker_sparql_builder_subject_iri (builder, priv->identifier);

	/* rdf:type needs to be first, otherwise you'll see 'subject x is not in domain y'
	 * errors for the properties you try to set.
	 */
	type_value = g_hash_table_lookup (priv->properties, "rdf:type");
	if (type_value != NULL) {
		tracker_sparql_builder_predicate (builder, "a");
		if (G_VALUE_TYPE (type_value) == G_TYPE_PTR_ARRAY) {
			g_ptr_array_foreach (g_value_get_boxed (type_value), (GFunc)generate_sparql_value, data);
		} else {
			generate_sparql_value (type_value, data);
		}
	}

	g_hash_table_foreach (priv->properties, generate_sparql_inserts_foreach, data);

	if (data->graph_id) {
		tracker_sparql_builder_graph_close (builder);
	}
	tracker_sparql_builder_insert_close (builder);
}

/**
 * tracker_resource_generate_sparql_update:
 * @self: a #TrackerResource
 * @builder: a #TrackerSparqlBuilder where the result will be returned
 *
 * Generates a SPARQL command to update a database with the information
 * stored in @resource.
 *
 * Since: 1.10
 */
void
tracker_resource_generate_sparql_update (TrackerResource *resource,
                                         TrackerSparqlBuilder *builder,
                                         TrackerNamespaceManager *namespaces,
                                         const char *graph_id)
{
	GenerateSparqlData context;

	context.namespaces = namespaces;
	context.builder = builder;
	context.graph_id = graph_id;

	/* Resources can be recursive, and may have repeated or even cyclic
	 * relationships. This list keeps track of what we already processed.
	 */
	context.done_list = NULL;

	generate_sparql_update (resource, &context);

	g_list_free (context.done_list);
}

typedef struct {
	JsonBuilder *builder;
	GList *done_list;
} GenerateJsonldData;

static void generate_jsonld_foreach (gpointer key, gpointer value_ptr, gpointer user_data);

static void
tracker_resource_generate_jsonld (TrackerResource *self,
                                  GenerateJsonldData *data)
{
	/* FIXME: generate a JSON-LD context ! */

	TrackerResourcePrivate *priv = GET_PRIVATE (self);
	JsonBuilder *builder = data->builder;
	JsonNode *result;

	json_builder_begin_object (builder);

	/* The JSON-LD spec says it is "important that nodes have an identifier", but
	 * doesn't mandate one. I think it's better to omit the ID for blank nodes
	 * (where the caller passed NULL as an identifier) than to emit something
	 * SPARQL-specific like '_:123'.
	 */
	if (strncmp (priv->identifier, "_:", 2) != 0) {
		json_builder_set_member_name (builder, "@id");
		json_builder_add_string_value (builder, priv->identifier);
	}

	g_hash_table_foreach (priv->properties, generate_jsonld_foreach, data);

	json_builder_end_object (builder);
};

static void
generate_jsonld_value (const GValue *value,
                       GenerateJsonldData *data)
{
	JsonNode *node;

	if (G_VALUE_HOLDS (value, TRACKER_TYPE_RESOURCE)) {
		TrackerResource *resource;

		resource = TRACKER_RESOURCE (g_value_get_object (value));

		if (g_list_find_custom (data->done_list, resource, (GCompareFunc) tracker_resource_compare) == NULL) {
			tracker_resource_generate_jsonld (resource, data);

			data->done_list = g_list_prepend (data->done_list, resource);
		} else {
			json_builder_add_string_value (data->builder, tracker_resource_get_identifier(resource));
		}
	} else if (G_VALUE_HOLDS (value, TRACKER_TYPE_URI)) {
		/* URIs can be treated the same as strings in JSON-LD provided the @context
		 * sets the type of that property correctly. However, json_node_set_value()
		 * will reject a GValue holding TRACKER_TYPE_URI, so we have to extract the
		 * string manually here.
		 */
		const char *uri = g_value_get_string (value);
		node = json_node_new (JSON_NODE_VALUE);
		json_node_set_string (node, uri);
		json_builder_add_value (data->builder, node);
	} else {
		node = json_node_new (JSON_NODE_VALUE);
		json_node_set_value (node, value);
		json_builder_add_value (data->builder, node);
	}
}

static void
generate_jsonld_foreach (gpointer key,
                         gpointer value_ptr,
                         gpointer user_data)
{
	const char *property = key;
	const GValue *value = value_ptr;
	GenerateJsonldData *data = user_data;
	JsonBuilder *builder = data->builder;

	if (strcmp (property, "rdf:type") == 0) {
		property = "@type";
	}

	json_builder_set_member_name (builder, property);
	if (G_VALUE_HOLDS (value, G_TYPE_PTR_ARRAY)) {
		json_builder_begin_array (builder);
		g_ptr_array_foreach (g_value_get_boxed (value), (GFunc) generate_jsonld_value, data);
		json_builder_end_array (builder);
	} else {
		generate_jsonld_value (value, data);
	}
}

/**
 * tracker_resource_print_jsonld:
 * @resource: a #TrackerResource
 * @error: address where an error can be returned
 *
 * Serialize all the information in @resource as a JSON-LD document.
 *
 * See <http://www.jsonld.org/> for more information on the JSON-LD
 * serialization format.
 *
 * Returns: a newly-allocated string
 *
 * Since: 1.10
 */
char *
tracker_resource_print_jsonld (TrackerResource *resource)
{
	GenerateJsonldData context;
	JsonNode *json_root_node;
	JsonGenerator *generator;
	char *result;

	context.done_list = NULL;
	context.builder = json_builder_new ();

	tracker_resource_generate_jsonld (resource, &context);
	json_root_node = json_builder_get_root (context.builder);

	generator = json_generator_new ();
	json_generator_set_root (generator, json_root_node);
	json_generator_set_pretty (generator, TRUE);

	result = json_generator_to_data (generator, NULL);

	g_list_free (context.done_list);
	json_node_free (json_root_node);
	g_object_unref (context.builder);
	g_object_unref (generator);

	return result;
}
