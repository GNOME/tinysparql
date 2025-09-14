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

#include <tracker-common.h>

#include "tracker-deserializer-rdf.h"

#include "core/tracker-class.h"
#include "core/tracker-data-manager.h"
#include "core/tracker-data-update.h"
#include "core/tracker-db-interface-sqlite.h"
#include "core/tracker-db-manager.h"
#include "core/tracker-namespace.h"
#include "core/tracker-ontologies.h"
#include "core/tracker-ontologies-diff.h"
#include "core/tracker-ontologies-introspect.h"
#include "core/tracker-ontologies-rdf.h"
#include "core/tracker-ontology.h"
#include "core/tracker-property.h"
#include "core/tracker-data-query.h"
#include "core/tracker-sparql-parser.h"

#define MAX_HTTP_URI_LEN 16000 /* De-facto limit in browsers is 8KB, double that for good measure */

struct _TrackerDataManager {
	GObject parent_instance;

	GFile *ontology_location;
	GFile *cache_location;
	guint initialized      : 1;
	guint flags;

	gint select_cache_size;
	guint generation;

	TrackerDBManager *db_manager;
	TrackerOntologies *ontologies;
	TrackerData *data_update;

	GHashTable *transaction_graphs;
	GHashTable *graphs;
	GMutex graphs_lock;
	TrackerRowid main_graph_id;

	/* Cached remote connections */
	GMutex connections_lock;
	GHashTable *cached_connections;
};

struct _TrackerDataManagerClass {
	GObjectClass parent_instance;
};

typedef struct {
	const gchar *from;
	const gchar *to;
} Conversion;

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

static void tracker_data_manager_initable_iface_init (GInitableIface *iface);

static gboolean tracker_data_manager_apply_db_changes (TrackerDataManager     *manager,
                                                       TrackerDBInterface     *iface,
                                                       const gchar            *graph,
                                                       TrackerOntologies      *db_ontology,
                                                       TrackerOntologies      *current_ontology,
                                                       TrackerOntologyChange  *changes,
                                                       gint                    n_changes,
                                                       GError                **error);

G_DEFINE_TYPE_WITH_CODE (TrackerDataManager, tracker_data_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, tracker_data_manager_initable_iface_init))

static void
tracker_data_manager_init (TrackerDataManager *manager)
{
	manager->generation = 1;
	manager->cached_connections =
		g_hash_table_new_full (g_str_hash, g_str_equal,
		                       g_free, g_object_unref);
	manager->graphs = g_hash_table_new_full (g_str_hash,
	                                         g_str_equal,
	                                         g_free,
	                                         (GDestroyNotify) tracker_rowid_free);

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
	TrackerSparqlCursor *cursor = NULL;
	TrackerDBStatement *stmt;

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
	                                              "SELECT ID, Uri FROM Resource WHERE ID IN (SELECT ID FROM Graph)");
	if (!stmt)
		return FALSE;

	cursor = TRACKER_SPARQL_CURSOR (tracker_db_statement_start_cursor (stmt, error));
	g_object_unref (stmt);

	if (!cursor)
		return FALSE;

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		const gchar *name;
		TrackerRowid id;

		id = tracker_sparql_cursor_get_integer (cursor, 0);
		name = tracker_sparql_cursor_get_string (cursor, 1, NULL);

		g_hash_table_insert (manager->graphs, g_strdup (name),
		                     tracker_rowid_copy (&id));
	}

	g_object_unref (cursor);

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

static gboolean
create_class_table (TrackerDataManager  *manager,
                    TrackerDBInterface  *iface,
                    const gchar         *graph,
                    TrackerClass        *class,
                    GError             **error)
{
	TrackerOntologies *ontologies;
	TrackerProperty **properties;
	GString *sql = NULL;
	const char *class_name;
	guint i, n_props;
	gboolean retval;

	class_name = tracker_class_get_name (class);

	if (g_str_has_prefix (class_name, "xsd:")) {
		/* xsd classes do not derive from rdfs:Resource and do not need separate tables */
		return TRUE;
	}

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Creating table for class %s",
	                         class_name));

	sql = g_string_new ("");
	g_string_append_printf (sql, "CREATE TABLE \"%s%s%s\" (ID INTEGER NOT NULL PRIMARY KEY",
	                        graph ? graph : "",
	                        graph ? "_" : "",
	                        class_name);

	/* Properties related to this new class table are inlined here */
	ontologies = tracker_class_get_ontologies (class);
	properties = tracker_ontologies_get_properties (ontologies, &n_props);

	for (i = 0; i < n_props; i++) {
		TrackerProperty *property = properties[i];
		const gchar *sql_type;
		const gchar *sql_collation;
		const gchar *field_name;

		if (tracker_property_get_domain (property) != class)
			continue;
		if (tracker_property_get_multiple_values (property))
			continue;

		field_name = tracker_property_get_name (property);
		property_get_sql_representation (property, &sql_type, &sql_collation);

		g_string_append_printf (sql, ", \"%s\" %s",
		                        field_name, sql_type);

		if (sql_collation)
			g_string_append_printf (sql, " COLLATE %s", sql_collation);
	}

	g_string_append (sql, ")");
	retval = tracker_db_interface_execute_query (iface, error, "%s", sql->str);
	g_string_free (sql, TRUE);

	return retval;
}

static gboolean
increase_refcount (TrackerDataManager  *manager,
                   TrackerDBInterface  *iface,
                   const gchar         *graph,
                   const gchar         *table_prefix,
                   const gchar         *table_suffix,
                   const gchar         *column,
                   const gchar         *query_modifier,
                   GError             **error)
{
	if (!tracker_db_interface_execute_query (iface, error,
	                                         "INSERT OR IGNORE INTO \"%s%sRefcount\" (ROWID, Refcount) "
	                                         "SELECT \"%s\", 0 FROM \"%s%s%s%s%s\"",
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         column,
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         table_prefix ? table_prefix : "",
	                                         table_prefix && table_suffix ? "_" : "",
	                                         table_suffix ? table_suffix : ""))
		return FALSE;

	return tracker_db_interface_execute_query (iface, error,
	                                           "UPDATE \"%s%sRefcount\" SET Refcount=Refcount+1 "
	                                           "WHERE ID IN (SELECT \"%s\" FROM \"%s%s%s%s%s\" ORDER BY \"%s\" DESC %s)",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           column,
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           table_prefix ? table_prefix : "",
	                                           table_prefix && table_suffix ? "_" : "",
	                                           table_suffix ? table_suffix : "",
	                                           column,
	                                           query_modifier ? query_modifier : "");
}

static gboolean
decrease_refcount (TrackerDataManager  *manager,
                   TrackerDBInterface  *iface,
                   const gchar         *graph,
                   const gchar         *table_prefix,
                   const gchar         *table_suffix,
                   const gchar         *column,
                   GError             **error)
{
	if (!tracker_db_interface_execute_query (iface, error,
	                                         "UPDATE \"%s%sRefcount\" SET Refcount=Refcount-1 "
	                                         "WHERE ID IN (SELECT \"%s\" FROM \"%s%s%s%s%s\")",
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         column,
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         table_prefix ? table_prefix : "",
	                                         table_prefix && table_suffix ? "_" : "",
	                                         table_suffix ? table_suffix : ""))
		return FALSE;

	return tracker_db_interface_execute_query (iface, error,
	                                           "DELETE FROM \"%s%sRefcount\" WHERE Refcount=0 "
	                                           "AND ID IN (SELECT \"%s\" FROM \"%s%s%s%s%s\")",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           column,
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           table_prefix ? table_prefix : "",
	                                           table_prefix && table_suffix ? "_" : "",
	                                           table_suffix ? table_suffix : "");
}

static gboolean
drop_class_table (TrackerDataManager  *manager,
                  TrackerDBInterface  *iface,
                  const gchar         *graph,
                  TrackerClass        *class,
                  GError             **error)
{
	const gchar *class_name;

	class_name = tracker_class_get_name (class);

	if (g_str_has_prefix (class_name, "xsd:")) {
		/* xsd classes do not derive from rdfs:Resource and do not need separate tables */
		return TRUE;
	}

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Dropping table for class %s",
	                         tracker_class_get_name (class)));

	if (!decrease_refcount (manager, iface, graph,
	                        NULL, tracker_class_get_name (class),
	                        "ID", error))
		return FALSE;

	return tracker_db_interface_execute_query (iface, error, "DROP TABLE \"%s%s%s\"",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (class));
}

static gboolean
alter_class_table_for_added_property (TrackerDataManager  *manager,
                                      TrackerDBInterface  *iface,
                                      const gchar         *graph,
                                      TrackerClass        *class,
                                      TrackerProperty     *property,
                                      GError             **error)
{
	const char *sql_type, *sql_collation;

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Adding column for property %s on class %s",
	                         tracker_property_get_name (property),
	                         tracker_class_get_name (class)));

	property_get_sql_representation (property, &sql_type, &sql_collation);

	return tracker_db_interface_execute_query (iface, error,
	                                           "ALTER TABLE \"%s%s%s\" ADD COLUMN \"%s\" %s %s %s",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (class),
	                                           tracker_property_get_name (property),
	                                           sql_type,
	                                           sql_collation ? "COLLATE" : "",
	                                           sql_collation ? sql_collation : "");
}

static gboolean
alter_class_table_for_removed_property (TrackerDataManager  *manager,
                                        TrackerDBInterface  *iface,
                                        const gchar         *graph,
                                        TrackerClass        *class,
                                        TrackerProperty     *property,
                                        GError             **error)
{
	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Removing column for property %s from class %s",
	                         tracker_property_get_name (property),
	                         tracker_class_get_name (class)));

	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE &&
	    !decrease_refcount (manager, iface, graph,
	                        NULL, tracker_class_get_name (class),
	                        tracker_property_get_name (property),
	                        error))
		return FALSE;

	return tracker_db_interface_execute_query (iface, error,
	                                           "ALTER TABLE \"%s%s%s\" DROP COLUMN \"%s\"",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (class),
	                                           tracker_property_get_name (property));
}

static gboolean
alter_class_table_for_type_change (TrackerDataManager  *manager,
                                   TrackerDBInterface  *iface,
                                   const gchar         *graph,
                                   TrackerClass        *class,
                                   TrackerProperty     *property,
                                   GError             **error)
{
	const char *sql_type;

	property_get_sql_representation (property, &sql_type, NULL);

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Altering class table %s to change property column %s type to %s",
	                         tracker_class_get_name (class),
	                         tracker_property_get_name (property),
	                         sql_type));

	if (!tracker_db_interface_execute_query (iface, error,
	                                         "CREATE TEMP TABLE \"TMP_%s\" (ROWID, value %s)",
	                                         tracker_property_get_name (property),
	                                         sql_type))
		return FALSE;

	if (!tracker_db_interface_execute_query (iface, error,
	                                         "INSERT INTO \"TMP_%s\" (ROWID, value) "
	                                         "SELECT ID, CAST (\"%s\" AS %s) FROM \"%s%s%s\"",
	                                         tracker_property_get_name (property),
	                                         tracker_property_get_name (property),
	                                         sql_type,
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         tracker_class_get_name (class)))
		return FALSE;

	if (!alter_class_table_for_removed_property (manager, iface, graph,
	                                             class, property, error))
		return FALSE;

	if (!alter_class_table_for_added_property (manager, iface, graph,
	                                           class, property, error))
		return FALSE;

	if (!tracker_db_interface_execute_query (iface, error,
	                                         "UPDATE \"%s%s%s\" AS A "
	                                         "SET \"%s\" = (SELECT value FROM \"TMP_%s\" WHERE ROWID = A.ID)",
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         tracker_class_get_name (class),
	                                         tracker_property_get_name (property),
	                                         tracker_property_get_name (property)))
		return FALSE;

	return tracker_db_interface_execute_query (iface, error,
	                                           "DROP TABLE \"TMP_%s\"",
	                                           tracker_property_get_name (property));
}

static gboolean
create_multivalue_property_table (TrackerDataManager  *manager,
                                  TrackerDBInterface  *iface,
                                  const gchar         *graph,
                                  TrackerClass        *class,
                                  TrackerProperty     *property,
                                  GError             **error)
{
	const char *sql_type, *sql_collation, *func = NULL;

	property_get_sql_representation (property, &sql_type, &sql_collation);

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Creating table for multi-value property %s",
	                         tracker_property_get_name (property)));

	if (!tracker_db_interface_execute_query (iface, error,
	                                         "CREATE TABLE \"%s%s%s_%s\" ("
	                                         "ID INTEGER NOT NULL, "
	                                         "\"%s\" %s %s %s NOT NULL)",
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         tracker_class_get_name (class),
	                                         tracker_property_get_name (property),
	                                         tracker_property_get_name (property),
	                                         sql_type,
	                                         sql_collation ? "COLLATE" : "",
	                                         sql_collation ? sql_collation : ""))
		return FALSE;

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Creating constraint index for multi-value property %s",
	                         tracker_property_get_name (property)));

	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME)
		func = "SparqlTimeSort";

	/* We use an unique index to ensure uniqueness of ID/value pairs */
	if (!tracker_db_interface_execute_query (iface, error,
	                                         "CREATE UNIQUE INDEX \"%s%s%s_%s_ID_ID\" ON \"%s%s%s_%s\" (ID, %s%s\"%s\"%s)",
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         tracker_class_get_name (class),
	                                         tracker_property_get_name (property),
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         tracker_class_get_name (class),
	                                         tracker_property_get_name (property),
	                                         func ? func : "",
	                                         func ? "(" : "",
	                                         tracker_property_get_name (property),
	                                         func ? ")" : ""))
		return FALSE;

	return TRUE;
}

static gboolean
drop_multivalue_property_table (TrackerDataManager  *manager,
                                TrackerDBInterface  *iface,
                                const gchar         *graph,
                                TrackerClass        *class,
                                TrackerProperty     *property,
                                GError             **error)
{
	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Dropping multi-valued Adding column for property %s on class %s",
	                         tracker_property_get_name (property),
	                         tracker_class_get_name (class)));

	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE &&
	    !decrease_refcount (manager, iface, graph,
	                        tracker_class_get_name (class),
	                        tracker_property_get_name (property),
	                        tracker_property_get_name (property),
	                        error))
		return FALSE;

	if (!tracker_db_interface_execute_query (iface, error,
	                                         "DROP INDEX \"%s%s%s_%s_ID_ID\"",
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         tracker_class_get_name (class),
	                                         tracker_property_get_name (property)))
		return FALSE;

	if (!tracker_db_interface_execute_query (iface, error, "DROP TABLE \"%s%s%s_%s\"",
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         tracker_class_get_name (class),
	                                         tracker_property_get_name (property)))
		return FALSE;

	return TRUE;
}

static gboolean
alter_multivalue_property_table_for_type_change (TrackerDataManager  *manager,
                                                 TrackerDBInterface  *iface,
                                                 const gchar         *graph,
                                                 TrackerClass        *class,
                                                 TrackerProperty     *property,
                                                 GError             **error)
{
	const char *sql_type;

	property_get_sql_representation (property, &sql_type, NULL);

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Altering multi-value property table %s type to %s",
	                         tracker_property_get_name (property),
	                         sql_type));

	if (!tracker_db_interface_execute_query (iface, error,
	                                         "CREATE TEMP TABLE \"TMP_%s_%s\" (ID, value %s)",
	                                         tracker_class_get_name (class),
	                                         tracker_property_get_name (property),
	                                         sql_type))
		return FALSE;

	if (!tracker_db_interface_execute_query (iface, error,
	                                         "INSERT INTO \"TMP_%s_%s\" (ROWID, ID, value) "
	                                         "SELECT ROWID, ID, CAST (\"%s\" AS %s) FROM \"%s%s%s_%s\"",
	                                         tracker_class_get_name (class),
	                                         tracker_property_get_name (property),
	                                         tracker_property_get_name (property),
	                                         sql_type,
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         tracker_class_get_name (class),
	                                         tracker_property_get_name (property)))
		return FALSE;

	if (!drop_multivalue_property_table (manager, iface, graph,
	                                     class, property, error))
		return FALSE;

	if (!create_multivalue_property_table (manager, iface, graph,
	                                       class, property, error))
		return FALSE;

	if (!tracker_db_interface_execute_query (iface, error,
	                                         "INSERT INTO \"%s%s%s_%s\"(ROWID, ID, \"%s\") "
	                                         "SELECT ROWID, ID, value FROM \"TMP_%s_%s\"",
	                                         graph ? graph : "",
	                                         graph ? "_" : "",
	                                         tracker_class_get_name (class),
	                                         tracker_property_get_name (property),
	                                         tracker_property_get_name (property),
	                                         tracker_class_get_name (class),
	                                         tracker_property_get_name (property)))
		return FALSE;

	return tracker_db_interface_execute_query (iface, error,
	                                           "DROP TABLE \"TMP_%s_%s\"",
	                                           tracker_class_get_name (class),
	                                           tracker_property_get_name (property));
}

static gboolean
create_index (TrackerDataManager  *manager,
              TrackerDBInterface  *iface,
              const gchar         *graph,
              TrackerClass        *class,
              TrackerProperty     *property,
              GError             **error)
{
	if (tracker_property_get_multiple_values (property)) {
		TRACKER_NOTE (ONTOLOGY_CHANGES,
		              g_message ("Creating index for multi-value property %s",
		                         tracker_property_get_name (property)));

		return tracker_db_interface_execute_query (iface, error,
		                                           "CREATE INDEX \"%s%s%s_%s_ID\" ON \"%s%s%s_%s\" (ID)",
		                                           graph ? graph : "",
		                                           graph ? "_" : "",
		                                           tracker_class_get_name (class),
		                                           tracker_property_get_name (property),
		                                           graph ? graph : "",
		                                           graph ? "_" : "",
		                                           tracker_class_get_name (class),
		                                           tracker_property_get_name (property));
	} else {
		const gchar *func = NULL;

		TRACKER_NOTE (ONTOLOGY_CHANGES,
		              g_message ("Creating index for single-value property %s",
		                         tracker_property_get_name (property)));

		if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME)
			func = "SparqlTimeSort";

		return tracker_db_interface_execute_query (iface, error,
		                                           "CREATE INDEX \"%s%s%s_%s\" ON \"%s%s%s\" (%s%s\"%s\"%s)",
		                                           graph ? graph : "",
		                                           graph ? "_" : "",
		                                           tracker_class_get_name (class),
		                                           tracker_property_get_name (property),
		                                           graph ? graph : "",
		                                           graph ? "_" : "",
		                                           tracker_class_get_name (class),
		                                           func ? func : "",
		                                           func ? "(" : "",
		                                           tracker_property_get_name (property),
		                                           func ? ")" : "");
	}
}

static gboolean
drop_index (TrackerDataManager  *manager,
            TrackerDBInterface  *iface,
            const gchar         *graph,
            TrackerClass        *class,
            TrackerProperty     *property,
            GError             **error)
{
	if (tracker_property_get_multiple_values (property)) {
		TRACKER_NOTE (ONTOLOGY_CHANGES,
		              g_message ("Dropping index from multi-value property %s",
		                         tracker_property_get_name (property)));

		return tracker_db_interface_execute_query (iface, error,
		                                           "DROP INDEX \"%s%s%s_%s_ID\"",
		                                           graph ? graph : "",
		                                           graph ? "_" : "",
		                                           tracker_class_get_name (class),
		                                           tracker_property_get_name (property));
	} else {
		TRACKER_NOTE (ONTOLOGY_CHANGES,
		              g_message ("Dropping index from single-value property %s",
		                         tracker_property_get_name (property)));

		return tracker_db_interface_execute_query (iface, error,
		                                           "DROP INDEX \"%s%s%s_%s\"",
		                                           graph ? graph : "",
		                                           graph ? "_" : "",
		                                           tracker_class_get_name (class),
		                                           tracker_property_get_name (property));
	}
}

static gboolean
create_secondary_index (TrackerDataManager  *manager,
                        TrackerDBInterface  *iface,
                        const gchar         *graph,
                        TrackerProperty     *property,
                        TrackerProperty     *secondary,
                        GError             **error)
{
	TrackerClass *class;

	g_assert (!tracker_property_get_multiple_values (property) &&
	          !tracker_property_get_multiple_values (secondary));

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Creating secondary index for single-value property pair %s / %s",
	                         tracker_property_get_name (property),
	                         tracker_property_get_name (secondary)));

	class = tracker_property_get_domain (property);

	return tracker_db_interface_execute_query (iface, error,
	                                           "CREATE INDEX \"%s%s%s_%s_%s\" ON \"%s%s%s\" (\"%s\", \"%s\")",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (class),
	                                           tracker_property_get_name (property),
	                                           tracker_property_get_name (secondary),
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (class),
	                                           tracker_property_get_name (property),
	                                           tracker_property_get_name (secondary));
}

static gboolean
drop_secondary_index (TrackerDataManager  *manager,
                      TrackerDBInterface  *iface,
                      const gchar         *graph,
                      TrackerProperty     *property,
                      TrackerProperty     *secondary,
                      GError             **error)
{
	TrackerClass *class;

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Dropping secondary index for single-value property pair %s / %s",
	                         tracker_property_get_name (property),
	                         tracker_property_get_name (secondary)));

	class = tracker_property_get_domain (property);

	return tracker_db_interface_execute_query (iface, error,
	                                           "DROP INDEX \"%s%s%s_%s_%s\"",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (class),
	                                           tracker_property_get_name (property),
	                                           tracker_property_get_name (secondary));
}

static gboolean
create_unique_index (TrackerDataManager  *manager,
                     TrackerDBInterface  *iface,
                     const gchar         *graph,
                     TrackerProperty     *property,
                     GError             **error)
{
	TrackerClass *class;

	class = tracker_property_get_domain (property);

	if (tracker_property_get_multiple_values (property)) {
		TRACKER_NOTE (ONTOLOGY_CHANGES,
		              g_message ("Creating unique index for multi-value property %s",
		                         tracker_property_get_name (property)));

		return tracker_db_interface_execute_query (iface, error,
		                                           "CREATE UNIQUE INDEX \"%s%s%s_%s_unique\" ON \"%s%s%s_%s\" (\"%s\")",
		                                           graph ? graph : "",
		                                           graph ? "_" : "",
		                                           tracker_class_get_name (class),
		                                           tracker_property_get_name (property),
		                                           graph ? graph : "",
		                                           graph ? "_" : "",
		                                           tracker_class_get_name (class),
		                                           tracker_property_get_name (property),
		                                           tracker_property_get_name (property));
	} else {
		TRACKER_NOTE (ONTOLOGY_CHANGES,
		              g_message ("Creating unique index for single-value property %s",
		                         tracker_property_get_name (property)));

		return tracker_db_interface_execute_query (iface, error,
		                                           "CREATE UNIQUE INDEX \"%s%s%s_%s_unique\" ON \"%s%s%s\" (\"%s\")",
		                                           graph ? graph : "",
		                                           graph ? "_" : "",
		                                           tracker_class_get_name (class),
		                                           tracker_property_get_name (property),
		                                           graph ? graph : "",
		                                           graph ? "_" : "",
		                                           tracker_class_get_name (class),
		                                           tracker_property_get_name (property));
	}
}

static gboolean
drop_unique_index (TrackerDataManager  *manager,
                   TrackerDBInterface  *iface,
                   const gchar         *graph,
                   TrackerProperty     *property,
                   GError             **error)
{
	TrackerClass *class;

	TRACKER_NOTE (ONTOLOGY_CHANGES,
	              g_message ("Dropping unique index for property %s",
	                         tracker_property_get_name (property)));

	class = tracker_property_get_domain (property);

	return tracker_db_interface_execute_query (iface, error,
	                                           "DROP INDEX \"%s%s%s_%s_unique\"",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (class),
	                                           tracker_property_get_name (property));
}

static gboolean
copy_from_class (TrackerDataManager  *manager,
                 TrackerDBInterface  *iface,
                 const gchar         *graph,
                 TrackerClass        *class,
                 TrackerClass        *target,
                 GError             **error)
{
	if (!increase_refcount (manager, iface, graph,
	                        NULL, tracker_class_get_name (class),
	                        "ID", NULL, error))
		return FALSE;

	return tracker_db_interface_execute_query (iface, error,
	                                           "INSERT OR IGNORE INTO \"%s%s%s\"(ID) SELECT ID FROM \"%s%s%s\"",
						   graph ? graph : "",
						   graph ? "_" : "",
	                                           tracker_class_get_name (target),
						   graph ? graph : "",
						   graph ? "_" : "",
	                                           tracker_class_get_name (class));
}

static gboolean
copy_single_value (TrackerDataManager  *manager,
                   TrackerDBInterface  *iface,
                   const gchar         *graph,
                   TrackerClass        *class,
                   TrackerProperty     *property,
                   TrackerClass        *target_class,
                   TrackerProperty     *target,
                   GError             **error)
{
	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE &&
	    !increase_refcount (manager, iface, graph,
	                        NULL, tracker_class_get_name (class),
	                        tracker_property_get_name (property),
	                        NULL, error))
		return FALSE;

	return tracker_db_interface_execute_query (iface, error,
	                                           "UPDATE \"%s%s%s\" AS A SET \"%s\" = "
	                                           "(SELECT \"%s\" FROM \"%s%s%s\" AS B WHERE A.ID = B.ID)",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (target_class),
	                                           tracker_property_get_name (target),
	                                           tracker_property_get_name (property),
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (class));
}

static gboolean
copy_multi_value (TrackerDataManager  *manager,
                  TrackerDBInterface  *iface,
                  const gchar         *graph,
                  TrackerClass        *class,
                  TrackerProperty     *property,
                  TrackerClass        *target_class,
                  TrackerProperty     *target,
                  GError             **error)
{
	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE &&
	    !increase_refcount (manager, iface, graph,
	                        tracker_class_get_name (class),
	                        tracker_property_get_name (property),
	                        tracker_property_get_name (property),
	                        NULL, error))
		return FALSE;

	return tracker_db_interface_execute_query (iface, error,
	                                           "INSERT OR IGNORE INTO \"%s%s%s_%s\"(ID, \"%s\") SELECT ID, \"%s\" FROM \"%s%s%s_%s\"",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (target_class),
	                                           tracker_property_get_name (target),
	                                           tracker_property_get_name (target),
	                                           tracker_property_get_name (property),
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (class),
	                                           tracker_property_get_name (property));
}

static gboolean
copy_multi_value_to_single_value (TrackerDataManager  *manager,
                                  TrackerDBInterface  *iface,
                                  const gchar         *graph,
                                  TrackerClass        *class,
                                  TrackerProperty     *property,
                                  TrackerClass        *target_class,
                                  TrackerProperty     *target,
                                  GError             **error)
{
	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE &&
	    !increase_refcount (manager, iface, graph,
	                        NULL, tracker_class_get_name (class),
	                        tracker_property_get_name (property),
	                        "LIMIT 1",
	                        error))
		return FALSE;

	return tracker_db_interface_execute_query (iface, error,
	                                           "UPDATE \"%s%s%s\" AS A SET \"%s\" = "
	                                           "(SELECT \"%s\" FROM \"%s%s%s_%s\" AS B WHERE A.ID = B.ID ORDER BY \"%s\" DESC LIMIT 1)",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (target_class),
	                                           tracker_property_get_name (target),
	                                           tracker_property_get_name (property),
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           tracker_class_get_name (class),
	                                           tracker_property_get_name (property),
	                                           tracker_property_get_name (property));
}

static gboolean
copy_single_value_to_multi_value (TrackerDataManager  *manager,
                                  TrackerDBInterface  *iface,
                                  const gchar         *graph,
                                  TrackerClass        *class,
                                  TrackerProperty     *property,
                                  TrackerClass        *target_class,
                                  TrackerProperty     *target,
                                  GError             **error)
{
	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE &&
	    !increase_refcount (manager, iface, graph,
	                        NULL, tracker_class_get_name (class),
	                        tracker_property_get_name (property),
	                        NULL, error))
		return FALSE;

	return tracker_db_interface_execute_query (iface, error,
	                                           "INSERT OR IGNORE INTO \"%s%s%s_%s\"(ID, \"%s\") SELECT ID, \"%s\" FROM \"%s%s%s\"",
						   graph ? graph : "",
						   graph ? "_" : "",
	                                           tracker_class_get_name (target_class),
	                                           tracker_property_get_name (target),
	                                           tracker_property_get_name (target),
	                                           tracker_property_get_name (property),
						   graph ? graph : "",
						   graph ? "_" : "",
	                                           tracker_class_get_name (class));
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

static void
get_superclasses (GPtrArray    *array,
                  TrackerClass *class)
{
	TrackerClass **superclasses;
	gint i;

	if (g_ptr_array_find (array, class, NULL))
		return;

	g_ptr_array_add (array, class);

	superclasses = tracker_class_get_super_classes (class);

	for (i = 0; superclasses[i]; i++)
		get_superclasses (array, superclasses[i]);
}

static void
get_superproperties (GPtrArray       *array,
                     TrackerProperty *property)
{
	TrackerProperty **superproperties;
	gint i;

	if (g_ptr_array_find (array, property, NULL))
		return;

	g_ptr_array_add (array, property);

	superproperties = tracker_property_get_super_properties (property);

	for (i = 0; superproperties[i]; i++)
		get_superproperties (array, superproperties[i]);
}

static gboolean
get_directory_ontologies (TrackerDataManager  *manager,
                          GFile               *directory,
                          GList              **ontologies,
                          GError             **error)
{
	GFileEnumerator *enumerator;
	GList *sorted = NULL;

	enumerator = g_file_enumerate_children (directory,
	                                        G_FILE_ATTRIBUTE_STANDARD_NAME,
	                                        G_FILE_QUERY_INFO_NONE,
	                                        NULL, error);
	if (!enumerator)
		return FALSE;

	while (TRUE) {
		GFileInfo *info;
		GFile *child;
		const gchar *name;

		if (!g_file_enumerator_iterate (enumerator, &info, &child, NULL, error)) {
			g_list_free_full (sorted, g_object_unref);
			g_object_unref (enumerator);
			return FALSE;
		}

		if (!info)
			break;

		name = g_file_info_get_name (info);
		if (g_str_has_suffix (name, ".ontology") ||
		    tracker_rdf_format_pick_for_file (child, NULL)) {
			sorted = g_list_prepend (sorted, g_object_ref (child));
		}
	}

	*ontologies = g_list_sort (sorted, (GCompareFunc) compare_file_names);
	g_object_unref (enumerator);

	return TRUE;
}

static GList*
get_ontologies (TrackerDataManager  *manager,
                GFile               *ontologies,
                GError             **error)
{
	GList *stock = NULL, *user = NULL;
	GFile *stock_location;

	stock_location = g_file_new_for_uri ("resource://org/freedesktop/tracker/ontology");
	if (!get_directory_ontologies (manager, stock_location, &stock, error)) {
		g_object_unref (stock_location);
		return NULL;
	}

	g_object_unref (stock_location);

	if (!get_directory_ontologies (manager, ontologies, &user, error)) {
		g_list_free_full (stock, g_object_unref);
		return NULL;
	}

	return g_list_concat (stock, user);
}

static gchar *
get_ontologies_checksum (GList   *ontologies,
                         GError **error)
{
	GFileInputStream *stream = NULL;
	GError *inner_error = NULL;
	gchar *retval = NULL;
	GChecksum *checksum;
	guchar buf[4096];
	gsize len;
	GList *l;

	checksum = g_checksum_new (G_CHECKSUM_MD5);

	for (l = ontologies; l && !inner_error; l = l->next) {
		stream = g_file_read (l->data, NULL, &inner_error);
		if (!stream)
			break;

		while (g_input_stream_read_all (G_INPUT_STREAM (stream),
		                                buf,
		                                sizeof (buf),
		                                &len,
		                                NULL,
		                                &inner_error)) {
			g_checksum_update (checksum, buf, len);
			if (len != sizeof (buf))
				break;
		}

		g_clear_object (&stream);
	}

	g_clear_object (&stream);

	if (!inner_error)
		retval = g_strdup (g_checksum_get_string (checksum));
	else
		g_propagate_error (error, inner_error);

	g_checksum_free (checksum);

	return retval;
}

static gboolean
tracker_data_manager_recreate_indexes (TrackerDataManager  *manager,
                                       TrackerDBInterface  *iface,
                                       GError             **error)
{
	TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("Starting index re-creation..."));

	if (!tracker_db_interface_execute_query (iface, error, "REINDEX"))
		return FALSE;

	TRACKER_NOTE (ONTOLOGY_CHANGES, g_message ("  Finished index re-creation..."));

	return TRUE;
}

static gboolean
has_fts_properties (TrackerOntologies *ontologies)
{
	TrackerProperty **properties;
	guint i, len;

	properties = tracker_ontologies_get_properties (ontologies, &len);

	for (i = 0; i < len; i++) {
		if (tracker_property_get_fulltext_indexed (properties[i]))
			return TRUE;
	}

	return FALSE;
}

static gboolean
tracker_data_manager_fts_rebuild (TrackerDataManager  *data_manager,
                                  TrackerDBInterface  *iface,
                                  const gchar         *graph,
                                  GError             **error)
{
	return tracker_db_interface_execute_query (iface, error,
	                                           "INSERT INTO \"%s%sfts5\" (\"%s%sfts5\") VALUES('rebuild')",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           graph ? graph : "",
	                                           graph ? "_" : "");
}

gboolean
tracker_data_manager_fts_integrity_check (TrackerDataManager  *data_manager,
                                          TrackerDBInterface  *iface,
                                          const gchar         *graph)
{
	return tracker_db_interface_execute_query (iface, NULL,
	                                           "INSERT INTO \"%s%sfts5\" (\"%s%sfts5\", rank) VALUES('integrity-check', 1)",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           graph ? graph : "",
	                                           graph ? "_" : "");
}

static gboolean
rebuild_fts_tokens (TrackerDataManager  *manager,
                    TrackerDBInterface  *iface,
                    GError             **error)
{
	GHashTableIter iter;
	gchar *graph;

	if (has_fts_properties (manager->ontologies)) {
		g_debug ("Rebuilding FTS tokens, this may take a moment...");
		g_hash_table_iter_init (&iter, manager->graphs);
		while (g_hash_table_iter_next (&iter, (gpointer*) &graph, NULL)) {
			if (g_strcmp0 (graph, TRACKER_DEFAULT_GRAPH) == 0)
				graph = NULL;

			if (!tracker_data_manager_fts_rebuild (manager, iface, graph, error))
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
                               const gchar         *graph,
                               TrackerOntologies   *ontologies,
                               GError             **error)
{
	GString *view, *fts, *rank;
	GString *from, *column_names, *weights;
	TrackerProperty **properties;
	GError *internal_error = NULL;
	GHashTable *tables;
	guint i, len;

	if (!has_fts_properties (ontologies))
		return TRUE;

	/* Create view on tables/columns marked as FTS-indexed */
	view = g_string_new ("CREATE VIEW ");
	g_string_append_printf (view, "\"%s%sfts_view\" AS SELECT \"%s%srdfs:Resource\".ID as rowid ",
	                        graph ? graph : "",
	                        graph ? "_" : "",
	                        graph ? graph : "",
	                        graph ? "_" : "");
	from = g_string_new (NULL);
	g_string_append_printf (from, "FROM \"%s%srdfs:Resource\" ",
	                        graph ? graph : "",
	                        graph ? "_" : "");

	fts = g_string_new ("CREATE VIRTUAL TABLE ");
	g_string_append_printf (fts, "\"%s%sfts5\" USING fts5(content=\"%s%sfts_view\", ",
	                        graph ? graph : "",
	                        graph ? "_" : "",
	                        graph ? graph : "",
	                        graph ? "_" : "");

	column_names = g_string_new (NULL);
	weights = g_string_new (NULL);

	tables = g_hash_table_new (g_str_hash, g_str_equal);
	properties = tracker_ontologies_get_properties (ontologies, &len);

	for (i = 0; i < len; i++) {
		const gchar *name, *table_name;

		if (!tracker_property_get_fulltext_indexed (properties[i]))
			continue;

		name = tracker_property_get_name (properties[i]);
		table_name = tracker_property_get_table_name (properties[i]);

		if (tracker_property_get_multiple_values (properties[i])) {
			g_string_append_printf (view, ", group_concat(\"%s%s%s\".\"%s\")",
						graph ? graph : "",
						graph ? "_" : "",
			                        table_name, name);
		} else {
			g_string_append_printf (view, ", \"%s%s%s\".\"%s\"",
						graph ? graph : "",
						graph ? "_" : "",
			                        table_name, name);
		}

		g_string_append_printf (view, " AS \"%s\" ", name);
		g_string_append_printf (column_names, "\"%s\", ", name);

		if (weights->len != 0)
			g_string_append_c (weights, ',');
		g_string_append_printf (weights, "%d", tracker_property_get_weight (properties[i]));

		if (!g_hash_table_contains (tables, table_name)) {
			g_string_append_printf (from, "LEFT OUTER JOIN \"%s%s%s\" ON "
			                        " \"%s%srdfs:Resource\".ID = \"%s%s%s\".ID ",
			                        graph ? graph : "",
			                        graph ? "_" : "",
						table_name,
			                        graph ? graph : "",
			                        graph ? "_" : "",
			                        graph ? graph : "",
			                        graph ? "_" : "",
			                        table_name);
			g_hash_table_add (tables, (gpointer) tracker_property_get_table_name (properties[i]));
		}
	}

	g_hash_table_unref (tables);

	g_string_append_printf (from, "WHERE COALESCE (%s NULL) IS NOT NULL ",
	                        column_names->str);
	g_string_append_printf (from, "GROUP BY \"%s%srdfs:Resource\".ID",
				graph ? graph : "",
				graph ? "_" : "");
	g_string_append (view, from->str);
	g_string_free (from, TRUE);

	g_string_append (fts, column_names->str);
	g_string_append (fts, "tokenize=TrackerTokenizer)");
	g_string_free (column_names, TRUE);

	rank = g_string_new (NULL);
	g_string_append_printf (rank,
	                        "INSERT INTO \"%s%sfts5\"(\"%s%sfts5\", rank) VALUES('rank', 'bm25(%s)')",
	                        graph ? graph : "",
	                        graph ? "_" : "",
	                        graph ? graph : "",
	                        graph ? "_" : "",
	                        weights->str);
	g_string_free (weights, TRUE);

	/* FTS view */
	if (!tracker_db_interface_execute_query (iface, &internal_error, "%s", view->str))
		goto error;

	/* FTS table */
	if (!tracker_db_interface_execute_query (iface, &internal_error, "%s", fts->str))
		goto error;

	/* FTS rank function */
	if (!tracker_db_interface_execute_query (iface, &internal_error, "%s", rank->str))
		goto error;

error:
	g_string_free (view, TRUE);
	g_string_free (fts, TRUE);
	g_string_free (rank, TRUE);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	return TRUE;
}

static gboolean
tracker_data_manager_update_fts (TrackerDataManager  *manager,
                                 TrackerDBInterface  *iface,
                                 const gchar         *graph,
                                 TrackerOntologies   *ontologies,
                                 GError             **error)
{
	if (!has_fts_properties (ontologies))
		return TRUE;

	if (!tracker_data_manager_init_fts (manager, iface, graph, ontologies, error))
		return FALSE;

	/* Insert rowids */
	if (!tracker_db_interface_execute_query (iface, error,
	                                         "INSERT INTO \"%s%sfts5\" (rowid) SELECT rowid FROM fts_view",
	                                         graph ? graph : "",
	                                         graph ? "_" : ""))
		return FALSE;

	/* Ask FTS to rebuild itself */
	return tracker_data_manager_fts_rebuild (manager, iface, graph, error);
}

static gboolean
tracker_data_manager_delete_fts (TrackerDataManager  *manager,
                                 TrackerDBInterface  *iface,
                                 const gchar         *graph,
                                 GError             **error)
{
	if (!tracker_db_interface_execute_query (iface, error,
	                                         "DROP VIEW IF EXISTS \"%s%sfts_view\"",
	                                         graph ? graph : "",
	                                         graph ? "_" : ""))
		return FALSE;

	if (!tracker_db_interface_execute_query (iface, error,
	                                         "DROP TABLE IF EXISTS \"%s%sfts5\"",
	                                         graph ? graph : "",
	                                         graph ? "_" : ""))
		return FALSE;

	return TRUE;
}

TrackerDataManager *
tracker_data_manager_new (TrackerDBManagerFlags   flags,
                          GFile                  *cache_location,
                          GFile                  *ontology_location,
                          guint                   select_cache_size)
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

	return manager;
}

static gboolean
tracker_data_manager_attempt_repair (TrackerDataManager  *data_manager,
                                     TrackerDBInterface  *iface,
                                     TrackerOntologies   *ontology,
                                     GError             **error)
{
	GError *inner_error = NULL;

	tracker_db_interface_execute_query (iface, &inner_error, "REINDEX");
	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	if (has_fts_properties (ontology)) {
		GHashTableIter iter;
		const gchar *graph;

		g_hash_table_iter_init (&iter, data_manager->graphs);
		while (g_hash_table_iter_next (&iter, (gpointer*) &graph, NULL)) {
			if (g_strcmp0 (graph, TRACKER_DEFAULT_GRAPH) == 0)
				graph = NULL;

			if (!tracker_data_manager_fts_integrity_check (data_manager, iface, graph)) {
				if (!tracker_data_manager_fts_rebuild (data_manager, iface, graph, error)) {
					return FALSE;
				}
			}
		}
	}

	if (!tracker_db_manager_check_integrity (data_manager->db_manager, error))
		return FALSE;

	return TRUE;
}

static gboolean
tracker_data_manager_create_refcount_trigger (TrackerDataManager  *manager,
                                              TrackerDBInterface  *iface,
                                              const gchar         *graph,
                                              GError             **error)
{
	return tracker_db_interface_execute_query (iface, error,
	                                           "CREATE TRIGGER IF NOT EXISTS "
	                                           "\"%s%strigger_Refcount\" "
	                                           "AFTER UPDATE OF Refcount ON \"%s%sRefcount\" "
	                                           "FOR EACH ROW WHEN NEW.Refcount = 0 "
	                                           "BEGIN "
	                                           "DELETE FROM \"%s%sRefcount\" WHERE ROWID = OLD.ROWID; "
	                                           "END",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           graph ? graph : "",
	                                           graph ? "_" : "",
	                                           graph ? graph : "",
	                                           graph ? "_" : "");
}

static gboolean
tracker_data_manager_update_from_version (TrackerDataManager  *manager,
                                          TrackerDBVersion     version,
                                          TrackerOntologies   *ontologies,
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

	if (version < TRACKER_DB_VERSION_3_7) {
		/* Update default graph only, other graphs have tables regenerated with 3_10_B */
		if (!tracker_data_manager_delete_fts (manager, iface, NULL, &internal_error))
			goto error;
		if (!tracker_data_manager_update_fts (manager, iface, NULL, ontologies, &internal_error))
			goto error;
	}

	if (version < TRACKER_DB_VERSION_3_10) {
		/* Update default graph only, other graphs have tables regenerated with 3_10_B */
		if (!tracker_data_manager_create_refcount_trigger (manager,
		                                                   iface,
		                                                   NULL,
		                                                   error))
			goto error;
	}

	if (version < TRACKER_DB_VERSION_3_10_B) {
		GHashTableIter iter;
		const gchar *graph;

		g_hash_table_iter_init (&iter, manager->graphs);

		while (g_hash_table_iter_next (&iter, (gpointer *) &graph, NULL)) {
			TrackerClass **classes;
			TrackerProperty **properties;
			TrackerOntologyChange *changes = NULL;
			gchar *escaped, *filename;
			GFile *database;
			gboolean success;
			gint n_changes;
			guint i, len;

			if (g_strcmp0 (graph, TRACKER_DEFAULT_GRAPH) == 0)
				continue;

			escaped = g_uri_escape_string (graph, NULL, FALSE);
			filename = g_strdup_printf ("%s.db", escaped);
			database = g_file_get_child (manager->cache_location, filename);
			g_free (filename);
			g_free (escaped);

			if (!g_file_query_exists (database, NULL)) {
				g_set_error (&internal_error,
				             TRACKER_SPARQL_ERROR,
				             TRACKER_SPARQL_ERROR_OPEN_ERROR,
				             "Database file for graph '%s' not found",
				             graph);
				goto error;
			}

			success = tracker_db_interface_attach_database (iface, database,
			                                                graph,
			                                                &internal_error);
			g_object_unref (database);

			if (!success)
				goto error;

			tracker_ontologies_diff (NULL, ontologies, &changes, &n_changes);
			success = tracker_data_manager_apply_db_changes (manager, iface, graph,
			                                                 NULL, ontologies,
			                                                 changes, n_changes,
			                                                 &internal_error);
			g_free (changes);

			if (!success)
				goto error;

			classes = tracker_ontologies_get_classes (ontologies, &len);

			for (i = 0; i < len; i++) {
				if (g_str_has_prefix (tracker_class_get_name (classes[i]), "xsd:")) {
					/* xsd classes do not derive from rdfs:Resource and do not need separate tables */
					continue;
				}

				if (!tracker_db_interface_execute_query (iface, &internal_error,
				                                         "INSERT INTO \"%s_%s\" SELECT * FROM \"%s\".\"%s\"",
				                                         graph,
				                                         tracker_class_get_name (classes[i]),
				                                         graph,
				                                         tracker_class_get_name (classes[i])))
					goto error;
			}

			properties = tracker_ontologies_get_properties (ontologies, &len);

			for (i = 0; i < len; i++) {
				TrackerClass *domain;

				if (!tracker_property_get_multiple_values (properties[i]))
					continue;

				domain = tracker_property_get_domain (properties[i]);

				if (!tracker_db_interface_execute_query (iface, &internal_error,
				                                         "INSERT INTO \"%s_%s_%s\" SELECT * FROM \"%s\".\"%s_%s\"",
				                                         graph,
				                                         tracker_class_get_name (domain),
				                                         tracker_property_get_name (properties[i]),
				                                         graph,
				                                         tracker_class_get_name (domain),
				                                         tracker_property_get_name (properties[i])))
					goto error;
			}

			if (!tracker_db_interface_execute_query (iface, &internal_error,
			                                         "INSERT INTO \"%s_Refcount\" SELECT * FROM \"%s\".Refcount",
			                                         graph, graph))
				goto error;

			if (!tracker_data_manager_fts_rebuild (manager, iface, graph, &internal_error))
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
ensure_ontology_ids (TrackerDataManager  *manager,
                     TrackerOntologies   *ontologies,
                     GError             **error)
{
	TrackerClass **classes;
	TrackerProperty **properties;
	TrackerRowid id;
	guint num, i;

	classes = tracker_ontologies_get_classes (ontologies, &num);
	for (i = 0; i < num; i++) {
		id = tracker_data_update_ensure_resource (manager->data_update,
		                                          tracker_class_get_uri (classes[i]),
		                                          error);
		if (id == 0)
			return FALSE;

		tracker_class_set_id (classes[i], id);
		tracker_ontologies_add_id_uri_pair (ontologies, id,
		                                    tracker_class_get_uri (classes[i]));
	}

	properties = tracker_ontologies_get_properties (ontologies, &num);
	for (i = 0; i < num; i++) {
		id = tracker_data_update_ensure_resource (manager->data_update,
		                                          tracker_property_get_uri (properties[i]),
		                                          error);
		if (id == 0)
			return FALSE;

		tracker_property_set_id (properties[i], id);
		tracker_ontologies_add_id_uri_pair (ontologies, id,
		                                    tracker_property_get_uri (properties[i]));
	}

	return TRUE;
}

static gboolean
tracker_data_manager_apply_db_change (TrackerDataManager     *manager,
                                      TrackerDBInterface     *iface,
                                      const gchar            *graph,
                                      TrackerOntologies      *db_ontology,
                                      TrackerOntologies      *current_ontology,
                                      TrackerOntologyChange  *change,
                                      gboolean               *update_fts, /* inout */
                                      GError                **error)
{
	switch (change->type) {
	case TRACKER_CHANGE_CLASS_NEW:
		return create_class_table (manager, iface, graph,
		                           change->d.class,
		                           error);
	case TRACKER_CHANGE_CLASS_DELETE:
		return drop_class_table (manager, iface, graph,
		                         change->d.class,
		                         error);
	case TRACKER_CHANGE_CLASS_SUPERCLASS_NEW: {
		GPtrArray *superclasses;
		TrackerClass *target;
		guint i;

		superclasses = g_ptr_array_new ();
		get_superclasses (superclasses, change->d.superclass.class2);

		for (i = 0; i < superclasses->len; i++) {
			target = g_ptr_array_index (superclasses, i);

			if (!copy_from_class (manager, iface, graph,
			                      change->d.superclass.class1,
			                      target,
			                      error)) {
				g_ptr_array_unref (superclasses);
				return FALSE;
			}
		}

		g_ptr_array_unref (superclasses);
		return TRUE;
	}
	case TRACKER_CHANGE_CLASS_SUPERCLASS_DELETE:
		/* Data already inserted stays preserved */
		return TRUE;
	case TRACKER_CHANGE_CLASS_DOMAIN_INDEX_NEW:
		if (tracker_property_get_multiple_values (change->d.domain_index.property)) {
			if (!create_multivalue_property_table (manager, iface, graph,
			                                       change->d.domain_index.class,
			                                       change->d.domain_index.property,
			                                       error))
				return FALSE;

			return copy_multi_value (manager, iface, graph,
			                         tracker_property_get_domain (change->d.domain_index.property),
			                         change->d.domain_index.property,
			                         change->d.domain_index.class,
			                         change->d.domain_index.property,
			                         error);
		} else {
			if (!alter_class_table_for_added_property (manager, iface, graph,
			                                           change->d.domain_index.class,
			                                           change->d.domain_index.property,
			                                           error))
				return FALSE;

			return copy_single_value (manager, iface, graph,
			                          tracker_property_get_domain (change->d.domain_index.property),
			                          change->d.domain_index.property,
			                          change->d.domain_index.class,
			                          change->d.domain_index.property,
			                          error);
		}
	case TRACKER_CHANGE_CLASS_DOMAIN_INDEX_DELETE:
		if (tracker_property_get_multiple_values (change->d.domain_index.property))
			return drop_multivalue_property_table (manager, iface, graph,
			                                       change->d.domain_index.class,
			                                       change->d.domain_index.property,
			                                       error);
		else
			return alter_class_table_for_removed_property (manager, iface, graph,
			                                               change->d.domain_index.class,
			                                               change->d.domain_index.property,
			                                               error);
	case TRACKER_CHANGE_PROPERTY_NEW:
		if (tracker_property_get_multiple_values (change->d.property))
			return create_multivalue_property_table (manager, iface, graph,
			                                         tracker_property_get_domain (change->d.property),
			                                         change->d.property,
			                                         error);
		else
			return alter_class_table_for_added_property (manager, iface, graph,
			                                             tracker_property_get_domain (change->d.property),
			                                             change->d.property,
			                                             error);
	case TRACKER_CHANGE_PROPERTY_DELETE:
		if (tracker_property_get_multiple_values (change->d.property))
			return drop_multivalue_property_table (manager, iface, graph,
			                                       tracker_property_get_domain (change->d.property),
			                                       change->d.property,
			                                       error);
		else
			return alter_class_table_for_removed_property (manager, iface, graph,
			                                               tracker_property_get_domain (change->d.property),
			                                               change->d.property,
			                                               error);
	case TRACKER_CHANGE_PROPERTY_SUPERPROPERTY_NEW: {
		GPtrArray *superproperties;
		TrackerProperty *prop, *target;
		gboolean prop_is_multi;
		guint i;

		superproperties = g_ptr_array_new ();
		get_superproperties (superproperties, change->d.superproperty.property2);
		prop = change->d.superproperty.property1;
		prop_is_multi = tracker_property_get_multiple_values (prop);

		for (i = 0; i < superproperties->len; i++) {
			gboolean super_is_multi;

			target = g_ptr_array_index (superproperties, i);
			super_is_multi = tracker_property_get_multiple_values (target);

			if (prop_is_multi && super_is_multi) {
				if (!copy_multi_value (manager, iface, graph,
				                       tracker_property_get_domain (prop), prop,
				                       tracker_property_get_domain (target), target,
				                       error)) {
					g_ptr_array_unref (superproperties);
					return FALSE;
				}
			} else if (!prop_is_multi && !super_is_multi) {
				if (!copy_single_value (manager, iface, graph,
				                        tracker_property_get_domain (prop), prop,
				                        tracker_property_get_domain (target), target,
				                        error)) {
					g_ptr_array_unref (superproperties);
					return FALSE;
				}
			} else if (prop_is_multi && !super_is_multi) {
				if (!copy_multi_value_to_single_value (manager, iface, graph,
				                                       tracker_property_get_domain (prop), prop,
				                                       tracker_property_get_domain (target), target,
				                                       error)) {
					g_ptr_array_unref (superproperties);
					return FALSE;
				}
			} else if (!prop_is_multi && super_is_multi) {
				if (!copy_single_value_to_multi_value (manager, iface, graph,
				                                       tracker_property_get_domain (prop), prop,
				                                       tracker_property_get_domain (target), target,
				                                       error)) {
					g_ptr_array_unref (superproperties);
					return FALSE;
				}
			}
		}

		g_ptr_array_unref (superproperties);
		return TRUE;
	}
	case TRACKER_CHANGE_PROPERTY_SUPERPROPERTY_DELETE:
		/* Data already inserted stays preserved */
		return TRUE;
	case TRACKER_CHANGE_PROPERTY_INDEX_NEW:
		return create_index (manager, iface, graph,
		                     tracker_property_get_domain (change->d.property),
		                     change->d.property,
		                     error);
	case TRACKER_CHANGE_PROPERTY_INDEX_DELETE:
		return drop_index (manager, iface, graph,
		                   tracker_property_get_domain (change->d.property),
		                   change->d.property,
		                   error);
	case TRACKER_CHANGE_PROPERTY_SECONDARY_INDEX_NEW: {
		TrackerProperty *secondary;

		secondary = tracker_property_get_secondary_index (change->d.property);

		if (tracker_property_get_multiple_values (change->d.property) ||
		    tracker_property_get_multiple_values (secondary)) {
			g_set_error (error,
			             TRACKER_DATA_ONTOLOGY_ERROR,
			             TRACKER_DATA_UNSUPPORTED_ONTOLOGY_CHANGE,
			             "nrl:secondaryIndex for property pair %s / %s is not supported, both properties need nrl:maxCardinality 1",
				     tracker_property_get_name (change->d.property),
				     tracker_property_get_name (secondary));
			return FALSE;
		} else {
			return create_secondary_index (manager, iface, graph,
			                               change->d.property,
			                               secondary,
			                               error);
		}
	}
	case TRACKER_CHANGE_PROPERTY_SECONDARY_INDEX_DELETE:
		return drop_secondary_index (manager, iface, graph,
		                             change->d.property,
		                             tracker_property_get_secondary_index (change->d.property),
		                             error);
	case TRACKER_CHANGE_PROPERTY_FTS_INDEX:
		if (!(*update_fts)) {
			*update_fts = TRUE;
			if (!tracker_data_manager_delete_fts (manager, iface, graph, error))
				return FALSE;
		}
		return TRUE;
	case TRACKER_CHANGE_PROPERTY_RANGE: {
		TrackerClass *range, *old_range;
		TrackerProperty *old_property;

		range = tracker_property_get_range (change->d.property);
		old_property = tracker_ontologies_get_property_by_uri (db_ontology,
		                                                       tracker_property_get_uri (change->d.property));
		old_range = tracker_property_get_range (old_property);

		if (is_allowed_conversion (tracker_class_get_uri (old_range),
		                           tracker_class_get_uri (range),
		                           allowed_range_conversions)) {
			TrackerClass **domain_indexes;

			domain_indexes = tracker_property_get_domain_indexes (change->d.property);
			if (domain_indexes) {
				while (*domain_indexes) {
					if (tracker_property_get_multiple_values (change->d.property)) {
						if (!alter_multivalue_property_table_for_type_change (manager, iface, graph,
						                                                      *domain_indexes,
						                                                      change->d.property,
						                                                      error))
							return FALSE;
					} else {
						if (!alter_class_table_for_type_change (manager, iface, graph,
						                                        *domain_indexes,
						                                        change->d.property,
						                                        error))
							return FALSE;
					}

					domain_indexes++;
				}
			}

			if (tracker_property_get_multiple_values (change->d.property)) {
				return alter_multivalue_property_table_for_type_change (manager, iface, graph,
				                                                        tracker_property_get_domain (change->d.property),
				                                                        change->d.property,
				                                                        error);
			} else {
				return alter_class_table_for_type_change (manager, iface, graph,
				                                          tracker_property_get_domain (change->d.property),
				                                          change->d.property,
				                                          error);
			}
		} else {
			g_set_error (error,
			             TRACKER_DATA_ONTOLOGY_ERROR,
			             TRACKER_DATA_UNSUPPORTED_ONTOLOGY_CHANGE,
			             "rdfs:range change at property %s from %s to %s is not supported",
			             tracker_property_get_name (change->d.property),
			             tracker_class_get_name (old_range),
			             tracker_class_get_name (range));
			return FALSE;
		}
	}
	case TRACKER_CHANGE_PROPERTY_DOMAIN:
		g_set_error (error,
		             TRACKER_DATA_ONTOLOGY_ERROR,
		             TRACKER_DATA_UNSUPPORTED_ONTOLOGY_CHANGE,
		             "rdf:domain change at property %s is not supported",
			     tracker_property_get_name (change->d.property));
		return FALSE;
	case TRACKER_CHANGE_PROPERTY_CARDINALITY:
		if (tracker_property_get_multiple_values (change->d.property)) {
			if (!create_multivalue_property_table (manager, iface, graph,
			                                       tracker_property_get_domain (change->d.property),
			                                       change->d.property,
			                                       error))
				return FALSE;

			if (!copy_single_value_to_multi_value (manager, iface, graph,
			                                       tracker_property_get_domain (change->d.property),
			                                       change->d.property,
			                                       tracker_property_get_domain (change->d.property),
			                                       change->d.property,
			                                       error))
				return FALSE;

			if (!alter_class_table_for_removed_property (manager, iface, graph,
			                                             tracker_property_get_domain (change->d.property),
			                                             change->d.property,
			                                             error))
				return FALSE;

			return TRUE;
		} else {
			g_set_error (error,
			             TRACKER_DATA_ONTOLOGY_ERROR,
			             TRACKER_DATA_UNSUPPORTED_ONTOLOGY_CHANGE,
			             "Introducing cardinality restrictions on the previously existing property %s is not supported",
			             tracker_property_get_name (change->d.property));
			return FALSE;
		}
	case TRACKER_CHANGE_PROPERTY_INVERSE_FUNCTIONAL_NEW: {
		TrackerProperty *old_property = NULL;

		if (db_ontology) {
			old_property = tracker_ontologies_get_property_by_uri (db_ontology,
									       tracker_property_get_uri (change->d.property));
		}

		if (old_property != NULL) {
			g_set_error (error,
				     TRACKER_DATA_ONTOLOGY_ERROR,
				     TRACKER_DATA_UNSUPPORTED_ONTOLOGY_CHANGE,
				     "Making the previously existing property %s a nrl:inverseFunctionalProperty is not supported",
				     tracker_property_get_name (change->d.property));
			return FALSE;
		} else {
			return create_unique_index (manager, iface, graph,
						    change->d.property,
						    error);
		}
	}
	case TRACKER_CHANGE_PROPERTY_INVERSE_FUNCTIONAL_DELETE:
		return drop_unique_index (manager, iface, graph,
					  change->d.property,
					  error);
	}

	return TRUE;
}

static gboolean
tracker_data_manager_apply_db_changes (TrackerDataManager     *manager,
                                       TrackerDBInterface     *iface,
                                       const gchar            *graph,
                                       TrackerOntologies      *db_ontology,
                                       TrackerOntologies      *current_ontology,
                                       TrackerOntologyChange  *changes,
                                       gint                    n_changes,
                                       GError                **error)
{
	gboolean update_fts = FALSE;
	gint i;

	if (current_ontology) {
		if (!tracker_db_interface_execute_query (iface, error,
		                                         "CREATE TABLE IF NOT EXISTS "
		                                         " \"%s%sRefcount\" (ID INTEGER NOT NULL PRIMARY KEY,"
		                                         " Refcount INTEGER DEFAULT 0)",
		                                         graph ? graph : "",
		                                         graph ? "_" : ""))
			return FALSE;

		if (!tracker_data_manager_create_refcount_trigger (manager, iface, graph, error))
			return FALSE;
	}

	for (i = 0; i < n_changes; i++) {
		if (!tracker_data_manager_apply_db_change (manager, iface, graph,
		                                           db_ontology, current_ontology,
		                                           &changes[i],
		                                           &update_fts,
		                                           error))
			return FALSE;
	}

	if (current_ontology) {
		if (update_fts &&
		    !tracker_data_manager_init_fts (manager, iface, graph, current_ontology, error))
			return FALSE;
	} else {
		if (!tracker_db_interface_execute_query (iface, error,
		                                         "DROP TRIGGER IF EXISTS "
		                                         "\"%s%strigger_Refcount\" ",
		                                         graph ? graph : "",
		                                         graph ? "_" : ""))
			return FALSE;

		if (!tracker_db_interface_execute_query (iface, error,
		                                         "DROP TABLE IF EXISTS \"%s%sRefcount\" ",
		                                         graph ? graph : "",
		                                         graph ? "_" : ""))
			return FALSE;

		if (update_fts &&
		    !tracker_data_manager_delete_fts (manager, iface, graph, error))
			return FALSE;
	}

	return TRUE;
}

static gboolean
tracker_data_manager_import_ontology (TrackerDataManager     *manager,
                                      TrackerDBInterface     *iface,
                                      TrackerOntologyChange  *changes,
                                      gint                    n_changes,
                                      GError                **error)
{
	TrackerSparqlCursor *deserializer = NULL;
	GList *ontology_files = NULL, *l;
	GError *inner_error = NULL;
	TrackerClass *rdfs_resource;
	GValue resource_value = G_VALUE_INIT;
	TrackerOntologies *ontologies;
	TrackerClass **classes;
	TrackerProperty **properties;
	guint num, i;

	ontologies = manager->ontologies;
	rdfs_resource = tracker_ontologies_get_class_by_uri (ontologies,
	                                                     TRACKER_PREFIX_RDFS "Resource");
	g_assert (rdfs_resource != NULL);

	g_value_init (&resource_value, G_TYPE_INT64);
	g_value_set_int64 (&resource_value, tracker_class_get_id (rdfs_resource));

	for (i = 0; (gint) i < n_changes; i++) {
		TrackerRowid id = 0;

		switch (changes[i].type) {
		case TRACKER_CHANGE_CLASS_DELETE:
			id = tracker_class_get_id (changes[i].d.class);
			break;
		case TRACKER_CHANGE_PROPERTY_DELETE:
			id = tracker_property_get_id (changes[i].d.property);
			break;
		default:
			break;
		}

		if (id == 0)
			continue;

		tracker_data_delete_statement (manager->data_update, NULL,
		                               id,
		                               tracker_ontologies_get_rdf_type (ontologies),
		                               &resource_value,
		                               &inner_error);
		if (inner_error)
			goto error;
	}

	/* Delete all data from the existing classes/properties in order to
	 * replace them fully.
	 */
	classes = tracker_ontologies_get_classes (ontologies, &num);
	for (i = 0; i < num; i++) {
		tracker_data_delete_statement (manager->data_update, NULL,
		                               tracker_class_get_id (classes[i]),
		                               tracker_ontologies_get_rdf_type (ontologies),
		                               &resource_value,
		                               &inner_error);
		if (inner_error)
			goto error;
	}

	properties = tracker_ontologies_get_properties (ontologies, &num);
	for (i = 0; i < num; i++) {
		tracker_data_delete_statement (manager->data_update, NULL,
		                               tracker_property_get_id (properties[i]),
		                               tracker_ontologies_get_rdf_type (ontologies),
		                               &resource_value,
		                               &inner_error);
		if (inner_error)
			goto error;
	}

	tracker_data_update_buffer_flush (manager->data_update, &inner_error);
	if (inner_error)
		goto error;

	/* Import all RDF from the ontology files */
	ontology_files = get_ontologies (manager, manager->ontology_location, &inner_error);
	if (!ontology_files)
		goto error;

	for (l = ontology_files; l; l = l->next) {
		deserializer = tracker_deserializer_new_for_file (l->data, NULL, &inner_error);
		if (!deserializer)
			goto error;

		while (tracker_sparql_cursor_next (deserializer, NULL, &inner_error)) {
			const gchar *subject, *predicate, *object, *object_lang;
			TrackerRowid subject_id;
			TrackerProperty *property;
			GValue value = G_VALUE_INIT;

			subject = tracker_sparql_cursor_get_string (deserializer,
			                                            TRACKER_RDF_COL_SUBJECT,
			                                            NULL);
			predicate = tracker_sparql_cursor_get_string (deserializer,
			                                              TRACKER_RDF_COL_PREDICATE,
			                                              NULL);
			object = tracker_sparql_cursor_get_langstring (deserializer,
			                                               TRACKER_RDF_COL_OBJECT,
			                                               &object_lang,
			                                               NULL);

			property = tracker_ontologies_get_property_by_uri (ontologies, predicate);

			subject_id = tracker_data_update_ensure_resource (manager->data_update,
			                                                  subject,
			                                                  &inner_error);
			if (subject_id == 0)
				goto error;

			if (!tracker_data_query_string_to_value (manager,
			                                         object, object_lang,
			                                         tracker_property_get_data_type (property),
			                                         &value, &inner_error))
				goto error;

			tracker_data_update_statement (manager->data_update, NULL,
			                               subject_id, property, &value,
			                               &inner_error);
			g_value_unset (&value);

			if (inner_error)
				break;

			tracker_data_update_buffer_might_flush (manager->data_update, &inner_error);
			if (inner_error)
				break;
		}

		if (inner_error)
			goto error;

		g_clear_object (&deserializer);
	}

	g_value_unset (&resource_value);
	g_clear_object (&deserializer);
	g_list_free_full (ontology_files, g_object_unref);
	return TRUE;

 error:
	g_propagate_error (error, inner_error);
	g_value_unset (&resource_value);
	g_clear_object (&deserializer);
	g_list_free_full (ontology_files, g_object_unref);
	return FALSE;
}

static gboolean
tracker_data_manager_initable_init (GInitable     *initable,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
	TrackerDataManager *manager = TRACKER_DATA_MANAGER (initable);
	TrackerDBInterface *iface;
	gboolean create_db = FALSE, read_only, check_apply_ontology;
	gboolean apply_ontology = FALSE, apply_base_tables, apply_locale, apply_fts_tokenizer;
	TrackerOntologies *current_ontology = NULL, *db_ontology = NULL;
	gchar *checksum = NULL;
	gint cur_version;

	if (manager->cache_location && !g_file_is_native (manager->cache_location)) {
		g_set_error (error,
		             TRACKER_DATA_ONTOLOGY_ERROR,
		             TRACKER_DATA_UNSUPPORTED_LOCATION,
		             "Database location must be local");
		goto error;
	}

	read_only = (manager->flags & TRACKER_DB_MANAGER_READONLY) ? TRUE : FALSE;

	/* Make sure we initialize all other modules we depend on */
	manager->data_update = tracker_data_new (manager);

	manager->db_manager = tracker_db_manager_new (manager->flags,
	                                              manager->cache_location,
	                                              manager->select_cache_size,
	                                              G_OBJECT (manager),
	                                              error);
	if (!manager->db_manager)
		goto error;

	create_db = tracker_db_manager_is_first_time (manager->db_manager);

	iface = tracker_db_manager_get_writable_db_interface (manager->db_manager);

	if (manager->ontology_location &&
	    g_file_query_file_type (manager->ontology_location, G_FILE_QUERY_INFO_NONE, NULL) != G_FILE_TYPE_DIRECTORY) {
		gchar *uri;

		uri = g_file_get_uri (manager->ontology_location);
		g_set_error (error, TRACKER_DATA_ONTOLOGY_ERROR,
		             TRACKER_DATA_ONTOLOGY_NOT_FOUND,
		             "'%s' is not a ontology location", uri);
		g_free (uri);
		goto error;
	}

	if (!create_db &&
	    !tracker_data_manager_initialize_graphs (manager, iface, error))
		goto error;

	if (create_db) {
		/* Database needs to be created */
		if (read_only) {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_OPEN_ERROR,
			             "Database does not exist and connection is readonly");
			goto error;
		}

		if (!manager->ontology_location) {
			g_set_error (error,
			             TRACKER_DATA_ONTOLOGY_ERROR,
			             TRACKER_DATA_ONTOLOGY_NOT_FOUND,
			             "Creating a database requires an ontology location.");
			goto error;
		}

		check_apply_ontology = TRUE;
		apply_base_tables = TRUE;
		apply_locale = TRUE;
		apply_fts_tokenizer = FALSE;
		cur_version = TRACKER_DB_VERSION_NOW;
		db_ontology = NULL;
	} else if (!create_db && !read_only) {
		/* r/w connection on already existing database */
		cur_version = tracker_db_manager_get_version (manager->db_manager);

		if (cur_version > TRACKER_DB_VERSION_NOW) {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_OPEN_ERROR,
			             "Database was created with a more recent version of Tracker");
			goto error;
		}

		db_ontology = tracker_ontologies_load_from_database (manager, error);
		if (!db_ontology)
			goto error;

		if (tracker_db_manager_needs_repair (manager->db_manager) &&
		    !tracker_data_manager_attempt_repair (manager, iface, db_ontology, error))
			goto error;

		check_apply_ontology = manager->ontology_location != NULL;
		apply_base_tables = FALSE;
		apply_locale = tracker_db_manager_locale_changed (manager->db_manager, NULL);
		apply_fts_tokenizer = tracker_db_manager_get_tokenizer_changed (manager->db_manager);
	} else {
		/* Readonly connection */
		g_assert (!create_db);
		g_assert (read_only);

		cur_version = tracker_db_manager_get_version (manager->db_manager);
		if (cur_version != TRACKER_DB_VERSION_NOW) {
			g_set_error (error,
			             TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_OPEN_ERROR,
			             "Cannot update internal database format while readonly");
			goto error;
		}

		current_ontology = tracker_ontologies_load_from_database (manager, error);
		if (!current_ontology)
			goto error;

		g_set_object (&manager->ontologies, current_ontology);
		goto no_updates;
	}

	if (check_apply_ontology) {
		GList *ontologies = NULL;

		g_assert (manager->ontology_location);
		ontologies = get_ontologies (manager, manager->ontology_location, error);
		if (!ontologies)
			goto error;

		checksum = get_ontologies_checksum (ontologies, error);
		if (!checksum) {
			g_list_free_full (ontologies, g_object_unref);
			goto error;
		}

		if (create_db ||
		    tracker_db_manager_ontology_checksum_changed (manager->db_manager, checksum)) {
			apply_ontology = TRUE;
			current_ontology = tracker_ontologies_load_from_rdf (ontologies, error);
			if (!current_ontology) {
				g_list_free_full (ontologies, g_object_unref);
				goto error;
			}
		}

		g_list_free_full (ontologies, g_object_unref);
	}

	/* Apply all database changes */
	if (!tracker_data_begin_ontology_transaction (manager->data_update, error))
		goto rollback;

	if (apply_base_tables && !create_base_tables (manager, iface, error))
		goto rollback;

	manager->main_graph_id = tracker_data_ensure_graph (manager->data_update,
	                                                    TRACKER_DEFAULT_GRAPH,
	                                                    error);
	if (manager->main_graph_id == 0)
		goto rollback;

	g_hash_table_insert (manager->graphs, g_strdup (TRACKER_DEFAULT_GRAPH),
	                     tracker_rowid_copy (&manager->main_graph_id));

	if (cur_version != TRACKER_DB_VERSION_NOW &&
	    !tracker_data_manager_update_from_version (manager, cur_version, db_ontology, error))
		goto rollback;

	if (apply_ontology) {
		TrackerOntologyChange *changes;
		gint n_changes;
		GHashTable *graphs;
		GHashTableIter iter;
		gpointer graph;

		if (!ensure_ontology_ids (manager, current_ontology, error))
			goto rollback;

		tracker_ontologies_diff (db_ontology, current_ontology, &changes, &n_changes);

		graphs = tracker_data_manager_get_graphs (manager, FALSE);
		if (graphs) {
			g_hash_table_iter_init (&iter, graphs);

			while (g_hash_table_iter_next (&iter, &graph, NULL)) {
				if (g_strcmp0 (graph, TRACKER_DEFAULT_GRAPH) == 0)
					graph = NULL;

				if (!tracker_data_manager_apply_db_changes (manager,
				                                            iface, graph,
				                                            db_ontology, current_ontology,
				                                            changes, n_changes,
				                                            error)) {
					g_hash_table_unref (graphs);
					g_free (changes);
					goto rollback;
				}
			}

			g_hash_table_unref (graphs);
		}

		g_set_object (&manager->ontologies, current_ontology);

		if (!tracker_data_manager_import_ontology (manager, iface,
		                                           changes, n_changes,
		                                           error)) {
			g_free (changes);
			goto rollback;
		}

		g_free (changes);

		tracker_db_manager_set_ontology_checksum (manager->db_manager, checksum);
	} else {
		g_set_object (&manager->ontologies, db_ontology);
	}

	if (create_db)
		tracker_db_manager_update_version (manager->db_manager);

	if (apply_locale) {
		tracker_db_manager_set_current_locale (manager->db_manager);

		if (!create_db && !tracker_data_manager_recreate_indexes (manager, iface, error))
			goto rollback;
	}

	if (apply_fts_tokenizer) {
		if (!create_db &&
		    !rebuild_fts_tokens (manager, iface, error))
			goto rollback;
		tracker_db_manager_tokenizer_update (manager->db_manager);
	}

	if (!tracker_data_commit_transaction (manager->data_update, error))
		goto rollback;

 no_updates:
	g_clear_object (&current_ontology);
	g_clear_object (&db_ontology);
	g_free (checksum);
	return TRUE;

 rollback:
	tracker_data_rollback_transaction (manager->data_update);

 error:
	if (create_db)
		tracker_db_manager_rollback_db_creation (manager->db_manager);

	g_clear_object (&manager->data_update);
	g_clear_object (&manager->db_manager);
	g_clear_object (&current_ontology);
	g_clear_object (&db_ontology);
	g_free (checksum);
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
	gboolean first = TRUE;

	graphs = tracker_data_manager_get_graphs (manager, FALSE);

	str = g_string_new ("WITH referencedElements(ID) AS (");

	g_hash_table_iter_init (&iter, graphs);

	while (g_hash_table_iter_next (&iter, (gpointer*) &graph, NULL)) {
		if (!first)
			g_string_append (str, "UNION ALL ");

		if (g_strcmp0 (graph, TRACKER_DEFAULT_GRAPH) == 0)
			graph = NULL;

		g_string_append_printf (str,
		                        "SELECT ID FROM \"%s%sRefcount\" ",
		                        graph ? graph : "",
		                        graph ? "_" : "");
		first = FALSE;
	}

	g_string_append (str, ") ");
	g_string_append_printf (str,
	                        "DELETE FROM Resource "
	                        "WHERE Resource.ID NOT IN (SELECT ID FROM referencedElements) "
	                        "AND Resource.ID NOT IN (SELECT ID FROM Graph)");
	g_hash_table_unref (graphs);

	stmt = tracker_db_interface_create_statement (iface,
	                                              TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
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
		readonly = (tracker_db_manager_get_flags (manager->db_manager) & TRACKER_DB_MANAGER_READONLY) != 0;

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
tracker_data_manager_class_init (TrackerDataManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = tracker_data_manager_dispose;
	object_class->finalize = tracker_data_manager_finalize;
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
	TrackerOntologyChange *changes = NULL;
	TrackerDBInterface *iface;
	TrackerRowid id;
	gint n_changes;

	iface = tracker_db_manager_get_writable_db_interface (manager->db_manager);

	if (strlen (name) > MAX_HTTP_URI_LEN) {
		g_set_error (error,
			     TRACKER_SPARQL_ERROR,
			     TRACKER_SPARQL_ERROR_TYPE,
			     "Graph name exceeds the %d bytes limit",
			     MAX_HTTP_URI_LEN);
		goto out;
	}

#if GLIB_CHECK_VERSION(2, 66, 0)
	if (!g_uri_is_valid (name,
			     G_URI_FLAGS_HAS_PASSWORD |
			     G_URI_FLAGS_HAS_AUTH_PARAMS |
			     G_URI_FLAGS_NON_DNS,
			     error))
		goto out;
#else
	{
		gchar *uri_scheme;

		uri_scheme = g_uri_parse_scheme (name);
		if (!uri_scheme) {
			g_set_error (error,
				     TRACKER_SPARQL_ERROR,
				     TRACKER_SPARQL_ERROR_TYPE,
				     "Graph name is not a correct URI");
			goto out;
		}

		g_free (uri_scheme);
	}
#endif

	tracker_ontologies_diff (NULL, manager->ontologies, &changes, &n_changes);

	if (!tracker_data_manager_apply_db_changes (manager, iface, name,
	                                            NULL, manager->ontologies,
	                                            changes, n_changes, error))
		goto out;

	id = tracker_data_ensure_graph (manager->data_update, name, error);
	if (id == 0)
		goto out;

	if (!manager->transaction_graphs)
		manager->transaction_graphs = copy_graphs (manager->graphs);

	g_hash_table_insert (manager->transaction_graphs, g_strdup (name),
	                     tracker_rowid_copy (&id));
	g_free (changes);

	return TRUE;

out:
	g_free (changes);
	return FALSE;
}

gboolean
tracker_data_manager_drop_graph (TrackerDataManager  *manager,
                                 const gchar         *graph,
                                 GError             **error)
{
	TrackerOntologies *ontologies = manager->ontologies;
	TrackerClass **classes;
	TrackerProperty **properties;
	guint i, n_classes, n_properties;
	TrackerDBInterface *iface;

	iface = tracker_db_manager_get_writable_db_interface (manager->db_manager);

	/* Silently refuse to drop the main graph, clear it instead */
	if (!graph || g_strcmp0 (graph, TRACKER_DEFAULT_GRAPH) == 0)
		return tracker_data_manager_clear_graph (manager, graph, error);

	if (!tracker_data_manager_delete_fts (manager, iface, graph, error))
		return FALSE;

	classes = tracker_ontologies_get_classes (ontologies, &n_classes);
	properties = tracker_ontologies_get_properties (ontologies, &n_properties);

	for (i = 0; i < n_classes; i++) {
		if (g_str_has_prefix (tracker_class_get_name (classes[i]), "xsd:"))
			continue;

		if (!tracker_db_interface_execute_query (iface, error,
		                                         "DROP TABLE \"%s%s%s\"",
		                                         graph, "_",
		                                         tracker_class_get_name (classes[i])))
			return FALSE;
	}

	for (i = 0; i < n_properties; i++) {
		TrackerClass *service;

		if (!tracker_property_get_multiple_values (properties[i]))
			continue;

		service = tracker_property_get_domain (properties[i]);
		if (!tracker_db_interface_execute_query (iface, error,
		                                         "DROP TABLE \"%s%s%s_%s\"",
		                                         graph, "_",
		                                         tracker_class_get_name (service),
		                                         tracker_property_get_name (properties[i])))
			return FALSE;
	}

	if (!tracker_db_interface_execute_query (iface,
	                                         error,
	                                         "DROP TABLE \"%s%sRefcount\"",
	                                         graph, "_"))
		return FALSE;

	if (!tracker_data_delete_graph (manager->data_update, graph, error))
		return FALSE;

	if (!manager->transaction_graphs)
		manager->transaction_graphs = copy_graphs (manager->graphs);
	g_hash_table_remove (manager->transaction_graphs, graph);

	return TRUE;
}

gboolean
tracker_data_manager_find_graph (TrackerDataManager *manager,
                                 const gchar        *name,
                                 gboolean            in_transaction)
{
	GHashTable *graphs;
	gboolean exists;

	graphs = tracker_data_manager_get_graphs (manager, in_transaction);
	exists = g_hash_table_contains (graphs, name);
	g_hash_table_unref (graphs);

	return exists;
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

	if (g_strcmp0 (graph, TRACKER_DEFAULT_GRAPH) == 0)
		graph = NULL;

	iface = tracker_db_manager_get_writable_db_interface (manager->db_manager);
	classes = tracker_ontologies_get_classes (ontologies, &n_classes);
	properties = tracker_ontologies_get_properties (ontologies, &n_properties);

	for (i = 0; !inner_error && i < n_classes; i++) {
		if (g_str_has_prefix (tracker_class_get_name (classes[i]), "xsd:"))
			continue;

		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &inner_error,
		                                               "DELETE FROM \"%s%s%s\"",
		                                               graph ? graph : "",
		                                               graph ? "_" : "",
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
		                                               "DELETE FROM \"%s%s%s_%s\"",
		                                               graph ? graph : "",
		                                               graph ? "_" : "",
		                                               tracker_class_get_name (service),
		                                               tracker_property_get_name (properties[i]));
		if (!stmt)
			goto out;

		tracker_db_statement_execute (stmt, &inner_error);
		g_object_unref (stmt);
	}

	tracker_db_interface_execute_query (iface,
					    &inner_error,
					    "DELETE FROM \"%s%sRefcount\"",
	                                    graph ? graph : "",
	                                    graph ? "_" : "");
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

	if (g_strcmp0 (source, TRACKER_DEFAULT_GRAPH) == 0)
		source = NULL;
	if (g_strcmp0 (destination, TRACKER_DEFAULT_GRAPH) == 0)
		destination = NULL;

	if (g_strcmp0 (source, destination) == 0)
		return TRUE;

	iface = tracker_db_manager_get_writable_db_interface (manager->db_manager);
	classes = tracker_ontologies_get_classes (ontologies, &n_classes);
	properties = tracker_ontologies_get_properties (ontologies, &n_properties);

	for (i = 0; !inner_error && i < n_classes; i++) {
		if (g_str_has_prefix (tracker_class_get_name (classes[i]), "xsd:"))
			continue;

		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &inner_error,
		                                               "INSERT OR REPLACE INTO \"%s%s%s\" "
		                                               "SELECT * from \"%s%s%s\"",
		                                               destination ? destination : "",
		                                               destination ? "_" : "",
		                                               tracker_class_get_name (classes[i]),
		                                               source ? source : "",
		                                               source ? "_" : "",
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
		                                               "INSERT OR REPLACE INTO \"%s%s%s_%s\" "
		                                               "SELECT * from \"%s%s%s_%s\"",
		                                               destination ? destination : "",
		                                               destination ? "_" : "",
		                                               tracker_class_get_name (service),
		                                               tracker_property_get_name (properties[i]),
		                                               source ? source : "",
		                                               source ? "_" : "",
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
	                                    "INSERT OR IGNORE INTO \"%s%sRefcount\" "
	                                    "SELECT ID, 0 from \"%s%sRefcount\"",
	                                    destination ? destination : "",
	                                    destination ? "_" : "",
	                                    source ? source : "",
	                                    source ? "_" : "");
	if (inner_error)
		goto out;

	tracker_db_interface_execute_query (iface,
	                                    &inner_error,
	                                    "UPDATE \"%s%sRefcount\" AS B "
	                                    "SET Refcount = Refcount + "
	                                    "(SELECT Refcount FROM \"%s%sRefcount\" AS A "
	                                    "WHERE B.ID = A.ID)",
	                                    destination ? destination : "",
	                                    destination ? "_" : "",
	                                    source ? source : "",
	                                    source ? "_" : "");
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

const char *
tracker_data_manager_expand_prefix (TrackerDataManager  *manager,
                                    const gchar         *term,
                                    GHashTable          *prefix_map,
                                    char               **free_str)
{
	const gchar *sep, *suffix = NULL, *expanded_ns = NULL, *retval;
	TrackerOntologies *ontologies;
	TrackerNamespace **namespaces;
	guint n_namespaces, i;
	gchar *free_me = NULL;

	sep = strchr (term, ':');

	if (sep) {
		if (prefix_map) {
			char *str;

			str = g_strndup (term, sep - term);
			expanded_ns = g_hash_table_lookup (prefix_map, str);
			g_free (str);
		}

		suffix = sep + 1;
	} else {
		sep = &term[strlen (term)];

		if (prefix_map)
			expanded_ns = g_hash_table_lookup (prefix_map, term);
	}

	if (!expanded_ns) {
		ontologies = tracker_data_manager_get_ontologies (manager);
		namespaces = tracker_ontologies_get_namespaces (ontologies, &n_namespaces);

		for (i = 0; i < n_namespaces; i++) {
			const char *ns_prefix;

			ns_prefix = tracker_namespace_get_prefix (namespaces[i]);

			if (strlen (ns_prefix) != (size_t) (sep - term) ||
			    strncmp (term, ns_prefix, sep - term) != 0)
				continue;

			expanded_ns = tracker_namespace_get_uri (namespaces[i]);

			if (prefix_map) {
				g_hash_table_insert (prefix_map,
				                     g_strdup (tracker_namespace_get_prefix (namespaces[i])),
				                     g_strdup (tracker_namespace_get_uri (namespaces[i])));
			}

			break;
		}
	}

	g_free (free_me);

	if (expanded_ns) {
		if (suffix) {
			*free_str = g_strconcat (expanded_ns, suffix, NULL);
			retval = *free_str;
		} else {
			retval = expanded_ns;
		}
	} else {
		retval = term;
	}

	return retval;
}

TrackerSparqlConnection *
tracker_data_manager_get_remote_connection (TrackerDataManager  *data_manager,
                                            const gchar         *uri,
                                            GError             **error)
{
	TrackerSparqlConnection *connection = NULL;
	GError *inner_error = NULL;
	gchar *uri_scheme = NULL;
	gchar *bus_name = NULL, *object_path = NULL;

	g_mutex_lock (&data_manager->connections_lock);

	connection = g_hash_table_lookup (data_manager->cached_connections, uri);

	if (!connection) {
		uri_scheme = g_uri_parse_scheme (uri);
		if (g_strcmp0 (uri_scheme, "dbus") == 0) {
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
			g_object_unref (dbus_connection);

			if (!connection)
				goto fail;
		} else if (g_strcmp0 (uri_scheme, "https") == 0 ||
			   g_strcmp0 (uri_scheme, "http") == 0) {
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
	g_clear_pointer (&bus_name, g_free);
	g_clear_pointer (&object_path, g_free);

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
