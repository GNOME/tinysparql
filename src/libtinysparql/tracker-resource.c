/*
 * Copyright (C) 2016-2017, Sam Thursfield <sam@afuera.me.uk>
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

#include <glib.h>
#include <json-glib/json-glib.h>

#include <string.h>

#include "tracker-deserializer-resource.h"
#include "tracker-uri.h"
#include "tracker-resource.h"
#include "tracker-ontologies.h"

/* For tracker_sparql_escape_string */
#include "tracker-utils.h"

/* For prefixed names parsing */
#include "core/tracker-sparql-grammar.h"

#include "tracker-private.h"

typedef struct {
	char *identifier;
	GHashTable *properties;
	GHashTable *overwrite;
} TrackerResourcePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (TrackerResource, tracker_resource, G_TYPE_OBJECT)
#define GET_PRIVATE(object)  (tracker_resource_get_instance_private (object))

/**
 * TrackerResource:
 *
 * `TrackerResource` is an in-memory representation of RDF data about a given resource.
 *
 * This object keeps track of a set of properties for a given resource, and can
 * also link to other `TrackerResource` objects to form trees or graphs of RDF
 * data. See [method@Resource.set_relation] and [method@Resource.set_uri]
 * on how to link a `TrackerResource` to other RDF data.
 *
 * `TrackerResource` may also hold data about literal values, added through
 * the specialized [method@Resource.set_int64], [method@Resource.set_string],
 * etc family of functions, or the generic [method@Resource.set_gvalue] method.
 *
 * Since RDF properties may be multi-valued, for every `set` call there exists
 * another `add` call (e.g. [method@Resource.add_int64], [method@Resource.add_string]
 * and so on). The `set` methods do also reset any previously value the
 * property might hold for the given resource.
 *
 * Resources may have an IRI set at creation through [ctor@Resource.new],
 * or set afterwards through [method@Resource.set_identifier]. Resources
 * without a name will represent a blank node, and will be dealt with as such
 * during database insertions.
 *
 * `TrackerResource` performs no validation on the data being coherent as per
 * any ontology. Errors will be found out at the time of using the TrackerResource
 * for e.g. database updates.
 *
 * Once the RDF data is built in memory, the (tree of) `TrackerResource` may be
 * converted to a RDF format through [method@Resource.print_rdf], or
 * directly inserted into a database through [method@Batch.add_resource]
 * or [method@SparqlConnection.update_resource].
 */

static char *
generate_blank_node_identifier (void)
{
	static gint64 counter = 0;

	return g_strdup_printf("_:%" G_GINT64_FORMAT, counter++);
}

enum {
	PROP_0,

	PROP_IDENTIFIER,
};

static void dispose      (GObject *object);
static void finalize     (GObject *object);
static void get_property (GObject    *object,
                          guint       param_id,
                          GValue     *value,
                          GParamSpec *pspec);
static void set_property (GObject      *object,
                          guint         param_id,
                          const GValue *value,
                          GParamSpec   *pspec);

static char *
escape_iri (const gchar *str)
{
	GString *iri;

	/* Escapes IRI references according to IRIREF in SPARQL grammar definition,
	 * further validation on IRI validity may happen deeper down.
	 */

	if (!str)
		return NULL;

	/* Fast path, check whether there's no characters to escape */
	if (!strpbrk (str,
	              "<>\"{}|^`\\"
	              "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
	              "\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20")) {
		return g_strdup (str);
	}

	iri = g_string_new (NULL);

	while (*str != '\0') {
		gunichar unichar;

		unichar = g_utf8_get_char (str);
		str = g_utf8_next_char (str);

		if (unichar <= 0x20 ||
		    unichar == '<' || unichar == '>' ||
		    unichar == '"' || unichar == '{' ||
		    unichar == '}' || unichar == '|' ||
		    unichar == '^' || unichar == '`' ||
		    unichar == '\\')
			g_string_append_printf (iri, "%%%X", unichar);
		else
			g_string_append_unichar (iri, unichar);
	}

	return g_string_free (iri, FALSE);
}

static void
tracker_resource_class_init (TrackerResourceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose      = dispose;
	object_class->finalize     = finalize;
	object_class->get_property = get_property;
	object_class->set_property = set_property;

	/**
	 * TrackerResource:identifier
	 *
	 * The URI identifier for this class, or %NULL for a
	 * blank node.
	 */
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
dispose (GObject *object)
{
	TrackerResourcePrivate *priv;

	priv = GET_PRIVATE (TRACKER_RESOURCE (object));

	g_clear_pointer (&priv->overwrite, g_hash_table_unref);
	g_clear_pointer (&priv->properties, g_hash_table_unref);

	G_OBJECT_CLASS (tracker_resource_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	TrackerResourcePrivate *priv;

	priv = GET_PRIVATE (TRACKER_RESOURCE (object));

	g_clear_pointer (&priv->identifier, g_free);

	(G_OBJECT_CLASS (tracker_resource_parent_class)->finalize)(object);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
	switch (param_id) {
	case PROP_IDENTIFIER:
		g_value_set_string (value, tracker_resource_get_identifier (TRACKER_RESOURCE (object)));
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
 * @identifier: (nullable): A string containing a URI, or %NULL.
 *
 * Creates a TrackerResource instance.
 *
 * Returns: a newly created `TrackerResource`.
 */
TrackerResource *
tracker_resource_new (const char *identifier)
{
	TrackerResource *resource;

	resource = g_object_new (TRACKER_TYPE_RESOURCE,
	                         "identifier", identifier,
	                         NULL);

	return resource;
}

/* Difference between 'set' and 'add': when generating a SPARQL update, the
 * setter will generate a corresponding DELETE, the adder will not. The setter
 * will also overwrite existing values in the Resource object, while the adder
 * will make a list.
 */

/**
 * tracker_resource_set_gvalue:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to set
 * @value: an initialised [struct@GObject.Value]
 *
 * Replace any previously existing value for @property_uri with @value.
 *
 * When serialising to SPARQL, any properties that were set with this function
 * will get a corresponding DELETE statement to remove any existing values in
 * the database.
 *
 * You can pass any kind of [struct@GObject.Value] for @value, but serialization functions will
 * normally only be able to serialize URIs/relationships and fundamental value
 * types (string, int, etc.).
 */
void
tracker_resource_set_gvalue (TrackerResource *self,
                             const char      *property_uri,
                             const GValue    *value)
{
	TrackerResourcePrivate *priv;
	GValue *our_value;

	g_return_if_fail (TRACKER_IS_RESOURCE (self));
	g_return_if_fail (property_uri != NULL);
	g_return_if_fail (G_IS_VALUE (value));

	priv = GET_PRIVATE (self);

	our_value = g_slice_new0 (GValue);
	g_value_init (our_value, G_VALUE_TYPE (value));
	g_value_copy (value, our_value);

	g_hash_table_insert (priv->properties, g_strdup (property_uri), our_value);

	g_hash_table_insert (priv->overwrite, g_strdup (property_uri), GINT_TO_POINTER (TRUE));
}

static gboolean
validate_boolean (gboolean    value,
                  const char *func_name) {
	return TRUE;
}

static gboolean
validate_double (double      value,
                 const char *func_name) {
	return TRUE;
}

static gboolean
validate_int (int         value,
              const char *func_name) {
	return TRUE;
}

static gboolean
validate_int64 (gint64      value,
                const char *func_name) {
	return TRUE;
}

static gboolean
validate_pointer (const void *pointer,
                  const char *func_name)
{
	if (pointer == NULL) {
		g_warning ("%s: NULL is not a valid value.", func_name);
		return FALSE;
	}

	return TRUE;
}

static void
value_set_uri (GValue      *value,
               const gchar *uri)
{
	g_value_take_string (value, escape_iri (uri));
}

#define SET_PROPERTY_FOR_GTYPE(name, ctype, gtype, set_function, validate_function) \
	void name (TrackerResource *self,                                           \
	           const char *property_uri,                                        \
	           ctype value)                                                     \
	{                                                                           \
		TrackerResourcePrivate *priv;                                       \
		GValue *our_value;                                                  \
                                                                                    \
		g_return_if_fail (TRACKER_IS_RESOURCE (self));                      \
		g_return_if_fail (property_uri != NULL);                            \
                                                                                    \
		priv = GET_PRIVATE (self);                                          \
                                                                                    \
		if (!validate_function (value, __func__)) {                         \
			return;                                                     \
		}                                                                   \
                                                                                    \
		our_value = g_slice_new0 (GValue);                                  \
		g_value_init (our_value, gtype);                                    \
		set_function (our_value, value);                                    \
                                                                                    \
		g_hash_table_insert (priv->properties,                              \
		                     g_strdup (property_uri),                       \
		                     our_value);                                    \
                                                                                    \
		g_hash_table_insert (priv->overwrite,                               \
		                     g_strdup (property_uri),                       \
		                     GINT_TO_POINTER (TRUE));                       \
	}

/**
 * tracker_resource_set_boolean:
 * @self: The `TrackerResource`
 * @property_uri: A string identifying the property to modify
 * @value: The property boolean value
 *
 * Sets a boolean property. Replaces any previous value.
 *
 * This method corresponds to [xsd:boolean](xsd-ontology.html#xsd:boolean).
 */
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_boolean, gboolean, G_TYPE_BOOLEAN, g_value_set_boolean, validate_boolean)

/**
 * tracker_resource_set_double:
 * @self: The `TrackerResource`
 * @property_uri: A string identifying the property to modify
 * @value: The property object
 *
 * Sets a numeric property with double precision. Replaces any previous value.
 *
 * This method corresponds to [xsd:double](xsd-ontology.html#xsd:double).
 */
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_double, double, G_TYPE_DOUBLE, g_value_set_double, validate_double)

/**
 * tracker_resource_set_int:
 * @self: The `TrackerResource`
 * @property_uri: A string identifying the property to modify
 * @value: The property object
 *
 * Sets a numeric property with integer precision. Replaces any previous value.
 *
 * This method corresponds to [xsd:integer](xsd-ontology.html#xsd:integer).
 */
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_int, int, G_TYPE_INT, g_value_set_int, validate_int)

/**
 * tracker_resource_set_int64:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @value: the property object
 *
 * Sets a numeric property with 64-bit integer precision. Replaces any previous value.
 *
 * This method corresponds to [xsd:integer](xsd-ontology.html#xsd:integer).
 */
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_int64, gint64, G_TYPE_INT64, g_value_set_int64, validate_int64)

/**
 * tracker_resource_set_relation:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @resource: the property object
 *
 * Sets a resource property as a `TrackerResource`. Replaces any previous value.
 *
 * This method applies to properties with a [rdfs:range](rdf-ontology.html#rdfs:range)
 * that points to a non-literal class (i.e. a subclass of
 * [rdfs:Resource](rdf-ontology.html#rdfs:Resource)).
 *
 * This function produces similar RDF to [method@Resource.set_uri],
 * although in this function the URI will depend on the identifier
 * set on @resource.
 */
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_relation, TrackerResource *, TRACKER_TYPE_RESOURCE, g_value_set_object, validate_pointer)

/**
 * tracker_resource_set_take_relation:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @resource: (transfer full): the property object
 *
 * Sets a resource property as a `TrackerResource`. Replaces any previous value.
 * Takes ownership on the given @resource.
 *
 * This method applies to properties with a [rdfs:range](rdf-ontology.html#rdfs:range)
 * that points to a non-literal class (i.e. a subclass of
 * [rdfs:Resource](rdf-ontology.html#rdfs:Resource)).
 *
 * This function produces similar RDF to [method@Resource.set_uri],
 * although in this function the URI will depend on the identifier
 * set on @resource.
 */
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_take_relation, TrackerResource *, TRACKER_TYPE_RESOURCE, g_value_take_object, validate_pointer)

/**
 * tracker_resource_set_string:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @value: the property object
 *
 * Sets a string property. Replaces any previous value.
 *
 * This method corresponds to [xsd:string](xsd-ontology.html#xsd:string).
 */
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_string, const char *, G_TYPE_STRING, g_value_set_string, validate_pointer)

/**
 * tracker_resource_set_uri:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @value: the property object
 *
 * Sets a resource property as an URI string. Replaces any previous value.
 *
 * This method applies to properties with a [rdfs:range](rdf-ontology.html#rdfs:range)
 * that points to a non-literal class (i.e. a subclass of
 * [rdfs:Resource](rdf-ontology.html#rdfs:Resource)).
 *
 * This function produces similar RDF to [method@Resource.set_relation], although
 * it requires that the URI is previously known.
 */
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_uri, const char *, TRACKER_TYPE_URI, value_set_uri, validate_pointer)

/**
 * tracker_resource_set_datetime:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @value: the property object
 *
 * Sets a date property as a [type@GLib.DateTime]. Replaces any previous value.
 *
 * This method corresponds to [xsd:date](xsd-ontology.html#xsd:date) and
 * [xsd:dateTime](xsd-ontology.html#xsd:dateTime).
 *
 * Since: 3.2
 */
SET_PROPERTY_FOR_GTYPE (tracker_resource_set_datetime, GDateTime *, G_TYPE_DATE_TIME, g_value_set_boxed, validate_pointer)

/**
 * tracker_resource_add_gvalue:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to set
 * @value: an initialised [struct@GObject.Value]
 *
 * Add @value to the list of values for given property.
 *
 * You can pass any kind of [struct@GObject.Value] for @value, but serialization functions will
 * normally only be able to serialize URIs/relationships and fundamental value
 * types (string, int, etc.).
 */
void
tracker_resource_add_gvalue (TrackerResource *self,
                             const char      *property_uri,
                             const GValue    *value)
{
	TrackerResourcePrivate *priv;
	GValue *existing_value, *array_holder, *our_value;
	GPtrArray *array;

	g_return_if_fail (TRACKER_IS_RESOURCE (self));
	g_return_if_fail (property_uri != NULL);
	g_return_if_fail (G_IS_VALUE (value));

	priv = GET_PRIVATE (self);

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
}

#define ADD_PROPERTY_FOR_GTYPE(name, ctype, gtype, set_function, validate_function)     \
	void name (TrackerResource *self,                                               \
	           const char *property_uri,                                            \
	           ctype value)                                                         \
	{                                                                               \
		TrackerResourcePrivate *priv;                                           \
		GValue *existing_value, *array_holder, *our_value;                      \
		GPtrArray *array;                                                       \
                                                                                        \
		g_return_if_fail (TRACKER_IS_RESOURCE (self));                          \
		g_return_if_fail (property_uri != NULL);                                \
                                                                                        \
		priv = GET_PRIVATE (self);                                              \
                                                                                        \
		if (!validate_function (value, __func__)) {                             \
			return;                                                         \
		}                                                                       \
                                                                                        \
		existing_value = g_hash_table_lookup (priv->properties,                 \
		                                      property_uri);                    \
                                                                                        \
		if (existing_value && G_VALUE_HOLDS (existing_value,                    \
		                                     G_TYPE_PTR_ARRAY)) {               \
			array = g_value_get_boxed (existing_value);                     \
			array_holder = existing_value;                                  \
		} else {                                                                \
			array = g_ptr_array_new_with_free_func (                        \
			        (GDestroyNotify)free_value);                            \
			array_holder = g_slice_new0 (GValue);                           \
			g_value_init (array_holder, G_TYPE_PTR_ARRAY);                  \
			g_value_take_boxed (array_holder, array);                       \
                                                                                        \
			if (existing_value) {                                           \
				/* existing_value is owned by hash table */             \
				our_value = g_slice_new0 (GValue);                      \
				g_value_init (our_value, G_VALUE_TYPE(existing_value)); \
				g_value_copy (existing_value, our_value);               \
				g_ptr_array_add (array, our_value);                     \
			}                                                               \
		}                                                                       \
                                                                                        \
		our_value = g_slice_new0 (GValue);                                      \
		g_value_init (our_value, gtype);                                        \
		set_function (our_value, value);                                        \
                                                                                        \
		g_ptr_array_add (array, our_value);                                     \
                                                                                        \
		if (array_holder != existing_value) {                                   \
			g_hash_table_insert (priv->properties,                          \
			                     g_strdup (property_uri), array_holder);    \
		}                                                                       \
	}

/**
 * tracker_resource_add_boolean:
 * @self: The `TrackerResource`
 * @property_uri: A string identifying the property to modify
 * @value: The property boolean value
 *
 * Adds a boolean property. Previous values for the same property are kept.
 *
 * This method is meant for RDF properties allowing multiple values, see
 * [nrl:maxCardinality](nrl-ontology.html#nrl:maxCardinality).
 *
 * This method corresponds to [xsd:boolean](xsd-ontology.html#xsd:boolean).
 */
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_boolean, gboolean, G_TYPE_BOOLEAN, g_value_set_boolean, validate_boolean)

/**
 * tracker_resource_add_double:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @value: the property object
 *
 * Adds a numeric property with double precision. Previous values for the same property are kept.
 *
 * This method is meant for RDF properties allowing multiple values, see
 * [nrl:maxCardinality](nrl-ontology.html#nrl:maxCardinality).
 *
 * This method corresponds to [xsd:double](xsd-ontology.html#xsd:double).
 */
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_double, double, G_TYPE_DOUBLE, g_value_set_double, validate_double)

/**
 * tracker_resource_add_int:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @value: the property object
 *
 * Adds a numeric property with integer precision. Previous values for the same property are kept.
 *
 * This method is meant for RDF properties allowing multiple values, see
 * [nrl:maxCardinality](nrl-ontology.html#nrl:maxCardinality).
 *
 * This method corresponds to [xsd:integer](xsd-ontology.html#xsd:integer).
 */
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_int, int, G_TYPE_INT, g_value_set_int, validate_int)

/**
 * tracker_resource_add_int64:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @value: the property object
 *
 * Adds a numeric property with 64-bit integer precision. Previous values for the same property are kept.
 *
 * This method is meant for RDF properties allowing multiple values, see
 * [nrl:maxCardinality](nrl-ontology.html#nrl:maxCardinality).
 *
 * This method corresponds to [xsd:integer](xsd-ontology.html#xsd:integer).
 */
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_int64, gint64, G_TYPE_INT64, g_value_set_int64, validate_int64)

/**
 * tracker_resource_add_relation:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @resource: the property object
 *
 * Adds a resource property as a `TrackerResource`. Previous values for the same property are kept.
 *
 * This method is meant for RDF properties allowing multiple values, see
 * [nrl:maxCardinality](nrl-ontology.html#nrl:maxCardinality).
 *
 * This method applies to properties with a [rdfs:range](rdf-ontology.html#rdfs:range)
 * that points to a non-literal class (i.e. a subclass of
 * [rdfs:Resource](rdf-ontology.html#rdfs:Resource)).
 *
 * This method produces similar RDF to [method@Resource.add_uri],
 * although in this function the URI will depend on the identifier
 * set on @resource.
 */
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_relation, TrackerResource *, TRACKER_TYPE_RESOURCE, g_value_set_object, validate_pointer)

/**
 * tracker_resource_add_take_relation:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @resource: (transfer full): the property object
 *
 * Adds a resource property as a `TrackerResource`. Previous values for the same property are kept.
 * Takes ownership on the given @resource.
 *
 * This method is meant to RDF properties allowing multiple values, see
 * [nrl:maxCardinality](nrl-ontology.html#nrl:maxCardinality).
 *
 * This method applies to properties with a [rdfs:range](rdf-ontology.html#rdfs:range)
 * that points to a non-literal class (i.e. a subclass of
 * [rdfs:Resource](rdf-ontology.html#rdfs:Resource)).
 *
 * This function produces similar RDF to [method@Resource.add_uri],
 * although in this function the URI will depend on the identifier
 * set on @resource. This function takes ownership of @resource.
 */
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_take_relation, TrackerResource *, TRACKER_TYPE_RESOURCE, g_value_take_object, validate_pointer)


/**
 * tracker_resource_add_string:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @value: the property object
 *
 * Adds a string property. Previous values for the same property are kept.
 *
 * This method is meant for RDF properties allowing multiple values, see
 * [nrl:maxCardinality](nrl-ontology.html#nrl:maxCardinality).
 *
 * This method corresponds to [xsd:string](xsd-ontology.html#xsd:string).
 */
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_string, const char *, G_TYPE_STRING, g_value_set_string, validate_pointer)

/**
 * tracker_resource_add_uri:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @value: the property object
 *
 * Adds a resource property as an URI string. Previous values for the same property are kept.
 *
 * This method applies to properties with a [rdfs:range](rdf-ontology.html#rdfs:range)
 * that points to a non-literal class (i.e. a subclass of
 * [rdfs:Resource](rdf-ontology.html#rdfs:Resource)).
 *
 * This method is meant for RDF properties allowing multiple values, see
 * [nrl:maxCardinality](nrl-ontology.html#nrl:maxCardinality).
 *
 * This function produces similar RDF to [method@Resource.add_relation], although
 * it requires that the URI is previously known.
 */
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_uri, const char *, TRACKER_TYPE_URI, value_set_uri, validate_pointer)

/**
 * tracker_resource_add_datetime:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to modify
 * @value: the property object
 *
 * Adds a date property as a [type@GLib.DateTime]. Previous values for the
 * same property are kept.
 *
 * This method is meant for RDF properties allowing multiple values, see
 * [nrl:maxCardinality](nrl-ontology.html#nrl:maxCardinality).
 *
 * This method corresponds to [xsd:date](xsd-ontology.html#xsd:date) and
 * [xsd:dateTime](xsd-ontology.html#xsd:dateTime).
 *
 * Since: 3.2
 */
ADD_PROPERTY_FOR_GTYPE (tracker_resource_add_datetime, GDateTime *, G_TYPE_DATE_TIME, g_value_set_boxed, validate_pointer)

/**
 * tracker_resource_get_values:
 * @self: the `TrackerResource`
 * @property_uri: a string identifying the property to look up
 *
 * Returns the list of all known values of the given property.
 *
 * Returns: (transfer container) (element-type GValue) (nullable): a [struct@GLib.List] of
 *   [struct@GObject.Value] instances. The list should be freed with [func@GLib.List.free]
 */
GList *tracker_resource_get_values (TrackerResource *self,
                                    const char      *property_uri)
{
	TrackerResourcePrivate *priv;
	GValue *value;

	g_return_val_if_fail (TRACKER_IS_RESOURCE (self), NULL);
	g_return_val_if_fail (property_uri, NULL);

	priv = GET_PRIVATE (self);

	value = g_hash_table_lookup (priv->properties, property_uri);

	if (value == NULL) {
		return NULL;
	}

	if (G_VALUE_HOLDS (value, G_TYPE_PTR_ARRAY)) {
		GList *result = NULL;
		GPtrArray *array;
		guint i;

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

#define GET_PROPERTY_FOR_GTYPE(name, ctype, gtype, get_function, no_value)    \
	ctype name (TrackerResource *self,                                    \
	            const char *property_uri)                                 \
	{                                                                     \
		TrackerResourcePrivate *priv;                                 \
		GValue *value;                                                \
                                                                              \
		g_return_val_if_fail (TRACKER_IS_RESOURCE (self), no_value);  \
		g_return_val_if_fail (property_uri, no_value);                \
                                                                              \
		priv = GET_PRIVATE (self);                                    \
                                                                              \
		value = g_hash_table_lookup (priv->properties, property_uri); \
                                                                              \
		if (value == NULL) {                                          \
			return no_value;                                      \
		};                                                            \
                                                                              \
		if (G_VALUE_HOLDS (value, G_TYPE_PTR_ARRAY)) {                \
			GPtrArray *array;                                     \
			array = g_value_get_boxed (value);                    \
			if (array->len == 0) {                                \
				return no_value;                              \
			} else {                                              \
				value = g_ptr_array_index (array, 0);         \
			}                                                     \
		}                                                             \
                                                                              \
		return get_function (value);                                  \
	}

/**
 * tracker_resource_get_first_boolean:
 * @self: A `TrackerResource`
 * @property_uri: a string identifying the property to look up
 *
 * Returns the first boolean object previously assigned to a property.
 *
 * Returns: the first boolean object
 */
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_boolean, gboolean, G_TYPE_BOOLEAN, g_value_get_boolean, FALSE)

/**
 * tracker_resource_get_first_double:
 * @self: A `TrackerResource`
 * @property_uri: a string identifying the property to look up
 *
 * Returns the first double object previously assigned to a property.
 *
 * Returns: the first double object
 */
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_double, double, G_TYPE_DOUBLE, g_value_get_double, 0.0)

/**
 * tracker_resource_get_first_int:
 * @self: A `TrackerResource`
 * @property_uri: a string identifying the property to look up
 *
 * Returns the first integer object previously assigned to a property.
 *
 * Returns: the first integer object
 */
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_int, int, G_TYPE_INT, g_value_get_int, 0)

/**
 * tracker_resource_get_first_int64:
 * @self: A `TrackerResource`
 * @property_uri: a string identifying the property to look up
 *
 * Returns the first integer object previously assigned to a property.
 *
 * Returns: the first integer object
 */
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_int64, gint64, G_TYPE_INT64, g_value_get_int64, 0)

/**
 * tracker_resource_get_first_relation:
 * @self: A `TrackerResource`
 * @property_uri: a string identifying the property to look up
 *
 * Returns the first resource object previously assigned to a property.
 *
 * Returns: (transfer none) (nullable): the first resource object
 */
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_relation, TrackerResource *, TRACKER_TYPE_RESOURCE, g_value_get_object, NULL)

/**
 * tracker_resource_get_first_string:
 * @self: A `TrackerResource`
 * @property_uri: a string identifying the property to look up
 *
 * Returns the first string object previously assigned to a property.
 *
 * Returns: (nullable): the first string object
 */
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_string, const char *, G_TYPE_STRING, g_value_get_string, NULL)

/**
 * tracker_resource_get_first_uri:
 * @self: A `TrackerResource`
 * @property_uri: a string identifying the property to look up
 *
 * Returns the first resource object previously assigned to a property.
 *
 * Returns: (nullable): the first resource object as an URI.
 */
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_uri, const char *, TRACKER_TYPE_URI, g_value_get_string, NULL)

/**
 * tracker_resource_get_first_datetime:
 * @self: A `TrackerResource`
 * @property_uri: a string identifying the property to look up
 *
 * Returns the first [type@GLib.DateTime] previously assigned to a property.
 *
 * Returns: (transfer none) (nullable): the first GDateTime object
 * Since: 3.2
 */
GET_PROPERTY_FOR_GTYPE (tracker_resource_get_first_datetime, GDateTime *, G_TYPE_DATE_TIME, g_value_get_boxed, NULL)

/**
 * tracker_resource_get_identifier:
 * @self: A `TrackerResource`
 *
 * Returns the identifier of a resource.
 *
 * If the identifier was set to NULL, the identifier returned will be a locally
 * unique SPARQL blank node identifier, such as `_:123`.
 *
 * Returns: (nullable): a string owned by the resource
 */
const char *
tracker_resource_get_identifier (TrackerResource *self)
{
	TrackerResourcePrivate *priv;

	g_return_val_if_fail (TRACKER_IS_RESOURCE (self), NULL);

	priv = GET_PRIVATE (self);

	if (!priv->identifier)
		priv->identifier = generate_blank_node_identifier ();

	return priv->identifier;
}

/**
 * tracker_resource_set_identifier:
 * @self: A `TrackerResource`
 * @identifier: (nullable): a string identifying the resource
 *
 * Changes the identifier of a `TrackerResource`. The identifier should be a
 * URI or compact URI, but this is not necessarily enforced. Invalid
 * identifiers may cause errors when serializing the resource or trying to
 * insert the results in a database.
 *
 * If the identifier is set to %NULL, a SPARQL blank node identifier such as
 * `_:123` is assigned to the resource.
 */
void
tracker_resource_set_identifier (TrackerResource *self,
                                 const char      *identifier)
{
	TrackerResourcePrivate *priv;

	g_return_if_fail (TRACKER_IS_RESOURCE (self));

	priv = GET_PRIVATE (self);

	g_clear_pointer (&priv->identifier, g_free);
	priv->identifier = escape_iri (identifier);
}

/**
 * tracker_resource_identifier_compare_func:
 * @resource: a `TrackerResource`
 * @identifier: a string identifying the resource
 *
 * A helper function that compares a `TrackerResource` by its identifier
 * string.
 *
 * Returns: an integer less than, equal to, or greater than zero, if the
 *          resource identifier is <, == or > than @identifier
 **/
gint
tracker_resource_identifier_compare_func (TrackerResource *resource,
                                          const char      *identifier)
{
	g_return_val_if_fail (TRACKER_IS_RESOURCE (resource), 0);
	g_return_val_if_fail (identifier != NULL, 0);

	return strcmp (tracker_resource_get_identifier (resource), identifier);
}

/**
 * tracker_resource_compare:
 * @a: A `TrackerResource`
 * @b: A second `TrackerResource` to compare
 *
 * Compare the identifiers of two TrackerResource instances. The resources
 * are considered identical if they have the same identifier.
 *
 * Note that there can be false negatives with this simplistic approach: two
 * resources may have different identifiers that actually refer to the same
 * thing.
 *
 * Returns: 0 if the identifiers are the same, -1 or +1 otherwise
 */
gint
tracker_resource_compare (TrackerResource *a,
                          TrackerResource *b)
{
	g_return_val_if_fail (TRACKER_IS_RESOURCE (a), 0);
	g_return_val_if_fail (TRACKER_IS_RESOURCE (b), 0);

	return strcmp (tracker_resource_get_identifier (a),
		       tracker_resource_get_identifier (b));
}

/**
 * tracker_resource_get_properties:
 * @resource: a `TrackerResource`
 *
 * Gets the list of properties defined in @resource
 *
 * Returns: (transfer container) (element-type utf8): The list of properties.
 **/
GList *
tracker_resource_get_properties (TrackerResource *resource)
{
	TrackerResourcePrivate *priv;

	g_return_val_if_fail (TRACKER_IS_RESOURCE (resource), NULL);

	priv = GET_PRIVATE (resource);

	return g_hash_table_get_keys (priv->properties);
}

static gchar *
parse_prefix (const gchar *prefixed_name)
{
	const gchar *end, *token_end;

	end = &prefixed_name[strlen(prefixed_name)];

	if (!terminal_PNAME_NS (prefixed_name, end, &token_end))
		return NULL;

	g_assert (token_end != NULL);

	/* We have read the ':', take a step back */
	if (token_end > prefixed_name)
		token_end--;

	if (*token_end != ':')
		return NULL;

	return g_strndup (prefixed_name, token_end - prefixed_name);
}

static gboolean
is_blank_node (const char *uri_or_curie_or_blank)
{
	return (strncmp(uri_or_curie_or_blank, "_:", 2) == 0);
}

static gboolean
is_builtin_class (const gchar             *uri_or_curie,
                  TrackerNamespaceManager *namespaces)
{
	gchar *prefix = NULL;
	gboolean has_prefix;

	/* blank nodes should be processed as nested resource
	 * parse_prefix returns NULL for blank nodes, i.e. _:1
	 */
	if (is_blank_node (uri_or_curie))
		return FALSE;

	prefix = parse_prefix (uri_or_curie);

	if (!prefix)
		return TRUE;

	has_prefix = tracker_namespace_manager_has_prefix (namespaces, prefix);
	g_free (prefix);

	return has_prefix;
}

static void
generate_turtle_uri_value (const char              *uri_or_curie_or_blank,
                           GString                 *string,
                           TrackerNamespaceManager *all_namespaces)
{
	/* The tracker_resource_set_uri() function accepts URIs
	 * (such as http://example.com/) and compact URIs (such as nie:DataObject),
	 * and blank node identifiers (_:0). The tracker_resource_set_identifier()
	 * function works the same.
	 *
	 * We could expand all CURIEs, but the generated Turtle or SPARQL will be
	 * clearer if we leave them be. We still need to attempt to expand them
	 * internally in order to know whether they need <> brackets around them.
	 */
	if (is_blank_node (uri_or_curie_or_blank)) {
		g_string_append (string, uri_or_curie_or_blank);
	} else {
		char *prefix = parse_prefix (uri_or_curie_or_blank);

		if (prefix && tracker_namespace_manager_has_prefix (all_namespaces, prefix)) {
			g_string_append (string, uri_or_curie_or_blank);
		} else {
			/* It's a full URI (or something invalid, but we can't really tell that here) */
			g_string_append_printf (string, "<%s>", uri_or_curie_or_blank);
		}

		g_free (prefix);
	}
}

static void
generate_turtle_value (const GValue            *value,
                       GString                 *string,
                       TrackerNamespaceManager *all_namespaces)
{
	GType type = G_VALUE_TYPE (value);
	if (type == TRACKER_TYPE_URI) {
		generate_turtle_uri_value (g_value_get_string (value),
		                           string,
		                           all_namespaces);
	} else if (type == TRACKER_TYPE_RESOURCE) {
		TrackerResource *relation = TRACKER_RESOURCE (g_value_get_object (value));
		generate_turtle_uri_value (tracker_resource_get_identifier (relation),
		                           string,
		                           all_namespaces);
	} else if (type == G_TYPE_STRING) {
		char *escaped = tracker_sparql_escape_string (g_value_get_string (value));
		g_string_append_printf(string, "\"%s\"", escaped);
		g_free (escaped);
	} else if (type == G_TYPE_DATE) {
		char date_string[256];
		g_date_strftime (date_string, 256,
		                 "\"%Y-%m-%d%z\"^^<http://www.w3.org/2001/XMLSchema#date>",
		                 g_value_get_boxed (value));
		g_string_append (string, date_string);
	} else if (type == G_TYPE_DATE_TIME) {
		char *datetime_string;
		datetime_string = g_date_time_format (g_value_get_boxed (value),
		                                      "\"%Y-%m-%dT%H:%M:%S%z\"^^<http://www.w3.org/2001/XMLSchema#dateTime>");
		g_string_append (string, datetime_string);
		g_free (datetime_string);
	} else if (type == G_TYPE_DOUBLE || type == G_TYPE_FLOAT) {
		/* We can't use GValue transformations here; they're locale-dependent. */
		char buffer[256];
		g_ascii_dtostr (buffer, 255, g_value_get_double (value));
		g_string_append (string, buffer);
	} else {
		GValue str_value = G_VALUE_INIT;
		g_value_init (&str_value, G_TYPE_STRING);
		if (g_value_transform (value, &str_value)) {
			g_string_append (string, g_value_get_string (&str_value));
		} else {
			g_warning ("Cannot serialize value of type %s to Turtle/SPARQL",
			           G_VALUE_TYPE_NAME (value));
		}
		g_value_unset (&str_value);
	}
}

void
generate_turtle_property (const char              *property,
                          const GValue            *value,
                          GString                 *string,
                          TrackerNamespaceManager *all_namespaces)
{
	if (strcmp (property, TRACKER_PREFIX_RDF "type") == 0 || strcmp (property, "rdf:type") == 0) {
		g_string_append (string, "a");
	} else {
		g_string_append (string, property);
	}

	g_string_append (string, " ");
	if (G_VALUE_HOLDS (value, G_TYPE_PTR_ARRAY)) {
		guint i;
		GPtrArray *array = g_value_get_boxed (value);
		if (array->len > 0) {
			generate_turtle_value (g_ptr_array_index (array, 0),
			                       string,
			                       all_namespaces);
			for (i = 1; i < array->len; i++) {
				g_string_append (string, " , ");
				generate_turtle_value (g_ptr_array_index (array, i),
				                       string,
				                       all_namespaces);
			}
		}
	} else {
		generate_turtle_value (value, string, all_namespaces);
	}
}

/**
 * tracker_resource_print_turtle:
 * @self: a `TrackerResource`
 * @namespaces: (allow-none): a set of prefixed URLs, or %NULL to use the
 *     Nepomuk set
 *
 * Serialize all the information in @resource as a Turtle document.
 *
 * The generated Turtle should correspond to this standard:
 * <https://www.w3.org/TR/2014/REC-turtle-20140225/>
 *
 * The @namespaces object is used to expand any compact URI values. In most
 * cases you should pass the one returned by [method@SparqlConnection.get_namespace_manager]
 * from the connection that is the intended recipient of this data.
 *
 * Returns: a newly-allocated string
 *
 * Deprecated: 3.4: Use [method@Resource.print_rdf] instead.
 */
char *
tracker_resource_print_turtle (TrackerResource         *self,
                               TrackerNamespaceManager *namespaces)
{
	g_return_val_if_fail (TRACKER_IS_RESOURCE (self), "");

	if (namespaces == NULL) {
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		namespaces = tracker_namespace_manager_get_default ();
		G_GNUC_END_IGNORE_DEPRECATIONS
	}

	return tracker_resource_print_rdf (self, namespaces, TRACKER_RDF_FORMAT_TURTLE, NULL);
}

typedef struct {
	TrackerNamespaceManager *namespaces;
	GString *string;
	char *graph_id;
	GList *done_list;
} GenerateSparqlData;

static void generate_sparql_deletes (TrackerResource *resource, GenerateSparqlData *data);
static void generate_sparql_insert_pattern (TrackerResource *resource, GenerateSparqlData *data);

static void
generate_sparql_relation_deletes_foreach (gpointer key,
                                          gpointer value_ptr,
                                          gpointer user_data)
{
	const GValue *value = value_ptr;
	GenerateSparqlData *data = user_data;
	guint i;

	if (G_VALUE_HOLDS (value, TRACKER_TYPE_RESOURCE)) {
		TrackerResource *relation = g_value_get_object (value);

		generate_sparql_deletes (relation, data);
	} else if (G_VALUE_HOLDS (value, G_TYPE_PTR_ARRAY)) {
		GPtrArray *array = g_value_get_boxed (value);

		for (i = 0; i < array->len; i ++) {
			GValue *value = g_ptr_array_index (array, i);

			if (G_VALUE_HOLDS (value, TRACKER_TYPE_RESOURCE)) {
				TrackerResource *relation = g_value_get_object (value);

				generate_sparql_deletes (relation, data);
			}
		}
	}
}

static void
generate_sparql_relation_inserts_foreach (gpointer key,
                                          gpointer value_ptr,
                                          gpointer user_data)
{
	const GValue *value = value_ptr;
	GenerateSparqlData *data = user_data;

	if (G_VALUE_HOLDS (value, TRACKER_TYPE_RESOURCE)) {
		TrackerResource *relation = g_value_get_object (value);

		/* We don't need to produce inserts for builtin classes */
		if (is_builtin_class (tracker_resource_get_identifier (relation),
		                      data->namespaces))
			return;

		generate_sparql_insert_pattern (relation, data);
	} else if (G_VALUE_HOLDS (value, G_TYPE_PTR_ARRAY)) {
		GPtrArray *array = g_value_get_boxed (value);
		const GValue *array_value;
		TrackerResource *relation;
		guint i;

		for (i = 0; i < array->len; i++) {
			array_value = g_ptr_array_index (array, i);

			if (!G_VALUE_HOLDS (array_value, TRACKER_TYPE_RESOURCE))
				continue;

			relation = g_value_get_object (array_value);

			/* We don't need to produce inserts for builtin classes */
			if (is_builtin_class (tracker_resource_get_identifier (relation),
					      data->namespaces))
				continue;

			generate_sparql_insert_pattern (relation, data);
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
generate_sparql_delete_queries (TrackerResource     *resource,
                                GHashTable          *overwrite_flags,
                                GenerateSparqlData  *data)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (resource);
	GHashTableIter iter;
	const char *property;
	const GValue *value;

	g_hash_table_iter_init (&iter, priv->properties);
	while (g_hash_table_iter_next (&iter, (gpointer *)&property, (gpointer *)&value)) {
		/* Whether to generate the DELETE is based on whether set_value was ever
		* called for this property. That's tracked in the overwrite_flags hash table.
		*/
		if (g_hash_table_lookup (overwrite_flags, property)) {
			char *variable_name = variable_name_for_property (property);

			g_string_append (data->string, "DELETE WHERE {\n");

			if (data->graph_id) {
				g_string_append_printf (data->string, "GRAPH <%s> {\n", data->graph_id);
			}

			g_string_append (data->string, "  ");
			generate_turtle_uri_value (tracker_resource_get_identifier (resource),
			                           data->string, data->namespaces);
			g_string_append_printf (data->string, " %s ?%s }", property, variable_name);
			g_free (variable_name);

			if (data->graph_id) {
				g_string_append (data->string, " }");
			}

			g_string_append (data->string, ";\n");
		}
	}
}

void
generate_sparql_deletes (TrackerResource    *resource,
                         GenerateSparqlData *data)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (resource);

	if (g_list_find (data->done_list, resource) != NULL)
		/* We already processed this resource. */
		return;

	data->done_list = g_list_prepend (data->done_list, resource);

	if (!tracker_resource_is_blank_node (resource) && g_hash_table_size (priv->overwrite) > 0) {
		generate_sparql_delete_queries (resource, priv->overwrite, data);
	}

	/* Now emit any sub-resources. */
	g_hash_table_foreach (priv->properties, generate_sparql_relation_deletes_foreach, data);
}

static void
generate_sparql_insert_pattern (TrackerResource    *resource,
                                GenerateSparqlData *data)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (resource);
	GHashTableIter iter;
	const char *property;
	char *full_property;
	const GValue *value;
	gboolean had_property = FALSE;

	if (g_list_find (data->done_list, resource) != NULL)
		/* We already processed this resource. */
		return;

	data->done_list = g_list_prepend (data->done_list, resource);

	/* First, emit any sub-resources. */
	g_hash_table_foreach (priv->properties, generate_sparql_relation_inserts_foreach, data);

	generate_turtle_uri_value (tracker_resource_get_identifier (resource),
	                           data->string, data->namespaces);
	g_string_append_printf (data->string, " ");

	/* rdf:type needs to be first, otherwise you'll see 'subject x is not in domain y'
	 * errors for the properties you try to set.
	 */
	value = g_hash_table_lookup (priv->properties, "rdf:type");
	if (value != NULL) {
		generate_turtle_property ("a", value, data->string, data->namespaces);
		had_property = TRUE;
	}

	g_hash_table_iter_init (&iter, priv->properties);
	while (g_hash_table_iter_next (&iter, (gpointer *)&property, (gpointer *)&value)) {
		full_property = tracker_namespace_manager_expand_uri (data->namespaces, property);

		if (strcmp (full_property, TRACKER_PREFIX_RDF "type") != 0 && strcmp (property, "rdf:type") != 0) {
			if (had_property) {
				g_string_append (data->string, " ; \n  ");
			}

			generate_turtle_property (property, value, data->string, data->namespaces);

			had_property = TRUE;
		}

		g_free (full_property);
	}

	g_string_append (data->string, " .\n");
}

/**
 * tracker_resource_print_sparql_update:
 * @self: a `TrackerResource`
 * @namespaces: (allow-none): a set of prefixed URLs, or %NULL to use the
 *     Nepomuk set
 * @graph_id: (allow-none): the URN of the graph the data should be added to,
 *     or %NULL
 *
 * Generates a SPARQL command to update a database with the information
 * stored in @resource.
 *
 * The @namespaces object is used to expand any compact URI values. In most
 * cases you should pass the one returned by [method@SparqlConnection.get_namespace_manager]
 * from the connection that is the intended recipient of this data.
 *
 * Returns: a newly-allocated string containing a SPARQL update command.
 */
char *
tracker_resource_print_sparql_update (TrackerResource         *resource,
                                      TrackerNamespaceManager *namespaces,
                                      const char              *graph_id)
{
	TrackerResourcePrivate *priv;
	GenerateSparqlData context = { 0, };

	g_return_val_if_fail (TRACKER_IS_RESOURCE (resource), "");

	priv = GET_PRIVATE(resource);

	if (namespaces == NULL) {
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		namespaces = tracker_namespace_manager_get_default ();
		G_GNUC_END_IGNORE_DEPRECATIONS
	}

	if (g_hash_table_size (priv->properties) == 0) {
		return g_strdup("");
	}

	context.namespaces = namespaces;
	context.string = g_string_new (NULL);

	if (graph_id)
		context.graph_id = tracker_namespace_manager_expand_uri (namespaces, graph_id);

	/* Resources can be recursive, and may have repeated or even cyclic
	 * relationships. This list keeps track of what we already processed.
	 */
	context.done_list = NULL;

	/* Delete the existing data. If we don't do this, we may get constraint
	 * violations due to trying to add a second value to a single-valued
	 * property, and we may get old metadata hanging around.
	 */
	generate_sparql_deletes (resource, &context);

	g_list_free (context.done_list);
	context.done_list = NULL;

	/* Finally insert the data */
	g_string_append (context.string, "INSERT DATA {\n");
	if (context.graph_id) {
		g_string_append_printf (context.string, "GRAPH <%s> {\n", context.graph_id);
	}

	generate_sparql_insert_pattern (resource, &context);

	if (context.graph_id) {
		g_string_append (context.string, "}\n");
	}
	g_string_append (context.string, "};\n");

	g_list_free (context.done_list);
	g_free (context.graph_id);
	context.done_list = NULL;

	return g_string_free (context.string, FALSE);
}

/**
 * tracker_resource_print_jsonld:
 * @self: a `TrackerResource`
 * @namespaces: (nullable): a set of prefixed URLs, or %NULL to use the
 *     Nepomuk set
 *
 * Serialize all the information in @resource as a JSON-LD document.
 *
 * See <http://www.jsonld.org/> for more information on the JSON-LD
 * serialization format.
 *
 * The @namespaces object is used to expand any compact URI values. In most
 * cases you should pass the one returned by [method@SparqlConnection.get_namespace_manager]
 * from the connection that is the intended recipient of this data.
 *
 * Returns: a newly-allocated string containing JSON-LD data.
 *
 * Deprecated: 3.5: Use [method@Resource.print_rdf] instead.
 */
char *
tracker_resource_print_jsonld (TrackerResource         *self,
                               TrackerNamespaceManager *namespaces)
{
	g_return_val_if_fail (TRACKER_IS_RESOURCE (self), "");

	if (namespaces == NULL) {
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		namespaces = tracker_namespace_manager_get_default ();
		G_GNUC_END_IGNORE_DEPRECATIONS
	}

	return tracker_resource_print_rdf (self, namespaces, TRACKER_RDF_FORMAT_JSON_LD, NULL);
}

static TrackerSerializerFormat
convert_format (TrackerRdfFormat format)
{
	switch (format) {
	case TRACKER_RDF_FORMAT_TURTLE:
		return TRACKER_SERIALIZER_FORMAT_TTL;
	case TRACKER_RDF_FORMAT_TRIG:
		return TRACKER_SERIALIZER_FORMAT_TRIG;
	case TRACKER_RDF_FORMAT_JSON_LD:
		return TRACKER_SERIALIZER_FORMAT_JSON_LD;
	case TRACKER_RDF_FORMAT_LAST:
		g_assert_not_reached ();
	}

	return -1;
}

/**
 * tracker_resource_print_rdf:
 * @self: a `TrackerResource`
 * @namespaces: a set of prefixed URLs
 * @format: RDF format of the printed string
 * @graph: (nullable): target graph of the resource RDF, or %NULL for the
 * default graph
 *
 * Serialize all the information in @resource into the selected RDF format.
 *
 * The @namespaces object is used to expand any compact URI values. In most
 * cases you should pass the one returned by [method@SparqlConnection.get_namespace_manager]
 * from the connection that is the intended recipient of this data.
 *
 * Returns: a newly-allocated string containing RDF data in the requested format.
 *
 * Since: 3.4
 **/
char *
tracker_resource_print_rdf (TrackerResource         *self,
                            TrackerNamespaceManager *namespaces,
                            TrackerRdfFormat         format,
                            const gchar             *graph)
{
	TrackerSparqlCursor *deserializer;
	GInputStream *serializer;
	GString *str;

	g_return_val_if_fail (TRACKER_IS_RESOURCE (self), NULL);
	g_return_val_if_fail (TRACKER_IS_NAMESPACE_MANAGER (namespaces), NULL);
	g_return_val_if_fail (format < TRACKER_N_RDF_FORMATS, NULL);

	deserializer = tracker_deserializer_resource_new (self, namespaces, graph);
	serializer = tracker_serializer_new (TRACKER_SPARQL_CURSOR (deserializer),
	                                     namespaces,
	                                     convert_format (format));
	g_object_unref (deserializer);

	str = g_string_new (NULL);

	if (format == TRACKER_RDF_FORMAT_JSON_LD) {
		JsonParser *parser;
		JsonGenerator *generator;
		JsonNode *root;

		/* Special case, ensure that json is pretty printed */
		parser = json_parser_new ();

		if (!json_parser_load_from_stream (parser,
		                                   serializer,
		                                   NULL,
		                                   NULL)) {
			g_object_unref (parser);
			return g_string_free (str, FALSE);
		}

		generator = json_generator_new ();
		root = json_parser_get_root (parser);
		json_generator_set_root (generator, root);
		json_generator_set_pretty (generator, TRUE);
		json_generator_to_gstring (generator, str);
		g_object_unref (generator);
		g_object_unref (parser);

		return g_string_free (str, FALSE);
	}

#define BUF_SIZE 4096
	while (TRUE) {
		GBytes *bytes;

		bytes = g_input_stream_read_bytes (serializer, BUF_SIZE, NULL, NULL);
		if (!bytes) {
			g_string_free (str, TRUE);
			return NULL;
		}

		if (g_bytes_get_size (bytes) == 0) {
			g_bytes_unref (bytes);
			break;
		}

		g_string_append_len (str,
		                     g_bytes_get_data (bytes, NULL),
		                     g_bytes_get_size (bytes));
		g_bytes_unref (bytes);
	}
#undef BUF_SIZE

	g_object_unref (serializer);

	return g_string_free (str, FALSE);
}

static GVariant *
tracker_serialize_single_value (TrackerResource         *resource,
                                const GValue            *value)
{
	if (G_VALUE_HOLDS_BOOLEAN (value)) {
		return g_variant_new_boolean (g_value_get_boolean (value));
	} else if (G_VALUE_HOLDS_INT (value)) {
		return g_variant_new_int32 (g_value_get_int (value));
	} else if (G_VALUE_HOLDS_INT64 (value)) {
		return g_variant_new_int64 (g_value_get_int64 (value));
	} else if (G_VALUE_HOLDS_DOUBLE (value)) {
		return g_variant_new_double (g_value_get_double (value));
	} else if (G_VALUE_HOLDS (value, TRACKER_TYPE_URI)) {
		/* Use bytestring for URIs, so they can be distinguised
		 * from plain strings
		 */
		return g_variant_new_bytestring (g_value_get_string (value));
	} else if (G_VALUE_HOLDS_STRING (value)) {
		return g_variant_new_string (g_value_get_string (value));
	} else if (G_VALUE_HOLDS (value, TRACKER_TYPE_RESOURCE)) {
		return tracker_resource_serialize (g_value_get_object (value));
	}

	g_warn_if_reached ();

	return NULL;
}

/**
 * tracker_resource_serialize:
 * @resource: A `TrackerResource`
 *
 * Serializes a `TrackerResource` to a [type@GLib.Variant] in a lossless way.
 * All child resources are subsequently serialized. It is implied
 * that both ends use a common [class@NamespaceManager].
 *
 * Returns: (transfer floating) (nullable): A variant describing the resource,
 *          the reference is floating.
 **/
GVariant *
tracker_resource_serialize (TrackerResource *resource)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (resource);
	GVariantBuilder builder;
	GHashTableIter iter;
	GList *properties, *l;
	const gchar *pred;
	GValue *value;

	g_return_val_if_fail (TRACKER_IS_RESOURCE (resource), NULL);

	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

	if (!tracker_resource_is_blank_node (resource)) {
		g_variant_builder_add (&builder, "{sv}", "@id",
		                       g_variant_new_string (priv->identifier));
	}

	g_hash_table_iter_init (&iter, priv->properties);

	/* Use a stable sort, so that GVariants are byte compatible */
	properties = tracker_resource_get_properties (resource);
	properties = g_list_sort (properties, (GCompareFunc) g_strcmp0);

	for (l = properties; l; l = l->next) {
		pred = l->data;
		value = g_hash_table_lookup (priv->properties, pred);

		if (G_VALUE_HOLDS (value, G_TYPE_PTR_ARRAY)) {
			GPtrArray *array = g_value_get_boxed (value);
			GVariantBuilder array_builder;
			guint i;

			g_variant_builder_init (&array_builder, G_VARIANT_TYPE_ARRAY);

			for (i = 0; i < array->len; i++) {
				GValue *child = g_ptr_array_index (array, i);
				GVariant *variant;

				variant = tracker_serialize_single_value (resource, child);
				if (!variant)
					return NULL;

				g_variant_builder_add_value (&array_builder, variant);
			}

			g_variant_builder_add (&builder, "{sv}", pred,
			                       g_variant_builder_end (&array_builder));
		} else {
			GVariant *variant;

			variant = tracker_serialize_single_value (resource, value);
			if (!variant)
				return NULL;

			g_variant_builder_add (&builder, "{sv}", pred, variant);
		}
	}

	g_list_free (properties);

	return g_variant_builder_end (&builder);
}

/**
 * tracker_resource_deserialize:
 * @variant: a [type@GLib.Variant]
 *
 * Deserializes a `TrackerResource` previously serialized with
 * [method@Resource.serialize]. It is implied that both ends
 * use a common [class@NamespaceManager].
 *
 * Returns: (transfer full) (nullable): A TrackerResource, or %NULL if
 *          deserialization fails.
 **/
TrackerResource *
tracker_resource_deserialize (GVariant *variant)
{
	TrackerResource *resource;
	GVariantIter iter;
	GVariant *obj;
	gchar *pred;

	g_return_val_if_fail (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARDICT), NULL);

	resource = tracker_resource_new (NULL);

	g_variant_iter_init (&iter, variant);

	while (g_variant_iter_next (&iter, "{sv}", &pred, &obj)) {
		/* Special case, "@id" for the resource identifier */
		if (g_strcmp0 (pred, "@id") == 0 &&
		    g_variant_is_of_type (obj, G_VARIANT_TYPE_STRING)) {
			tracker_resource_set_identifier (resource, g_variant_get_string (obj, NULL));
			continue;
		}

		if (g_variant_is_of_type (obj, G_VARIANT_TYPE_STRING)) {
			tracker_resource_set_string (resource, pred,
			                             g_variant_get_string (obj, NULL));
		} else if (g_variant_is_of_type (obj, G_VARIANT_TYPE_BOOLEAN)) {
			tracker_resource_set_boolean (resource, pred,
			                              g_variant_get_boolean (obj));
		} else if (g_variant_is_of_type (obj, G_VARIANT_TYPE_INT16)) {
			tracker_resource_set_int (resource, pred,
						  (gint) g_variant_get_int16 (obj));
		} else if (g_variant_is_of_type (obj, G_VARIANT_TYPE_INT32)) {
			tracker_resource_set_int (resource, pred,
						  (gint) g_variant_get_int32 (obj));
		} else if (g_variant_is_of_type (obj, G_VARIANT_TYPE_INT64)) {
			tracker_resource_set_int64 (resource, pred,
			                            (gint64) g_variant_get_int64 (obj));
		} else if (g_variant_is_of_type (obj, G_VARIANT_TYPE_DOUBLE)) {
			tracker_resource_set_double (resource, pred,
			                             g_variant_get_double (obj));
		} else if (g_variant_is_of_type (obj, G_VARIANT_TYPE_BYTESTRING)) {
			tracker_resource_set_uri (resource, pred,
			                          g_variant_get_bytestring (obj));
		} else if (g_variant_is_of_type (obj, G_VARIANT_TYPE_VARDICT)) {
			TrackerResource *child;

			child = tracker_resource_deserialize (obj);
			if (!child) {
				g_object_unref (resource);
				return NULL;
			}

			tracker_resource_set_relation (resource, pred, child);
		} else if (g_variant_is_of_type (obj, G_VARIANT_TYPE_ARRAY)) {
			GVariant *elem;
			GVariantIter iter2;

			g_variant_iter_init (&iter2, obj);

			/* Other arrays are multi-valued */
			while ((elem = g_variant_iter_next_value (&iter2)) != NULL) {
				if (g_variant_is_of_type (elem, G_VARIANT_TYPE_STRING)) {
					tracker_resource_add_string (resource, pred,
					                             g_variant_get_string (elem, NULL));
				} else if (g_variant_is_of_type (elem, G_VARIANT_TYPE_BOOLEAN)) {
					tracker_resource_add_boolean (resource, pred,
					                              g_variant_get_boolean (elem));
				} else if (g_variant_is_of_type (elem, G_VARIANT_TYPE_INT16)) {
					tracker_resource_add_int (resource, pred,
								  (gint) g_variant_get_int16 (elem));
				} else if (g_variant_is_of_type (elem, G_VARIANT_TYPE_INT32)) {
					tracker_resource_add_int (resource, pred,
								  (gint) g_variant_get_int32 (elem));
				} else if (g_variant_is_of_type (elem, G_VARIANT_TYPE_INT64)) {
					tracker_resource_add_int64 (resource, pred,
					                            (gint64) g_variant_get_int64 (elem));
				} else if (g_variant_is_of_type (elem, G_VARIANT_TYPE_DOUBLE)) {
					tracker_resource_add_double (resource, pred,
					                             g_variant_get_double (elem));
				} else if (g_variant_is_of_type (elem, G_VARIANT_TYPE_BYTESTRING)) {
					tracker_resource_add_uri (resource, pred,
					                          g_variant_get_bytestring (elem));
				} else if (g_variant_is_of_type (elem, G_VARIANT_TYPE_VARDICT)) {
					TrackerResource *child;

					child = tracker_resource_deserialize (elem);
					if (!child) {
						g_object_unref (resource);
						return NULL;
					}

					tracker_resource_add_relation (resource, pred, child);
				} else {
					g_warning ("Unhandled GVariant signature '%s'",
					           g_variant_get_type_string (elem));
					g_object_unref (resource);
					return NULL;
				}
			}
		} else {
			g_warning ("Unhandled GVariant signature '%s'",
			           g_variant_get_type_string (obj));
			g_object_unref (resource);
			return NULL;
		}
	}

	return resource;
}

/**
 * tracker_resource_get_property_overwrite:
 * @resource: a `TrackerResource`
 * @property_uri: a string identifying the property to query
 *
 * Returns whether the prior values for this property would be deleted
 * in the SPARQL issued by @resource.
 *
 * Returns: #TRUE if the property would be overwritten
 *
 * Since: 3.1
 **/
gboolean
tracker_resource_get_property_overwrite (TrackerResource *resource,
                                         const gchar     *property_uri)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (resource);

	return g_hash_table_contains (priv->overwrite, property_uri);
}

void
tracker_resource_iterator_init (TrackerResourceIterator *iter,
                                TrackerResource         *resource)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (resource);

	bzero (iter, sizeof (TrackerResourceIterator));
	g_hash_table_iter_init (&iter->prop_iter, priv->properties);
}

gboolean
tracker_resource_iterator_next (TrackerResourceIterator  *iter,
                                const gchar             **property,
                                const GValue            **value)
{
	gpointer key, val;

	if (iter->cur_values && iter->cur_prop) {
		iter->idx++;

		if (iter->idx < iter->cur_values->len) {
			*property = iter->cur_prop;
			*value = g_ptr_array_index (iter->cur_values, iter->idx);
			return TRUE;
		} else {
			iter->cur_values = NULL;
			iter->cur_prop = NULL;
		}
	}

	if (!g_hash_table_iter_next (&iter->prop_iter, &key, &val))
		return FALSE;

	if (G_VALUE_HOLDS (val, G_TYPE_PTR_ARRAY)) {
		iter->cur_prop = key;
		iter->cur_values = g_value_get_boxed (val);
		iter->idx = 0;
		*property = iter->cur_prop;
		*value = g_ptr_array_index (iter->cur_values, iter->idx);
		return TRUE;
	}

	*property = key;
	*value = val;
	return TRUE;
}

const gchar *
tracker_resource_get_identifier_internal (TrackerResource *resource)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (resource);

	return priv->identifier;
}

gboolean
tracker_resource_is_blank_node (TrackerResource *resource)
{
	TrackerResourcePrivate *priv = GET_PRIVATE (resource);

	if (!priv->identifier)
		return TRUE;

	return strncmp (priv->identifier, "_:", 2) == 0;
}
