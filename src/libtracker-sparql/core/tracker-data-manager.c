/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2007, Jason Kivlighn <jkivlighn@gmail.com>
 * Copyright (C) 2007, Creative Commons <http://creativecommons.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <glib/gstdio.h>

#include <libtracker-common/tracker-debug.h>
#include <libtracker-common/tracker-locale.h>

#include <libtracker-sparql/tracker-deserializer-rdf.h>

#include "tracker-class.h"
#include "tracker-data-manager.h"
#include "tracker-data-update.h"
#include "tracker-db-interface-sqlite.h"
#include "tracker-db-manager.h"
#include "tracker-fts.h"
#include "tracker-namespace.h"
#include "tracker-ontologies.h"
#include "tracker-ontology.h"
#include "tracker-property.h"
#include "tracker-data-query.h"
#include "tracker-sparql-parser.h"

#define RDF_PROPERTY                    TRACKER_PREFIX_RDF "Property"
#define RDF_TYPE                        TRACKER_PREFIX_RDF "type"

#define RDFS_CLASS                      TRACKER_PREFIX_RDFS "Class"
#define RDFS_DOMAIN                     TRACKER_PREFIX_RDFS "domain"
#define RDFS_RANGE                      TRACKER_PREFIX_RDFS "range"
#define RDFS_SUB_CLASS_OF               TRACKER_PREFIX_RDFS "subClassOf"
#define RDFS_SUB_PROPERTY_OF            TRACKER_PREFIX_RDFS "subPropertyOf"

#define NRL_INVERSE_FUNCTIONAL_PROPERTY TRACKER_PREFIX_NRL "InverseFunctionalProperty"
#define NRL_MAX_CARDINALITY             TRACKER_PREFIX_NRL "maxCardinality"

#define NRL_LAST_MODIFIED           TRACKER_PREFIX_NRL "lastModified"

struct _TrackerDataManager {
	GObject parent_instance;

	GFile *ontology_location;
	GFile *cache_location;
	guint initialized      : 1;
	guint flags;

	gint select_cache_size;
	gint update_cache_size;
	guint generation;

	TrackerDBManager *db_manager;
	TrackerOntologies *ontologies;
	TrackerData *data_update;

	GHashTable *transaction_graphs;
	GHashTable *graphs;
	GMutex graphs_lock;

	/* Cached remote connections */
	GMutex connections_lock;
	GHashTable *cached_connections;

	gchar *status;
};

struct _TrackerDataManagerClass {
	GObjectClass parent_instance;
};

typedef struct {
	const gchar *from;
	const gchar *to;
} Conversion;

static Conversion allowed_boolean_conversions[] = {
	{ "false", "true" },
	{ "true", "false" },
	{ NULL, NULL }
};

static Conversion allowed_cardinality_conversions[] = {
	{ "1", NULL },
	{ NULL, NULL }
};

static Conversion allowed_range_conversions[] = {
	{ TRACKER_PREFIX_XSD "integer", TRACKER_PREFIX_XSD "string" },
	{ TRACKER_PREFIX_XSD "integer", TRACKER_PREFIX_XSD "double" },
	{ TRACKER_PREFIX_XSD "integer", TRACKER_PREFIX_XSD "boolean" },

	{ TRACKER_PREFIX_XSD "string", TRACKER_PREFIX_XSD "integer" },
	{ TRACKER_PREFIX_XSD "string", TRACKER_PREFIX_XSD "double" },
	{ TRACKER_PREFIX_XSD "string", TRACKER_PREFIX_XSD "boolean" },

	{ TRACKER_PREFIX_XSD "double", TRACKER_PREFIX_XSD "integer" },
	{ TRACKER_PREFIX_XSD "double", TRACKER_PREFIX_XSD "string" },
	{ TRACKER_PREFIX_XSD "double", TRACKER_PREFIX_XSD "boolean" },

	{ NULL, NULL }
};

enum {
	PROP_0,
	PROP_STATUS,
	N_PROPS
};

static gboolean tracker_data_manager_fts_changed (TrackerDataManager *manager);
static gboolean tracker_data_manager_update_fts (TrackerDataManager  *manager,
                                                 TrackerDBInterface  *iface,
                                                 const gchar         *database,
                                                 GError             **error);

static void tracker_data_manager_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (TrackerDataManager, tracker_data_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, tracker_data_manager_initable_iface_init))

#define print_parsing_err(format, ...) g_printerr ("\nparsing-error: " format "\n", __VA_ARGS__)

static void
tracker_data_manager_init (TrackerDataManager *manager)
{
	manager->generation = 1;
	manager->cached_connections =
		g_hash_table_new_full (g_str_hash, g_str_equal,
		                       g_free, g_object_unref);
	g_mutex_init (&manager->connections_lock);
	g_mutex_init (&manager->graphs_lock);
}

GQuark
tracker_data_ontology_error_quark (void)
{
	return g_quark_from_static_string ("tracker-data-ontology-error-quark");
}

static gboolean
tracker_data_manager_initialize_graphs (TrackerDataManager  *manager,
                                        TrackerDBInterface  *iface,
                                        GError             **error)
{
	TrackerDBCursor *cursor = NULL;
	TrackerDBStatement *stmt;
	GHashTable *graphs;

	graphs = g_hash_table_new_full (g_str_hash,
					g_str_equal,
					g_free,
	                                (GDestroyNotify) tracker_rowid_free);

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
						      "SELECT ID, Uri FROM Resource WHERE ID IN (SELECT ID FROM Graph)");
	if (!stmt) {
		g_hash_table_unref (graphs);
		return FALSE;
	}

	cursor = tracker_db_statement_start_cursor (stmt, error);
	g_object_unref (stmt);

	if (!cursor) {
		g_hash_table_unref (graphs);
		return FALSE;
	}

	while (tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
		const gchar *name;
		TrackerRowid id;

		id = tracker_db_cursor_get_int (cursor, 0);
		name = tracker_db_cursor_get_string (cursor, 1, NULL);

		g_hash_table_insert (graphs, g_strdup (name),
		                     tracker_rowid_copy (&id));
	}

	g_object_unref (cursor);
	manager->graphs = graphs;
	return TRUE;
}

GHashTable *
tracker_data_manager_get_graphs (TrackerDataManager *manager,
                                 gboolean            in_transaction)
{
	GHashTable *ht;

	g_mutex_lock (&manager->graphs_lock);

	if (manager->transaction_graphs && in_transaction)
		ht = g_hash_table_ref (manager->transaction_graphs);
	else
		ht = g_hash_table_ref (manager->graphs);

	g_mutex_unlock (&manager->graphs_lock);

	return ht;
}

static void
handle_unsupported_ontology_change (TrackerDataManager  *manager,
                                    const gchar         *ontology_path,
                                    goffset              stmt_line_no,
                                    goffset              stmt_column_no,
                                    const gchar         *subject,
                                    const gchar         *change,
                                    const gchar         *old,
                                    const gchar         *attempted_new,
                                    GError             **error)
{
	gchar *stmt_location;

	if (ontology_path == NULL)
		stmt_location = g_strdup ("");
	else if (stmt_line_no == -1 || stmt_column_no == -1)
		stmt_location = g_strdup_printf ("%s: ", ontology_path);
	else
		stmt_location = g_strdup_printf ("%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT ": ",
		                                 ontology_path, stmt_line_no, stmt_column_no);

	g_set_error (error, TRACKER_DATA_ONTOLOGY_ERROR,
	             TRACKER_DATA_UNSUPPORTED_ONTOLOGY_CHANGE,
	             "%sUnsupported ontology change for %s: can't change %s (old=%s, attempted new=%s)",
	             stmt_location,
	             subject != NULL ? subject : "Unknown",
	             change != NULL ? change : "Unknown",
	             old != NULL ? old : "Unknown",
	             attempted_new != NULL ? attempted_new : "Unknown");
	g_free (stmt_location);
}

static void
set_secondary_index_for_single_value_property (TrackerDBInterface  *iface,
                                               const gchar         *database,
                                               TrackerClass        *class,
                                               TrackerProperty     *property,
                                               TrackerProperty     *secondary,
                                               gboolean             enabled,
                                               GError             **error)
{
	GError *internal_error = NULL;
	const gchar *class_name = tracker_class_get_name (class);
	const gchar *property_name = tracker_property_get_name (property);
	const gchar *secondary_name = tracker_property_get_name (secondary);

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Dropping secondary index (single-value property):  "
	                         "DROP INDEX IF EXISTS \"%s_%s\"",
	                         class_name, property_name));

	tracker_db_interface_execute_query (iface, &internal_error,
	                                    "DROP INDEX IF EXISTS \"%s\".\"%s_%s\"",
	                                    database,
	                                    class_name,
	                                    property_name);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return;
	}

	if (enabled) {
		TRACKER_NOTE (ONTOLOGY_CHANGES,
		              g_message ("Creating secondary index (single-value property): "
		                         "CREATE INDEX \"%s_%s\" ON \"%s\" (\"%s\", \"%s\")",
		                         class_name, property_name, class_name, property_name, secondary_name));

		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "CREATE INDEX \"%s\".\"%s_%s\" ON \"%s\" (\"%s\", \"%s\")",
		                                    database,
		                                    class_name,
		                                    property_name,
		                                    class_name,
		                                    property_name,
		                                    secondary_name);

		if (internal_error) {
			g_propagate_error (error, internal_error);
		}
	}
}

static void
set_index_for_single_value_property (TrackerDBInterface  *iface,
                                     const gchar         *database,
                                     TrackerClass        *class,
                                     TrackerProperty     *property,
                                     gboolean             enabled,
                                     GError             **error)
{
	GError *internal_error = NULL;
        const gchar *class_name = tracker_class_get_name (class);
        const gchar *property_name = tracker_property_get_name (property);

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Dropping index (single-value property): "
	                         "DROP INDEX IF EXISTS \"%s_%s\"",
	                         class_name, property_name));

	tracker_db_interface_execute_query (iface, &internal_error,
	                                    "DROP INDEX IF EXISTS \"%s\".\"%s_%s\"",
	                                    database,
	                                    class_name,
	                                    property_name);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return;
	}

	if (enabled) {
		gchar *expr;

		if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME)
			expr = g_strdup_printf ("SparqlTimeSort(\"%s\")", property_name);
		else
			expr = g_strdup_printf ("\"%s\"", property_name);

		TRACKER_NOTE (ONTOLOGY_CHANGES,
		              g_message ("Creating index (single-value property): "
		                         "CREATE INDEX \"%s_%s\" ON \"%s\" (%s)",
		                         class_name, property_name, class_name, expr));

		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "CREATE INDEX \"%s\".\"%s_%s\" ON \"%s\" (%s)",
		                                    database,
		                                    class_name,
		                                    property_name,
		                                    class_name,
		                                    expr);
		g_free (expr);

		if (internal_error) {
			g_propagate_error (error, internal_error);
		}
	}
}

static void
set_index_for_multi_value_property (TrackerDBInterface  *iface,
                                    const gchar         *database,
                                    TrackerClass        *class,
                                    TrackerProperty     *property,
                                    GError             **error)
{
	GError *internal_error = NULL;
	gchar *expr;
        const gchar *class_name = tracker_class_get_name (class);
        const gchar *property_name = tracker_property_get_name (property);

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Dropping index (multi-value property): "
	                         "DROP INDEX IF EXISTS \"%s_%s_ID_ID\"",
	                         class_name, property_name));

	tracker_db_interface_execute_query (iface, &internal_error,
	                                    "DROP INDEX IF EXISTS \"%s\".\"%s_%s_ID_ID\"",
	                                    database,
	                                    class_name,
	                                    property_name);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return;
	}

	/* Useful to have this here for the cases where we want to fully
	 * re-create the indexes even without an ontology change (when locale
	 * of the user changes) */
	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Dropping index (multi-value property): "
	                         "DROP INDEX IF EXISTS \"%s_%s_ID\"",
	                         class_name, property_name));
	tracker_db_interface_execute_query (iface, &internal_error,
	                                    "DROP INDEX IF EXISTS \"%s\".\"%s_%s_ID\"",
	                                    database,
	                                    class_name,
	                                    property_name);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return;
	}

	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME)
		expr = g_strdup_printf ("SparqlTimeSort(\"%s\")", property_name);
	else
		expr = g_strdup_printf ("\"%s\"", property_name);

	if (tracker_property_get_indexed (property)) {
                /* use different UNIQUE index for properties whose
                 * value should be indexed to minimize index size */

		TRACKER_NOTE (ONTOLOGY_CHANGES,
		              g_message ("Creating index (multi-value property): "
		                         "CREATE INDEX \"%s_%s_ID\" ON \"%s_%s\" (ID)",
		                         class_name, property_name, class_name, property_name));

		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "CREATE INDEX \"%s\".\"%s_%s_ID\" ON \"%s_%s\" (ID)",
		                                    database,
		                                    class_name,
		                                    property_name,
		                                    class_name,
		                                    property_name);

		if (internal_error)
			goto out;

		TRACKER_NOTE (ONTOLOGY_CHANGES,
		              g_message ("Creating index (multi-value property): "
		                        "CREATE UNIQUE INDEX \"%s_%s_ID_ID\" ON \"%s_%s\" (%s, ID)",
		                        class_name, property_name, class_name, property_name, expr));

		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "CREATE UNIQUE INDEX \"%s\".\"%s_%s_ID_ID\" ON \"%s_%s\" (%s, ID)",
		                                    database,
		                                    class_name,
		                                    property_name,
		                                    class_name,
		                                    property_name,
		                                    expr);

		if (internal_error)
			goto out;
	} else {
                /* we still have to include the property value in
                 * the unique index for proper constraints */

		TRACKER_NOTE (ONTOLOGY_CHANGES,
		              g_message ("Creating index (multi-value property): "
		                         "CREATE UNIQUE INDEX \"%s_%s_ID_ID\" ON \"%s_%s\" (ID, %s)",
		                         class_name, property_name, class_name, property_name, expr));

		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "CREATE UNIQUE INDEX \"%s\".\"%s_%s_ID_ID\" ON \"%s_%s\" (ID, %s)",
		                                    database,
		                                    class_name,
		                                    property_name,
		                                    class_name,
		                                    property_name,
		                                    expr);

		if (internal_error)
			goto out;
	}

out:
	if (internal_error) {
		g_propagate_error (error, internal_error);
	}

	g_free (expr);
}

static gboolean
is_allowed_conversion (const gchar *oldv,
                       const gchar *newv,
                       Conversion   allowed[])
{
	guint i;

	for (i = 0; allowed[i].from != NULL; i++) {
		if (g_strcmp0 (allowed[i].from, oldv) == 0) {
			if (g_strcmp0 (allowed[i].to, newv) == 0) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

static gboolean
update_property_value (TrackerDataManager  *manager,
                       const gchar         *kind,
                       const gchar         *subject,
                       const gchar         *predicate,
                       const gchar         *object,
                       Conversion           allowed[],
                       TrackerClass        *class,
                       TrackerProperty     *property,
                       GError             **error_in)
{
	TrackerOntologies *ontologies;
	TrackerProperty *pred;
	GError *error = NULL;
	gboolean needed = TRUE;
	gboolean is_new = FALSE;
	const gchar *ontology_path = NULL;
	goffset line_no = -1;
	goffset column_no = -1;

	if (class) {
		is_new = tracker_class_get_is_new (class);
		ontology_path = tracker_class_get_ontology_path (class);
		line_no = tracker_class_get_definition_line_no (class);
		column_no = tracker_class_get_definition_column_no (class);
	} else if (property) {
		is_new = tracker_property_get_is_new (property);
		ontology_path = tracker_property_get_ontology_path (property);
		line_no = tracker_property_get_definition_line_no (property);
		column_no = tracker_property_get_definition_column_no (property);
	}

	ontologies = tracker_data_manager_get_ontologies (manager);
	pred = tracker_ontologies_get_property_by_uri (ontologies, predicate);
	if (!pred) {
		g_set_error (error_in, TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_UNKNOWN_PROPERTY,
		             "Property '%s' not found in the ontology",
		             predicate);
		return FALSE;
	}

	if (!is_new) {
		gchar *query = NULL;
		TrackerDBCursor *cursor;

		query = g_strdup_printf ("SELECT ?old_value WHERE { "
		                         "<%s> %s ?old_value "
		                         "}", subject, kind);

		cursor = tracker_data_query_sparql_cursor (manager, query, &error);

		if (cursor && tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
			const gchar *str = NULL;

			str = tracker_db_cursor_get_string (cursor, 0, NULL);

			if (g_strcmp0 (object, str) == 0) {
				needed = FALSE;
			} else {
				gboolean unsup_onto_err = FALSE;

				if (allowed && !is_allowed_conversion (str, object, allowed)) {
					handle_unsupported_ontology_change (manager,
					                                    ontology_path,
					                                    line_no,
					                                    column_no,
					                                    subject,
					                                    kind,
					                                    str,
					                                    object,
					                                    error_in);
					needed = FALSE;
					unsup_onto_err = TRUE;
				}

				if (!unsup_onto_err) {
					GValue value = G_VALUE_INIT;
					TrackerRowid subject_id = 0;

					tracker_data_query_string_to_value (manager,
					                                    str, NULL,
					                                    tracker_property_get_data_type (pred),
					                                    &value, &error);

					if (!error)
						subject_id = tracker_data_update_ensure_resource (manager->data_update,
						                                                  subject,
						                                                  &error);

					if (!error)
						tracker_data_delete_statement (manager->data_update, NULL, subject_id, pred, &value, &error);
					g_value_unset (&value);

					if (!error)
						tracker_data_update_buffer_flush (manager->data_update, &error);
				}
			}

		} else {
			if (object && (g_strcmp0 (object, "false") == 0)) {
				needed = FALSE;
			} else {
				needed = (object != NULL);
			}
		}
		g_free (query);
		if (cursor) {
			g_object_unref (cursor);
		}
	} else {
		needed = FALSE;
	}


	if (!error && needed && object) {
		GValue value = G_VALUE_INIT;
		TrackerRowid subject_id = 0;

		tracker_data_query_string_to_value (manager,
		                                    object, NULL,
		                                    tracker_property_get_data_type (pred),
		                                    &value, &error);

		if (!error)
			subject_id = tracker_data_update_ensure_resource (manager->data_update,
			                                                  subject,
			                                                  &error);

		if (!error)
			tracker_data_insert_statement (manager->data_update, NULL, subject_id,
			                               pred, &value,
			                               &error);
		g_value_unset (&value);

		if (!error)
			tracker_data_update_buffer_flush (manager->data_update, &error);
	}

	if (error) {
		g_critical ("Ontology change, %s", error->message);
		g_clear_error (&error);
	}

	return needed;
}

static void
check_range_conversion_is_allowed (TrackerDataManager  *manager,
                                   const gchar         *subject,
                                   const gchar         *predicate,
                                   const gchar         *object,
                                   GError             **error)
{
	TrackerDBCursor *cursor;
	gchar *query;

	query = g_strdup_printf ("SELECT ?old_value WHERE { "
	                         "<%s> rdfs:range ?old_value "
	                         "}", subject);

	cursor = tracker_data_query_sparql_cursor (manager, query, NULL);

	g_free (query);

	if (cursor && tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
		const gchar *str;

		str = tracker_db_cursor_get_string (cursor, 0, NULL);

		if (g_strcmp0 (object, str) != 0) {
			if (!is_allowed_conversion (str, object, allowed_range_conversions)) {
				handle_unsupported_ontology_change (manager,
				                                    NULL,
				                                    -1,
				                                    -1,
				                                    subject,
				                                    "rdfs:range",
				                                    str,
				                                    object,
				                                    error);
			}
		}
	}

	if (cursor) {
		g_object_unref (cursor);
	}
}

static void
check_max_cardinality_change_is_allowed (TrackerDataManager  *manager,
                                         const gchar         *subject,
                                         const gchar         *predicate,
                                         const gchar         *object,
                                         GError             **error)
{
	TrackerDBCursor *cursor;
	gchar *query;
	gint new_max_cardinality = atoi (object);
	gboolean unsupported = FALSE;

	query = g_strdup_printf ("SELECT ?old_value WHERE { "
	                         "<%s> nrl:maxCardinality ?old_value "
	                         "}", subject);

	cursor = tracker_data_query_sparql_cursor (manager, query, NULL);

	g_free (query);

	if (cursor && tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
		const gchar *orig_object = tracker_db_cursor_get_string (cursor, 0, NULL);
		gint orig_max_cardinality;

		orig_max_cardinality = atoi (orig_object);

		// If nrl:maxCardinality != 1, then the property support multiple values
		if (new_max_cardinality == 1 && orig_max_cardinality != 1)
			unsupported = TRUE;

		g_object_unref (cursor);
	} else {
		/* nrl:maxCardinality not set
		   by default it allows multiple values */
		if (new_max_cardinality == 1)
			unsupported = TRUE;
	}

	if (unsupported) {
		handle_unsupported_ontology_change (manager,
		                                    NULL,
		                                    -1,
		                                    -1,
		                                    subject,
		                                    "nrl:maxCardinality",
		                                    "N",
		                                    object,
		                                    error);
	}
}

static void
ensure_inverse_functional_property (TrackerDataManager  *manager,
                                    const gchar         *property_uri,
                                    GError             **error)
{
	TrackerDBCursor *cursor;
	gchar *query;

	query = g_strdup_printf ("ASK { <%s> a nrl:InverseFunctionalProperty }", property_uri);
	cursor = tracker_data_query_sparql_cursor (manager, query, error);
	g_free (query);

	if (cursor && tracker_db_cursor_iter_next (cursor, NULL, error)) {
		if (!tracker_sparql_cursor_get_boolean (TRACKER_SPARQL_CURSOR (cursor), 0)) {
			handle_unsupported_ontology_change (manager,
			                                    NULL, -1, -1,
			                                    property_uri,
			                                    "nrl:InverseFunctionalProperty", "-", "-",
			                                    error);
		}
	}

	g_clear_object (&cursor);
}

static void
fix_indexed_on_db (TrackerDataManager  *manager,
                   const gchar         *database,
                   TrackerProperty     *property,
                   GError             **error)
{
	GError *internal_error = NULL;
	TrackerDBInterface *iface;
	TrackerClass *class;

	iface = tracker_db_manager_get_writable_db_interface (manager->db_manager);
	class = tracker_property_get_domain (property);

	if (tracker_property_get_multiple_values (property)) {
		set_index_for_multi_value_property (iface, database, class, property, &internal_error);
	} else {
		TrackerProperty *secondary_index;
		TrackerClass **domain_index_classes;

		secondary_index = tracker_property_get_secondary_index (property);
		if (secondary_index == NULL) {
			set_index_for_single_value_property (iface, database, class, property,
			                                     tracker_property_get_indexed (property),
			                                     &internal_error);
		} else {
			set_secondary_index_for_single_value_property (iface, database, class, property,
			                                               secondary_index,
			                                               tracker_property_get_indexed (property),
			                                               &internal_error);
		}

		/* single-valued properties may also have domain-specific indexes */
		domain_index_classes = tracker_property_get_domain_indexes (property);
		while (!internal_error && domain_index_classes && *domain_index_classes) {
			set_index_for_single_value_property (iface,
			                                     database,
			                                     *domain_index_classes,
			                                     property,
			                                     TRUE,
			                                     &internal_error);
			domain_index_classes++;
		}
	}

	if (internal_error) {
		g_propagate_error (error, internal_error);
	}
}

static void
fix_indexed (TrackerDataManager  *manager,
             TrackerProperty     *property,
             GError             **error)
{
	GHashTable *graphs;
	GHashTableIter iter;
	GError *internal_error = NULL;
	gpointer value;

	graphs = tracker_data_manager_get_graphs (manager, FALSE);
	g_hash_table_iter_init (&iter, graphs);

	while (g_hash_table_iter_next (&iter, &value, NULL)) {
		fix_indexed_on_db (manager, value, property, &internal_error);
		if (internal_error) {
			g_propagate_error (error, internal_error);
			break;
		}
	}

	g_hash_table_unref (graphs);
}

static void
tracker_data_ontology_load_statement (TrackerDataManager  *manager,
                                      const gchar         *ontology_path,
                                      const gchar         *subject,
                                      const gchar         *predicate,
                                      const gchar         *object,
                                      goffset              object_line_no,
                                      goffset              object_column_no,
                                      gboolean             in_update,
                                      gboolean            *loaded_successfully,
                                      GPtrArray           *seen_classes,
                                      GPtrArray           *seen_properties,
                                      GError             **error)
{
	gchar *object_location = g_strdup_printf("%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT,
	                                         ontology_path, object_line_no, object_column_no);

	*loaded_successfully = TRUE;

	if (g_strcmp0 (predicate, RDF_TYPE) == 0) {
		if (g_strcmp0 (object, RDFS_CLASS) == 0) {
			TrackerClass *class;
			TrackerRowid subject_id;

			class = tracker_ontologies_get_class_by_uri (manager->ontologies, subject);

			if (class != NULL) {
				if (seen_classes)
					g_ptr_array_add (seen_classes, g_object_ref (class));
				if (!in_update) {
					print_parsing_err ("%s: Duplicate definition of class %s", object_location, subject);
				} else {
					/* Reset for a correct post-check */
					tracker_class_reset_domain_indexes (class);
					tracker_class_reset_super_classes (class);
					tracker_class_set_notify (class, FALSE);
					tracker_class_set_ontology_path (class, ontology_path);
					tracker_class_set_definition_line_no (class, object_line_no);
					tracker_class_set_definition_column_no (class, object_column_no);
				}
				goto out;
			}

			subject_id = tracker_data_update_ensure_resource (manager->data_update,
			                                                  subject, error);
			if (!subject_id) {
				g_prefix_error (error, "%s:", object_location);
				goto fail;
			}

			class = tracker_class_new (FALSE);
			tracker_class_set_ontologies (class, manager->ontologies);
			tracker_class_set_is_new (class, in_update);
			tracker_class_set_uri (class, subject);
			tracker_class_set_id (class, subject_id);
			tracker_class_set_ontology_path (class, ontology_path);
			tracker_class_set_definition_line_no (class, object_line_no);
			tracker_class_set_definition_column_no (class, object_column_no);
			tracker_ontologies_add_class (manager->ontologies, class);
			tracker_ontologies_add_id_uri_pair (manager->ontologies, subject_id, subject);

			if (seen_classes)
				g_ptr_array_add (seen_classes, g_object_ref (class));

			g_object_unref (class);
		} else if (g_strcmp0 (object, RDF_PROPERTY) == 0) {
			TrackerProperty *property;
			TrackerRowid subject_id;

			property = tracker_ontologies_get_property_by_uri (manager->ontologies, subject);
			if (property != NULL) {
				if (seen_properties)
					g_ptr_array_add (seen_properties, g_object_ref (property));
				if (!in_update) {
					print_parsing_err ("%s: Duplicate definition of property %s", object_location, subject);
				} else {
					/* Reset for a correct post and pre-check */
					tracker_property_set_last_multiple_values (property, TRUE);
					tracker_property_reset_domain_indexes (property);
					tracker_property_reset_super_properties (property);
					tracker_property_set_indexed (property, FALSE);
					tracker_property_set_cardinality_changed (property, FALSE);
					tracker_property_set_secondary_index (property, NULL);
					tracker_property_set_is_inverse_functional_property (property, FALSE);
					tracker_property_set_multiple_values (property, TRUE);
					tracker_property_set_fulltext_indexed (property, FALSE);
					tracker_property_set_ontology_path (property, ontology_path);
					tracker_property_set_definition_line_no (property, object_line_no);
					tracker_property_set_definition_column_no (property, object_column_no);
				}
				goto out;
			}

			subject_id = tracker_data_update_ensure_resource (manager->data_update,
			                                                  subject,
			                                                  error);
			if (!subject_id) {
				g_prefix_error (error, "%s:", object_location);
				goto fail;
			}

			property = tracker_property_new (FALSE);
			tracker_property_set_ontologies (property, manager->ontologies);
			tracker_property_set_is_new (property, in_update);
			tracker_property_set_uri (property, subject);
			tracker_property_set_id (property, subject_id);
			tracker_property_set_multiple_values (property, TRUE);
			tracker_property_set_ontology_path (property, ontology_path);
			tracker_property_set_definition_line_no (property, object_line_no);
			tracker_property_set_definition_column_no (property, object_column_no);
			tracker_ontologies_add_property (manager->ontologies, property);
			tracker_ontologies_add_id_uri_pair (manager->ontologies, subject_id, subject);

			if (seen_properties)
				g_ptr_array_add (seen_properties, g_object_ref (property));

			g_object_unref (property);
		} else if (g_strcmp0 (object, NRL_INVERSE_FUNCTIONAL_PROPERTY) == 0) {
			TrackerProperty *property;

			property = tracker_ontologies_get_property_by_uri (manager->ontologies, subject);
			if (property == NULL) {
				print_parsing_err ("%s: Unknown property %s", object_location, subject);
				goto fail;
			}

			if (in_update) {
				GError *err = NULL;

				if (tracker_property_get_is_new (property)) {
					const gchar* property_uri = tracker_property_get_uri (property);

					g_set_error (&err,
					             TRACKER_SPARQL_ERROR,
					             TRACKER_SPARQL_ERROR_UNSUPPORTED,
					             "Unsupported adding the new inverse functional property: %s",
					             property_uri);
				} else {
					ensure_inverse_functional_property (manager, subject, &err);
				}

				if (err) {
					g_propagate_prefixed_error (error, err, "%s: ", object_location);
					goto fail;
				}
			}

			tracker_property_set_is_inverse_functional_property (property, TRUE);
		} else if (g_strcmp0 (object, TRACKER_PREFIX_NRL "Namespace") == 0) {
			TrackerNamespace *namespace;

			if (tracker_ontologies_get_namespace_by_uri (manager->ontologies, subject) != NULL) {
				if (!in_update)
					print_parsing_err ("%s: Duplicate definition of namespace %s", object_location, subject);
				goto out;
			}

			namespace = tracker_namespace_new (FALSE);
			tracker_namespace_set_ontologies (namespace, manager->ontologies);
			tracker_namespace_set_is_new (namespace, in_update);
			tracker_namespace_set_uri (namespace, subject);
			tracker_ontologies_add_namespace (manager->ontologies, namespace);
			g_object_unref (namespace);

		} else if (g_strcmp0 (object, TRACKER_PREFIX_NRL "Ontology") == 0) {
			TrackerOntology *ontology;

			if (tracker_ontologies_get_ontology_by_uri (manager->ontologies, subject) != NULL) {
				if (!in_update)
					print_parsing_err ("%s: Duplicate definition of ontology %s", object_location, subject);
				goto out;
			}

			ontology = tracker_ontology_new ();
			tracker_ontology_set_ontologies (ontology, manager->ontologies);
			tracker_ontology_set_is_new (ontology, in_update);
			tracker_ontology_set_uri (ontology, subject);
			tracker_ontologies_add_ontology (manager->ontologies, ontology);
			g_object_unref (ontology);

		}
	} else if (g_strcmp0 (predicate, RDFS_SUB_CLASS_OF) == 0) {
		TrackerClass *class, *super_class;
		gboolean is_new;

		class = tracker_ontologies_get_class_by_uri (manager->ontologies, subject);
		if (class == NULL) {
			print_parsing_err ("%s: Unknown class %s", object_location, subject);
			goto fail;
		}

		is_new = tracker_class_get_is_new (class);
		if (is_new != in_update) {
			gboolean ignore = FALSE;
			/* Detect unsupported ontology change */
			if (in_update == TRUE && is_new == FALSE && g_strcmp0 (object, TRACKER_PREFIX_RDFS "Resource") != 0) {
				TrackerClass **super_classes = tracker_class_get_super_classes (class);
				gboolean had = FALSE;

				super_class = tracker_ontologies_get_class_by_uri (manager->ontologies, object);
				if (super_class == NULL) {
					print_parsing_err ("%s: Unknown class %s", object_location, object);
					goto fail;
				}

				while (*super_classes) {
					if (*super_classes == super_class) {
						ignore = TRUE;
						TRACKER_NOTE (ONTOLOGY_CHANGES,
						              g_message ("%s: Class %s already has rdfs:subClassOf in %s",
						                         object_location, object, subject));
						break;
					}
					super_classes++;
				}

				super_classes = tracker_class_get_last_super_classes (class);
				if (super_classes) {
					while (*super_classes) {
						if (super_class == *super_classes) {
							had = TRUE;
						}
						super_classes++;
					}
				}

				/* This doesn't detect removed rdfs:subClassOf situations, it
				 * only checks whether no new ones are being added. For
				 * detecting the removal of a rdfs:subClassOf, please check the
				 * tracker_data_ontology_process_changes_pre_db stuff */


				if (!ignore && !had) {
					handle_unsupported_ontology_change (manager,
					                                    ontology_path,
					                                    object_line_no,
					                                    object_column_no,
					                                    tracker_class_get_name (class),
					                                    "rdfs:subClassOf",
					                                    "-",
					                                    tracker_class_get_name (super_class),
					                                    error);
				}
			}

			if (!ignore) {
				super_class = tracker_ontologies_get_class_by_uri (manager->ontologies, object);
				tracker_class_add_super_class (class, super_class);
			}

			if (*error)
				goto fail;

			goto out;
		}

		super_class = tracker_ontologies_get_class_by_uri (manager->ontologies, object);
		if (super_class == NULL) {
			print_parsing_err ("%s: Unknown class %s", object_location, object);
			goto fail;
		}

		tracker_class_add_super_class (class, super_class);

	} else if (g_strcmp0 (predicate, TRACKER_PREFIX_NRL "notify") == 0) {
		TrackerClass *class;

		class = tracker_ontologies_get_class_by_uri (manager->ontologies, subject);

		if (class == NULL) {
			print_parsing_err ("%s: Unknown class %s", object_location, subject);
			goto fail;
		}

		tracker_class_set_notify (class, (strcmp (object, "true") == 0));
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX_NRL "domainIndex") == 0) {
		TrackerClass *class;
		TrackerProperty *property;
		TrackerProperty **properties;
		gboolean ignore = FALSE;
		gboolean had = FALSE;
		guint n_props, i;

		class = tracker_ontologies_get_class_by_uri (manager->ontologies, subject);

		if (class == NULL) {
			print_parsing_err ("%s: Unknown class %s", object_location, subject);
			goto fail;
		}

		property = tracker_ontologies_get_property_by_uri (manager->ontologies, object);

		if (property == NULL) {
			print_parsing_err ("%s: Unknown property %s for nrl:domainIndex in %s.",
			                   object_location, object, subject);
			goto fail;
		}

		if (tracker_property_get_multiple_values (property)) {
			print_parsing_err ("%s: Property %s has multiple values while trying to add it as nrl:domainIndex in %s, this isn't supported",
			                   object_location, object, subject);
			goto fail;
		}

		properties = tracker_ontologies_get_properties (manager->ontologies, &n_props);
		for (i = 0; i < n_props; i++) {
			if (tracker_property_get_domain (properties[i]) == class &&
			    properties[i] == property) {
				print_parsing_err ("%s: Property %s is already a first-class property of %s while trying to add it as nrl:domainIndex",
				                   object_location, object, subject);
			}
		}

		properties = tracker_class_get_domain_indexes (class);
		while (*properties) {
			if (property == *properties) {
				TRACKER_NOTE (ONTOLOGY_CHANGES,
				              g_message ("%s: Property %s already a nrl:domainIndex in %s",
				                         object_location, object, subject));
				ignore = TRUE;
			}
			properties++;
		}

		properties = tracker_class_get_last_domain_indexes (class);
		if (properties) {
			while (*properties) {
				if (property == *properties) {
					had = TRUE;
				}
				properties++;
			}
		}

		/* This doesn't detect removed nrl:domainIndex situations, it
		 * only checks whether no new ones are being added. For
		 * detecting the removal of a nrl:domainIndex, please check the
		 * tracker_data_ontology_process_changes_pre_db stuff */

		if (!ignore) {
			if (!had) {
				tracker_property_set_is_new_domain_index (property, class, in_update);
			}
			tracker_class_add_domain_index (class, property);
			tracker_property_add_domain_index (property, class);
		}

	} else if (g_strcmp0 (predicate, RDFS_SUB_PROPERTY_OF) == 0) {
		TrackerProperty *property, *super_property;
		gboolean is_new;

		property = tracker_ontologies_get_property_by_uri (manager->ontologies, subject);
		if (property == NULL) {
			print_parsing_err ("%s: Unknown property %s", object_location, subject);
			goto fail;
		}

		is_new = tracker_property_get_is_new (property);
		if (is_new != in_update) {
			gboolean ignore = FALSE;
			/* Detect unsupported ontology change */
			if (in_update == TRUE && is_new == FALSE) {
				TrackerProperty **super_properties = tracker_property_get_super_properties (property);
				gboolean had = FALSE;

				super_property = tracker_ontologies_get_property_by_uri (manager->ontologies, object);
				if (super_property == NULL) {
					print_parsing_err ("%s: Unknown property %s", object_location, object);
					goto fail;
				}

				while (*super_properties) {
					if (*super_properties == super_property) {
						ignore = TRUE;
						TRACKER_NOTE (ONTOLOGY_CHANGES,
						              g_message ("%s: Property %s already has rdfs:subPropertyOf in %s",
						                         object_location, object, subject));
						break;
					}
					super_properties++;
				}

				super_properties = tracker_property_get_last_super_properties (property);
				if (super_properties) {
					while (*super_properties) {
						if (super_property == *super_properties) {
							had = TRUE;
						}
						super_properties++;
					}
				}

				/* This doesn't detect removed rdfs:subPropertyOf situations, it
				 * only checks whether no new ones are being added. For
				 * detecting the removal of a rdfs:subPropertyOf, please check the
				 * tracker_data_ontology_process_changes_pre_db stuff */

				if (!ignore && !had) {
					handle_unsupported_ontology_change (manager,
					                                    ontology_path,
					                                    object_line_no,
					                                    object_column_no,
					                                    tracker_property_get_name (property),
					                                    "rdfs:subPropertyOf",
					                                    "-",
					                                    tracker_property_get_name (super_property),
					                                    error);
				}
			}

			if (!ignore) {
				super_property = tracker_ontologies_get_property_by_uri (manager->ontologies, object);
				tracker_property_add_super_property (property, super_property);
			}

			if (*error)
				goto fail;

			goto out;
		}

		super_property = tracker_ontologies_get_property_by_uri (manager->ontologies, object);
		if (super_property == NULL) {
			print_parsing_err ("%s: Unknown property %s", object_location, object);
			goto fail;
		}

		tracker_property_add_super_property (property, super_property);
	} else if (g_strcmp0 (predicate, RDFS_DOMAIN) == 0) {
		TrackerProperty *property;
		TrackerClass *domain;
		gboolean is_new;

		property = tracker_ontologies_get_property_by_uri (manager->ontologies, subject);
		if (property == NULL) {
			print_parsing_err ("%s: Unknown property %s", object_location, subject);
			goto fail;
		}

		domain = tracker_ontologies_get_class_by_uri (manager->ontologies, object);
		if (domain == NULL) {
			print_parsing_err ("%s: Unknown class %s", object_location, object);
			goto fail;
		}

		is_new = tracker_property_get_is_new (property);
		if (is_new != in_update) {
			/* Detect unsupported ontology change */
			if (in_update == TRUE && is_new == FALSE) {
				TrackerClass *old_domain = tracker_property_get_domain (property);
				if (old_domain != domain) {
					handle_unsupported_ontology_change (manager,
					                                    ontology_path,
					                                    object_line_no,
					                                    object_column_no,
					                                    tracker_property_get_name (property),
					                                    "rdfs:domain",
					                                    tracker_class_get_name (old_domain),
					                                    tracker_class_get_name (domain),
					                                    error);
				}
			}

			if (*error)
				goto fail;

			goto out;
		}

		tracker_property_set_domain (property, domain);
	} else if (g_strcmp0 (predicate, RDFS_RANGE) == 0) {
		TrackerProperty *property;
		TrackerClass *range;

		property = tracker_ontologies_get_property_by_uri (manager->ontologies, subject);
		if (property == NULL) {
			print_parsing_err ("%s: Unknown property %s", object_location, subject);
			goto fail;
		}

		if (tracker_property_get_is_new (property) != in_update) {
			GError *err = NULL;
			check_range_conversion_is_allowed (manager,
			                                   subject,
			                                   predicate,
			                                   object,
			                                   &err);
			if (err) {
				g_propagate_prefixed_error (error, err, "%s: ", object_location);
				goto fail;
			}
		}

		range = tracker_ontologies_get_class_by_uri (manager->ontologies, object);
		if (range == NULL) {
			print_parsing_err ("%s: Unknown class %s", object_location, object);
			goto fail;
		}

		tracker_property_set_range (property, range);
	} else if (g_strcmp0 (predicate, NRL_MAX_CARDINALITY) == 0) {
		TrackerProperty *property;

		property = tracker_ontologies_get_property_by_uri (manager->ontologies, subject);
		if (property == NULL) {
			print_parsing_err ("%s: Unknown property %s", object_location, subject);
			goto fail;
		}

		if (atoi (object) == 0) {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_TYPE,
			             "Property nrl:maxCardinality only accepts integers greater than 0");
			goto fail;
		}

		if (tracker_property_get_is_new (property) != in_update) {
			GError *err = NULL;
			check_max_cardinality_change_is_allowed (manager,
			                                         subject,
			                                         predicate,
			                                         object,
			                                         &err);
			if (err) {
				g_propagate_prefixed_error (error, err, "%s: ", object_location);
				goto fail;
			}
		}

		if (atoi (object) == 1) {
			tracker_property_set_multiple_values (property, FALSE);
			tracker_property_set_last_multiple_values (property, FALSE);
		} else {
			tracker_property_set_multiple_values (property, TRUE);
			tracker_property_set_last_multiple_values (property, TRUE);
		}

	} else if (g_strcmp0 (predicate, TRACKER_PREFIX_NRL "indexed") == 0) {
		TrackerProperty *property;

		property = tracker_ontologies_get_property_by_uri (manager->ontologies, subject);
		if (property == NULL) {
			print_parsing_err ("%s: Unknown property %s", object_location, subject);
			goto fail;
		}

		tracker_property_set_indexed (property, (strcmp (object, "true") == 0));
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX_NRL "secondaryIndex") == 0) {
		TrackerProperty *property, *secondary_index;

		property = tracker_ontologies_get_property_by_uri (manager->ontologies, subject);
		if (property == NULL) {
			print_parsing_err ("%s: Unknown property %s", object_location, subject);
			goto fail;
		}

		secondary_index = tracker_ontologies_get_property_by_uri (manager->ontologies, object);
		if (secondary_index == NULL) {
			print_parsing_err ("%s: Unknown property %s", object_location, object);
			goto fail;
		}

		if (!tracker_property_get_indexed (property)) {
			print_parsing_err ("%s: nrl:secondaryindex only applies to nrl:indexed properties", object_location);
			goto fail;
		}

		if (tracker_property_get_multiple_values (property) ||
		    tracker_property_get_multiple_values (secondary_index)) {
			print_parsing_err ("%s: nrl:secondaryindex cannot be applied to properties with nrl:maxCardinality higher than one", object_location);
			goto fail;
		}

		tracker_property_set_secondary_index (property, secondary_index);
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX_NRL "fulltextIndexed") == 0) {
		TrackerProperty *property;

		property = tracker_ontologies_get_property_by_uri (manager->ontologies, subject);
		if (property == NULL) {
			print_parsing_err ("%s: Unknown property %s", object_location, subject);
			goto fail;
		}

		tracker_property_set_fulltext_indexed (property,
		                                       strcmp (object, "true") == 0);
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX_NRL "prefix") == 0) {
		TrackerNamespace *namespace;

		namespace = tracker_ontologies_get_namespace_by_uri (manager->ontologies, subject);
		if (namespace == NULL) {
			print_parsing_err ("%s: Unknown namespace %s", object_location, subject);
			goto fail;
		}

		if (tracker_namespace_get_is_new (namespace) != in_update) {
			goto out;
		}

		tracker_namespace_set_prefix (namespace, object);
	} else if (g_strcmp0 (predicate, NRL_LAST_MODIFIED) == 0) {
		TrackerOntology *ontology;
		GDateTime *datetime;
		GError *error = NULL;

		ontology = tracker_ontologies_get_ontology_by_uri (manager->ontologies, subject);
		if (ontology == NULL) {
			print_parsing_err ("%s: Unknown ontology %s", object_location, subject);
			goto fail;
		}

		if (tracker_ontology_get_is_new (ontology) != in_update) {
			goto out;
		}

		datetime = tracker_date_new_from_iso8601 (object, &error);
		if (!datetime) {
			print_parsing_err ("%s: error parsing nrl:lastModified: %s",
			                   object_location, error->message);
			g_error_free (error);
			goto fail;
		}

		tracker_ontology_set_last_modified (ontology, g_date_time_to_unix (datetime));
		g_date_time_unref (datetime);
	}

out:
	g_free(object_location);
	return;

fail:
	g_free(object_location);
	*loaded_successfully = FALSE;
}


static void
check_for_deleted_domain_index (TrackerDataManager *manager,
                                TrackerClass       *class,
                                GError            **error)
{
	TrackerProperty **last_domain_indexes, *property;
	TrackerOntologies *ontologies;
	GSList *hfound = NULL, *deleted = NULL;

	last_domain_indexes = tracker_class_get_last_domain_indexes (class);

	if (!last_domain_indexes) {
		return;
	}

	ontologies = tracker_data_manager_get_ontologies (manager);
	property = tracker_ontologies_get_property_by_uri (ontologies,
	                                                   TRACKER_PREFIX_NRL "domainIndex");
	g_assert (property);

	while (*last_domain_indexes) {
		TrackerProperty *last_domain_index = *last_domain_indexes;
		gboolean found = FALSE;
		TrackerProperty **domain_indexes;

		domain_indexes = tracker_class_get_domain_indexes (class);

		while (*domain_indexes) {
			TrackerProperty *domain_index = *domain_indexes;
			if (last_domain_index == domain_index) {
				found = TRUE;
				hfound = g_slist_prepend (hfound, domain_index);
				break;
			}
			domain_indexes++;
		}

		if (!found) {
			deleted = g_slist_prepend (deleted, last_domain_index);
		}

		last_domain_indexes++;
	}


	if (deleted) {
		GSList *l;
		TrackerProperty **properties;
		guint n_props, i;

		tracker_class_set_db_schema_changed (class, TRUE);

		properties = tracker_ontologies_get_properties (manager->ontologies, &n_props);
		for (i = 0; i < n_props; i++) {
			if (tracker_property_get_domain (properties[i]) == class &&
			    !tracker_property_get_multiple_values (properties[i])) {

				/* These aren't domain-indexes, but it's just a flag for the
				 * functionality that'll recreate the table to know that the
				 * property must be involved in the recreation and copy */

				tracker_property_set_is_new_domain_index (properties[i], class, TRUE);
			}
		}

		for (l = hfound; l != NULL; l = l->next) {
			TrackerProperty *prop = l->data;
			TRACKER_NOTE (ONTOLOGY_CHANGES,
			              g_message ("Ontology change: keeping nrl:domainIndex: %s",
			                         tracker_property_get_name (prop)));
			tracker_property_set_is_new_domain_index (prop, class, TRUE);
		}

		for (l = deleted; l != NULL; l = l->next) {
			TrackerProperty *prop = l->data;
			const gchar *uri;
			GValue value = G_VALUE_INIT;
			TrackerRowid class_id = 0;

			TRACKER_NOTE (ONTOLOGY_CHANGES,
			              g_message ("Ontology change: deleting nrl:domainIndex: %s",
			                         tracker_property_get_name (prop)));
			tracker_property_del_domain_index (prop, class);
			tracker_class_del_domain_index (class, prop);

			uri = tracker_property_get_uri (prop);

			if (tracker_data_query_string_to_value (manager,
			                                        uri, NULL,
			                                        tracker_property_get_data_type (property),
			                                        &value, error)) {
				class_id = tracker_data_update_ensure_resource (manager->data_update,
				                                                tracker_class_get_uri (class),
				                                                error);
			}

			if (class_id != 0) {
				tracker_data_delete_statement (manager->data_update, NULL,
				                               class_id,
				                               property,
				                               &value,
				                               error);
			}

			g_value_unset (&value);

			if (!(*error))
				tracker_data_update_buffer_flush (manager->data_update, error);
		}

		g_slist_free (deleted);
	}

	g_slist_free (hfound);
}

static void
check_for_deleted_super_classes (TrackerDataManager  *manager,
                                 TrackerClass        *class,
                                 GError             **error)
{
	TrackerClass **last_super_classes;

	last_super_classes = tracker_class_get_last_super_classes (class);

	if (!last_super_classes) {
		return;
	}

	while (*last_super_classes) {
		TrackerClass *last_super_class = *last_super_classes;
		gboolean found = FALSE;
		TrackerClass **super_classes;

		if (g_strcmp0 (tracker_class_get_uri (last_super_class), TRACKER_PREFIX_RDFS "Resource") == 0) {
			last_super_classes++;
			continue;
		}

		super_classes = tracker_class_get_super_classes (class);

		while (*super_classes) {
			TrackerClass *super_class = *super_classes;

			if (last_super_class == super_class) {
				found = TRUE;
				break;
			}
			super_classes++;
		}

		if (!found) {
			const gchar *ontology_path = tracker_class_get_ontology_path (class);
			goffset line_no = tracker_class_get_definition_line_no (class);
			goffset column_no = tracker_class_get_definition_column_no (class);
			const gchar *subject = tracker_class_get_uri (class);
			const gchar *last_super_class_uri = tracker_class_get_uri (last_super_class);

			handle_unsupported_ontology_change (manager,
			                                    ontology_path,
			                                    line_no,
			                                    column_no,
			                                    subject,
			                                    "rdfs:subClassOf",
			                                    last_super_class_uri, "-",
			                                    error);
			return;
		}

		last_super_classes++;
	}
}

static void
check_for_max_cardinality_change (TrackerDataManager  *manager,
                                  TrackerProperty     *property,
                                  GError             **error)
{
	gboolean orig_multiple_values = tracker_property_get_orig_multiple_values (property);
	gboolean new_multiple_values = tracker_property_get_multiple_values (property);

	if (tracker_property_get_is_new (property) == FALSE &&
	    orig_multiple_values != new_multiple_values &&
	    orig_multiple_values == FALSE) {
		GError *n_error = NULL;
		const gchar *subject = tracker_property_get_uri (property);

		if (update_property_value (manager,
		                           "nrl:maxCardinality",
		                           subject,
		                           TRACKER_PREFIX_NRL "maxCardinality",
		                           NULL, allowed_cardinality_conversions,
		                           NULL, property,
		                           &n_error)) {
			TrackerClass *class;
			class = tracker_property_get_domain(property);

			tracker_property_set_db_schema_changed (property, TRUE);
			tracker_property_set_cardinality_changed (property, TRUE);
			tracker_class_set_db_schema_changed (class, TRUE);
		}

		if (n_error) {
			const gchar* ontology_path = tracker_property_get_ontology_path (property);
			goffset line_no = tracker_property_get_definition_line_no (property);
			goffset column_no = tracker_property_get_definition_column_no (property);
			gchar* property_location;

			property_location = g_strdup_printf ("%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT,
			                                     ontology_path, line_no, column_no);

			g_propagate_prefixed_error (error, n_error, "%s:", property_location);

			g_free (property_location);
			return;
		}
	}
}

static void
check_for_deleted_super_properties (TrackerDataManager  *manager,
                                    TrackerProperty     *property,
                                    GError             **error)
{
	TrackerProperty **last_super_properties;
	TrackerOntologies *ontologies;
	GList *to_remove = NULL;

	ontologies = tracker_data_manager_get_ontologies (manager);
	last_super_properties = tracker_property_get_last_super_properties (property);

	if (!last_super_properties) {
		return;
	}

	while (*last_super_properties) {
		TrackerProperty *last_super_property = *last_super_properties;
		gboolean found = FALSE;
		TrackerProperty **super_properties;

		super_properties = tracker_property_get_super_properties (property);

		while (*super_properties) {
			TrackerProperty *super_property = *super_properties;

			if (last_super_property == super_property) {
				found = TRUE;
				break;
			}
			super_properties++;
		}

		if (!found) {
			to_remove = g_list_prepend (to_remove, last_super_property);
		}

		last_super_properties++;
	}

	if (to_remove) {
		GList *copy = to_remove;

		while (copy) {
			GError *n_error = NULL;
			TrackerProperty *prop_to_remove = copy->data;
			const gchar *object = tracker_property_get_uri (prop_to_remove);
			const gchar *subject = tracker_property_get_uri (property);
			GValue value = G_VALUE_INIT;
			TrackerRowid subject_id = 0;

			property = tracker_ontologies_get_property_by_uri (ontologies,
			                                                   TRACKER_PREFIX_RDFS "subPropertyOf");
			if (!property) {
				g_set_error (error, TRACKER_SPARQL_ERROR,
				             TRACKER_SPARQL_ERROR_UNKNOWN_PROPERTY,
				             "Property '%s' not found in the ontology",
				             TRACKER_PREFIX_RDFS "subPropertyOf");
				return;
			}

			tracker_property_del_super_property (property, prop_to_remove);

			tracker_data_query_string_to_value (manager,
			                                    object, NULL,
			                                    tracker_property_get_data_type (property),
			                                    &value, &n_error);

			if (!n_error)
				subject_id = tracker_data_update_ensure_resource (manager->data_update,
				                                                  subject,
				                                                  &n_error);

			if (!n_error)
				tracker_data_delete_statement (manager->data_update, NULL, subject_id,
				                               property, &value, &n_error);
			g_value_unset (&value);

			if (!n_error) {
				tracker_data_update_buffer_flush (manager->data_update, &n_error);
			}

			if (n_error) {
				g_propagate_error (error, n_error);
				return;
			}

			copy = copy->next;
		}
		g_list_free (to_remove);
	}
}

static void
tracker_data_ontology_process_changes_pre_db (TrackerDataManager  *manager,
                                              GPtrArray           *seen_classes,
                                              GPtrArray           *seen_properties,
                                              GError             **error)
{
	guint i;
	if (seen_classes) {
		for (i = 0; i < seen_classes->len; i++) {
			GError *n_error = NULL;
			TrackerClass *class = g_ptr_array_index (seen_classes, i);

			check_for_deleted_domain_index (manager, class, &n_error);

			if (n_error) {
				g_propagate_error (error, n_error);
				return;
			}

			check_for_deleted_super_classes (manager, class, &n_error);

			if (n_error) {
				g_propagate_error (error, n_error);
				return;
			}
		}
	}

	if (seen_properties) {
		for (i = 0; i < seen_properties->len; i++) {
			GError *n_error = NULL;
			TrackerProperty *property = g_ptr_array_index (seen_properties, i);

			check_for_max_cardinality_change (manager, property, &n_error);

			if (n_error) {
				g_propagate_error (error, n_error);
				return;
			}

			check_for_deleted_super_properties (manager, property, &n_error);

			if (n_error) {
				g_propagate_error (error, n_error);
				return;
			}
		}
	}
}

static void
tracker_data_ontology_process_changes_post_db (TrackerDataManager  *manager,
                                               GPtrArray           *seen_classes,
                                               GPtrArray           *seen_properties,
                                               GError             **error)
{
	guint i;

	/* This updates property-property changes and marks classes for necessity
	 * of having their tables recreated later. There's support for
	 * nrl:notify and nrl:indexed */

	if (seen_classes) {
		for (i = 0; i < seen_classes->len; i++) {
			TrackerClass *class = g_ptr_array_index (seen_classes, i);
			const gchar *subject;
			GError *n_error = NULL;

			subject = tracker_class_get_uri (class);

			if (tracker_class_get_notify (class)) {
				update_property_value (manager,
				                       "nrl:notify",
				                       subject,
				                       TRACKER_PREFIX_NRL "notify",
				                       "true", allowed_boolean_conversions,
				                       class, NULL, &n_error);
			} else {
				update_property_value (manager,
				                       "nrl:notify",
				                       subject,
				                       TRACKER_PREFIX_NRL "notify",
				                       "false", allowed_boolean_conversions,
				                       class, NULL, &n_error);
			}

			if (n_error) {
				g_propagate_error (error, n_error);
				return;
			}
		}
	}

	if (seen_properties) {
		for (i = 0; i < seen_properties->len; i++) {
			TrackerProperty *property = g_ptr_array_index (seen_properties, i);
			const gchar *subject;
			gchar *query;
			TrackerProperty *secondary_index;
			gboolean indexed_set = FALSE, in_onto;
			GError *n_error = NULL;
			TrackerSparqlCursor *cursor;

			subject = tracker_property_get_uri (property);

			/* Check for nrl:InverseFunctionalProperty changes (not supported) */
			in_onto = tracker_property_get_is_inverse_functional_property (property);

			query = g_strdup_printf ("ASK { <%s> a nrl:InverseFunctionalProperty }", subject);
			cursor = TRACKER_SPARQL_CURSOR (tracker_data_query_sparql_cursor (manager, query, &n_error));
			g_free (query);

			if (n_error) {
				g_propagate_error (error, n_error);
				return;
			}

			if (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
				if (tracker_sparql_cursor_get_boolean (cursor, 0) != in_onto) {
					const gchar *ontology_path = tracker_property_get_ontology_path (property);
					goffset line_no = tracker_property_get_definition_line_no (property);
					goffset column_no = tracker_property_get_definition_column_no (property);

					handle_unsupported_ontology_change (manager,
					                                    ontology_path,
					                                    line_no,
					                                    column_no,
					                                    subject,
					                                    "nrl:InverseFunctionalProperty", "-", "-",
					                                    &n_error);

					if (n_error) {
						g_object_unref (cursor);
						g_propagate_error (error, n_error);
						return;
					}
				}
			}

			if (cursor) {
				g_object_unref (cursor);
			}

			if (tracker_property_get_indexed (property)) {
				if (update_property_value (manager,
				                           "nrl:indexed",
				                           subject,
				                           TRACKER_PREFIX_NRL "indexed",
				                           "true", allowed_boolean_conversions,
				                           NULL, property, &n_error)) {
					fix_indexed (manager, property, &n_error);
					indexed_set = TRUE;
				}
			} else {
				if (update_property_value (manager,
				                           "nrl:indexed",
				                           subject,
				                           TRACKER_PREFIX_NRL "indexed",
				                           "false", allowed_boolean_conversions,
				                           NULL, property, &n_error)) {
					fix_indexed (manager, property, &n_error);
					indexed_set = TRUE;
				}
			}

			if (n_error) {
				g_propagate_error (error, n_error);
				return;
			}

			secondary_index = tracker_property_get_secondary_index (property);

			if (secondary_index) {
				if (update_property_value (manager,
				                           "nrl:secondaryIndex",
				                           subject,
				                           TRACKER_PREFIX_NRL "secondaryIndex",
				                           tracker_property_get_uri (secondary_index), NULL,
				                           NULL, property, &n_error)) {
					if (!indexed_set) {
						fix_indexed (manager, property, &n_error);
					}
				}
			} else {
				if (update_property_value (manager,
				                           "nrl:secondaryIndex",
				                           subject,
				                           TRACKER_PREFIX_NRL "secondaryIndex",
				                           NULL, NULL,
				                           NULL, property, &n_error)) {
					if (!indexed_set) {
						fix_indexed (manager, property, &n_error);
					}
				}
			}

			if (n_error) {
				g_propagate_error (error, n_error);
				return;
			}

			if (update_property_value (manager,
			                           "rdfs:range", subject, TRACKER_PREFIX_RDFS "range",
			                           tracker_class_get_uri (tracker_property_get_range (property)),
			                           allowed_range_conversions,
			                           NULL, property, &n_error)) {
				TrackerClass *class;

				class = tracker_property_get_domain (property);
				tracker_class_set_db_schema_changed (class, TRUE);
				tracker_property_set_db_schema_changed (property, TRUE);
			}

			if (n_error) {
				g_propagate_error (error, n_error);
				return;
			}
		}
	}
}

static void
tracker_data_ontology_process_changes_post_import (GPtrArray *seen_classes,
                                                   GPtrArray *seen_properties)
{
	return;
}

static void
check_properties_completeness (TrackerOntologies  *ontologies,
                               GError            **error)
{
	guint i;
	guint n_properties;
	TrackerProperty **properties;

	properties = tracker_ontologies_get_properties (ontologies, &n_properties);

	for (i = 0; i < n_properties; i++) {
		TrackerProperty *property = properties[i];
		gchar *missing_definition = NULL;

		if (!tracker_property_get_domain (property))
			missing_definition = "domain";
		else if (!tracker_property_get_range (property))
			missing_definition = "range";

		if (missing_definition) {
			const gchar *ontology_path = tracker_property_get_ontology_path (property);
			goffset line_no = tracker_property_get_definition_line_no (property);
			goffset column_no = tracker_property_get_definition_column_no (property);
			const gchar *property_name = tracker_property_get_name (property);
			gchar *definition_location = g_strdup_printf ("%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT,
			                                              ontology_path, line_no, column_no);

			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_INCOMPLETE_PROPERTY_DEFINITION,
			             "%s: Property %s has no defined %s.",
			             definition_location, property_name, missing_definition);

			g_free (definition_location);
			return;
		}
	}
}

static void
load_ontology_file (TrackerDataManager  *manager,
                    GFile               *file,
                    gboolean             in_update,
                    GPtrArray           *seen_classes,
                    GPtrArray           *seen_properties,
                    guint               *num_parsing_errors,
                    GError             **error)
{
	TrackerSparqlCursor *deserializer;
	GError *ttl_error = NULL;
	gchar *ontology_uri = g_file_get_uri (file);
	const gchar *subject, *predicate, *object;
	goffset object_line_no = 0, object_column_no = 0;

	if (num_parsing_errors)
		*num_parsing_errors = 0;

	deserializer = tracker_deserializer_new_for_file (file, NULL, &ttl_error);

	if (ttl_error) {
		g_propagate_prefixed_error (error, ttl_error, "%s: ", ontology_uri);
		g_free (ontology_uri);
		return;
	}

	/* Post checks are only needed for ontology updates, not the initial
	 * ontology */

	while (tracker_sparql_cursor_next (deserializer, NULL, &ttl_error)) {
		GError *ontology_error = NULL;
		gboolean loaded_successfully;

		subject = tracker_sparql_cursor_get_string (deserializer,
		                                            TRACKER_RDF_COL_SUBJECT,
		                                            NULL);
		predicate = tracker_sparql_cursor_get_string (deserializer,
		                                              TRACKER_RDF_COL_PREDICATE,
		                                              NULL);
		object = tracker_sparql_cursor_get_string (deserializer,
		                                           TRACKER_RDF_COL_OBJECT,
		                                           NULL);

		tracker_deserializer_get_parser_location (TRACKER_DESERIALIZER (deserializer),
		                                          &object_line_no,
		                                          &object_column_no);

		tracker_data_ontology_load_statement (manager, ontology_uri,
		                                      subject, predicate, object,
		                                      object_line_no, object_column_no, in_update,
		                                      &loaded_successfully, seen_classes,
		                                      seen_properties, &ontology_error);

		if (num_parsing_errors && !loaded_successfully)
			(*num_parsing_errors)++;

		if (ontology_error) {
			g_propagate_error (error, ontology_error);
			break;
		}
	}

	if (ttl_error) {
		tracker_deserializer_get_parser_location (TRACKER_DESERIALIZER (deserializer),
		                                          &object_line_no,
		                                          &object_column_no);
		g_propagate_prefixed_error (error, ttl_error,
		                            "%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT ": ",
		                            ontology_uri, object_line_no, object_column_no);
	}

	g_free (ontology_uri);
	g_object_unref (deserializer);
}


static TrackerOntology*
get_ontology_from_file (TrackerDataManager *manager,
                        GFile              *file,
                        GError            **error)
{
	const gchar *subject, *predicate, *object;
	TrackerSparqlCursor *deserializer;
	GError *internal_error = NULL;
	GHashTable *ontology_uris;
	TrackerOntology *ret = NULL;
	goffset object_line_no = 0, object_column_no = 0;
	gchar *ontology_uri = g_file_get_uri (file);

	deserializer = tracker_deserializer_new_for_file (file, NULL, &internal_error);

	if (internal_error) {
		g_propagate_prefixed_error (error, internal_error, "%s: ", ontology_uri);
		goto out;
	}

	ontology_uris = g_hash_table_new_full (g_str_hash,
	                                       g_str_equal,
	                                       g_free,
	                                       g_object_unref);

	while (tracker_sparql_cursor_next (deserializer, NULL, &internal_error)) {
		subject = tracker_sparql_cursor_get_string (deserializer,
		                                            TRACKER_RDF_COL_SUBJECT,
		                                            NULL);
		predicate = tracker_sparql_cursor_get_string (deserializer,
		                                              TRACKER_RDF_COL_PREDICATE,
		                                              NULL);
		object = tracker_sparql_cursor_get_string (deserializer,
		                                           TRACKER_RDF_COL_OBJECT,
		                                           NULL);

		tracker_deserializer_get_parser_location (TRACKER_DESERIALIZER (deserializer),
		                                          &object_line_no,
		                                          &object_column_no);

		if (g_strcmp0 (predicate, RDF_TYPE) == 0) {
			if (g_strcmp0 (object, TRACKER_PREFIX_NRL "Ontology") == 0) {
				TrackerOntology *ontology;

				ontology = tracker_ontology_new ();
				tracker_ontology_set_ontologies (ontology, manager->ontologies);
				tracker_ontology_set_uri (ontology, subject);

				/* Passes ownership */
				g_hash_table_insert (ontology_uris,
				                     g_strdup (subject),
				                     ontology);
			}
		} else if (g_strcmp0 (predicate, NRL_LAST_MODIFIED) == 0) {
			TrackerOntology *ontology;
			GDateTime *datetime;
			GError *parsing_error;

			ontology = g_hash_table_lookup (ontology_uris, subject);
			if (ontology == NULL) {
				g_critical ("%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT ": Unknown ontology %s",
				            ontology_uri, object_line_no, object_column_no, subject);
				continue;
			}

			datetime = tracker_date_new_from_iso8601 (object, &parsing_error);
			if (!datetime) {
				g_propagate_prefixed_error (error, parsing_error,
				                            "%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT ": error parsing nrl:lastModified: ",
				                            ontology_uri, object_line_no, object_column_no);
				goto out;
			}

			tracker_ontology_set_last_modified (ontology, g_date_time_to_unix (datetime));
			g_date_time_unref (datetime);

			/* This one is here because lower ontology_uris is destroyed, and
			 * else would this one's reference also be destroyed with it */
			ret = g_object_ref (ontology);

			break;
		}
	}

	g_hash_table_unref (ontology_uris);
	g_object_unref (deserializer);

	if (internal_error) {
		g_propagate_prefixed_error (error, internal_error,
		                            "%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT ": Turtle parse error: ",
		                            ontology_uri, object_line_no, object_column_no);
		goto out;
	}

	if (ret == NULL) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_MISSING_LAST_MODIFIED_HEADER,
		             "%s: Ontology has no nrl:lastModified header", ontology_uri);
		goto out;
	}

out:
	g_free (ontology_uri);
	return ret;
}

static void
tracker_data_ontology_process_statement (TrackerDataManager *manager,
                                         const gchar        *subject,
                                         const gchar        *predicate,
                                         const gchar        *object,
                                         gboolean            in_update,
                                         GError            **error)
{
	TrackerProperty *property;
	GValue value = G_VALUE_INIT;
	TrackerRowid subject_id = 0;

	if (g_strcmp0 (predicate, RDF_TYPE) == 0) {
		if (g_strcmp0 (object, RDFS_CLASS) == 0) {
			TrackerClass *class;

			class = tracker_ontologies_get_class_by_uri (manager->ontologies, subject);

			if (class && tracker_class_get_is_new (class) != in_update) {
				return;
			}
		} else if (g_strcmp0 (object, RDF_PROPERTY) == 0) {
			TrackerProperty *prop;

			prop = tracker_ontologies_get_property_by_uri (manager->ontologies, subject);

			if (prop && tracker_property_get_is_new (prop) != in_update) {
				return;
			}
		} else if (g_strcmp0 (object, TRACKER_PREFIX_NRL "Namespace") == 0) {
			TrackerNamespace *namespace;

			namespace = tracker_ontologies_get_namespace_by_uri (manager->ontologies, subject);

			if (namespace && tracker_namespace_get_is_new (namespace) != in_update) {
				return;
			}
		} else if (g_strcmp0 (object, TRACKER_PREFIX_NRL "Ontology") == 0) {
			TrackerOntology *ontology;

			ontology = tracker_ontologies_get_ontology_by_uri (manager->ontologies, subject);

			if (ontology && tracker_ontology_get_is_new (ontology) != in_update) {
				return;
			}
		}
	} else if (g_strcmp0 (predicate, RDFS_SUB_CLASS_OF) == 0) {
		TrackerClass *class;

		class = tracker_ontologies_get_class_by_uri (manager->ontologies, subject);

		if (class && tracker_class_get_is_new (class) != in_update) {
			return;
		}
	} else if (g_strcmp0 (predicate, RDFS_SUB_PROPERTY_OF) == 0          ||
	           g_strcmp0 (predicate, RDFS_DOMAIN) == 0                   ||
	           g_strcmp0 (predicate, RDFS_RANGE) == 0                    ||
	           /* g_strcmp0 (predicate, NRL_MAX_CARDINALITY) == 0        || */
	           g_strcmp0 (predicate, TRACKER_PREFIX_NRL "indexed") == 0      ||
	           g_strcmp0 (predicate, TRACKER_PREFIX_NRL "fulltextIndexed") == 0) {
		TrackerProperty *prop;

		prop = tracker_ontologies_get_property_by_uri (manager->ontologies, subject);

		if (prop && tracker_property_get_is_new (prop) != in_update) {
			return;
		}
	} else if (g_strcmp0 (predicate, TRACKER_PREFIX_NRL "prefix") == 0) {
		TrackerNamespace *namespace;

		namespace = tracker_ontologies_get_namespace_by_uri (manager->ontologies, subject);

		if (namespace && tracker_namespace_get_is_new (namespace) != in_update) {
			return;
		}
	} else if (g_strcmp0 (predicate, NRL_LAST_MODIFIED) == 0) {
		TrackerOntology *ontology;

		ontology = tracker_ontologies_get_ontology_by_uri (manager->ontologies, subject);

		if (ontology && tracker_ontology_get_is_new (ontology) != in_update) {
			return;
		}
	}

	property = tracker_ontologies_get_property_by_uri (manager->ontologies, predicate);

	if (!property) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_UNKNOWN_PROPERTY,
		             "Unknown property %s", predicate);
		goto out;
	}

	if (!tracker_data_query_string_to_value (manager,
	                                         object, NULL,
	                                         tracker_property_get_data_type (property),
	                                         &value, error))
		goto out;

	subject_id = tracker_data_update_ensure_resource (manager->data_update,
	                                                  subject,
	                                                  error);
	if (subject_id == 0)
		goto out;

	if (tracker_property_get_is_new (property) ||
	    tracker_property_get_multiple_values (property)) {
		tracker_data_insert_statement (manager->data_update, NULL,
		                               subject_id, property, &value,
		                               error);
	} else {
		tracker_data_update_statement (manager->data_update, NULL,
		                               subject_id, property, &value,
		                               error);
	}

out:
	g_value_unset (&value);
}

static void
import_ontology_file (TrackerDataManager  *manager,
                      GFile               *file,
                      gboolean             in_update,
                      GError             **error)
{
	const gchar *subject, *predicate, *object;
	TrackerSparqlCursor *deserializer;
	goffset object_line_no = 0, object_column_no = 0;
	gchar *ontology_uri = g_file_get_uri (file);

	deserializer = tracker_deserializer_new_for_file (file, NULL, error);

	if (!deserializer) {
		g_prefix_error (error, "%s:", ontology_uri);
		goto out;
	}

	while (tracker_sparql_cursor_next (deserializer, NULL, error)) {
		GError *internal_error = NULL;
		subject = tracker_sparql_cursor_get_string (deserializer,
		                                            TRACKER_RDF_COL_SUBJECT,
		                                            NULL);
		predicate = tracker_sparql_cursor_get_string (deserializer,
		                                              TRACKER_RDF_COL_PREDICATE,
		                                              NULL);
		object = tracker_sparql_cursor_get_string (deserializer,
		                                           TRACKER_RDF_COL_OBJECT,
		                                           NULL);

		tracker_deserializer_get_parser_location (TRACKER_DESERIALIZER (deserializer),
		                                          &object_line_no,
		                                          &object_column_no);

		tracker_data_ontology_process_statement (manager,
		                                         subject, predicate, object,
		                                         in_update, &internal_error);

		if (internal_error) {
			g_propagate_prefixed_error (error, internal_error,
			                            "%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT ": ",
			                            ontology_uri, object_line_no, object_column_no);
			break;
		}
	}

	if (*error) {
		g_prefix_error (error,
		                "%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT ":",
		                ontology_uri, object_line_no, object_column_no);
	}

	g_object_unref (deserializer);

out:
	g_free (ontology_uri);
}

static void
class_add_super_classes_from_db (TrackerDBInterface *iface,
                                 TrackerDataManager *manager,
                                 TrackerClass       *class)
{
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor;
	GError *error = NULL;

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, &error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"rdfs:subClassOf\") "
	                                              "FROM \"rdfs:Class_rdfs:subClassOf\" "
	                                              "WHERE ID = (SELECT ID FROM Resource WHERE Uri = ?)");

	if (!stmt) {
		g_critical ("%s", error->message);
		g_error_free (error);
		return;
	}

	tracker_db_statement_bind_text (stmt, 0, tracker_class_get_uri (class));
	cursor = tracker_db_statement_start_cursor (stmt, NULL);
	g_object_unref (stmt);

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
			TrackerClass *super_class;
			const gchar *super_class_uri;

			super_class_uri = tracker_db_cursor_get_string (cursor, 0, NULL);
			super_class = tracker_ontologies_get_class_by_uri (manager->ontologies, super_class_uri);
			tracker_class_add_super_class (class, super_class);
		}

		g_object_unref (cursor);
	}
}


static void
class_add_domain_indexes_from_db (TrackerDBInterface *iface,
                                  TrackerDataManager *manager,
                                  TrackerClass       *class)
{
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor;
	GError *error = NULL;

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, &error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"nrl:domainIndex\") "
	                                              "FROM \"rdfs:Class_nrl:domainIndex\" "
	                                              "WHERE ID = (SELECT ID FROM Resource WHERE Uri = ?)");

	if (!stmt) {
		g_critical ("%s", error->message);
		g_error_free (error);
		return;
	}

	tracker_db_statement_bind_text (stmt, 0, tracker_class_get_uri (class));
	cursor = tracker_db_statement_start_cursor (stmt, NULL);
	g_object_unref (stmt);

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
			TrackerProperty *domain_index;
			const gchar *domain_index_uri;

			domain_index_uri = tracker_db_cursor_get_string (cursor, 0, NULL);
			domain_index = tracker_ontologies_get_property_by_uri (manager->ontologies, domain_index_uri);
			tracker_class_add_domain_index (class, domain_index);
			tracker_property_add_domain_index (domain_index, class);
		}

		g_object_unref (cursor);
	}
}

static void
property_add_super_properties_from_db (TrackerDBInterface *iface,
                                       TrackerDataManager *manager,
                                       TrackerProperty    *property)
{
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor;
	GError *error = NULL;

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, &error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"rdfs:subPropertyOf\") "
	                                              "FROM \"rdf:Property_rdfs:subPropertyOf\" "
	                                              "WHERE ID = (SELECT ID FROM Resource WHERE Uri = ?)");

	if (!stmt) {
		g_critical ("%s", error->message);
		g_error_free (error);
		return;
	}

	tracker_db_statement_bind_text (stmt, 0, tracker_property_get_uri (property));
	cursor = tracker_db_statement_start_cursor (stmt, NULL);
	g_object_unref (stmt);

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
			TrackerProperty *super_property;
			const gchar *super_property_uri;

			super_property_uri = tracker_db_cursor_get_string (cursor, 0, NULL);
			super_property = tracker_ontologies_get_property_by_uri (manager->ontologies, super_property_uri);
			tracker_property_add_super_property (property, super_property);
		}

		g_object_unref (cursor);
	}
}

static void
db_get_static_data (TrackerDBInterface  *iface,
                    TrackerDataManager  *manager,
                    GError             **error)
{
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor = NULL;
	TrackerClass **classes;
	guint n_classes, i;
	GError *internal_error = NULL;

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, &internal_error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"nrl:Ontology\".ID), "
	                                              "       \"nrl:lastModified\" "
	                                              "FROM \"nrl:Ontology\"");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &internal_error);
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, NULL, &internal_error)) {
			TrackerOntology *ontology;
			const gchar     *uri;
			time_t           last_mod;

			ontology = tracker_ontology_new ();
			tracker_ontology_set_ontologies (ontology, manager->ontologies);

			uri = tracker_db_cursor_get_string (cursor, 0, NULL);
			last_mod = (time_t) tracker_db_cursor_get_int (cursor, 1);

			tracker_ontology_set_is_new (ontology, FALSE);
			tracker_ontology_set_uri (ontology, uri);
			tracker_ontology_set_last_modified (ontology, last_mod);
			tracker_ontologies_add_ontology (manager->ontologies, ontology);

			g_object_unref (ontology);
		}

		g_object_unref (cursor);
		cursor = NULL;
	}

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return;
	}

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, &internal_error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"nrl:Namespace\".ID), "
	                                              "\"nrl:prefix\" "
	                                              "FROM \"nrl:Namespace\"");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &internal_error);
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, NULL, &internal_error)) {
			TrackerNamespace *namespace;
			const gchar      *uri, *prefix;

			namespace = tracker_namespace_new (FALSE);

			uri = tracker_db_cursor_get_string (cursor, 0, NULL);
			prefix = tracker_db_cursor_get_string (cursor, 1, NULL);

			tracker_namespace_set_ontologies (namespace, manager->ontologies);
			tracker_namespace_set_is_new (namespace, FALSE);
			tracker_namespace_set_uri (namespace, uri);
			tracker_namespace_set_prefix (namespace, prefix);
			tracker_ontologies_add_namespace (manager->ontologies, namespace);

			g_object_unref (namespace);

		}

		g_object_unref (cursor);
		cursor = NULL;
	}

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return;
	}

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, &internal_error,
	                                              "SELECT \"rdfs:Class\".ID, "
	                                              "(SELECT Uri FROM Resource WHERE ID = \"rdfs:Class\".ID), "
	                                              "\"nrl:notify\" "
	                                              "FROM \"rdfs:Class\" ORDER BY ID");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &internal_error);
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, NULL, &internal_error)) {
			TrackerClass *class;
			const gchar  *uri;
			TrackerRowid id;
			GValue        value = { 0 };
			gboolean      notify;

			class = tracker_class_new (FALSE);

			id = tracker_db_cursor_get_int (cursor, 0);
			uri = tracker_db_cursor_get_string (cursor, 1, NULL);

			tracker_db_cursor_get_value (cursor, 2, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				notify = (g_value_get_int64 (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				notify = FALSE;
			}

			tracker_class_set_ontologies (class, manager->ontologies);
			tracker_class_set_db_schema_changed (class, FALSE);
			tracker_class_set_is_new (class, FALSE);
			tracker_class_set_uri (class, uri);
			tracker_class_set_notify (class, notify);

			class_add_super_classes_from_db (iface, manager, class);

			/* We add domain indexes later , we first need to load the properties */

			tracker_ontologies_add_class (manager->ontologies, class);
			tracker_ontologies_add_id_uri_pair (manager->ontologies, id, uri);
			tracker_class_set_id (class, id);

			g_object_unref (class);
		}

		g_object_unref (cursor);
		cursor = NULL;
	}

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return;
	}

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, &internal_error,
	                                              "SELECT \"rdf:Property\".ID, (SELECT Uri FROM Resource WHERE ID = \"rdf:Property\".ID), "
	                                              "(SELECT Uri FROM Resource WHERE ID = \"rdfs:domain\"), "
	                                              "(SELECT Uri FROM Resource WHERE ID = \"rdfs:range\"), "
	                                              "\"nrl:maxCardinality\", "
	                                              "\"nrl:indexed\", "
	                                              "(SELECT Uri FROM Resource WHERE ID = \"nrl:secondaryIndex\"), "
	                                              "\"nrl:fulltextIndexed\", "
	                                              "(SELECT 1 FROM \"rdfs:Resource_rdf:type\" WHERE ID = \"rdf:Property\".ID AND "
	                                              "\"rdf:type\" = (SELECT ID FROM Resource WHERE Uri = '" NRL_INVERSE_FUNCTIONAL_PROPERTY "')) "
	                                              "FROM \"rdf:Property\" ORDER BY ID");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &internal_error);
		g_object_unref (stmt);
	}

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor, NULL, &internal_error)) {
			GValue value = { 0 };
			TrackerProperty *property;
			const gchar     *uri, *domain_uri, *range_uri, *secondary_index_uri;
			gboolean         multi_valued, indexed, fulltext_indexed;
			gboolean         is_inverse_functional_property;
			TrackerRowid id;

			property = tracker_property_new (FALSE);

			id = tracker_db_cursor_get_int (cursor, 0);
			uri = tracker_db_cursor_get_string (cursor, 1, NULL);
			domain_uri = tracker_db_cursor_get_string (cursor, 2, NULL);
			range_uri = tracker_db_cursor_get_string (cursor, 3, NULL);

			tracker_db_cursor_get_value (cursor, 4, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				multi_valued = (g_value_get_int64 (&value) > 1);
				g_value_unset (&value);
			} else {
				/* nrl:maxCardinality not set
				   not limited to single value */
				multi_valued = TRUE;
			}

			tracker_db_cursor_get_value (cursor, 5, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				indexed = (g_value_get_int64 (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				indexed = FALSE;
			}

			secondary_index_uri = tracker_db_cursor_get_string (cursor, 6, NULL);

			tracker_db_cursor_get_value (cursor, 7, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				fulltext_indexed = (g_value_get_int64 (&value) == 1);
				g_value_unset (&value);
			} else {
				/* NULL */
				fulltext_indexed = FALSE;
			}

			/* NRL_INVERSE_FUNCTIONAL_PROPERTY column */
			tracker_db_cursor_get_value (cursor, 8, &value);

			if (G_VALUE_TYPE (&value) != 0) {
				is_inverse_functional_property = TRUE;
				g_value_unset (&value);
			} else {
				/* NULL */
				is_inverse_functional_property = FALSE;
			}

			tracker_property_set_ontologies (property, manager->ontologies);
			tracker_property_set_is_new_domain_index (property, tracker_ontologies_get_class_by_uri (manager->ontologies, domain_uri), FALSE);
			tracker_property_set_is_new (property, FALSE);
			tracker_property_set_cardinality_changed (property, FALSE);
			tracker_property_set_uri (property, uri);
			tracker_property_set_id (property, id);
			tracker_property_set_domain (property, tracker_ontologies_get_class_by_uri (manager->ontologies, domain_uri));
			tracker_property_set_range (property, tracker_ontologies_get_class_by_uri (manager->ontologies, range_uri));
			tracker_property_set_multiple_values (property, multi_valued);
			tracker_property_set_orig_multiple_values (property, multi_valued);
			tracker_property_set_indexed (property, indexed);

			tracker_property_set_db_schema_changed (property, FALSE);

			if (secondary_index_uri) {
				tracker_property_set_secondary_index (property, tracker_ontologies_get_property_by_uri (manager->ontologies, secondary_index_uri));
			}

			tracker_property_set_orig_fulltext_indexed (property, fulltext_indexed);
			tracker_property_set_fulltext_indexed (property, fulltext_indexed);
			tracker_property_set_is_inverse_functional_property (property, is_inverse_functional_property);

			/* super properties are only used in updates, never for queries */
			if ((tracker_db_manager_get_flags (manager->db_manager, NULL, NULL) & TRACKER_DB_MANAGER_READONLY) == 0) {
				property_add_super_properties_from_db (iface, manager, property);
			}

			tracker_ontologies_add_property (manager->ontologies, property);
			tracker_ontologies_add_id_uri_pair (manager->ontologies, id, uri);

			g_object_unref (property);

		}

		g_object_unref (cursor);
		cursor = NULL;
	}

	/* Now that the properties are loaded we can do this foreach class */
	classes = tracker_ontologies_get_classes (manager->ontologies, &n_classes);
	for (i = 0; i < n_classes; i++) {
		class_add_domain_indexes_from_db (iface, manager, classes[i]);
	}

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return;
	}
}

static void
insert_uri_in_resource_table (TrackerDataManager  *manager,
                              TrackerDBInterface  *iface,
                              const gchar         *uri,
                              TrackerRowid         id,
                              GError             **error)
{
	TrackerDBStatement *stmt;
	GError *internal_error = NULL;

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE,
	                                              &internal_error,
	                                              "INSERT OR IGNORE "
	                                              "INTO main.Resource "
	                                              "(ID, Uri) "
	                                              "VALUES (?, ?)");
	if (internal_error) {
		g_propagate_error (error, internal_error);
		return;
	}

	tracker_db_statement_bind_int (stmt, 0, id);
	tracker_db_statement_bind_text (stmt, 1, uri);
	tracker_db_statement_execute (stmt, &internal_error);

	if (internal_error) {
		g_object_unref (stmt);
		g_propagate_error (error, internal_error);
		return;
	}

	g_object_unref (stmt);

}

static void
range_change_for (TrackerProperty *property,
                  GString         *in_col_sql,
                  GString         *sel_col_sql,
                  const gchar     *field_name)
{
	/* TODO: TYPE_RESOURCE and TYPE_DATETIME are completely unhandled atm, we
	 * should forbid conversion from anything to resource or datetime in error
	 * handling earlier */

	g_string_append_printf (in_col_sql, ", \"%s\"", field_name);

	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_INTEGER ||
	    tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DOUBLE) {
			g_string_append_printf (sel_col_sql, ", \"%s\" + 0", field_name);
	} else if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME) {

		/* TODO (see above) */

		g_string_append_printf (sel_col_sql, ", \"%s\"", field_name);
	} else if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_BOOLEAN) {
		g_string_append_printf (sel_col_sql, ", \"%s\" != 0", field_name);
	} else {
		g_string_append_printf (sel_col_sql, ", \"%s\"", field_name);
	}
}

static void property_get_sql_representation (TrackerProperty  *property,
                                             const gchar     **type,
                                             const gchar     **collation)
{
        const gchar *_type = NULL;
        const gchar *_collation = NULL;

        switch (tracker_property_get_data_type (property)) {
	case TRACKER_PROPERTY_TYPE_STRING:
	case TRACKER_PROPERTY_TYPE_LANGSTRING:
		_type = "TEXT";
                _collation = TRACKER_COLLATION_NAME;
		break;
	case TRACKER_PROPERTY_TYPE_INTEGER:
	case TRACKER_PROPERTY_TYPE_BOOLEAN:
	case TRACKER_PROPERTY_TYPE_DATE:
	case TRACKER_PROPERTY_TYPE_DATETIME:
	case TRACKER_PROPERTY_TYPE_RESOURCE:
		_type = "INTEGER";
		break;
	case TRACKER_PROPERTY_TYPE_DOUBLE:
		_type = "REAL";
		break;
	case TRACKER_PROPERTY_TYPE_UNKNOWN:
                g_assert_not_reached();
		break;
	}

        if (type)
                *type = _type;
        if (collation)
                *collation = _collation;
}

static void
create_decomposed_metadata_property_table (TrackerDBInterface *iface,
                                           TrackerProperty    *property,
                                           const gchar        *database,
                                           const gchar        *service_name,
                                           TrackerClass       *service,
                                           gboolean            in_update,
                                           gboolean            in_change,
                                           GError            **error)
{
	GError *internal_error = NULL;
	const char *field_name;
	const char *sql_type;

        property_get_sql_representation (property, &sql_type, NULL);

	field_name = tracker_property_get_name (property);

	if (!in_update || (in_update && (tracker_property_get_is_new (property) ||
	                                 tracker_property_get_is_new_domain_index (property, service) ||
	                                 tracker_property_get_cardinality_changed (property) ||
	                                 tracker_property_get_db_schema_changed (property)))) {

		GString *sql = NULL;
		GString *in_col_sql = NULL;
		GString *sel_col_sql = NULL;

		if (in_update) {
			TRACKER_NOTE (ONTOLOGY_CHANGES,
			              g_message ("Altering database for class '%s' property '%s': multi value",
			                         service_name, field_name));
		}

		if (in_change && !tracker_property_get_is_new (property) && !tracker_property_get_cardinality_changed (property)) {
			TRACKER_NOTE (ONTOLOGY_CHANGES,
			              g_message ("Drop index: DROP INDEX IF EXISTS \"%s_%s_ID\"\nRename: ALTER TABLE \"%s_%s\" RENAME TO \"%s_%s_TEMP\"",
			                         service_name, field_name, service_name, field_name, service_name, field_name));

			tracker_db_interface_execute_query (iface, &internal_error,
			                                    "DROP INDEX IF EXISTS \"%s\".\"%s_%s_ID\"",
			                                    database,
			                                    service_name,
			                                    field_name);

			if (internal_error) {
				g_propagate_error (error, internal_error);
				goto error_out;
			}

			tracker_db_interface_execute_query (iface, &internal_error,
			                                    "ALTER TABLE \"%s\".\"%s_%s\" RENAME TO \"%s_%s_TEMP\"",
			                                    database, service_name, field_name,
			                                    service_name, field_name);

			if (internal_error) {
				g_propagate_error (error, internal_error);
				goto error_out;
			}
		} else if (in_change && tracker_property_get_cardinality_changed (property)) {
			/* We should be dropping all indices colliding with the new table name */
			tracker_db_interface_execute_query (iface, &internal_error,
			                                    "DROP INDEX IF EXISTS \"%s\".\"%s_%s\"",
			                                    database,
			                                    service_name,
			                                    field_name);
		}

		sql = g_string_new ("");
		g_string_append_printf (sql,
		                        "CREATE TABLE \"%s\".\"%s_%s\" ("
		                        "ID INTEGER NOT NULL, "
		                        "\"%s\" %s NOT NULL",
		                        database,
		                        service_name,
		                        field_name,
		                        field_name,
		                        sql_type);

		if (in_change && !tracker_property_get_is_new (property)) {
			in_col_sql = g_string_new ("ID");
			sel_col_sql = g_string_new ("ID");

			range_change_for (property, in_col_sql, sel_col_sql, field_name);
		}

		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "%s)", sql->str);

		if (internal_error) {
			g_propagate_error (error, internal_error);
			goto error_out;
		}

		if (in_change && !tracker_property_get_is_new (property) &&
		    !tracker_property_get_cardinality_changed (property) && in_col_sql && sel_col_sql) {
			gchar *query;

			query = g_strdup_printf ("INSERT INTO \"%s\".\"%s_%s\"(%s) "
			                         "SELECT %s FROM \"%s\".\"%s_%s_TEMP\"",
			                         database, service_name, field_name, in_col_sql->str,
			                         sel_col_sql->str, database, service_name, field_name);

			tracker_db_interface_execute_query (iface, &internal_error, "%s", query);

			if (internal_error) {
				g_free (query);
				g_propagate_error (error, internal_error);
				goto error_out;
			}

			g_free (query);
			tracker_db_interface_execute_query (iface, &internal_error, "DROP TABLE \"%s\".\"%s_%s_TEMP\"",
			                                    database, service_name, field_name);

			if (internal_error) {
				g_propagate_error (error, internal_error);
				goto error_out;
			}
		}

                set_index_for_multi_value_property (iface, database, service, property, &internal_error);
                if (internal_error) {
                        g_propagate_error (error, internal_error);
                        goto error_out;
                }

		error_out:

		if (sql) {
			g_string_free (sql, TRUE);
		}

		if (sel_col_sql) {
			g_string_free (sel_col_sql, TRUE);
		}

		if (in_col_sql) {
			g_string_free (in_col_sql, TRUE);
		}
	}
}

static gboolean
is_a_domain_index (TrackerProperty **domain_indexes, TrackerProperty *property)
{
	while (*domain_indexes) {

		if (*domain_indexes == property) {
			return TRUE;
		}

		domain_indexes++;
	}

	return FALSE;
}

static void
copy_from_domain_to_domain_index (TrackerDBInterface  *iface,
                                  const gchar         *database,
                                  TrackerProperty     *domain_index,
                                  const gchar         *column_name,
                                  const gchar         *column_suffix,
                                  TrackerClass        *dest_domain,
                                  GError             **error)
{
	GError *internal_error = NULL;
	TrackerClass *source_domain;
	const gchar *source_name, *dest_name;
	gchar *query;

	source_domain = tracker_property_get_domain (domain_index);
	source_name = tracker_class_get_name (source_domain);
	dest_name = tracker_class_get_name (dest_domain);

	query = g_strdup_printf ("UPDATE \"%s\".\"%s\" SET \"%s%s\"=("
	                         "SELECT \"%s%s\" FROM \"%s\".\"%s\" "
	                         "WHERE \"%s\".ID = \"%s\".ID)",
	                         database,
	                         dest_name,
	                         column_name,
	                         column_suffix ? column_suffix : "",
	                         column_name,
	                         column_suffix ? column_suffix : "",
	                         database,
	                         source_name,
	                         source_name,
	                         dest_name);

	TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("Copying: '%s'", query));

	tracker_db_interface_execute_query (iface, &internal_error, "%s", query);

	if (internal_error) {
		g_propagate_error (error, internal_error);
	}

	g_free (query);
}

typedef struct {
	TrackerProperty *prop;
	const gchar *field_name;
	const gchar *suffix;
} ScheduleCopy;

static void
schedule_copy (GPtrArray *schedule,
               TrackerProperty *prop,
               const gchar *field_name,
               const gchar *suffix)
{
	ScheduleCopy *sched = g_new0 (ScheduleCopy, 1);
	sched->prop = prop;
	sched->field_name = field_name,
	sched->suffix = suffix;
	g_ptr_array_add (schedule, sched);
}

static void
create_decomposed_metadata_tables (TrackerDataManager  *manager,
                                   TrackerDBInterface  *iface,
                                   const gchar         *database,
                                   TrackerClass        *service,
                                   gboolean             in_update,
                                   GError             **error)
{
	const char       *service_name;
	GString          *create_sql = NULL;
	GString          *in_col_sql = NULL;
	GString          *sel_col_sql = NULL;
	TrackerProperty **properties, *property, **domain_indexes;
	GSList           *class_properties = NULL, *field_it;
	guint             i, n_props;
	gboolean          in_alter = in_update;
        gboolean          in_change;
	GError           *internal_error = NULL;
	GPtrArray        *copy_schedule = NULL;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	service_name = tracker_class_get_name (service);

	g_return_if_fail (service_name != NULL);

	if (g_str_has_prefix (service_name, "xsd:")) {
		/* xsd classes do not derive from rdfs:Resource and do not need separate tables */
		return;
	}

        in_change = tracker_class_get_db_schema_changed (service);

	if (in_change && !tracker_class_get_is_new (service)) {
		TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("Rename: ALTER TABLE \"%s\" RENAME TO \"%s_TEMP\"", service_name, service_name));
		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "ALTER TABLE \"%s\".\"%s\" RENAME TO \"%s_TEMP\"",
		                                    database, service_name, service_name);
		in_col_sql = g_string_new ("ID");
		sel_col_sql = g_string_new ("ID");
		if (internal_error) {
			g_propagate_error (error, internal_error);
			goto error_out;
		}
	}

	if (in_change || !in_update || (in_update && tracker_class_get_is_new (service))) {
		if (in_update)
			TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("Altering database with new class '%s' (create)", service_name));
		in_alter = FALSE;
		create_sql = g_string_new ("");
		g_string_append_printf (create_sql, "CREATE TABLE \"%s\".\"%s\" (ID INTEGER NOT NULL PRIMARY KEY",
					database, service_name);
	}

	properties = tracker_ontologies_get_properties (manager->ontologies, &n_props);
	domain_indexes = tracker_class_get_domain_indexes (service);

	for (i = 0; i < n_props; i++) {
                const gchar *sql_type;
                const gchar *sql_collation;
		gboolean put_change;
		const gchar *field_name;
		gboolean is_domain_index;

		property = properties[i];
		is_domain_index = is_a_domain_index (domain_indexes, property);

		if (tracker_property_get_domain (property) != service && !is_domain_index) {
                        continue;
                }

                if (tracker_property_get_multiple_values (property)) {
                        /* Multi-valued property. */

                        create_decomposed_metadata_property_table (iface, property,
                                                                   database,
                                                                   service_name,
                                                                   service,
                                                                   in_alter,
                                                                   in_change,
                                                                   &internal_error);
                        if (internal_error) {
                                g_propagate_error (error, internal_error);
                                goto error_out;
                        }
                        continue;
                }

                /* Single-valued property. */

                field_name = tracker_property_get_name (property);
                property_get_sql_representation (property, &sql_type, &sql_collation);

		if (in_update) {
			TRACKER_NOTE (ONTOLOGY_CHANGES,
			              g_message ("%sAltering database for class '%s' property '%s': single value (%s)",
			                         in_alter ? "" : "  ",
			                         service_name,
			                         field_name,
			                         in_alter ? "alter" : "create"));
		}

		if (!in_alter) {
			put_change = TRUE;
			class_properties = g_slist_prepend (class_properties, property);

			g_string_append_printf (create_sql, ", \"%s\" %s",
			                        field_name,
			                        sql_type);

			if (!copy_schedule) {
				copy_schedule = g_ptr_array_new_with_free_func (g_free);
			}

			if (is_domain_index && tracker_property_get_is_new_domain_index (property, service)) {
				schedule_copy (copy_schedule, property, field_name, NULL);
			}

			if (sql_collation) {
				g_string_append_printf (create_sql, " COLLATE %s", sql_collation);
			}

			if (tracker_property_get_is_inverse_functional_property (property)) {
				g_string_append (create_sql, " UNIQUE");
			}
		} else if ((!is_domain_index && tracker_property_get_is_new (property)) ||
		           (is_domain_index && tracker_property_get_is_new_domain_index (property, service))) {
			GString *alter_sql = NULL;

			put_change = FALSE;
			class_properties = g_slist_prepend (class_properties, property);

			alter_sql = g_string_new ("ALTER TABLE ");
			g_string_append_printf (alter_sql, "\"%s\".\"%s\" ADD COLUMN \"%s\" %s",
			                        database,
			                        service_name,
			                        field_name,
			                        sql_type);

			if (sql_collation) {
				g_string_append_printf (alter_sql, " COLLATE %s", sql_collation);
			}

			if (tracker_property_get_is_inverse_functional_property (property)) {
				g_string_append (alter_sql, " UNIQUE");
			}

			TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("Altering: '%s'", alter_sql->str));
			tracker_db_interface_execute_query (iface, &internal_error, "%s", alter_sql->str);
			if (internal_error) {
				g_string_free (alter_sql, TRUE);
				g_propagate_error (error, internal_error);
				goto error_out;
			} else if (is_domain_index) {
				copy_from_domain_to_domain_index (iface, database, property,
				                                  field_name, NULL,
				                                  service,
				                                  &internal_error);
				if (internal_error) {
					g_string_free (alter_sql, TRUE);
					g_propagate_error (error, internal_error);
					goto error_out;
				}

				/* This is implicit for all domain-specific-indices */
				set_index_for_single_value_property (iface, database, service,
				                                     property, TRUE,
				                                     &internal_error);
				if (internal_error) {
					g_string_free (alter_sql, TRUE);
					g_propagate_error (error, internal_error);
					goto error_out;
				}
			}

			g_string_free (alter_sql, TRUE);
		} else {
			put_change = TRUE;
		}

		if (in_change && put_change && in_col_sql && sel_col_sql) {
			range_change_for (property, in_col_sql, sel_col_sql, field_name);
		}
	}

	if (create_sql) {
		g_string_append (create_sql, ")");
		TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("Creating: '%s'", create_sql->str));
		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "%s", create_sql->str);
		if (internal_error) {
			g_propagate_error (error, internal_error);
			goto error_out;
		}
	}

	/* create index for single-valued fields */
	for (field_it = class_properties; field_it != NULL; field_it = field_it->next) {
		TrackerProperty *field, *secondary_index;
		gboolean is_domain_index;

		field = field_it->data;

		/* This is implicit for all domain-specific-indices */
		is_domain_index = is_a_domain_index (domain_indexes, field);

		if (!tracker_property_get_multiple_values (field)
		    && (tracker_property_get_indexed (field) || is_domain_index)) {

			secondary_index = tracker_property_get_secondary_index (field);
			if (secondary_index == NULL) {
				set_index_for_single_value_property (iface, database, service,
				                                     field, TRUE,
				                                     &internal_error);
				if (internal_error) {
					g_propagate_error (error, internal_error);
					goto error_out;
				}
			} else {
				set_secondary_index_for_single_value_property (iface, database, service, field,
				                                               secondary_index,
				                                               TRUE, &internal_error);
				if (internal_error) {
					g_propagate_error (error, internal_error);
					goto error_out;
				}
			}
		}
	}

	if (!tracker_class_get_is_new (service) && in_change && sel_col_sql && in_col_sql) {
		guint i;
		gchar *query;

		query = g_strdup_printf ("INSERT INTO \"%s\".\"%s\"(%s) "
		                         "SELECT %s FROM \"%s\".\"%s_TEMP\"",
		                         database, service_name, in_col_sql->str,
		                         sel_col_sql->str, database, service_name);

		TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("Copy: %s", query));

		tracker_db_interface_execute_query (iface, &internal_error, "%s", query);
		g_free (query);

		if (internal_error) {
			g_propagate_error (error, internal_error);
			goto error_out;
		}

		for (i = 0; i < n_props; i++) {
			property = properties[i];

			if (tracker_property_get_domain (property) == service && tracker_property_get_cardinality_changed (property)) {
				GString *n_sel_col_sql, *n_in_col_sql;
				const gchar *field_name = tracker_property_get_name (property);

				n_in_col_sql = g_string_new ("ID");
				n_sel_col_sql = g_string_new ("ID");

				/* Function does what it must do, so reusable atm */
				range_change_for (property, n_in_col_sql, n_sel_col_sql, field_name);

                                /* Columns happen to be the same for decomposed multi-value and single value atm */

				query = g_strdup_printf ("INSERT INTO \"%s\".\"%s_%s\"(%s) "
				                         "SELECT %s FROM \"%s\".\"%s_TEMP\" "
				                         "WHERE ID IS NOT NULL AND \"%s\" IS NOT NULL",
				                         database, service_name, field_name,
				                         n_in_col_sql->str, n_sel_col_sql->str,
				                         database, service_name, field_name);

				g_string_free (n_in_col_sql, TRUE);
				g_string_free (n_sel_col_sql, TRUE);

				TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("Copy supported nlr:maxCardinality change: %s", query));

				tracker_db_interface_execute_query (iface, &internal_error, "%s", query);
				g_free (query);

				if (internal_error) {
					g_propagate_error (error, internal_error);
					goto error_out;
				}
			}
		}

		TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("Rename (drop): DROP TABLE \"%s_TEMP\"", service_name));
		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "DROP TABLE \"%s\".\"%s_TEMP\"",
						    database, service_name);

		if (internal_error) {
			g_propagate_error (error, internal_error);
			goto error_out;
		}
	}

	/* FIXME: We are trusting object refcount will stay intact across
	 * ontology changes. One situation where this is not true are
	 * removal or properties with rdfs:Resource range.
	 */

	if (copy_schedule) {
		guint i;
		for (i = 0; i < copy_schedule->len; i++) {
			ScheduleCopy *sched = g_ptr_array_index (copy_schedule, i);
			copy_from_domain_to_domain_index (iface, database, sched->prop,
			                                  sched->field_name, sched->suffix,
			                                  service,
			                                  &internal_error);

			if (internal_error) {
				g_propagate_error (error, internal_error);
				break;
			}
		}
	}

error_out:

	if (copy_schedule) {
		g_ptr_array_free (copy_schedule, TRUE);
	}

	if (create_sql) {
		g_string_free (create_sql, TRUE);
	}

	g_slist_free (class_properties);

	if (in_col_sql) {
		g_string_free (in_col_sql, TRUE);
	}

	if (sel_col_sql) {
		g_string_free (sel_col_sql, TRUE);
	}
}

static void
tracker_data_ontology_import_finished (TrackerDataManager *manager)
{
	TrackerClass **classes;
	TrackerProperty **properties;
	guint i, n_props, n_classes;

	classes = tracker_ontologies_get_classes (manager->ontologies, &n_classes);
	properties = tracker_ontologies_get_properties (manager->ontologies, &n_props);

	for (i = 0; i < n_classes; i++) {
		tracker_class_set_is_new (classes[i], FALSE);
		tracker_class_set_db_schema_changed (classes[i], FALSE);
	}

	for (i = 0; i < n_props; i++) {
		tracker_property_set_is_new_domain_index (properties[i], NULL, FALSE);
		tracker_property_set_is_new (properties[i], FALSE);
		tracker_property_set_db_schema_changed (properties[i], FALSE);
		tracker_property_set_cardinality_changed (properties[i], FALSE);
	}
}

static gboolean
query_table_exists (TrackerDBInterface  *iface,
                    const gchar         *table_name,
                    GError             **error)
{
	TrackerDBCursor *cursor = NULL;
	TrackerDBStatement *stmt;
	gboolean exists = FALSE;

	stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, error,
	                                               "SELECT 1 FROM sqlite_master WHERE tbl_name=\"%s\" AND type=\"table\"",
	                                               table_name);
	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, error);
		g_object_unref (stmt);
	}

	if (cursor) {
		if (tracker_db_cursor_iter_next (cursor, NULL, error)) {
			exists = TRUE;
		}
		g_object_unref (cursor);
	}

	return exists;
}

static gboolean
create_base_tables (TrackerDataManager  *manager,
                    TrackerDBInterface  *iface,
                    GError             **error)
{
	GError *internal_error = NULL;

	tracker_db_interface_execute_query (iface, &internal_error,
	                                    "CREATE TABLE Resource (ID INTEGER NOT NULL PRIMARY KEY,"
	                                    " Uri TEXT, BlankNode INTEGER DEFAULT 0, UNIQUE (Uri))");

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	tracker_db_interface_execute_query (iface, &internal_error,
	                                    "CREATE TABLE Graph (ID INTEGER NOT NULL PRIMARY KEY)");

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	tracker_db_interface_execute_query (iface, &internal_error,
	                                    "CREATE TABLE metadata (key TEXT NOT NULL PRIMARY KEY, value TEXT)");

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	return TRUE;
}

static gboolean
tracker_data_ontology_setup_db (TrackerDataManager  *manager,
                                TrackerDBInterface  *iface,
                                const gchar         *database,
                                gboolean             in_update,
                                GError             **error)
{
	GError *internal_error = NULL;
	TrackerClass **classes;
	guint i, n_classes;

	tracker_db_interface_execute_query (iface, &internal_error,
	                                    "CREATE TABLE IF NOT EXISTS "
	                                    " \"%s\".Refcount (ID INTEGER NOT NULL PRIMARY KEY,"
	                                    " Refcount INTEGER DEFAULT 0)",
	                                    database);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	classes = tracker_ontologies_get_classes (manager->ontologies, &n_classes);

	/* create tables */
	for (i = 0; i < n_classes; i++) {
		/* Also !is_new classes are processed, they might have new properties */
		create_decomposed_metadata_tables (manager, iface, database, classes[i], in_update,
		                                   &internal_error);

		if (internal_error) {
			g_propagate_error (error, internal_error);
			return FALSE;
		}
	}

	return TRUE;
}

static void
tracker_data_ontology_import_into_db (TrackerDataManager  *manager,
                                      TrackerDBInterface  *iface,
                                      gboolean             in_update,
                                      GError             **error)
{
	TrackerClass **classes;
	TrackerProperty **properties;
	guint i, n_props, n_classes;

	classes = tracker_ontologies_get_classes (manager->ontologies, &n_classes);
	properties = tracker_ontologies_get_properties (manager->ontologies, &n_props);

	/* insert classes into rdfs:Resource table */
	for (i = 0; i < n_classes; i++) {
		if (tracker_class_get_is_new (classes[i]) == in_update) {
			GError *internal_error = NULL;

			insert_uri_in_resource_table (manager, iface,
			                              tracker_class_get_uri (classes[i]),
			                              tracker_class_get_id (classes[i]),
			                              &internal_error);

			if (internal_error) {
				g_propagate_error (error, internal_error);
				return;
			}
		}
	}

	/* insert properties into rdfs:Resource table */
	for (i = 0; i < n_props; i++) {
		if (tracker_property_get_is_new (properties[i]) == in_update) {
			GError *internal_error = NULL;

			insert_uri_in_resource_table (manager, iface,
			                              tracker_property_get_uri (properties[i]),
			                              tracker_property_get_id (properties[i]),
			                              &internal_error);

			if (internal_error) {
				g_propagate_error (error, internal_error);
				return;
			}
		}
	}

}

static gint
compare_file_names (GFile *file_a,
                    GFile *file_b)
{
	gchar *name_a, *name_b;
	gint return_val;

	name_a = g_file_get_basename (file_a);
	name_b = g_file_get_basename (file_b);
	return_val = strcmp (name_a, name_b);

	g_free (name_a);
	g_free (name_b);

	return return_val;
}

static GList*
get_ontologies (TrackerDataManager  *manager,
                GFile               *ontologies,
                GError             **error)
{
	GFileEnumerator *enumerator;
	GList *sorted = NULL;

	enumerator = g_file_enumerate_children (ontologies,
	                                        G_FILE_ATTRIBUTE_STANDARD_NAME,
	                                        G_FILE_QUERY_INFO_NONE,
	                                        NULL, error);
	if (!enumerator)
		return NULL;

	while (TRUE) {
		GFileInfo *info;
		GFile *child;
		const gchar *name;

		if (!g_file_enumerator_iterate (enumerator, &info, &child, NULL, error)) {
			g_list_free_full (sorted, g_object_unref);
			g_object_unref (enumerator);
			return NULL;
		}

		if (!info)
			break;

		name = g_file_info_get_name (info);
		if (g_str_has_suffix (name, ".ontology"))
			sorted = g_list_prepend (sorted, g_object_ref (child));
	}

	sorted = g_list_sort (sorted, (GCompareFunc) compare_file_names);

	/* Add our builtin ontologies so they are loaded first */
	sorted = g_list_prepend (sorted, g_file_new_for_uri ("resource://org/freedesktop/tracker/ontology/20-dc.ontology"));
	sorted = g_list_prepend (sorted, g_file_new_for_uri ("resource://org/freedesktop/tracker/ontology/12-nrl.ontology"));
	sorted = g_list_prepend (sorted, g_file_new_for_uri ("resource://org/freedesktop/tracker/ontology/11-rdf.ontology"));
	sorted = g_list_prepend (sorted, g_file_new_for_uri ("resource://org/freedesktop/tracker/ontology/10-xsd.ontology"));

	g_object_unref (enumerator);

	return sorted;
}

static void
tracker_data_manager_update_status (TrackerDataManager *manager,
                                    const gchar        *status)
{
	g_free (manager->status);
	manager->status = g_strdup (status);
	g_object_notify (G_OBJECT (manager), "status");
}

static void
busy_callback (const gchar *status,
               gdouble      progress,
               gpointer     user_data)
{
	tracker_data_manager_update_status (user_data, status);
}


static void
tracker_data_manager_recreate_indexes (TrackerDataManager  *manager,
                                       GError             **error)
{
	GError *internal_error = NULL;
	TrackerProperty **properties;
	guint n_properties;
	guint i;

	properties = tracker_ontologies_get_properties (manager->ontologies, &n_properties);
	if (!properties) {
		g_critical ("Couldn't get all properties to recreate indexes");
		return;
	}

	TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("Starting index re-creation..."));
	for (i = 0; i < n_properties; i++) {
		fix_indexed (manager, properties [i], &internal_error);

		if (internal_error) {
			g_critical ("Unable to create index for %s: %s",
			            tracker_property_get_name (properties[i]),
			            internal_error->message);
			g_clear_error (&internal_error);
		}

		busy_callback ("Recreating indexes",
		               (gdouble) ((gdouble) i / (gdouble) n_properties),
		               manager);
	}
	TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("  Finished index re-creation..."));
}

static gboolean
write_ontologies_gvdb (TrackerDataManager  *manager,
                       gboolean             overwrite,
                       GError             **error)
{
	gboolean retval = TRUE;
	gchar *filename;
	GFile *child;

	if ((manager->flags & TRACKER_DB_MANAGER_IN_MEMORY) != 0)
		return TRUE;
	if (!manager->cache_location)
		return TRUE;

	child = g_file_get_child (manager->cache_location, "ontologies.gvdb");
	filename = g_file_get_path (child);
	g_object_unref (child);

	if (overwrite || !g_file_test (filename, G_FILE_TEST_EXISTS)) {
		retval = tracker_ontologies_write_gvdb (manager->ontologies, filename, error);
	}

	g_free (filename);

	return retval;
}

static void
load_ontologies_gvdb (TrackerDataManager  *manager,
                      GError             **error)
{
	gchar *filename;
	GFile *child;

	child = g_file_get_child (manager->cache_location, "ontologies.gvdb");
	filename = g_file_get_path (child);
	g_object_unref (child);

	tracker_ontologies_load_gvdb (manager->ontologies, filename, error);
	g_free (filename);
}

static gboolean
tracker_data_manager_fts_changed (TrackerDataManager *manager)
{
	TrackerProperty **properties;
	gboolean has_changed = FALSE;
	guint i, len;

	properties = tracker_ontologies_get_properties (manager->ontologies, &len);

	for (i = 0; i < len; i++) {
		TrackerClass *class;

		if (tracker_property_get_fulltext_indexed (properties[i]) !=
		    tracker_property_get_orig_fulltext_indexed (properties[i])) {
			has_changed |= TRUE;
		}

		if (!tracker_property_get_fulltext_indexed (properties[i])) {
			continue;
		}

		has_changed |= tracker_property_get_is_new (properties[i]);

		/* We must also regenerate FTS if any table in the view
		 * updated its schema.
		 */
		class = tracker_property_get_domain (properties[i]);
		has_changed |= tracker_class_get_db_schema_changed (class);
	}

	return has_changed;
}

static void
ontology_get_fts_properties (TrackerDataManager  *manager,
                             GHashTable         **fts_properties,
                             GHashTable         **multivalued)
{
	TrackerProperty **properties;
	guint i, len;

	properties = tracker_ontologies_get_properties (manager->ontologies, &len);
	*multivalued = g_hash_table_new (g_str_hash, g_str_equal);
	*fts_properties = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                         NULL, (GDestroyNotify) g_list_free);

	for (i = 0; i < len; i++) {
		const gchar *name, *table_name;
		GList *list;

		if (!tracker_property_get_fulltext_indexed (properties[i])) {
			continue;
		}

		table_name = tracker_property_get_table_name (properties[i]);
		name = tracker_property_get_name (properties[i]);
		list = g_hash_table_lookup (*fts_properties, table_name);

		if (tracker_property_get_multiple_values (properties[i])) {
			g_hash_table_insert (*multivalued, (gpointer) table_name,
			                     GUINT_TO_POINTER (TRUE));
		}

		if (!list) {
			list = g_list_prepend (NULL, (gpointer) name);
			g_hash_table_insert (*fts_properties, (gpointer) table_name, list);
		} else {
			g_hash_table_steal (*fts_properties, (gpointer) table_name);
			list = g_list_insert_sorted (list, (gpointer) name,
			                             (GCompareFunc) strcmp);
			g_hash_table_insert (*fts_properties, (gpointer) table_name, list);
		}
	}
}

static gboolean
rebuild_fts_tokens (TrackerDataManager  *manager,
                    TrackerDBInterface  *iface,
                    GError             **error)
{
	TrackerProperty **properties;
	GHashTableIter iter;
	gchar *graph;
	gboolean has_fts = FALSE;
	guint len, i;

	properties = tracker_ontologies_get_properties (manager->ontologies, &len);

	for (i = 0; i < len; i++) {
		has_fts |= tracker_property_get_fulltext_indexed (properties[i]);
		if (has_fts)
			break;
	}

	if (has_fts) {
		g_debug ("Rebuilding FTS tokens, this may take a moment...");
		if (!tracker_db_interface_sqlite_fts_rebuild_tokens (iface, "main", error))
			return FALSE;

		g_hash_table_iter_init (&iter, manager->graphs);
		while (g_hash_table_iter_next (&iter, (gpointer*) &graph, NULL)) {
			if (!tracker_db_interface_sqlite_fts_rebuild_tokens (iface, graph, error))
				return FALSE;
		}

		g_debug ("FTS tokens rebuilt");
	}

	/* Update the stamp file */
	tracker_db_manager_tokenizer_update (manager->db_manager);

	return TRUE;
}

static gboolean
tracker_data_manager_init_fts (TrackerDataManager  *manager,
                               TrackerDBInterface  *iface,
                               const gchar         *database,
                               gboolean             create,
                               GError             **error)
{
	GHashTable *fts_props, *multivalued;
	gboolean retval;

	ontology_get_fts_properties (manager, &fts_props, &multivalued);
	retval = tracker_db_interface_sqlite_fts_init (iface,
	                                               database,
	                                               fts_props,
	                                               multivalued, create,
	                                               error);
	g_hash_table_unref (fts_props);
	g_hash_table_unref (multivalued);

	return retval;
}

static gboolean
tracker_data_manager_update_fts (TrackerDataManager  *manager,
                                 TrackerDBInterface  *iface,
                                 const gchar         *database,
                                 GError             **error)
{
	GHashTable *fts_properties, *multivalued;
	gboolean retval;

	ontology_get_fts_properties (manager, &fts_properties, &multivalued);
	retval = tracker_db_interface_sqlite_fts_alter_table (iface, database,
	                                                      fts_properties,
	                                                      multivalued,
	                                                      error);
	g_hash_table_unref (fts_properties);
	g_hash_table_unref (multivalued);

	return retval;
}

GFile *
tracker_data_manager_get_cache_location (TrackerDataManager *manager)
{
	return manager->cache_location ? g_object_ref (manager->cache_location) : NULL;
}

TrackerDataManager *
tracker_data_manager_new (TrackerDBManagerFlags   flags,
                          GFile                  *cache_location,
                          GFile                  *ontology_location,
                          guint                   select_cache_size,
                          guint                   update_cache_size)
{
	TrackerDataManager *manager;

	if ((flags & TRACKER_DB_MANAGER_IN_MEMORY) == 0 && !cache_location) {
		g_warning ("Data storage location must be provided");
		return NULL;
	}

	manager = g_object_new (TRACKER_TYPE_DATA_MANAGER, NULL);

	/* TODO: Make these properties */
	g_set_object (&manager->cache_location, cache_location);
	g_set_object (&manager->ontology_location, ontology_location);
	manager->flags = flags;
	manager->select_cache_size = select_cache_size;
	manager->update_cache_size = update_cache_size;

	return manager;
}

static void
update_ontology_last_modified (TrackerDataManager  *manager,
                               TrackerDBInterface  *iface,
                               TrackerOntology     *ontology,
                               GError             **error)
{
	TrackerDBStatement *stmt;
	const gchar *ontology_uri;
	time_t last_mod;

	ontology_uri = tracker_ontology_get_uri (ontology);
	last_mod = tracker_ontology_get_last_modified (ontology);
	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE, error,
	                                              "UPDATE \"nrl:Ontology\" SET \"nrl:lastModified\"= ? "
	                                              "WHERE \"nrl:Ontology\".ID = "
	                                              "(SELECT Resource.ID FROM Resource WHERE "
	                                              "Resource.Uri = ?)");
	if (stmt) {
		tracker_db_statement_bind_int (stmt, 0, last_mod);
		tracker_db_statement_bind_text (stmt, 1, ontology_uri);
		tracker_db_statement_execute (stmt, error);
		g_object_unref (stmt);
	}
}

static gboolean
tracker_data_manager_initialize_iface (TrackerDataManager  *data_manager,
                                       TrackerDBInterface  *iface,
                                       GError             **error)
{
	GHashTable *graphs;

	graphs = tracker_data_manager_get_graphs (data_manager, FALSE);

	if (graphs) {
		GHashTableIter iter;
		gpointer value;

		g_hash_table_iter_init (&iter, graphs);

		while (g_hash_table_iter_next (&iter, &value, NULL)) {
			if (!tracker_db_manager_attach_database (data_manager->db_manager,
			                                         iface, value, FALSE,
			                                         error))
				goto error;

			if (!tracker_data_manager_init_fts (data_manager, iface,
			                                    value, FALSE, error))
				goto error;
		}

		g_hash_table_unref (graphs);
	}

	if (!tracker_data_manager_init_fts (data_manager, iface, "main", FALSE, error))
		return FALSE;

	return TRUE;
 error:
	g_clear_pointer (&graphs, g_hash_table_unref);
	return FALSE;
}

static void
setup_interface_cb (TrackerDBManager   *db_manager,
                    TrackerDBInterface *iface,
                    TrackerDataManager *data_manager)
{
	GError *error = NULL;
	guint flags;

	if (!tracker_data_manager_initialize_iface (data_manager, iface, &error)) {
		g_critical ("Could not set up interface : %s",
		            error->message);
		g_error_free (error);
	}

	g_object_get (iface, "flags", &flags, NULL);
}

static gboolean
update_attached_databases (TrackerDBInterface  *iface,
                           TrackerDataManager  *data_manager,
                           gboolean            *changed,
                           GError             **error)
{
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor;
	gboolean retval = TRUE;

	*changed = FALSE;
	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, error,
	                                              "SELECT name, 1, 0, 0 FROM pragma_database_list "
	                                              "WHERE name NOT IN (SELECT Uri FROM RESOURCE WHERE ID IN (SELECT ID FROM Graph))"
	                                              "UNION "
	                                              "SELECT Uri, 0, 1, ID FROM Resource "
	                                              "WHERE ID IN (SELECT ID FROM Graph) "
	                                              "AND Uri NOT IN (SELECT name FROM pragma_database_list)");

	if (!stmt)
		return FALSE;

	cursor = tracker_db_statement_start_cursor (stmt, error);
	g_object_unref (stmt);

	if (!cursor)
		return FALSE;

	while (retval && tracker_db_cursor_iter_next (cursor, NULL, NULL)) {
		const gchar *name;

		name = tracker_db_cursor_get_string (cursor, 0, NULL);

		/* Ignore the main and temp databases, they are always attached */
		if (strcmp (name, "main") == 0 || strcmp (name, "temp") == 0)
			continue;

		if (tracker_db_cursor_get_int (cursor, 1)) {
			if (!tracker_db_manager_detach_database (data_manager->db_manager,
			                                         iface, name, error)) {
				retval = FALSE;
				break;
			}

			g_hash_table_remove (data_manager->graphs, name);
			*changed = TRUE;
		} else if (tracker_db_cursor_get_int (cursor, 2)) {
			TrackerRowid id;

			if (!tracker_db_manager_attach_database (data_manager->db_manager,
			                                         iface, name, FALSE, error)) {
				retval = FALSE;
				break;
			}

			id = tracker_db_cursor_get_int (cursor, 3);
			g_hash_table_insert (data_manager->graphs, g_strdup (name),
			                     tracker_rowid_copy (&id));
			*changed = TRUE;
		}
	}

	g_object_unref (cursor);

	return retval;
}

static void
update_interface_cb (TrackerDBManager   *db_manager,
                     TrackerDBInterface *iface,
                     TrackerDataManager *data_manager)
{
	GError *error = NULL;
	guint iface_generation;
	gboolean update = FALSE, changed, readonly;

	readonly = (tracker_db_manager_get_flags (db_manager, NULL, NULL) & TRACKER_DB_MANAGER_READONLY) != 0;

	if (readonly) {
		update = TRUE;
	} else {
		iface_generation = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (iface),
		                                                        "tracker-data-iface-generation"));
		update = (iface_generation != data_manager->generation);
	}

	if (update) {
		if (update_attached_databases (iface, data_manager, &changed, &error)) {
			/* This is where we bump the generation for the readonly case, in response to
			 * tables being attached or detached due to graph changes.
			 */
			if (readonly && changed)
				data_manager->generation++;
		} else {
			g_critical ("Could not update attached databases: %s\n",
			            error->message);
			g_error_free (error);
		}

		g_object_set_data (G_OBJECT (iface), "tracker-data-iface-generation",
		                   GUINT_TO_POINTER (data_manager->generation));
	}
}

static gboolean
tracker_data_manager_update_from_version (TrackerDataManager  *manager,
                                          TrackerDBVersion     version,
                                          GError             **error)
{
	TrackerDBInterface *iface;
	GError *internal_error = NULL;

	iface = tracker_data_manager_get_writable_db_interface (manager);

	if (version < TRACKER_DB_VERSION_3_3) {
		/* Anonymous blank nodes, remove "NOT NULL" restriction
		 * from Resource.Uri.
		 */
		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "CREATE TABLE Resource_TEMP (ID INTEGER NOT NULL PRIMARY KEY,"
		                                    " Uri TEXT, BlankNode INTEGER DEFAULT 0, UNIQUE (Uri))");
		if (internal_error)
			goto error;

		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "INSERT INTO Resource_TEMP SELECT * FROM Resource");
		if (internal_error)
			goto error;

		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "DROP TABLE Resource");
		if (internal_error)
			goto error;

		tracker_db_interface_execute_query (iface, &internal_error,
		                                    "ALTER TABLE Resource_TEMP RENAME TO Resource");
		if (internal_error)
			goto error;
	}

	if (version < TRACKER_DB_VERSION_3_4) {
		GHashTableIter iter;
		const gchar *graph;

		if (!tracker_db_interface_sqlite_fts_delete_table (iface, "main", &internal_error))
			goto error;
		if (!tracker_data_manager_update_fts (manager, iface, "main", &internal_error))
			goto error;

		g_hash_table_iter_init (&iter, manager->graphs);

		while (g_hash_table_iter_next (&iter, (gpointer *) &graph, NULL)) {
			if (!tracker_db_interface_sqlite_fts_delete_table (iface, graph, &internal_error))
				goto error;
			if (!tracker_data_manager_update_fts (manager, iface, graph, &internal_error))
				goto error;
		}
	}

	tracker_db_manager_update_version (manager->db_manager);
	return TRUE;

error:
	g_propagate_error (error, internal_error);
	return FALSE;
}

static gboolean
tracker_data_manager_initable_init (GInitable     *initable,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
	TrackerDataManager *manager = TRACKER_DATA_MANAGER (initable);
	TrackerDBInterface *iface;
	gboolean is_create, has_graph_table = FALSE;
	TrackerDBCursor *cursor;
	TrackerDBStatement *stmt;
	GHashTable *ontos_table;
	GHashTable *graphs = NULL;
	GList *sorted = NULL, *l;
	gboolean read_only;
	GError *internal_error = NULL;

	if (manager->initialized) {
		return TRUE;
	}

	if (manager->cache_location && !g_file_is_native (manager->cache_location)) {
		g_set_error (error,
		             TRACKER_DATA_ONTOLOGY_ERROR,
		             TRACKER_DATA_UNSUPPORTED_LOCATION,
		             "Cache and data locations must be local");
		return FALSE;
	}

	read_only = (manager->flags & TRACKER_DB_MANAGER_READONLY) ? TRUE : FALSE;

	/* Make sure we initialize all other modules we depend on */
	manager->data_update = tracker_data_new (manager);
	manager->ontologies = tracker_ontologies_new ();

	manager->db_manager = tracker_db_manager_new (manager->flags,
	                                              manager->cache_location,
	                                              FALSE,
	                                              manager->select_cache_size,
	                                              manager->update_cache_size,
	                                              busy_callback, manager,
	                                              G_OBJECT (manager),
	                                              manager,
	                                              &internal_error);
	if (!manager->db_manager) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	is_create = tracker_db_manager_is_first_time (manager->db_manager);

	g_signal_connect (manager->db_manager, "setup-interface",
	                  G_CALLBACK (setup_interface_cb), manager);
	g_signal_connect (manager->db_manager, "update-interface",
	                  G_CALLBACK (update_interface_cb), manager);

	tracker_data_manager_update_status (manager, "Initializing data manager");

	iface = tracker_db_manager_get_writable_db_interface (manager->db_manager);

	if (manager->ontology_location &&
	    g_file_query_file_type (manager->ontology_location, G_FILE_QUERY_INFO_NONE, NULL) != G_FILE_TYPE_DIRECTORY) {
		gchar *uri;

		uri = g_file_get_uri (manager->ontology_location);
		g_set_error (error, TRACKER_DATA_ONTOLOGY_ERROR,
		             TRACKER_DATA_ONTOLOGY_NOT_FOUND,
		             "'%s' is not a ontology location", uri);
		g_free (uri);
		return FALSE;
	}

	if (is_create) {
		TrackerOntology **ontologies;
		guint n_ontologies, i;
		guint num_parsing_errors = 0;

		if (!manager->ontology_location) {
			g_set_error (error,
			             TRACKER_DATA_ONTOLOGY_ERROR,
			             TRACKER_DATA_ONTOLOGY_NOT_FOUND,
			             "You must pass an ontology location. "
			             "Use tracker_sparql_get_ontology_nepomuk() to find the default ontologies.");
			return FALSE;
		}

		g_debug ("Applying ontologies from %s", g_file_peek_path (manager->ontology_location));
		sorted = get_ontologies (manager, manager->ontology_location, &internal_error);

		if (internal_error) {
			g_propagate_error (error, internal_error);
			return FALSE;
		}

		tracker_data_begin_ontology_transaction (manager->data_update, &internal_error);
		if (internal_error) {
			g_propagate_error (error, internal_error);
			return FALSE;
		}

		if (!create_base_tables (manager, iface, error)) {
			return FALSE;
		}

		tracker_db_manager_update_version (manager->db_manager);

		for (l = sorted; l; l = l->next) {
			GError *ontology_error = NULL;
			GFile *ontology_file = l->data;
			gchar *uri = g_file_get_uri (ontology_file);
			guint num_ontology_parsing_errors;

			TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("Loading ontology %s", uri));

			load_ontology_file (manager, ontology_file,
			                    FALSE,
			                    NULL,
			                    NULL,
			                    &num_ontology_parsing_errors,
			                    &ontology_error);
			g_free (uri);

			if (ontology_error) {
				g_propagate_error (error, ontology_error);
				goto rollback_newly_created_db;
			}

			num_parsing_errors += num_ontology_parsing_errors;
		}

		if (num_parsing_errors) {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_PARSE,
			             "The ontology files contain %u parsing error(s)",
			             num_parsing_errors);
			goto rollback_newly_created_db;
		}

		check_properties_completeness (manager->ontologies, &internal_error);

		if (!internal_error) {
			tracker_data_ontology_setup_db (manager, iface, "main", FALSE,
			                                &internal_error);
		}

		if (!internal_error) {
			tracker_data_ontology_import_into_db (manager, iface,
			                                      FALSE,
			                                      &internal_error);
		}

		if (internal_error) {
			g_propagate_error (error, internal_error);
			goto rollback_newly_created_db;
		}

		if (!tracker_data_manager_init_fts (manager, iface, "main", TRUE, &internal_error)) {
			g_propagate_error (error, internal_error);
			goto rollback_newly_created_db;
		}

		if (!tracker_data_manager_initialize_graphs (manager, iface, &internal_error)) {
			g_propagate_error (error, internal_error);
			goto rollback_newly_created_db;
		}

		tracker_data_manager_initialize_iface (manager, iface, &internal_error);
		if (internal_error) {
			g_propagate_error (error, internal_error);
			goto rollback_newly_created_db;
		}

		/* store ontology in database */
		for (l = sorted; l; l = l->next) {
			import_ontology_file (manager, l->data, FALSE, &internal_error);
			if (internal_error) {
				g_propagate_error (error, internal_error);
				goto rollback_newly_created_db;
			}
		}

		if (!write_ontologies_gvdb (manager, TRUE /* overwrite */, &internal_error)) {
			g_propagate_error (error, internal_error);
			goto rollback_newly_created_db;
		}

		ontologies = tracker_ontologies_get_ontologies (manager->ontologies, &n_ontologies);

		for (i = 0; i < n_ontologies; i++) {
			GError *n_error = NULL;

			update_ontology_last_modified (manager, iface, ontologies[i], &n_error);

			if (n_error) {
				g_propagate_error (error, n_error);
				goto rollback_newly_created_db;
			}
		}

		tracker_data_commit_transaction (manager->data_update, &internal_error);

		if (internal_error) {
			g_propagate_error (error, internal_error);
			goto rollback_newly_created_db;
		}

		g_list_free_full (sorted, g_object_unref);
		sorted = NULL;
	} else {
		GError *gvdb_error = NULL;
		gboolean load_from_db = TRUE;

		if (read_only) {
			/* Not all ontology information is saved in the gvdb cache, so
			 * it can only be used for read-only connections. */
			g_debug ("Loading cached ontologies from gvdb cache");
			load_ontologies_gvdb (manager, &gvdb_error);

			if (gvdb_error) {
				g_debug ("Error loading ontology cache: %s. ", gvdb_error->message);
				g_clear_error (&gvdb_error);
			} else {
				load_from_db = FALSE;
			}
		}

		if (load_from_db) {
			g_debug ("Loading ontologies from database.");

			db_get_static_data (iface, manager, &internal_error);
			if (internal_error) {
				g_propagate_error (error, internal_error);
				return FALSE;
			}

			if (!read_only) {
				if (!write_ontologies_gvdb (manager, FALSE /* overwrite */, &internal_error)) {
					g_propagate_error (error, internal_error);
					return FALSE;
				}
			}
		}

		if (!tracker_data_manager_initialize_graphs (manager, iface, &internal_error)) {
			g_propagate_error (error, internal_error);
			return FALSE;
		}

		tracker_data_manager_initialize_iface (manager, iface, &internal_error);
		if (internal_error) {
			g_propagate_error (error, internal_error);
			return FALSE;
		}
	}

	if (!is_create && manager->ontology_location) {
		GList *to_reload = NULL;
		GList *ontos = NULL;
		GPtrArray *seen_classes;
		GPtrArray *seen_properties;
		GError *n_error = NULL;
		gboolean transaction_started = FALSE;
		guint num_parsing_errors = 0;
		TrackerDBVersion cur_version;

		cur_version = tracker_db_manager_get_version (manager->db_manager);

		if (cur_version < TRACKER_DB_VERSION_NOW) {
			tracker_data_begin_ontology_transaction (manager->data_update, &internal_error);
			if (internal_error) {
				g_propagate_error (error, internal_error);
				return FALSE;
			}
			transaction_started = TRUE;

			if (!tracker_data_manager_update_from_version (manager,
			                                               cur_version,
			                                               error))
				return FALSE;
		}

		seen_classes = g_ptr_array_new_with_free_func (g_object_unref);
		seen_properties = g_ptr_array_new_with_free_func (g_object_unref);

		g_debug ("Applying ontologies from %s to existing database", g_file_peek_path (manager->ontology_location));

		/* Get all the ontology files from ontology_location */
		ontos = get_ontologies (manager, manager->ontology_location, &internal_error);

		if (internal_error) {
			g_propagate_error (error, internal_error);
			return FALSE;
		}

		/* check ontology against database */

		/* Get a map of nrl:Ontology v. nrl:lastModified so that we can test
		 * for all the ontology files in ontology_location whether the last-modified
		 * has changed since we dealt with the file last time. */

		stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, &n_error,
		                                              "SELECT Resource.Uri, \"nrl:Ontology\".\"nrl:lastModified\" FROM \"nrl:Ontology\" "
		                                              "INNER JOIN Resource ON Resource.ID = \"nrl:Ontology\".ID ");

		if (stmt) {
			cursor = tracker_db_statement_start_cursor (stmt, &n_error);
			g_object_unref (stmt);
		} else {
			cursor = NULL;
		}

		ontos_table = g_hash_table_new_full (g_str_hash,
		                                     g_str_equal,
		                                     g_free,
		                                     NULL);

		if (cursor) {
			while (tracker_db_cursor_iter_next (cursor, NULL, &n_error)) {
				const gchar *onto_uri = tracker_db_cursor_get_string (cursor, 0, NULL);
				/* It's stored as an int in the db anyway. This is caused by
				 * string_to_gvalue in tracker-data-update.c */
				gint value = tracker_db_cursor_get_int (cursor, 1);

				g_hash_table_insert (ontos_table, g_strdup (onto_uri),
				                     GINT_TO_POINTER (value));
			}

			g_object_unref (cursor);
		}

		if (n_error) {
			g_propagate_error (error, n_error);
			return FALSE;
		}

		for (l = ontos; l; l = l->next) {
			TrackerOntology *ontology;
			GFile *ontology_file = l->data;
			const gchar *ontology_uri;
			gboolean found, update_nao = FALSE;
			gpointer value;
			gint last_mod;

			/* Parse a TrackerOntology from ontology_file */
			ontology = get_ontology_from_file (manager, ontology_file, &internal_error);

			if (internal_error) {
				g_propagate_error (error, internal_error);
				return FALSE;
			}

			ontology_uri = tracker_ontology_get_uri (ontology);
			/* We can't do better than this cast, it's stored as an int in the
			 * db. See above comment for more info. */
			last_mod = (gint) tracker_ontology_get_last_modified (ontology);

			found = g_hash_table_lookup_extended (ontos_table,
			                                      ontology_uri,
			                                      NULL, &value);

			if (found) {
				GError *ontology_error = NULL;
				gint val = GPOINTER_TO_INT (value);

				has_graph_table = query_table_exists (iface, "Graph", &internal_error);
				if (!has_graph_table) {
					/* No graph table and no resource triggers,
					 * the table must be recreated.
					 */
					if (!transaction_started) {
						tracker_data_begin_ontology_transaction (manager->data_update, &internal_error);
						if (internal_error) {
							g_propagate_error (error, internal_error);
							return FALSE;
						}
						transaction_started = TRUE;
					}

					to_reload = g_list_prepend (to_reload, ontology_file);
					continue;
				}

				/* When the last-modified in our database isn't the same as the last
				 * modified in the latest version of the file, deal with changes. */
				if (val != last_mod) {
					gchar *uri = g_file_get_uri (ontology_file);
					guint num_ontology_parsing_errors;

					TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("Ontology file '%s' needs update", uri));
					g_free (uri);

					if (!transaction_started) {
						tracker_data_begin_ontology_transaction (manager->data_update, &internal_error);
						if (internal_error) {
							g_propagate_error (error, internal_error);
							return FALSE;
						}
						transaction_started = TRUE;
					}

					/* load ontology from files into memory, set all new's
					 * is_new to TRUE */
					load_ontology_file (manager, ontology_file,
					                    TRUE,
					                    seen_classes,
					                    seen_properties,
					                    &num_ontology_parsing_errors,
					                    &ontology_error);

					if (ontology_error) {
						g_propagate_error (error, ontology_error);

						g_clear_pointer (&seen_classes, g_ptr_array_unref);
						g_clear_pointer (&seen_properties, g_ptr_array_unref);

						if (ontos) {
							g_list_free_full (ontos, g_object_unref);
						}

						goto rollback_db_changes;
					}

					num_parsing_errors += num_ontology_parsing_errors;

					to_reload = g_list_prepend (to_reload, l->data);
					update_nao = TRUE;
				}
			} else {
				GError *ontology_error = NULL;
				gchar *uri = g_file_get_uri (ontology_file);
				guint num_ontology_parsing_errors;

				g_debug ("Ontology file '%s' got added", uri);
				g_free (uri);

				if (!transaction_started) {
					tracker_data_begin_ontology_transaction (manager->data_update, &internal_error);
					if (internal_error) {
						g_propagate_error (error, internal_error);
						return FALSE;
					}
					transaction_started = TRUE;
				}

				/* load ontology from files into memory, set all new's
				 * is_new to TRUE */
				load_ontology_file (manager, ontology_file,
				                    TRUE,
				                    seen_classes,
				                    seen_properties,
				                    &num_ontology_parsing_errors,
				                    &ontology_error);

				if (ontology_error) {
					g_propagate_error (error, ontology_error);

					g_clear_pointer (&seen_classes, g_ptr_array_unref);
					g_clear_pointer (&seen_properties, g_ptr_array_unref);

					if (ontos) {
						g_list_free_full (ontos, g_object_unref);
					}

					goto rollback_db_changes;
				}

				num_parsing_errors += num_ontology_parsing_errors;

				to_reload = g_list_prepend (to_reload, l->data);
				update_nao = TRUE;
			}

			if (update_nao) {
				update_ontology_last_modified (manager, iface, ontology, &n_error);

				if (n_error) {
					g_critical ("%s", n_error->message);
					g_clear_error (&n_error);
				}
			}

			g_object_unref (ontology);
		}

		if (num_parsing_errors) {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_PARSE,
			             "The ontology files contain %u parsing error(s)",
			             num_parsing_errors);
			goto rollback_db_changes;
		}

		check_properties_completeness (manager->ontologies, &n_error);

		if (n_error) {
			g_propagate_error (error, n_error);

			g_clear_pointer (&seen_classes, g_ptr_array_unref);
			g_clear_pointer (&seen_properties, g_ptr_array_unref);

			if (ontos) {
				g_list_free_full (ontos, g_object_unref);
			}

			goto rollback_db_changes;
		}

		if (to_reload) {
			GError *ontology_error = NULL;

			tracker_data_ontology_process_changes_pre_db (manager,
			                                              seen_classes,
			                                              seen_properties,
			                                              &ontology_error);

			if (!ontology_error) {
				/* Perform ALTER-TABLE and CREATE-TABLE calls for all that are is_new */
				gboolean update_fts;

				update_fts = tracker_data_manager_fts_changed (manager);

				if (update_fts)
					tracker_db_interface_sqlite_fts_delete_table (iface, "main", &ontology_error);

				if (!ontology_error)
					tracker_data_ontology_setup_db (manager, iface, "main", TRUE, &ontology_error);

				if (!ontology_error)
					graphs = tracker_data_manager_get_graphs (manager, FALSE);

				if (graphs) {
					GHashTableIter iter;
					gpointer value;

					g_hash_table_iter_init (&iter, graphs);

					while (g_hash_table_iter_next (&iter, &value, NULL)) {
						if (update_fts)
							tracker_db_interface_sqlite_fts_delete_table (iface, value, &ontology_error);

						if (ontology_error)
							break;

						tracker_data_ontology_setup_db (manager, iface, value, TRUE,
						                                &ontology_error);
						if (ontology_error)
							break;

						if (update_fts) {
							tracker_data_manager_update_fts (manager, iface, value, &ontology_error);
						} else {
							tracker_data_manager_init_fts (manager, iface, value, FALSE, &ontology_error);
						}

						if (ontology_error)
							break;
					}

					g_hash_table_unref (graphs);
				}

				if (!ontology_error) {
					if (update_fts) {
						tracker_data_manager_update_fts (manager, iface, "main", &ontology_error);
					} else {
						tracker_data_manager_init_fts (manager, iface, "main", FALSE, &ontology_error);
					}
				}

				if (!ontology_error) {
					tracker_data_ontology_import_into_db (manager, iface, TRUE,
					                                      &ontology_error);
				}

				if (!ontology_error) {
					tracker_data_ontology_process_changes_post_db (manager,
					                                               seen_classes,
					                                               seen_properties,
					                                               &ontology_error);
				}
			}

			if (ontology_error) {
				g_propagate_error (error, ontology_error);

				g_clear_pointer (&seen_classes, g_ptr_array_unref);
				g_clear_pointer (&seen_properties, g_ptr_array_unref);

				if (ontos) {
					g_list_free_full (ontos, g_object_unref);
				}

				goto rollback_db_changes;
			}

			for (l = to_reload; l; l = l->next) {
				GFile *ontology_file = l->data;
				/* store ontology in database */
				import_ontology_file (manager, ontology_file, TRUE, &internal_error);
				if (internal_error) {
					g_propagate_error (error, internal_error);
					goto rollback_db_changes;
				}
			}
			g_list_free (to_reload);

			tracker_data_ontology_process_changes_post_import (seen_classes, seen_properties);

			if (!write_ontologies_gvdb (manager, TRUE /* overwrite */, &internal_error)) {
				g_propagate_error (error, internal_error);
				goto rollback_db_changes;
			}
		}

		g_clear_pointer (&seen_classes, g_ptr_array_unref);
		g_clear_pointer (&seen_properties, g_ptr_array_unref);

		/* Reset the is_new flag for all classes and properties */
		tracker_data_ontology_import_finished (manager);

		if (transaction_started) {
			tracker_data_commit_transaction (manager->data_update, &internal_error);
			if (internal_error) {
				g_propagate_error (error, internal_error);
				return FALSE;
			}
		}

		g_hash_table_unref (ontos_table);
		g_list_free_full (ontos, g_object_unref);

	}

	if (!read_only && is_create) {
		tracker_db_manager_set_current_locale (manager->db_manager);
		tracker_db_manager_tokenizer_update (manager->db_manager);
	} else if (!read_only && tracker_db_manager_locale_changed (manager->db_manager, NULL)) {
		/* If locale changed, re-create indexes.
		 * No need to reset the collator in the db interface,
		 * as this is only executed during startup, which should
		 * already have the proper locale set in the collator */
		tracker_data_manager_recreate_indexes (manager, &internal_error);

		if (internal_error) {
			g_propagate_error (error, internal_error);
			return FALSE;
		}

		tracker_db_manager_set_current_locale (manager->db_manager);

		if (!rebuild_fts_tokens (manager, iface, error))
			return FALSE;
	} else if (!read_only && tracker_db_manager_get_tokenizer_changed (manager->db_manager)) {
		if (!rebuild_fts_tokens (manager, iface, error))
			return FALSE;
	}

	if (!read_only) {
		tracker_ontologies_sort (manager->ontologies);
	}

	manager->initialized = TRUE;

	/* This is the only one which doesn't show the 'OPERATION' part */
	tracker_data_manager_update_status (manager, "Idle");

	return TRUE;

rollback_db_changes:
	tracker_data_ontology_import_finished (manager);
	tracker_data_rollback_transaction (manager->data_update);

	if (ontos_table) {
		g_hash_table_unref (ontos_table);
	}

	return FALSE;

rollback_newly_created_db:
	tracker_data_rollback_transaction (manager->data_update);
	tracker_db_manager_rollback_db_creation (manager->db_manager);
	return FALSE;
}

static gboolean
data_manager_perform_cleanup (TrackerDataManager  *manager,
                              TrackerDBInterface  *iface,
                              GError             **error)
{
	TrackerDBStatement *stmt;
	GError *internal_error = NULL;
	GHashTable *graphs;
	GHashTableIter iter;
	const gchar *graph;
	GString *str;

	graphs = tracker_data_manager_get_graphs (manager, FALSE);

	str = g_string_new ("WITH referencedElements(ID) AS ("
	                    "SELECT ID FROM \"main\".Refcount ");

	g_hash_table_iter_init (&iter, graphs);

	while (g_hash_table_iter_next (&iter, (gpointer*) &graph, NULL)) {
		g_string_append_printf (str,
		                        "UNION ALL SELECT ID FROM \"%s\".Refcount ",
		                        graph);
	}

	g_string_append (str, ") ");
	g_string_append_printf (str,
	                        "DELETE FROM Resource "
	                        "WHERE Resource.ID NOT IN (SELECT ID FROM referencedElements) "
	                        "AND Resource.ID NOT IN (SELECT ID FROM Graph)");
	g_hash_table_unref (graphs);

	stmt = tracker_db_interface_create_statement (iface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE,
	                                              &internal_error,
	                                              str->str);
	g_string_free (str, TRUE);

	if (!stmt)
		goto fail;

	tracker_db_statement_execute (stmt, &internal_error);
	g_object_unref (stmt);

fail:
	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	return TRUE;
}

void
tracker_data_manager_dispose (GObject *object)
{
	TrackerDataManager *manager = TRACKER_DATA_MANAGER (object);
	TrackerDBInterface *iface;
	GError *error = NULL;
	gboolean readonly = TRUE;

	g_clear_object (&manager->data_update);

	if (manager->db_manager) {
		readonly = (tracker_db_manager_get_flags (manager->db_manager, NULL, NULL) & TRACKER_DB_MANAGER_READONLY) != 0;

		if (!readonly) {
			/* Delete stale URIs in the Resource table */
			g_debug ("Cleaning up stale resource URIs");

			iface = tracker_db_manager_get_writable_db_interface (manager->db_manager);

			if (!data_manager_perform_cleanup (manager, iface, &error)) {
				g_warning ("Could not clean up stale resource URIs: %s\n",
				           error->message);
				g_clear_error (&error);
			}

			tracker_db_manager_check_perform_vacuum (manager->db_manager);
		}

		g_clear_object (&manager->db_manager);
	}

	g_clear_pointer (&manager->cached_connections, g_hash_table_unref);

	G_OBJECT_CLASS (tracker_data_manager_parent_class)->dispose (object);
}

void
tracker_data_manager_finalize (GObject *object)
{
	TrackerDataManager *manager = TRACKER_DATA_MANAGER (object);

	g_clear_object (&manager->ontologies);
	g_clear_object (&manager->ontology_location);
	g_clear_object (&manager->cache_location);
	g_clear_pointer (&manager->graphs, g_hash_table_unref);
	g_free (manager->status);
	g_mutex_clear (&manager->connections_lock);
	g_mutex_clear (&manager->graphs_lock);

	G_OBJECT_CLASS (tracker_data_manager_parent_class)->finalize (object);
}

static void
tracker_data_manager_initable_iface_init (GInitableIface *iface)
{
	iface->init = tracker_data_manager_initable_init;
}

static void
tracker_data_manager_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
	TrackerDataManager *manager = TRACKER_DATA_MANAGER (object);

	switch (prop_id) {
	case PROP_STATUS:
		g_value_set_string (value, manager->status);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_data_manager_class_init (TrackerDataManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = tracker_data_manager_get_property;
	object_class->dispose = tracker_data_manager_dispose;
	object_class->finalize = tracker_data_manager_finalize;

	g_object_class_install_property (object_class,
	                                 PROP_STATUS,
	                                 g_param_spec_string ("status",
	                                                      "Status",
	                                                      "Status",
	                                                      NULL,
	                                                      G_PARAM_READABLE));
}

TrackerOntologies *
tracker_data_manager_get_ontologies (TrackerDataManager *manager)
{
	return manager->ontologies;
}

TrackerDBManager *
tracker_data_manager_get_db_manager (TrackerDataManager *manager)
{
	return manager->db_manager;
}

TrackerDBInterface *
tracker_data_manager_get_db_interface (TrackerDataManager  *manager,
                                       GError             **error)
{
	if (!manager->db_manager) {
		g_set_error (error,
		             TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_QUERY_FAILED,
		             "Triple store is closing");
		return NULL;
	}

	return tracker_db_manager_get_db_interface (manager->db_manager, error);
}

TrackerDBInterface *
tracker_data_manager_get_writable_db_interface (TrackerDataManager *manager)
{
	return tracker_db_manager_get_writable_db_interface (manager->db_manager);
}

TrackerData *
tracker_data_manager_get_data (TrackerDataManager *manager)
{
	return manager->data_update;
}

void
tracker_data_manager_shutdown (TrackerDataManager *manager)
{
	g_object_run_dispose (G_OBJECT (manager));
}

GHashTable *
tracker_data_manager_get_namespaces (TrackerDataManager *manager)
{
	TrackerNamespace **namespaces;
	guint i, n_namespaces;
	GHashTable *ht;

	ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	namespaces = tracker_ontologies_get_namespaces (manager->ontologies,
	                                                &n_namespaces);
	for (i = 0; i < n_namespaces; i++) {
		g_hash_table_insert (ht,
		                     g_strdup (tracker_namespace_get_prefix (namespaces[i])),
		                     g_strdup (tracker_namespace_get_uri (namespaces[i])));
	}

	return ht;
}

static GHashTable *
copy_graphs (GHashTable *graphs)
{
	GHashTable *copy;
	GHashTableIter iter;
	gpointer key, value;

	copy = g_hash_table_new_full (g_str_hash,
				      g_str_equal,
				      g_free,
	                              (GDestroyNotify) tracker_rowid_free);
	g_hash_table_iter_init (&iter, graphs);

	while (g_hash_table_iter_next (&iter, &key, &value))
		g_hash_table_insert (copy, g_strdup (key), tracker_rowid_copy (value));

	return copy;
}

gboolean
tracker_data_manager_create_graph (TrackerDataManager  *manager,
                                   const gchar         *name,
                                   GError             **error)
{
	TrackerDBInterface *iface;
	TrackerRowid id;

	iface = tracker_db_manager_get_writable_db_interface (manager->db_manager);

	if (!tracker_db_manager_attach_database (manager->db_manager, iface,
	                                         name, TRUE, error))
		return FALSE;

	if (!tracker_data_ontology_setup_db (manager, iface, name,
	                                     FALSE, error))
		goto detach;

	if (!tracker_data_manager_init_fts (manager, iface, name, TRUE, error))
		goto detach;

	id = tracker_data_ensure_graph (manager->data_update, name, error);
	if (id == 0)
		goto detach;

	if (!manager->transaction_graphs)
		manager->transaction_graphs = copy_graphs (manager->graphs);

	g_hash_table_insert (manager->transaction_graphs, g_strdup (name),
	                     tracker_rowid_copy (&id));

	return TRUE;

detach:
	tracker_db_manager_detach_database (manager->db_manager, iface, name, NULL);
	return FALSE;
}

gboolean
tracker_data_manager_drop_graph (TrackerDataManager  *manager,
                                 const gchar         *name,
                                 GError             **error)
{
	TrackerDBInterface *iface;

	iface = tracker_db_manager_get_writable_db_interface (manager->db_manager);

	/* Silently refuse to drop the main graph, clear it instead */
	if (!name)
		return tracker_data_manager_clear_graph (manager, name, error);

	/* Ensure the current transaction doesn't keep tables in this database locked */
	tracker_data_commit_transaction (manager->data_update, NULL);
	tracker_data_begin_transaction (manager->data_update, NULL);

	if (!tracker_db_manager_detach_database (manager->db_manager, iface,
	                                         name, error))
		return FALSE;

	if (!tracker_data_delete_graph (manager->data_update, name, error))
		return FALSE;

	if (!manager->transaction_graphs)
		manager->transaction_graphs = copy_graphs (manager->graphs);

	g_hash_table_remove (manager->transaction_graphs, name);

	return TRUE;
}

TrackerRowid
tracker_data_manager_find_graph (TrackerDataManager *manager,
                                 const gchar        *name,
                                 gboolean            in_transaction)
{
	GHashTable *graphs;
	TrackerRowid graph_id;

	graphs = tracker_data_manager_get_graphs (manager, in_transaction);
	graph_id = GPOINTER_TO_UINT (g_hash_table_lookup (graphs, name));
	g_hash_table_unref (graphs);

	return graph_id;
}

gboolean
tracker_data_manager_clear_graph (TrackerDataManager  *manager,
                                  const gchar         *graph,
                                  GError             **error)
{
	TrackerOntologies *ontologies = manager->ontologies;
	TrackerClass **classes;
	TrackerProperty **properties;
	TrackerDBStatement *stmt;
	guint i, n_classes, n_properties;
	GError *inner_error = NULL;
	TrackerDBInterface *iface;

	if (!graph)
		graph = "main";

	iface = tracker_db_manager_get_writable_db_interface (manager->db_manager);
	classes = tracker_ontologies_get_classes (ontologies, &n_classes);
	properties = tracker_ontologies_get_properties (ontologies, &n_properties);

	for (i = 0; !inner_error && i < n_classes; i++) {
		if (g_str_has_prefix (tracker_class_get_name (classes[i]), "xsd:"))
			continue;

		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &inner_error,
		                                               "DELETE FROM \"%s\".\"%s\"",
		                                               graph,
		                                               tracker_class_get_name (classes[i]));
		if (!stmt)
			goto out;

		tracker_db_statement_execute (stmt, &inner_error);
		g_object_unref (stmt);
	}

	for (i = 0; !inner_error && i < n_properties; i++) {
		TrackerClass *service;

		if (!tracker_property_get_multiple_values (properties[i]))
			continue;

		service = tracker_property_get_domain (properties[i]);
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &inner_error,
		                                               "DELETE FROM \"%s\".\"%s_%s\"",
		                                               graph,
		                                               tracker_class_get_name (service),
		                                               tracker_property_get_name (properties[i]));
		if (!stmt)
			goto out;

		tracker_db_statement_execute (stmt, &inner_error);
		g_object_unref (stmt);
	}

	tracker_db_interface_execute_query (iface,
					    &inner_error,
					    "DELETE FROM \"%s\".Refcount",
					    graph);
out:

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_data_manager_copy_graph (TrackerDataManager  *manager,
                                 const gchar         *source,
                                 const gchar         *destination,
                                 GError             **error)
{
	TrackerOntologies *ontologies = manager->ontologies;
	TrackerClass **classes;
	TrackerProperty **properties;
	TrackerDBStatement *stmt;
	guint i, n_classes, n_properties;
	GError *inner_error = NULL;
	TrackerDBInterface *iface;

	if (!source)
		source = "main";
	if (!destination)
		destination = "main";

	if (strcmp (source, destination) == 0)
		return TRUE;

	iface = tracker_db_manager_get_writable_db_interface (manager->db_manager);
	classes = tracker_ontologies_get_classes (ontologies, &n_classes);
	properties = tracker_ontologies_get_properties (ontologies, &n_properties);

	for (i = 0; !inner_error && i < n_classes; i++) {
		if (g_str_has_prefix (tracker_class_get_name (classes[i]), "xsd:"))
			continue;

		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &inner_error,
		                                               "INSERT OR REPLACE INTO \"%s\".\"%s\" "
		                                               "SELECT * from \"%s\".\"%s\"",
		                                               destination,
		                                               tracker_class_get_name (classes[i]),
		                                               source,
		                                               tracker_class_get_name (classes[i]));
		if (!stmt)
			break;

		tracker_db_statement_execute (stmt, &inner_error);
		g_object_unref (stmt);
	}

	for (i = 0; !inner_error && i < n_properties; i++) {
		TrackerClass *service;

		if (!tracker_property_get_multiple_values (properties[i]))
			continue;

		service = tracker_property_get_domain (properties[i]);
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &inner_error,
		                                               "INSERT OR REPLACE INTO \"%s\".\"%s_%s\" "
		                                               "SELECT * from \"%s\".\"%s_%s\"",
		                                               destination,
		                                               tracker_class_get_name (service),
		                                               tracker_property_get_name (properties[i]),
		                                               source,
		                                               tracker_class_get_name (service),
		                                               tracker_property_get_name (properties[i]));
		if (!stmt)
			goto out;

		tracker_db_statement_execute (stmt, &inner_error);
		g_object_unref (stmt);
	}

	/* Transfer refcounts */
	tracker_db_interface_execute_query (iface,
	                                    &inner_error,
	                                    "INSERT OR IGNORE INTO \"%s\".Refcount "
	                                    "SELECT ID, 0 from \"%s\".Refcount",
	                                    destination,
	                                    source);
	if (inner_error)
		goto out;

	tracker_db_interface_execute_query (iface,
	                                    &inner_error,
	                                    "UPDATE \"%s\".Refcount AS B "
	                                    "SET Refcount = Refcount + "
	                                    "(SELECT Refcount FROM \"%s\".Refcount AS A "
	                                    "WHERE B.ID = A.ID)",
	                                    destination, source);
out:
	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}

guint
tracker_data_manager_get_generation (TrackerDataManager *manager)
{
	return manager->generation;
}

void
tracker_data_manager_commit_graphs (TrackerDataManager *manager)
{
	g_mutex_lock (&manager->graphs_lock);

	if (manager->transaction_graphs) {
		g_clear_pointer (&manager->graphs, g_hash_table_unref);
		manager->graphs = manager->transaction_graphs;
		manager->transaction_graphs = NULL;
		manager->generation++;
	}

	g_mutex_unlock (&manager->graphs_lock);
}

void
tracker_data_manager_rollback_graphs (TrackerDataManager *manager)
{
	g_clear_pointer (&manager->transaction_graphs, g_hash_table_unref);
}

void
tracker_data_manager_release_memory (TrackerDataManager *manager)
{
	tracker_db_manager_release_memory (manager->db_manager);
}

gboolean
tracker_data_manager_expand_prefix (TrackerDataManager  *manager,
                                    const gchar         *term,
                                    GHashTable          *prefix_map,
                                    gchar              **prefix,
                                    gchar              **expanded)
{
	const gchar *sep, *expanded_ns = NULL;
	TrackerOntologies *ontologies;
	TrackerNamespace **namespaces;
	guint n_namespaces, i;
	gchar *ns;

	sep = strchr (term, ':');

	if (sep) {
		ns = g_strndup (term, sep - term);
		sep++;
	} else {
		ns = g_strdup (term);
	}

	if (prefix_map)
		expanded_ns = g_hash_table_lookup (prefix_map, ns);

	if (!expanded_ns) {
		ontologies = tracker_data_manager_get_ontologies (manager);
		namespaces = tracker_ontologies_get_namespaces (ontologies, &n_namespaces);

		for (i = 0; i < n_namespaces; i++) {
			if (!g_str_equal (ns, tracker_namespace_get_prefix (namespaces[i])))
				continue;

			expanded_ns = tracker_namespace_get_uri (namespaces[i]);

			if (prefix_map)
				g_hash_table_insert (prefix_map, g_strdup (ns), g_strdup (expanded_ns));
			break;
		}
	}

	g_free (ns);

	if (!expanded_ns) {
		if (prefix)
			*prefix = NULL;
		if (expanded)
			*expanded = g_strdup (term);

		return FALSE;
	}

	if (prefix)
		*prefix = g_strdup (expanded_ns);

	if (expanded) {
		if (sep) {
			*expanded = g_strconcat (expanded_ns, sep, NULL);
		} else {
			*expanded = g_strdup (expanded_ns);
		}
	}

	return TRUE;
}

TrackerSparqlConnection *
tracker_data_manager_get_remote_connection (TrackerDataManager  *data_manager,
                                            const gchar         *uri,
                                            GError             **error)
{
	TrackerSparqlConnection *connection = NULL;
	GError *inner_error = NULL;
	gchar *uri_scheme = NULL;

	g_mutex_lock (&data_manager->connections_lock);

	connection = g_hash_table_lookup (data_manager->cached_connections, uri);

	if (!connection) {
		uri_scheme = g_uri_parse_scheme (uri);
		if (g_strcmp0 (uri_scheme, "dbus") == 0) {
			gchar *bus_name, *object_path;
			GDBusConnection *dbus_connection;
			GBusType bus_type;

			if (!tracker_util_parse_dbus_uri (uri,
			                                  &bus_type,
			                                  &bus_name, &object_path)) {
				g_set_error (&inner_error,
					     TRACKER_SPARQL_ERROR,
					     TRACKER_SPARQL_ERROR_PARSE,
					     "Failed to parse uri '%s'",
					     uri);
				goto fail;
			}

			if (!g_dbus_is_name (bus_name)) {
				g_set_error (&inner_error,
				             TRACKER_SPARQL_ERROR,
				             TRACKER_SPARQL_ERROR_PARSE,
				             "Invalid bus name '%s'",
				             bus_name);
				goto fail;
			}

			dbus_connection = g_bus_get_sync (bus_type, NULL, &inner_error);
			if (!dbus_connection)
				goto fail;

			connection = tracker_sparql_connection_bus_new (bus_name, object_path,
			                                                dbus_connection, &inner_error);
			g_free (bus_name);
			g_free (object_path);
			g_object_unref (dbus_connection);

			if (!connection)
				goto fail;
		} else if (g_strcmp0 (uri_scheme, "http") == 0) {
			connection = tracker_sparql_connection_remote_new (uri);
		}

		if (!connection) {
			g_set_error (&inner_error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_UNSUPPORTED,
			             "Unsupported uri '%s'",
			             uri);
			goto fail;
		}

		g_hash_table_insert (data_manager->cached_connections,
		                     g_strdup (uri),
		                     connection);
	}

fail:
	g_mutex_unlock (&data_manager->connections_lock);
	g_free (uri_scheme);

	if (inner_error)
		g_propagate_error (error, inner_error);

	return connection;
}

void
tracker_data_manager_map_connection (TrackerDataManager      *data_manager,
                                     const gchar             *handle_name,
                                     TrackerSparqlConnection *connection)
{
	gchar *uri;

	uri = g_strdup_printf ("private:%s", handle_name);

	g_mutex_lock (&data_manager->connections_lock);
	g_hash_table_insert (data_manager->cached_connections,
	                     uri, g_object_ref (connection));
	g_mutex_unlock (&data_manager->connections_lock);
}
