/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <libtracker-common/tracker-common.h>

#include "tracker-class.h"
#include "tracker-data-manager.h"
#include "tracker-data-update.h"
#include "tracker-data-query.h"
#include "tracker-db-interface-sqlite.h"
#include "tracker-db-manager.h"
#include "tracker-ontologies.h"
#include "tracker-property.h"
#include "tracker-sparql.h"
#include "tracker-turtle-reader.h"
#include "tracker-uuid.h"

typedef struct _TrackerDataUpdateBuffer TrackerDataUpdateBuffer;
typedef struct _TrackerDataUpdateBufferGraph TrackerDataUpdateBufferGraph;
typedef struct _TrackerDataUpdateBufferResource TrackerDataUpdateBufferResource;
typedef struct _TrackerDataUpdateBufferProperty TrackerDataUpdateBufferProperty;
typedef struct _TrackerDataUpdateBufferTable TrackerDataUpdateBufferTable;
typedef struct _TrackerDataBlankBuffer TrackerDataBlankBuffer;
typedef struct _TrackerStatementDelegate TrackerStatementDelegate;
typedef struct _TrackerCommitDelegate TrackerCommitDelegate;

struct _TrackerDataUpdateBuffer {
	/* string -> ID */
	GHashTable *resource_cache;
	/* set of IDs, key is same pointer than resource_cache, and owned there */
	GHashTable *new_resources;
	/* string -> TrackerDataUpdateBufferGraph */
	GPtrArray *graphs;
};

struct _TrackerDataUpdateBufferGraph {
	gchar *graph;
	TrackerRowid id;

	/* string -> TrackerDataUpdateBufferResource */
	GHashTable *resources;
	/* id -> integer */
	GHashTable *refcounts;
};

struct _TrackerDataUpdateBufferResource {
	const TrackerDataUpdateBufferGraph *graph;
	TrackerRowid id;
	gboolean create;
	gboolean modified;
	/* TrackerProperty -> GArray */
	GHashTable *predicates;
	/* string -> TrackerDataUpdateBufferTable */
	GHashTable *tables;
	/* TrackerClass */
	GPtrArray *types;

	gboolean fts_updated;
};

struct _TrackerDataUpdateBufferProperty {
	const gchar *name;
	GValue value;
	guint delete_all_values : 1;
	guint delete_value : 1;
};

struct _TrackerDataUpdateBufferTable {
	gboolean insert;
	gboolean delete_row;
	gboolean multiple_values;
	TrackerClass *class;
	/* TrackerDataUpdateBufferProperty */
	GArray *properties;
};

struct _TrackerStatementDelegate {
	TrackerStatementCallback callback;
	gpointer user_data;
};

struct _TrackerCommitDelegate {
	TrackerCommitCallback callback;
	gpointer user_data;
};

struct _TrackerData {
	GObject parent_instance;

	TrackerDataManager *manager;

	gboolean in_transaction;
	gboolean in_ontology_transaction;
	TrackerDataUpdateBuffer update_buffer;

	/* current resource */
	TrackerDataUpdateBufferResource *resource_buffer;
	time_t resource_time;
	gint transaction_modseq;
	gboolean has_persistent;

	GPtrArray *insert_callbacks;
	GPtrArray *delete_callbacks;
	GPtrArray *commit_callbacks;
	GPtrArray *rollback_callbacks;
};

struct _TrackerDataClass {
	GObjectClass parent_class;
};

enum {
	PROP_0,
	PROP_MANAGER
};

G_DEFINE_TYPE (TrackerData, tracker_data, G_TYPE_OBJECT)

static void         cache_insert_value         (TrackerData      *data,
                                                const gchar      *table_name,
                                                const gchar      *field_name,
                                                const GValue     *value,
                                                gboolean          multiple_values);
static GArray      *get_old_property_values    (TrackerData      *data,
                                                TrackerProperty  *property,
                                                GError          **error);
static gboolean     delete_metadata_decomposed (TrackerData      *data,
                                                TrackerProperty  *property,
                                                const GValue     *object,
                                                GError          **error);
static gboolean update_resource_single (TrackerData      *data,
                                        const gchar      *graph,
                                        TrackerResource  *resource,
                                        GHashTable       *visited,
                                        GHashTable       *bnodes,
                                        TrackerRowid     *id,
                                        GError          **error);

void tracker_data_insert_statement_with_uri    (TrackerData      *data,
                                                const gchar      *graph,
                                                TrackerRowid      subject,
                                                TrackerProperty  *predicate,
                                                const GValue     *object,
                                                GError          **error);
void tracker_data_insert_statement_with_string (TrackerData      *data,
                                                const gchar      *graph,
                                                TrackerRowid      subject,
                                                TrackerProperty  *predicate,
                                                const GValue     *object,
                                                GError          **error);


void
tracker_data_add_commit_statement_callback (TrackerData             *data,
                                            TrackerCommitCallback    callback,
                                            gpointer                 user_data)
{
	TrackerCommitDelegate *delegate = g_new0 (TrackerCommitDelegate, 1);

	if (!data->commit_callbacks) {
		data->commit_callbacks = g_ptr_array_new_with_free_func (g_free);
	}

	delegate->callback = callback;
	delegate->user_data = user_data;

	g_ptr_array_add (data->commit_callbacks, delegate);
}

void
tracker_data_remove_commit_statement_callback (TrackerData           *data,
                                               TrackerCommitCallback  callback,
                                               gpointer               user_data)
{
	TrackerCommitDelegate *delegate;
	guint i;

	if (!data->commit_callbacks) {
		return;
	}

	for (i = 0; i < data->commit_callbacks->len; i++) {
		delegate = g_ptr_array_index (data->commit_callbacks, i);
		if (delegate->callback == callback && delegate->user_data == user_data) {
			g_ptr_array_remove_index (data->commit_callbacks, i);
			return;
		}
	}
}

void
tracker_data_dispatch_commit_statement_callbacks (TrackerData *data)
{
	if (data->commit_callbacks) {
		guint n;
		for (n = 0; n < data->commit_callbacks->len; n++) {
			TrackerCommitDelegate *delegate;
			delegate = g_ptr_array_index (data->commit_callbacks, n);
			delegate->callback (delegate->user_data);
		}
	}
}

void
tracker_data_add_rollback_statement_callback (TrackerData             *data,
                                              TrackerCommitCallback    callback,
                                              gpointer                 user_data)
{
	TrackerCommitDelegate *delegate = g_new0 (TrackerCommitDelegate, 1);

	if (!data->rollback_callbacks) {
		data->rollback_callbacks = g_ptr_array_new_with_free_func (g_free);
	}

	delegate->callback = callback;
	delegate->user_data = user_data;

	g_ptr_array_add (data->rollback_callbacks, delegate);
}


void
tracker_data_remove_rollback_statement_callback (TrackerData          *data,
                                                 TrackerCommitCallback callback,
                                                 gpointer              user_data)
{
	TrackerCommitDelegate *delegate;
	guint i;

	if (!data->rollback_callbacks) {
		return;
	}

	for (i = 0; i < data->rollback_callbacks->len; i++) {
		delegate = g_ptr_array_index (data->rollback_callbacks, i);
		if (delegate->callback == callback && delegate->user_data == user_data) {
			g_ptr_array_remove_index (data->rollback_callbacks, i);
			return;
		}
	}
}

void
tracker_data_dispatch_rollback_statement_callbacks (TrackerData *data)
{
	if (data->rollback_callbacks) {
		guint n;
		for (n = 0; n < data->rollback_callbacks->len; n++) {
			TrackerCommitDelegate *delegate;
			delegate = g_ptr_array_index (data->rollback_callbacks, n);
			delegate->callback (delegate->user_data);
		}
	}
}

void
tracker_data_add_insert_statement_callback (TrackerData             *data,
                                            TrackerStatementCallback callback,
                                            gpointer                 user_data)
{
	TrackerStatementDelegate *delegate = g_new0 (TrackerStatementDelegate, 1);

	if (!data->insert_callbacks) {
		data->insert_callbacks = g_ptr_array_new_with_free_func (g_free);
	}

	delegate->callback = callback;
	delegate->user_data = user_data;

	g_ptr_array_add (data->insert_callbacks, delegate);
}

void
tracker_data_remove_insert_statement_callback (TrackerData             *data,
                                               TrackerStatementCallback callback,
                                               gpointer                 user_data)
{
	TrackerStatementDelegate *delegate;
	guint i;

	if (!data->insert_callbacks) {
		return;
	}

	for (i = 0; i < data->insert_callbacks->len; i++) {
		delegate = g_ptr_array_index (data->insert_callbacks, i);
		if (delegate->callback == callback && delegate->user_data == user_data) {
			g_ptr_array_remove_index (data->insert_callbacks, i);
			return;
		}
	}
}

void
tracker_data_dispatch_insert_statement_callbacks (TrackerData  *data,
                                                  TrackerRowid  predicate_id,
                                                  TrackerRowid  class_id)
{
	if (data->insert_callbacks) {
		guint n;

		for (n = 0; n < data->insert_callbacks->len; n++) {
			TrackerStatementDelegate *delegate;

			delegate = g_ptr_array_index (data->insert_callbacks, n);
			delegate->callback (data->resource_buffer->graph->id,
			                    data->resource_buffer->graph->graph,
			                    data->resource_buffer->id,
			                    predicate_id,
			                    class_id,
			                    data->resource_buffer->types,
			                    delegate->user_data);
		}
	}
}

void
tracker_data_add_delete_statement_callback (TrackerData             *data,
                                            TrackerStatementCallback callback,
                                            gpointer                 user_data)
{
	TrackerStatementDelegate *delegate = g_new0 (TrackerStatementDelegate, 1);

	if (!data->delete_callbacks) {
		data->delete_callbacks = g_ptr_array_new_with_free_func (g_free);
	}

	delegate->callback = callback;
	delegate->user_data = user_data;

	g_ptr_array_add (data->delete_callbacks, delegate);
}

void
tracker_data_remove_delete_statement_callback (TrackerData             *data,
                                               TrackerStatementCallback callback,
                                               gpointer                 user_data)
{
	TrackerStatementDelegate *delegate;
	guint i;

	if (!data->delete_callbacks) {
		return;
	}

	for (i = 0; i < data->delete_callbacks->len; i++) {
		delegate = g_ptr_array_index (data->delete_callbacks, i);
		if (delegate->callback == callback && delegate->user_data == user_data) {
			g_ptr_array_remove_index (data->delete_callbacks, i);
			return;
		}
	}
}

void
tracker_data_dispatch_delete_statement_callbacks (TrackerData  *data,
                                                  TrackerRowid  predicate_id,
                                                  TrackerRowid  class_id)
{
	if (data->delete_callbacks) {
		guint n;

		for (n = 0; n < data->delete_callbacks->len; n++) {
			TrackerStatementDelegate *delegate;

			delegate = g_ptr_array_index (data->delete_callbacks, n);
			delegate->callback (data->resource_buffer->graph->id,
			                    data->resource_buffer->graph->graph,
			                    data->resource_buffer->id,
			                    predicate_id,
			                    class_id,
			                    data->resource_buffer->types,
			                    delegate->user_data);
		}
	}
}

static gboolean
tracker_data_update_initialize_modseq (TrackerData  *data,
                                       GError      **error)
{
	TrackerDBCursor    *cursor = NULL;
	TrackerDBInterface *temp_iface;
	TrackerDBStatement *stmt;
	GError             *inner_error = NULL;
	gint                max_modseq = 0;

	/* Is it already initialized? */
	if (data->transaction_modseq != 0)
		return TRUE;

	temp_iface = tracker_data_manager_get_writable_db_interface (data->manager);

	stmt = tracker_db_interface_create_statement (temp_iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, &inner_error,
	                                              "SELECT MAX(\"nrl:modified\") AS A FROM \"rdfs:Resource\"");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &inner_error);
		g_object_unref (stmt);
	}

	if (cursor) {
		if (tracker_db_cursor_iter_next (cursor, NULL, &inner_error)) {
			max_modseq = tracker_db_cursor_get_int (cursor, 0);
			data->transaction_modseq = max_modseq + 1;
		}

		g_object_unref (cursor);
	}

	if (G_UNLIKELY (inner_error)) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}

static void
tracker_data_init (TrackerData *data)
{
}

static void
tracker_data_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
	TrackerData *data = TRACKER_DATA (object);

	switch (prop_id) {
	case PROP_MANAGER:
		data->manager = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_data_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
	TrackerData *data = TRACKER_DATA (object);

	switch (prop_id) {
	case PROP_MANAGER:
		g_value_set_object (value, data->manager);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tracker_data_class_init (TrackerDataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = tracker_data_set_property;
	object_class->get_property = tracker_data_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_MANAGER,
	                                 g_param_spec_object ("manager",
	                                                      "manager",
	                                                      "manager",
	                                                      TRACKER_TYPE_DATA_MANAGER,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY));
}

TrackerData *
tracker_data_new (TrackerDataManager *manager)
{
	return g_object_new (TRACKER_TYPE_DATA,
	                     "manager", manager,
	                     NULL);
}

static gint
get_transaction_modseq (TrackerData *data)
{
	/* Always use 1 for ontology transactions */
	if (data->in_ontology_transaction) {
		return 1;
	}

	return data->transaction_modseq;
}

static TrackerDataUpdateBufferTable *
cache_table_new (gboolean multiple_values)
{
	TrackerDataUpdateBufferTable *table;

	table = g_slice_new0 (TrackerDataUpdateBufferTable);
	table->multiple_values = multiple_values;
	table->properties = g_array_sized_new (FALSE, FALSE, sizeof (TrackerDataUpdateBufferProperty), 4);

	return table;
}

static void
cache_table_free (TrackerDataUpdateBufferTable *table)
{
	TrackerDataUpdateBufferProperty *property;
	guint i;

	for (i = 0; i < table->properties->len; i++) {
		property = &g_array_index (table->properties, TrackerDataUpdateBufferProperty, i);
		g_value_unset (&property->value);
	}

	g_array_free (table->properties, TRUE);
	g_slice_free (TrackerDataUpdateBufferTable, table);
}

static TrackerDataUpdateBufferTable *
cache_ensure_table (TrackerData *data,
                    const gchar *table_name,
                    gboolean     multiple_values)
{
	TrackerDataUpdateBufferTable *table;

	if (!data->resource_buffer->modified) {
		/* first modification of this particular resource, update nrl:modified */

		GValue gvalue = { 0 };

		data->resource_buffer->modified = TRUE;

		g_value_init (&gvalue, G_TYPE_INT64);
		g_value_set_int64 (&gvalue, get_transaction_modseq (data));
		cache_insert_value (data, "rdfs:Resource", "nrl:modified",
		                    &gvalue, FALSE);
	}

	table = g_hash_table_lookup (data->resource_buffer->tables, table_name);
	if (table == NULL) {
		table = cache_table_new (multiple_values);
		g_hash_table_insert (data->resource_buffer->tables, g_strdup (table_name), table);
		table->insert = multiple_values;
	}

	return table;
}

static void
cache_insert_row (TrackerData  *data,
                  TrackerClass *class)
{
	TrackerDataUpdateBufferTable *table;

	table = cache_ensure_table (data, tracker_class_get_name (class), FALSE);
	table->class = class;
	table->insert = TRUE;
}

static void
cache_insert_value (TrackerData  *data,
                    const gchar  *table_name,
                    const gchar  *field_name,
                    const GValue *value,
                    gboolean      multiple_values)
{
	TrackerDataUpdateBufferTable    *table;
	TrackerDataUpdateBufferProperty  property = { 0 };

	/* No need to strdup here, the incoming string is either always static, or
	 * long-standing as tracker_property_get_name return value. */
	property.name = field_name;

	g_value_init (&property.value, G_VALUE_TYPE (value));
	g_value_copy (value, &property.value);

	table = cache_ensure_table (data, table_name, multiple_values);
	g_array_append_val (table->properties, property);
}

static void
cache_delete_row (TrackerData  *data,
                  TrackerClass *class)
{
	TrackerDataUpdateBufferTable    *table;

	table = cache_ensure_table (data, tracker_class_get_name (class), FALSE);
	table->class = class;
	table->delete_row = TRUE;
}

/* Use only for multi-valued properties */
static void
cache_delete_all_values (TrackerData *data,
                         const gchar *table_name,
                         const gchar *field_name)
{
	TrackerDataUpdateBufferTable    *table;
	TrackerDataUpdateBufferProperty  property = { 0 };

	property.name = field_name;
	property.delete_all_values = TRUE;

	table = cache_ensure_table (data, table_name, TRUE);
	g_array_append_val (table->properties, property);
}

static void
cache_delete_value (TrackerData  *data,
                    const gchar  *table_name,
                    const gchar  *field_name,
                    const GValue *value,
                    gboolean      multiple_values)
{
	TrackerDataUpdateBufferTable    *table;
	TrackerDataUpdateBufferProperty  property = { 0 };

	property.name = field_name;
	property.delete_value = TRUE;

	g_value_init (&property.value, G_VALUE_TYPE (value));
	g_value_copy (value, &property.value);

	table = cache_ensure_table (data, table_name, multiple_values);
	g_array_append_val (table->properties, property);
}

static TrackerRowid
query_resource_id (TrackerData  *data,
                   const gchar  *uri,
                   GError      **error)
{
	TrackerDBInterface *iface;
	TrackerRowid *value, id;

	value = g_hash_table_lookup (data->update_buffer.resource_cache, uri);

	if (value == NULL) {
		iface = tracker_data_manager_get_writable_db_interface (data->manager);
		id = tracker_data_query_resource_id (data->manager, iface, uri, error);

		if (id != 0) {
			g_hash_table_insert (data->update_buffer.resource_cache, g_strdup (uri),
			                     tracker_rowid_copy (&id));
		}

		return id;
	}

	return *value;
}

TrackerRowid
tracker_data_update_ensure_resource (TrackerData  *data,
                                     const gchar  *uri,
                                     GError      **error)
{
	TrackerDBManager *db_manager;
	TrackerDBManagerFlags db_flags;
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt = NULL;
	GError *inner_error = NULL;
	gchar *key;
	TrackerRowid *value, id;

	value = g_hash_table_lookup (data->update_buffer.resource_cache, uri);

	if (value != NULL) {
		return *value;
	}

	db_manager = tracker_data_manager_get_db_manager (data->manager);
	db_flags = tracker_db_manager_get_flags (db_manager, NULL, NULL);

	if ((db_flags & TRACKER_DB_MANAGER_ANONYMOUS_BNODES) == 0 &&
	    g_str_has_prefix (uri, "urn:bnode:")) {
		gchar *end;

		id = g_ascii_strtoll (&uri[strlen ("urn:bnode:")], &end, 10);
		if (id != 0 && end == &uri[strlen (uri)])
			return id;
	}

	iface = tracker_data_manager_get_writable_db_interface (data->manager);

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE, &inner_error,
	                                              "INSERT INTO Resource (Uri, BlankNode) VALUES (?, ?)");

	if (stmt) {
		tracker_db_statement_bind_text (stmt, 0, uri);
		tracker_db_statement_bind_int (stmt, 1, FALSE);
		tracker_db_statement_execute (stmt, &inner_error);
		g_object_unref (stmt);
	}

	if (inner_error) {
		if (g_error_matches (inner_error,
		                     TRACKER_DB_INTERFACE_ERROR,
		                     TRACKER_DB_CONSTRAINT)) {
			g_clear_error (&inner_error);
			id = query_resource_id (data, uri, &inner_error);

			if (id != 0)
				return id;
		}

		g_propagate_error (error, inner_error);

		return 0;
	}

	id = tracker_db_interface_sqlite_get_last_insert_id (iface);
	key = g_strdup (uri);
	g_hash_table_insert (data->update_buffer.resource_cache, key,
	                     tracker_rowid_copy (&id));

	g_hash_table_add (data->update_buffer.new_resources,
	                  tracker_rowid_copy (&id));

	return id;
}

static void
statement_bind_gvalue (TrackerDBStatement *stmt,
                       gint               *idx,
                       const GValue       *value)
{
	GType type;

	type = G_VALUE_TYPE (value);
	switch (type) {
	case G_TYPE_STRING:
		tracker_db_statement_bind_text (stmt, (*idx)++, g_value_get_string (value));
		break;
	case G_TYPE_INT:
		tracker_db_statement_bind_int (stmt, (*idx)++, g_value_get_int (value));
		break;
	case G_TYPE_INT64:
		tracker_db_statement_bind_int (stmt, (*idx)++, g_value_get_int64 (value));
		break;
	case G_TYPE_DOUBLE:
		tracker_db_statement_bind_double (stmt, (*idx)++, g_value_get_double (value));
		break;
	case G_TYPE_BOOLEAN:
		tracker_db_statement_bind_int (stmt, (*idx)++, g_value_get_boolean (value));
		break;
	default:
		if (type == G_TYPE_DATE_TIME) {
			GDateTime *datetime = g_value_get_boxed (value);

			/* If we have anything that prevents a unix timestamp to be
			 * lossless, we use the ISO8601 string.
			 */
			if (g_date_time_get_utc_offset (datetime) != 0 ||
			    g_date_time_get_microsecond (datetime) != 0) {
				gchar *str;

				str = tracker_date_format_iso8601 (datetime);
				tracker_db_statement_bind_text (stmt, (*idx)++, str);
				g_free (str);
			} else {
				tracker_db_statement_bind_int (stmt, (*idx)++,
				                               g_date_time_to_unix (datetime));
			}
		} else if (type == G_TYPE_BYTES) {
			GBytes *bytes;
			gconstpointer data;
			gsize len;

			bytes = g_value_get_boxed (value);
			data = g_bytes_get_data (bytes, &len);

			if (len == strlen (data) + 1) {
				/* No ancillary data */
				tracker_db_statement_bind_text (stmt, (*idx)++, data);
			} else {
				/* String with langtag */
				tracker_db_statement_bind_bytes (stmt, (*idx)++, bytes);
			}
		} else if (g_strcmp0 (g_type_name (type), "TrackerUri") == 0) {
			/* FIXME: We can't access TrackerUri GType here */
			tracker_db_statement_bind_text (stmt, (*idx)++, g_value_get_string (value));
		} else {
			g_warning ("Unknown type for binding: %s\n", G_VALUE_TYPE_NAME (value));
		}
		break;
	}
}

static void
tracker_data_resource_buffer_flush (TrackerData                      *data,
                                    TrackerDataUpdateBufferResource  *resource,
                                    GError                          **error)
{
	TrackerDBInterface             *iface;
	TrackerDBStatement             *stmt;
	TrackerDataUpdateBufferTable    *table;
	TrackerDataUpdateBufferProperty *property;
	GHashTableIter                  iter;
	const gchar                    *table_name, *database;
	guint                           i;
        gint                            param;
	GError                         *actual_error = NULL;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);
	database = resource->graph->graph ? resource->graph->graph : "main";

	g_hash_table_iter_init (&iter, resource->tables);
	while (g_hash_table_iter_next (&iter, (gpointer*) &table_name, (gpointer*) &table)) {
		if (table->multiple_values) {
			for (i = 0; i < table->properties->len; i++) {
				property = &g_array_index (table->properties, TrackerDataUpdateBufferProperty, i);

				if (property->delete_all_values) {
					stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE, &actual_error,
					                                               "DELETE FROM \"%s\".\"%s\" WHERE ID = ?",
					                                               database,
					                                               table_name);
				} else if (property->delete_value) {
					/* delete rows for multiple value properties */
					stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE, &actual_error,
					                                               "DELETE FROM \"%s\".\"%s\" WHERE ID = ? AND \"%s\" = ?",
					                                               database,
					                                               table_name,
					                                               property->name);
				} else {
					stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE, &actual_error,
					                                               "INSERT OR IGNORE INTO \"%s\".\"%s\" (ID, \"%s\") VALUES (?, ?)",
					                                               database,
					                                               table_name,
					                                               property->name);
				}

				if (actual_error) {
					g_propagate_error (error, actual_error);
					return;
				}

				param = 0;

				tracker_db_statement_bind_int (stmt, param++, resource->id);

				if (!property->delete_all_values)
					statement_bind_gvalue (stmt, &param, &property->value);

				tracker_db_statement_execute (stmt, &actual_error);
				g_object_unref (stmt);

				if (actual_error) {
					g_propagate_error (error, actual_error);
					return;
				}
			}
		} else {
			GString *sql, *values_sql;
			GHashTable *visited_properties;
			gint n;

			if (table->delete_row) {
				/* remove row from class table */
				stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE, &actual_error,
				                                               "DELETE FROM \"%s\".\"%s\" WHERE ID = ?",
				                                               database, table_name);

				if (stmt) {
					tracker_db_statement_bind_int (stmt, 0, resource->id);
					tracker_db_statement_execute (stmt, &actual_error);
					g_object_unref (stmt);
				}

				if (actual_error) {
					g_propagate_error (error, actual_error);
					return;
				}

				continue;
			}

			if (table->insert) {
				sql = g_string_new ("INSERT INTO ");
				values_sql = g_string_new ("VALUES (?");
			} else {
				sql = g_string_new ("UPDATE ");
				values_sql = NULL;
			}

			g_string_append_printf (sql, "\"%s\".\"%s\"",
			                        database, table_name);

			if (table->insert) {
				g_string_append (sql, " (ID");

				if (strcmp (table_name, "rdfs:Resource") == 0) {
					g_string_append (sql, ", \"nrl:added\", \"nrl:modified\"");
					g_string_append (values_sql, ", ?, ?");
				}
			} else {
				g_string_append (sql, " SET ");
			}

			visited_properties = g_hash_table_new (g_str_hash, g_str_equal);

			for (n = table->properties->len - 1; n >= 0; n--) {
				property = &g_array_index (table->properties, TrackerDataUpdateBufferProperty, n);
				if (g_hash_table_contains (visited_properties, property->name))
					continue;

				if (table->insert) {
					g_string_append_printf (sql, ", \"%s\"", property->name);
					g_string_append (values_sql, ", ?");
				} else {
					if (n < (int) table->properties->len - 1) {
						g_string_append (sql, ", ");
					}
					g_string_append_printf (sql, "\"%s\" = ?", property->name);
				}

				g_hash_table_add (visited_properties, (gpointer) property->name);
			}

			g_hash_table_unref (visited_properties);

			if (table->insert) {
				g_string_append (sql, ")");
				g_string_append (values_sql, ")");

				stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE, &actual_error,
				                                               "%s %s", sql->str, values_sql->str);
				g_string_free (sql, TRUE);
				g_string_free (values_sql, TRUE);
			} else {
				g_string_append (sql, " WHERE ID = ?");

				stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE, &actual_error,
				                                              sql->str);
				g_string_free (sql, TRUE);
			}

			if (actual_error) {
				g_propagate_error (error, actual_error);
				return;
			}

			if (table->insert) {
				tracker_db_statement_bind_int (stmt, 0, resource->id);

				if (strcmp (table_name, "rdfs:Resource") == 0) {
					g_warn_if_fail	(data->resource_time != 0);
					tracker_db_statement_bind_int (stmt, 1, (gint64) data->resource_time);
					tracker_db_statement_bind_int (stmt, 2, get_transaction_modseq (data));
					param = 3;
				} else {
					param = 1;
				}
			} else {
				param = 0;
			}

			visited_properties = g_hash_table_new (g_str_hash, g_str_equal);

			for (n = table->properties->len - 1; n >= 0; n--) {
				property = &g_array_index (table->properties, TrackerDataUpdateBufferProperty, n);
				if (g_hash_table_contains (visited_properties, property->name))
					continue;

				if (property->delete_value) {
					/* just set value to NULL for single value properties */
					tracker_db_statement_bind_null (stmt, param++);
				} else {
					statement_bind_gvalue (stmt, &param, &property->value);
				}

				g_hash_table_add (visited_properties, (gpointer) property->name);
			}

			g_hash_table_unref (visited_properties);

			if (!table->insert) {
				tracker_db_statement_bind_int (stmt, param++, resource->id);
			}

			tracker_db_statement_execute (stmt, &actual_error);
			g_object_unref (stmt);

			if (actual_error) {
				g_propagate_error (error, actual_error);
				return;
			}
		}
	}

	if (resource->fts_updated) {
		TrackerProperty *prop;
		GArray *values;
		GPtrArray *properties, *text;

		properties = text = NULL;
		g_hash_table_iter_init (&iter, resource->predicates);
		while (g_hash_table_iter_next (&iter, (gpointer*) &prop, (gpointer*) &values)) {
			if (tracker_property_get_fulltext_indexed (prop)) {
				GString *fts;

				fts = g_string_new ("");
				for (i = 0; i < values->len; i++) {
					GValue *v = &g_array_index (values, GValue, i);
					g_string_append (fts, g_value_get_string (v));
					g_string_append_c (fts, ' ');
				}

				if (!properties && !text) {
					properties = g_ptr_array_new ();
					text = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);
				}

				g_ptr_array_add (properties, (gpointer) tracker_property_get_name (prop));
				g_ptr_array_add (text, g_string_free (fts, FALSE));
			}
		}

		if (properties && text) {
			g_ptr_array_add (properties, NULL);
			g_ptr_array_add (text, NULL);

			tracker_db_interface_sqlite_fts_update_text (iface,
								     database,
			                                             resource->id,
			                                             (const gchar **) properties->pdata,
			                                             (const gchar **) text->pdata);
			g_ptr_array_free (properties, TRUE);
			g_ptr_array_free (text, TRUE);
		}
	}
}

static void
tracker_data_update_refcount (TrackerData  *data,
                              TrackerRowid  id,
                              gint          refcount)
{
	const TrackerDataUpdateBufferGraph *graph;
	gint old_refcount;

	g_assert (data->resource_buffer != NULL);
	graph = data->resource_buffer->graph;

	old_refcount = GPOINTER_TO_INT (g_hash_table_lookup (graph->refcounts, &id));
	g_hash_table_insert (graph->refcounts,
	                     tracker_rowid_copy (&id),
	                     GINT_TO_POINTER (old_refcount + refcount));
}

static void
tracker_data_resource_ref (TrackerData  *data,
                           TrackerRowid  id,
                           gboolean      multivalued)
{
	if (multivalued)
		tracker_data_update_refcount (data, data->resource_buffer->id, 1);

	tracker_data_update_refcount (data, id, 1);
}

static void
tracker_data_resource_unref (TrackerData  *data,
                             TrackerRowid  id,
                             gboolean      multivalued)
{
	if (multivalued)
		tracker_data_update_refcount (data, data->resource_buffer->id, -1);

	tracker_data_update_refcount (data, id, -1);
}

/* Only applies to multivalued properties */
static gboolean
tracker_data_resource_unref_all (TrackerData      *data,
                                 TrackerProperty  *property,
                                 GError          **error)
{
	GArray *old_values;
	guint i;

	g_assert (tracker_property_get_multiple_values (property) == TRUE);
	g_assert (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE);

	old_values = get_old_property_values (data, property, error);
	if (!old_values)
		return FALSE;

	for (i = 0; i < old_values->len; i++) {
		GValue *value;

		value = &g_array_index (old_values, GValue, i);
		tracker_data_resource_unref (data, g_value_get_int64 (value), TRUE);
	}

	return TRUE;
}

static void
tracker_data_flush_graph_refcounts (TrackerData                   *data,
                                    TrackerDataUpdateBufferGraph  *graph,
                                    GError                       **error)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	GHashTableIter iter;
	gpointer key, value;
	TrackerRowid id;
	gint refcount;
	GError *inner_error = NULL;
	const gchar *database;
	gchar *insert_query;
	gchar *update_query;
	gchar *delete_query;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);
	database = graph->graph ? graph->graph : "main";

	insert_query = g_strdup_printf ("INSERT OR IGNORE INTO \"%s\".Refcount (ROWID, Refcount) VALUES (?1, 0)",
	                                database);
	update_query = g_strdup_printf ("UPDATE \"%s\".Refcount SET Refcount = Refcount + ?2 WHERE Refcount.ROWID = ?1",
	                                database);
	delete_query = g_strdup_printf ("DELETE FROM \"%s\".Refcount WHERE Refcount.ROWID = ?1 AND Refcount.Refcount = 0",
	                                database);

	g_hash_table_iter_init (&iter, graph->refcounts);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		id = *(TrackerRowid *) key;
		refcount = GPOINTER_TO_INT (value);

		if (refcount > 0) {
			stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE,
			                                              &inner_error, insert_query);
			if (inner_error) {
				g_propagate_error (error, inner_error);
				break;
			}

			tracker_db_statement_bind_int (stmt, 0, id);
			tracker_db_statement_execute (stmt, &inner_error);
			g_object_unref (stmt);

			if (inner_error) {
				g_propagate_error (error, inner_error);
				break;
			}
		}

		if (refcount != 0) {
			stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE,
			                                              &inner_error, update_query);
			if (inner_error) {
				g_propagate_error (error, inner_error);
				break;
			}

			tracker_db_statement_bind_int (stmt, 0, id);
			tracker_db_statement_bind_int (stmt, 1, refcount);
			tracker_db_statement_execute (stmt, &inner_error);
			g_object_unref (stmt);

			if (inner_error) {
				g_propagate_error (error, inner_error);
				break;
			}
		}

		if (refcount < 0) {
			stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE,
			                                              &inner_error, delete_query);
			if (inner_error) {
				g_propagate_error (error, inner_error);
				break;
			}

			tracker_db_statement_bind_int (stmt, 0, id);
			tracker_db_statement_execute (stmt, &inner_error);
			g_object_unref (stmt);

			if (inner_error) {
				g_propagate_error (error, inner_error);
				break;
			}
		}

		g_hash_table_iter_remove (&iter);
	}

	g_free (insert_query);
	g_free (update_query);
	g_free (delete_query);
}

static void
graph_buffer_free (TrackerDataUpdateBufferGraph *graph)
{
	g_hash_table_unref (graph->resources);
	g_hash_table_unref (graph->refcounts);
	g_free (graph->graph);
	g_slice_free (TrackerDataUpdateBufferGraph, graph);
}

static void resource_buffer_free (TrackerDataUpdateBufferResource *resource)
{
	g_hash_table_unref (resource->predicates);
	g_hash_table_unref (resource->tables);

	g_ptr_array_free (resource->types, TRUE);
	resource->types = NULL;

	g_slice_free (TrackerDataUpdateBufferResource, resource);
}

void
tracker_data_update_buffer_flush (TrackerData  *data,
                                  GError      **error)
{
	TrackerDataUpdateBufferGraph *graph;
	TrackerDataUpdateBufferResource *resource;
	GHashTableIter iter;
	GError *actual_error = NULL;
	guint i;

	for (i = 0; i < data->update_buffer.graphs->len; i++) {
		graph = g_ptr_array_index (data->update_buffer.graphs, i);
		g_hash_table_iter_init (&iter, graph->resources);

		while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &resource)) {
			tracker_data_resource_buffer_flush (data, resource, &actual_error);
			if (actual_error) {
				g_propagate_error (error, actual_error);
				goto out;
			}
		}

		tracker_data_flush_graph_refcounts (data, graph, &actual_error);
		if (actual_error) {
			g_propagate_error (error, actual_error);
			goto out;
		}
	}

out:
	g_ptr_array_set_size (data->update_buffer.graphs, 0);
	g_hash_table_remove_all (data->update_buffer.new_resources);
	data->resource_buffer = NULL;
}

void
tracker_data_update_buffer_might_flush (TrackerData  *data,
                                        GError      **error)
{
	TrackerDataUpdateBufferGraph *graph;
	guint i, count = 0;

	for (i = 0; i < data->update_buffer.graphs->len; i++) {
		graph = g_ptr_array_index (data->update_buffer.graphs, i);
		count += g_hash_table_size (graph->resources);

		if (count >= 50) {
			tracker_data_update_buffer_flush (data, error);
			break;
		}
	}
}

static void
tracker_data_update_buffer_clear (TrackerData *data)
{
	g_ptr_array_set_size (data->update_buffer.graphs, 0);
	g_hash_table_remove_all (data->update_buffer.new_resources);
	g_hash_table_remove_all (data->update_buffer.resource_cache);
	data->resource_buffer = NULL;
}

static gboolean
cache_create_service_decomposed (TrackerData   *data,
                                 TrackerClass  *cl,
                                 GError       **error)
{
	TrackerClass       **super_classes;
	TrackerProperty    **domain_indexes;
	GValue              gvalue = { 0 };
	guint               i;
        TrackerRowid        class_id;
	TrackerOntologies  *ontologies;

	/* also create instance of all super classes */
	super_classes = tracker_class_get_super_classes (cl);
	while (*super_classes) {
		if (!cache_create_service_decomposed (data, *super_classes, error))
			return FALSE;
		super_classes++;
	}

	for (i = 0; i < data->resource_buffer->types->len; i++) {
		if (g_ptr_array_index (data->resource_buffer->types, i) == cl) {
			/* ignore duplicate statement */
			return TRUE;
		}
	}

	g_ptr_array_add (data->resource_buffer->types, cl);

	g_value_init (&gvalue, G_TYPE_INT64);

	cache_insert_row (data, cl);
	tracker_data_resource_ref (data, data->resource_buffer->id, FALSE);

	class_id = tracker_class_get_id (cl);
	ontologies = tracker_data_manager_get_ontologies (data->manager);

	g_value_set_int64 (&gvalue, class_id);
	cache_insert_value (data, "rdfs:Resource_rdf:type", "rdf:type",
	                    &gvalue, TRUE);
	tracker_data_resource_ref (data, class_id, TRUE);

	tracker_data_dispatch_insert_statement_callbacks (data,
	                                                  tracker_property_get_id (tracker_ontologies_get_rdf_type (ontologies)),
	                                                  class_id);

	/* When a new class created, make sure we propagate to the domain indexes
	 * the property values already set, if any. */
	domain_indexes = tracker_class_get_domain_indexes (cl);
	if (!domain_indexes) {
		/* Nothing else to do, return */
		return TRUE;
	}

	while (*domain_indexes) {
		GError *inner_error = NULL;
		GArray *old_values;

		/* read existing property values */
		old_values = get_old_property_values (data, *domain_indexes, &inner_error);
		if (inner_error) {
			g_propagate_prefixed_error (error,
			                            inner_error,
			                            "Getting old values for '%s':",
			                            tracker_property_get_name (*domain_indexes));
			return FALSE;
		}

		if (old_values &&
		    old_values->len > 0) {
			GValue *v;

			/* Don't expect several values for property which is a domain index */
			g_assert_cmpint (old_values->len, ==, 1);

			g_debug ("Propagating '%s' property value from '%s' to domain index in '%s'",
			         tracker_property_get_name (*domain_indexes),
			         tracker_property_get_table_name (*domain_indexes),
			         tracker_class_get_name (cl));

			v = &g_array_index (old_values, GValue, 0);

			cache_insert_value (data,
			                    tracker_class_get_name (cl),
			                    tracker_property_get_name (*domain_indexes),
			                    v,
			                    tracker_property_get_multiple_values (*domain_indexes));
		}

		domain_indexes++;
	}

	return TRUE;
}

static gboolean
value_equal (const GValue *value1,
             const GValue *value2)
{
	GType type = G_VALUE_TYPE (value1);

	if (type != G_VALUE_TYPE (value2)) {
		/* Handle booleans specially */
		if (type == G_TYPE_BOOLEAN && G_VALUE_TYPE (value2) == G_TYPE_INT64) {
			return (g_value_get_boolean (value1) ==
			        (g_value_get_int64 (value2) != 0));
		} else if (type == G_TYPE_INT64 && G_VALUE_TYPE (value2) == G_TYPE_BOOLEAN) {
			return ((g_value_get_int64 (value1) != 0) ==
			        g_value_get_boolean (value2));
		}

		return FALSE;
	}

	switch (type) {
	case G_TYPE_STRING:
		return (strcmp (g_value_get_string (value1), g_value_get_string (value2)) == 0);
	case G_TYPE_INT64:
		return g_value_get_int64 (value1) == g_value_get_int64 (value2);
	case G_TYPE_BOOLEAN:
		return g_value_get_boolean (value1) == g_value_get_boolean (value2);
	case G_TYPE_DOUBLE:
		/* does RDF define equality for floating point values? */
		return g_value_get_double (value1) == g_value_get_double (value2);
	default:
		if (type == G_TYPE_DATE_TIME) {
			/* UTC offset is ignored for comparison, irrelevant according to xsd:dateTime spec
			 * http://www.w3.org/TR/xmlschema-2/#dateTime
			 */
			return g_date_time_compare (g_value_get_boxed (value1),
			                            g_value_get_boxed (value2)) == 0;
		}

		g_critical ("No conversion for type %s", g_type_name (type));
		g_assert_not_reached ();
	}
}

static gboolean
value_set_add_value (GArray       *value_set,
                     const GValue *value)
{
	GValue gvalue_copy = { 0 };
	guint i;

	g_return_val_if_fail (G_VALUE_TYPE (value), FALSE);

	for (i = 0; i < value_set->len; i++) {
		GValue *v;

		v = &g_array_index (value_set, GValue, i);
		if (value_equal (v, value)) {
			/* no change, value already in set */
			return FALSE;
		}
	}

	g_value_init (&gvalue_copy, G_VALUE_TYPE (value));
	g_value_copy (value, &gvalue_copy);
	g_array_append_val (value_set, gvalue_copy);

	return TRUE;
}

static gboolean
value_set_remove_value (GArray       *value_set,
                        const GValue *value)
{
	guint i;

	g_return_val_if_fail (G_VALUE_TYPE (value), FALSE);

	for (i = 0; i < value_set->len; i++) {
		GValue *v = &g_array_index (value_set, GValue, i);

		if (value_equal (v, value)) {
			/* value found, remove from set */
			g_array_remove_index (value_set, i);

			return TRUE;
		}
	}

	/* no change, value not found */
	return FALSE;
}

static gboolean
check_property_domain (TrackerData     *data,
                       TrackerProperty *property)
{
	guint type_index;

	for (type_index = 0; type_index < data->resource_buffer->types->len; type_index++) {
		if (tracker_property_get_domain (property) == g_ptr_array_index (data->resource_buffer->types, type_index)) {
			return TRUE;
		}
	}
	return FALSE;
}

static GArray *
get_property_values (TrackerData      *data,
                     TrackerProperty  *property,
                     GError          **error)
{
	gboolean multiple_values;
	const gchar *database;
	GArray *old_values;

	multiple_values = tracker_property_get_multiple_values (property);

	old_values = g_array_sized_new (FALSE, TRUE, sizeof (GValue), multiple_values ? 4 : 1);
	g_array_set_clear_func (old_values, (GDestroyNotify) g_value_unset);
	g_hash_table_insert (data->resource_buffer->predicates, g_object_ref (property), old_values);

	database = data->resource_buffer->graph->graph ?
		data->resource_buffer->graph->graph : "main";

	if (!data->resource_buffer->create) {
		TrackerDBInterface *iface;
		TrackerDBStatement *stmt;
		TrackerDBCursor    *cursor = NULL;
		const gchar        *table_name;
		const gchar        *field_name;
		GError             *inner_error = NULL;

		table_name = tracker_property_get_table_name (property);
		field_name = tracker_property_get_name (property);

		iface = tracker_data_manager_get_writable_db_interface (data->manager);

		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, &inner_error,
		                                               "SELECT \"%s\" FROM \"%s\".\"%s\" WHERE ID = ?",
		                                               field_name, database, table_name);

		if (stmt) {
			tracker_db_statement_bind_int (stmt, 0, data->resource_buffer->id);
			cursor = tracker_db_statement_start_cursor (stmt, &inner_error);
			g_object_unref (stmt);
		}

		if (cursor) {
			while (tracker_db_cursor_iter_next (cursor, NULL, &inner_error)) {
				GValue gvalue = { 0 };

				tracker_db_cursor_get_value (cursor, 0, &gvalue);

				if (G_VALUE_TYPE (&gvalue)) {
					if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME) {
						GDateTime *datetime;

						if (G_VALUE_TYPE (&gvalue) == G_TYPE_INT64) {
							datetime = g_date_time_new_from_unix_utc (g_value_get_int64 (&gvalue));
							g_value_unset (&gvalue);
							g_value_init (&gvalue, G_TYPE_DATE_TIME);
							g_value_take_boxed (&gvalue, datetime);
						} else {
							datetime = tracker_date_new_from_iso8601 (g_value_get_string (&gvalue),
												  &inner_error);
							g_value_unset (&gvalue);

							if (inner_error) {
								g_propagate_prefixed_error (error,
								                            inner_error,
											    "Error in date conversion:");
								return NULL;
							}

							g_value_init (&gvalue, G_TYPE_DATE_TIME);
							g_value_take_boxed (&gvalue, datetime);
						}
					}

					g_array_append_val (old_values, gvalue);
				}
			}

			g_object_unref (cursor);
		}

		if (inner_error) {
			g_propagate_error (error, inner_error);
			return NULL;
		}
	}

	return old_values;
}

static GArray *
get_old_property_values (TrackerData      *data,
                         TrackerProperty  *property,
                         GError          **error)
{
	GArray *old_values;
	const gchar *database;

	database = data->resource_buffer->graph->graph ?
		data->resource_buffer->graph->graph : "main";

	/* read existing property values */
	old_values = g_hash_table_lookup (data->resource_buffer->predicates, property);
	if (old_values == NULL) {
		if (!check_property_domain (data, property)) {
			TrackerDBInterface *iface;
			gchar *resource;

			iface = tracker_data_manager_get_writable_db_interface (data->manager);
			resource = tracker_data_query_resource_urn (data->manager,
			                                            iface,
			                                            data->resource_buffer->id);

			g_set_error (error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_CONSTRAINT,
			             "%s %s is not is not a %s, cannot have property `%s'",
			             resource ? "Subject" : "Blank node",
			             resource ? resource : "",
			             tracker_class_get_name (tracker_property_get_domain (property)),
			             tracker_property_get_name (property));
			g_free (resource);

			return NULL;
		}

		if (tracker_property_get_fulltext_indexed (property)) {
			TrackerDBInterface *iface;

			iface = tracker_data_manager_get_writable_db_interface (data->manager);

			if (!data->resource_buffer->fts_updated && !data->resource_buffer->create) {
				TrackerOntologies *ontologies;
				guint i, n_props;
				TrackerProperty   **properties, *prop;
				GPtrArray *fts_props, *fts_text;

				/* first fulltext indexed property to be modified
				 * retrieve values of all fulltext indexed properties
				 */
				ontologies = tracker_data_manager_get_ontologies (data->manager);
				properties = tracker_ontologies_get_properties (ontologies, &n_props);

				fts_props = g_ptr_array_new ();
				fts_text = g_ptr_array_new_with_free_func (g_free);

				for (i = 0; i < n_props; i++) {
					prop = properties[i];

					if (tracker_property_get_fulltext_indexed (prop)
					    && check_property_domain (data, prop)) {
						const gchar *property_name;
						GString *str;
						guint j;

						old_values = get_property_values (data, prop, error);
						if (!old_values) {
							g_ptr_array_unref (fts_props);
							g_ptr_array_unref (fts_text);
							return NULL;
						}

						property_name = tracker_property_get_name (prop);
						str = g_string_new (NULL);

						/* delete old fts entries */
						for (j = 0; j < old_values->len; j++) {
							GValue *value = &g_array_index (old_values, GValue, j);
							if (j != 0)
								g_string_append_c (str, ',');
							g_string_append (str, g_value_get_string (value));
						}

						g_ptr_array_add (fts_props, (gpointer) property_name);
						g_ptr_array_add (fts_text, g_string_free (str, FALSE));
					}
				}

				g_ptr_array_add (fts_props, NULL);
				g_ptr_array_add (fts_text, NULL);

				tracker_db_interface_sqlite_fts_delete_text (iface,
				                                             database,
				                                             data->resource_buffer->id,
				                                             (const gchar **) fts_props->pdata,
				                                             (const gchar **) fts_text->pdata);

				g_ptr_array_unref (fts_props);
				g_ptr_array_unref (fts_text);

				old_values = g_hash_table_lookup (data->resource_buffer->predicates, property);
			} else {
				old_values = get_property_values (data, property, error);
			}

		} else {
			old_values = get_property_values (data, property, error);
		}
	}

	return old_values;
}

static TrackerRowid
get_bnode_id (GHashTable       *bnodes,
              TrackerData      *data,
              const gchar      *str,
              GError          **error)
{
	TrackerRowid *value, bnode_id;

	value = g_hash_table_lookup (bnodes, str);
	if (value)
		return *value;

	bnode_id = tracker_data_generate_bnode (data, error);
	if (bnode_id == 0)
		return 0;

	g_hash_table_insert (bnodes, g_strdup (str),
	                     tracker_rowid_copy (&bnode_id));

	return bnode_id;
}

static TrackerRowid
get_bnode_for_resource (GHashTable       *bnodes,
                        TrackerData      *data,
                        TrackerResource  *resource,
                        GError          **error)
{
	const gchar *identifier;

	identifier = tracker_resource_get_identifier (resource);

	return get_bnode_id (bnodes, data, identifier, error);
}

static gboolean
resource_in_domain_index_class (TrackerData  *data,
                                TrackerClass *domain_index_class)
{
	guint i;
	for (i = 0; i < data->resource_buffer->types->len; i++) {
		if (g_ptr_array_index (data->resource_buffer->types, i) == domain_index_class) {
			return TRUE;
		}
	}
	return FALSE;
}

static void
process_domain_indexes (TrackerData     *data,
                        TrackerProperty *property,
                        const GValue    *gvalue,
                        const gchar     *field_name)
{
	TrackerClass **domain_index_classes;

	domain_index_classes = tracker_property_get_domain_indexes (property);
	while (*domain_index_classes) {
		if (resource_in_domain_index_class (data, *domain_index_classes)) {
			cache_insert_value (data,
			                    tracker_class_get_name (*domain_index_classes),
			                    field_name,
			                    gvalue,
			                    FALSE);
		}
		domain_index_classes++;
	}
}

static gboolean
cache_insert_metadata_decomposed (TrackerData      *data,
                                  TrackerProperty  *property,
                                  const GValue     *object,
                                  GError          **error)
{
	gboolean            multiple_values;
	const gchar        *table_name;
	const gchar        *field_name;
	TrackerProperty   **super_properties;
	GArray             *old_values;
	GError             *new_error = NULL;
	gboolean            change = FALSE;

	/* read existing property values */
	old_values = get_old_property_values (data, property, &new_error);
	if (new_error) {
		g_propagate_error (error, new_error);
		return FALSE;
	}

	/* also insert super property values */
	super_properties = tracker_property_get_super_properties (property);
	multiple_values = tracker_property_get_multiple_values (property);

	data->resource_buffer->fts_updated |=
		tracker_property_get_fulltext_indexed (property);

	while (*super_properties) {
		gboolean super_is_multi;
		GArray *super_old_values;

		super_is_multi = tracker_property_get_multiple_values (*super_properties);
		super_old_values = get_old_property_values (data, *super_properties, &new_error);
		if (new_error) {
			g_propagate_error (error, new_error);
			return FALSE;
		}

		data->resource_buffer->fts_updated |=
			tracker_property_get_fulltext_indexed (*super_properties);

		if (super_is_multi || super_old_values->len == 0) {
			change |= cache_insert_metadata_decomposed (data, *super_properties, object,
			                                            &new_error);
			if (new_error) {
				g_propagate_error (error, new_error);
				return FALSE;
			}
		}
		super_properties++;
	}

	table_name = tracker_property_get_table_name (property);
	field_name = tracker_property_get_name (property);

	if (!value_set_add_value (old_values, object)) {
		/* value already inserted */
	} else if (!multiple_values && old_values->len > 1) {
		/* trying to add second value to single valued property */
		TrackerDBInterface *iface;
		gchar *resource;
		GValue old_value = { 0 };
		GValue new_value = { 0 };
		GValue *v;
		gchar *old_value_str = NULL;
		gchar *new_value_str = NULL;

		g_value_init (&old_value, G_TYPE_STRING);
		g_value_init (&new_value, G_TYPE_STRING);

		/* Get both old and new values as strings letting glib do
		 * whatever transformation needed */
		v = &g_array_index (old_values, GValue, 0);
		if (g_value_transform (v, &old_value)) {
			old_value_str = tracker_utf8_truncate (g_value_get_string (&old_value), 255);
		}

		v = &g_array_index (old_values, GValue, 1);
		if (g_value_transform (v, &new_value)) {
			new_value_str = tracker_utf8_truncate (g_value_get_string (&new_value), 255);
		}

		iface = tracker_data_manager_get_writable_db_interface (data->manager);
		resource = tracker_data_query_resource_urn (data->manager,
		                                            iface,
		                                            data->resource_buffer->id);

		g_set_error (error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_CONSTRAINT,
		             "Unable to insert multiple values on single valued property `%s' for %s %s "
		             "(old_value: '%s', new value: '%s')",
		             field_name,
		             resource ? "resource" : "blank node",
		             resource ? resource : "",
		             old_value_str ? old_value_str : "<untransformable>",
		             new_value_str ? new_value_str : "<untransformable>");

		g_free (resource);
		g_free (old_value_str);
		g_free (new_value_str);
		g_value_unset (&old_value);
		g_value_unset (&new_value);
	} else {
		cache_insert_value (data, table_name, field_name,
		                    object,
		                    multiple_values);

		if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE)
			tracker_data_resource_ref (data, g_value_get_int64 (object), multiple_values);

		if (!multiple_values) {
			process_domain_indexes (data, property, object, field_name);
		}

		change = TRUE;
	}

	return change;
}

static gboolean
delete_metadata_decomposed (TrackerData      *data,
                            TrackerProperty  *property,
                            const GValue     *object,
                            GError          **error)
{
	gboolean            multiple_values;
	const gchar        *table_name;
	const gchar        *field_name;
	TrackerProperty   **super_properties;
	GArray             *old_values;
	GError             *new_error = NULL;
	gboolean            change = FALSE;

	multiple_values = tracker_property_get_multiple_values (property);
	table_name = tracker_property_get_table_name (property);
	field_name = tracker_property_get_name (property);

	/* read existing property values */
	old_values = get_old_property_values (data, property, &new_error);
	if (new_error) {
		/* no need to error out if statement does not exist for any reason */
		g_clear_error (&new_error);
		return FALSE;
	}

	if (!value_set_remove_value (old_values, object)) {
		/* value not found */
	} else {
		cache_delete_value (data, table_name, field_name,
		                    object, multiple_values);
		if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE)
			tracker_data_resource_unref (data, g_value_get_int64 (object), multiple_values);

		if (!multiple_values) {
			TrackerClass **domain_index_classes;

			domain_index_classes = tracker_property_get_domain_indexes (property);

			while (*domain_index_classes) {
				if (resource_in_domain_index_class (data, *domain_index_classes)) {
					cache_delete_value (data,
					                    tracker_class_get_name (*domain_index_classes),
					                    field_name,
					                    object, multiple_values);
				}
				domain_index_classes++;
			}
		}

		change = TRUE;
	}

	/* also delete super property values */
	super_properties = tracker_property_get_super_properties (property);
	while (*super_properties) {
		change |= delete_metadata_decomposed (data, *super_properties, object, error);
		super_properties++;
	}

	return change;
}

static gboolean
cache_delete_resource_type_full (TrackerData   *data,
                                 TrackerClass  *class,
                                 gboolean       single_type,
                                 GError       **error)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBCursor    *cursor = NULL;
	TrackerProperty   **properties, *prop;
	gboolean            found;
	guint               i, p, n_props;
	GError             *inner_error = NULL;
	TrackerOntologies  *ontologies;
	const gchar        *database;
	GValue gvalue = G_VALUE_INIT;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);
	ontologies = tracker_data_manager_get_ontologies (data->manager);
	database = data->resource_buffer->graph->graph ?
		data->resource_buffer->graph->graph : "main";

	if (!single_type) {
		if (strcmp (tracker_class_get_uri (class), TRACKER_PREFIX_RDFS "Resource") == 0 &&
		    g_hash_table_size (data->resource_buffer->tables) == 0) {

			/* skip subclass query when deleting whole resource
			   to improve performance */

			while (data->resource_buffer->types->len > 0) {
				TrackerClass *type;

				type = g_ptr_array_index (data->resource_buffer->types,
				                          data->resource_buffer->types->len - 1);
				if (!cache_delete_resource_type_full (data, type, TRUE, error))
					return FALSE;
			}

			return TRUE;
		}

		found = FALSE;
		for (i = 0; i < data->resource_buffer->types->len; i++) {
			if (g_ptr_array_index (data->resource_buffer->types, i) == class) {
				found = TRUE;
				break;
			}
		}

		if (!found) {
			/* type not found, nothing to do */
			return TRUE;
		}

		/* retrieve all subclasses we need to remove from the subject
		 * before we can remove the class specified as object of the statement */
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_SELECT, &inner_error,
		                                               "SELECT (SELECT Uri FROM Resource WHERE ID = subclass.ID) "
		                                               "FROM \"%s\".\"rdfs:Resource_rdf:type\" AS type INNER JOIN \"%s\".\"rdfs:Class_rdfs:subClassOf\" AS subclass ON (type.\"rdf:type\" = subclass.ID) "
		                                               "WHERE type.ID = ? AND subclass.\"rdfs:subClassOf\" = (SELECT ID FROM Resource WHERE Uri = ?)",
		                                               database, database);

		if (stmt) {
			tracker_db_statement_bind_int (stmt, 0, data->resource_buffer->id);
			tracker_db_statement_bind_text (stmt, 1, tracker_class_get_uri (class));
			cursor = tracker_db_statement_start_cursor (stmt, &inner_error);
			g_object_unref (stmt);
		}

		if (cursor) {
			while (tracker_db_cursor_iter_next (cursor, NULL, &inner_error)) {
				const gchar *class_uri;

				class_uri = tracker_db_cursor_get_string (cursor, 0, NULL);
				if (!cache_delete_resource_type_full (data, tracker_ontologies_get_class_by_uri (ontologies, class_uri),
				                                      FALSE, error))
					return FALSE;
			}

			g_object_unref (cursor);
		}

		if (inner_error) {
			g_propagate_prefixed_error (error,
			                            inner_error,
			                            "Deleting resource:");
			return FALSE;
		}
	}

	/* delete all property values */

	properties = tracker_ontologies_get_properties (ontologies, &n_props);

	for (p = 0; p < n_props; p++) {
		gboolean            multiple_values;
		const gchar        *table_name;
		const gchar        *field_name;
		GArray *old_values;
		gint                y;

		prop = properties[p];

		if (prop == tracker_ontologies_get_rdf_type (ontologies))
			continue;
		if (tracker_property_get_domain (prop) != class)
			continue;

		multiple_values = tracker_property_get_multiple_values (prop);
		table_name = tracker_property_get_table_name (prop);
		field_name = tracker_property_get_name (prop);

		old_values = get_old_property_values (data, prop, error);
		if (!old_values)
			return FALSE;

		for (y = old_values->len - 1; y >= 0 ; y--) {
			GValue *old_gvalue, copy = G_VALUE_INIT;

			old_gvalue = &g_array_index (old_values, GValue, y);
			g_value_init (&copy, G_VALUE_TYPE (old_gvalue));
			g_value_copy (old_gvalue, &copy);

			value_set_remove_value (old_values, old_gvalue);
			cache_delete_value (data, table_name, field_name,
			                    &copy, multiple_values);
			if (tracker_property_get_data_type (prop) == TRACKER_PROPERTY_TYPE_RESOURCE)
				tracker_data_resource_unref (data, g_value_get_int64 (&copy), multiple_values);

			if (!multiple_values) {
				TrackerClass **domain_index_classes;

				domain_index_classes = tracker_property_get_domain_indexes (prop);
				while (*domain_index_classes) {
					if (resource_in_domain_index_class (data, *domain_index_classes)) {
						cache_delete_value (data,
						                    tracker_class_get_name (*domain_index_classes),
						                    field_name,
						                    &copy, multiple_values);
					}
					domain_index_classes++;
				}
			}

			g_value_unset (&copy);
		}
	}

	g_value_init (&gvalue, G_TYPE_INT64);
	g_value_set_int64 (&gvalue, tracker_class_get_id (class));
	cache_delete_value (data, "rdfs:Resource_rdf:type", "rdf:type",
	                    &gvalue, TRUE);
	tracker_data_resource_unref (data, tracker_class_get_id (class), TRUE);

	cache_delete_row (data, class);
	tracker_data_resource_unref (data, data->resource_buffer->id, FALSE);

	tracker_data_dispatch_delete_statement_callbacks (data,
	                                                  tracker_property_get_id (tracker_ontologies_get_rdf_type (ontologies)),
	                                                  tracker_class_get_id (class));

	g_ptr_array_remove (data->resource_buffer->types, class);

	return TRUE;
}

static gboolean
cache_delete_resource_type (TrackerData   *data,
                            TrackerClass  *class,
                            GError       **error)
{
	return cache_delete_resource_type_full (data, class, FALSE, error);
}

static TrackerDataUpdateBufferGraph *
ensure_graph_buffer (TrackerDataUpdateBuffer  *buffer,
                     TrackerData              *data,
                     const gchar              *name,
                     GError                  **error)
{
	TrackerDataUpdateBufferGraph *graph_buffer;
	guint i;

	for (i = 0; i < buffer->graphs->len; i++) {
		graph_buffer = g_ptr_array_index (buffer->graphs, i);
		if (g_strcmp0 (graph_buffer->graph, name) == 0)
			return graph_buffer;
	}

	if (name && !tracker_data_manager_find_graph (data->manager, name, TRUE)) {
		if (!tracker_data_manager_create_graph (data->manager, name, error))
			return NULL;
	}

	graph_buffer = g_slice_new0 (TrackerDataUpdateBufferGraph);
	graph_buffer->refcounts =
		g_hash_table_new_full (tracker_rowid_hash, tracker_rowid_equal,
		                       (GDestroyNotify) tracker_rowid_free, NULL);
	graph_buffer->graph = g_strdup (name);
	if (graph_buffer->graph) {
		graph_buffer->id = tracker_data_manager_find_graph (data->manager,
		                                                    graph_buffer->graph,
		                                                    TRUE);
	}

	graph_buffer->resources =
		g_hash_table_new_full (tracker_rowid_hash, tracker_rowid_equal, NULL,
		                       (GDestroyNotify) resource_buffer_free);
	g_ptr_array_add (buffer->graphs, graph_buffer);

	return graph_buffer;
}

static gboolean
resource_buffer_switch (TrackerData   *data,
                        const gchar   *graph,
                        TrackerRowid   subject,
                        GError       **error)
{
	TrackerDataUpdateBufferGraph *graph_buffer;
	GError *inner_error = NULL;

	if (data->resource_buffer != NULL &&
	    g_strcmp0 (data->resource_buffer->graph->graph, graph) == 0 &&
	    data->resource_buffer->id == subject) {
		/* Resource buffer stays the same */
		return TRUE;
	}

	/* large INSERTs with thousands of resources could lead to
	   high peak memory usage due to the update buffer
	   flush the buffer if it already contains 1000 resources */
	tracker_data_update_buffer_might_flush (data, &inner_error);
	if (inner_error)
		return FALSE;

	data->resource_buffer = NULL;

	graph_buffer = ensure_graph_buffer (&data->update_buffer, data, graph, error);
	if (!graph_buffer)
		return FALSE;

	data->resource_buffer =
		g_hash_table_lookup (graph_buffer->resources, &subject);

	if (data->resource_buffer == NULL) {
		TrackerDataUpdateBufferResource *resource_buffer;
		gboolean create;
		GPtrArray *rdf_types;

		create = g_hash_table_contains (data->update_buffer.new_resources,
		                                &subject);
		if (!create) {
			rdf_types = tracker_data_query_rdf_type (data->manager,
			                                         graph,
			                                         subject,
			                                         &inner_error);
			if (!rdf_types) {
				g_propagate_error (error, inner_error);
				return FALSE;
			}
		}

		resource_buffer = g_slice_new0 (TrackerDataUpdateBufferResource);
		resource_buffer->id = subject;
		resource_buffer->create = create;

		resource_buffer->fts_updated = FALSE;
		if (resource_buffer->create) {
			resource_buffer->types = g_ptr_array_new ();
		} else {
			resource_buffer->types = rdf_types;
		}
		resource_buffer->predicates = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, (GDestroyNotify) g_array_unref);
		resource_buffer->tables = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) cache_table_free);
		resource_buffer->graph = graph_buffer;

		g_hash_table_insert (graph_buffer->resources, &resource_buffer->id, resource_buffer);

		data->resource_buffer = resource_buffer;
	}

	return TRUE;
}

void
tracker_data_delete_statement (TrackerData      *data,
                               const gchar      *graph,
                               TrackerRowid      subject,
                               TrackerProperty  *predicate,
                               const GValue     *object,
                               GError          **error)
{
	TrackerClass       *class;
	gboolean            change = FALSE;
	TrackerOntologies  *ontologies;

	g_return_if_fail (subject != 0);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);
	g_return_if_fail (data->in_transaction);

	if (!resource_buffer_switch (data, graph, subject, error))
		return;

	ontologies = tracker_data_manager_get_ontologies (data->manager);

	if (predicate == tracker_ontologies_get_rdf_type (ontologies)) {
		const gchar *object_str = NULL;
		TrackerRowid object_id = g_value_get_int64 (object);

		object_str = tracker_ontologies_get_uri_by_id (ontologies, object_id);
		class = tracker_ontologies_get_class_by_uri (ontologies, object_str);
		if (class != NULL) {
			data->has_persistent = TRUE;
			if (!cache_delete_resource_type (data, class, error))
				return;
		} else {
			g_set_error (error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_UNKNOWN_CLASS,
			             "Class '%s' not found in the ontology", object_str);
		}
	} else {
		TrackerRowid pred_id;

		pred_id = tracker_property_get_id (predicate);
		data->has_persistent = TRUE;
		change = delete_metadata_decomposed (data, predicate, object, error);

		if (change) {
			tracker_data_dispatch_delete_statement_callbacks (data,
			                                                  pred_id,
			                                                  0);
		}
	}
}

static gboolean
delete_all_helper (TrackerData      *data,
                   const gchar      *graph,
                   TrackerRowid      subject,
                   TrackerProperty  *subproperty,
                   TrackerProperty  *property,
                   GArray           *old_values,
                   GError          **error)
{
	TrackerProperty **super_properties;
	GArray *super_old_values;
	GValue *value;
	guint i;

	if (subproperty == property) {
		if (tracker_property_get_multiple_values (property)) {
			cache_delete_all_values (data,
			                         tracker_property_get_table_name (property),
			                         tracker_property_get_name (property));
			if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
				if (!tracker_data_resource_unref_all (data, property, error))
					return FALSE;
			}
		} else {
			value = &g_array_index (old_values, GValue, 0);
			cache_delete_value (data,
			                    tracker_property_get_table_name (property),
			                    tracker_property_get_name (property),
			                    value,
			                    FALSE);
			if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE)
				tracker_data_resource_unref (data, g_value_get_int64 (value), FALSE);
		}
	} else {
		super_old_values = get_old_property_values (data, property, error);
		if (!super_old_values)
			return FALSE;

		for (i = 0; i < old_values->len; i++) {
			value = &g_array_index (old_values, GValue, i);

			if (!value_set_remove_value (super_old_values, value))
				continue;

			cache_delete_value (data,
			                    tracker_property_get_table_name (property),
			                    tracker_property_get_name (property),
			                    value,
			                    tracker_property_get_multiple_values (property));
			if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
				tracker_data_resource_unref (data, g_value_get_int64 (value),
				                             tracker_property_get_multiple_values (property));
			}
		}
	}

	/* also delete super property values */
	super_properties = tracker_property_get_super_properties (property);
	while (*super_properties) {
		if (!delete_all_helper (data, graph, subject,
		                        subproperty, *super_properties,
		                        old_values, error))
			return FALSE;

		super_properties++;
	}

	if (subproperty == property) {
		/* Clear the buffered property values */
		g_array_remove_range (old_values, 0, old_values->len);
	}

	return TRUE;
}

static gboolean
tracker_data_delete_all (TrackerData   *data,
                         const gchar   *graph,
                         TrackerRowid   subject,
                         const gchar   *predicate,
                         GError       **error)
{
	TrackerOntologies *ontologies;
	TrackerProperty *property;
	GArray *old_values;
	GError *inner_error = NULL;

	g_return_val_if_fail (subject != 0, FALSE);
	g_return_val_if_fail (predicate != NULL, FALSE);
	g_return_val_if_fail (data->in_transaction, FALSE);

	if (!resource_buffer_switch (data, graph, subject, error))
		return FALSE;

	ontologies = tracker_data_manager_get_ontologies (data->manager);
	property = tracker_ontologies_get_property_by_uri (ontologies,
	                                                   predicate);
	old_values = get_old_property_values (data, property, &inner_error);
	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	} else if (!old_values || old_values->len == 0) {
		return FALSE;
	}

	return delete_all_helper (data, graph, subject,
	                          property, property,
	                          old_values, error);
}

static gboolean
delete_single_valued (TrackerData       *data,
                      const gchar       *graph,
                      TrackerRowid       subject,
                      TrackerProperty   *predicate,
                      gboolean           super_is_single_valued,
                      GError           **error)
{
	TrackerProperty **super_properties;
	gboolean multiple_values;

	super_properties = tracker_property_get_super_properties (predicate);
	multiple_values = tracker_property_get_multiple_values (predicate);

	if (super_is_single_valued && multiple_values) {
		cache_delete_all_values (data,
		                         tracker_property_get_table_name (predicate),
		                         tracker_property_get_name (predicate));
		if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			if (!tracker_data_resource_unref_all (data, predicate, error))
				return FALSE;
		}
	} else if (!multiple_values) {
		GError *inner_error = NULL;
		GArray *old_values;

		old_values = get_old_property_values (data, predicate, &inner_error);

		if (old_values && old_values->len == 1) {
			GValue *value;

			value = &g_array_index (old_values, GValue, 0);
			cache_delete_value (data,
			                    tracker_property_get_table_name (predicate),
			                    tracker_property_get_name (predicate),
			                    value,
			                    FALSE);
			if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE)
				tracker_data_resource_unref (data, g_value_get_int64 (value), multiple_values);
		} else {
			/* no need to error out if statement does not exist for any reason */
			g_clear_error (&inner_error);
		}
	}

	while (*super_properties) {
		if (!delete_single_valued (data, graph, subject,
		                           *super_properties,
		                           super_is_single_valued,
		                           error))
			return FALSE;

		super_properties++;
	}

	return TRUE;
}

void
tracker_data_insert_statement (TrackerData      *data,
                               const gchar      *graph,
                               TrackerRowid      subject,
                               TrackerProperty  *predicate,
                               const GValue     *object,
                               GError          **error)
{
	g_return_if_fail (subject != 0);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);
	g_return_if_fail (data->in_transaction);

	if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE) {
		tracker_data_insert_statement_with_uri (data, graph, subject, predicate, object, error);
	} else {
		tracker_data_insert_statement_with_string (data, graph, subject, predicate, object, error);
	}
}

void
tracker_data_insert_statement_with_uri (TrackerData      *data,
                                        const gchar      *graph,
                                        TrackerRowid      subject,
                                        TrackerProperty  *predicate,
                                        const GValue     *object,
                                        GError          **error)
{
	GError          *actual_error = NULL;
	TrackerClass    *class;
	TrackerRowid prop_id = 0;
	gboolean change = FALSE;
	TrackerOntologies *ontologies;

	g_return_if_fail (subject != 0);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);
	g_return_if_fail (data->in_transaction);

	g_assert (tracker_property_get_data_type (predicate) ==
	          TRACKER_PROPERTY_TYPE_RESOURCE);

	ontologies = tracker_data_manager_get_ontologies (data->manager);
	prop_id = tracker_property_get_id (predicate);

	data->has_persistent = TRUE;

	if (!resource_buffer_switch (data, graph, subject, error))
		return;

	if (predicate == tracker_ontologies_get_rdf_type (ontologies)) {
		const gchar *object_str = NULL;
		TrackerRowid object_id;

		object_id = g_value_get_int64 (object);
		object_str = tracker_ontologies_get_uri_by_id (ontologies, object_id);

		/* handle rdf:type statements specially to
		   cope with inference and insert blank rows */
		class = tracker_ontologies_get_class_by_uri (ontologies, object_str);
		if (class != NULL) {
			if (!cache_create_service_decomposed (data, class, error))
				return;
		} else {
			g_set_error (error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_UNKNOWN_CLASS,
			             "Class '%s' not found in the ontology", object_str);
			return;
		}
	} else {
		/* add value to metadata database */
		change = cache_insert_metadata_decomposed (data, predicate, object, &actual_error);

		if (actual_error) {
			g_propagate_error (error, actual_error);
			return;
		}

		if (change) {
			tracker_data_dispatch_insert_statement_callbacks (data,
			                                                  prop_id,
			                                                  0);
		}
	}
}

void
tracker_data_insert_statement_with_string (TrackerData      *data,
                                           const gchar      *graph,
                                           TrackerRowid      subject,
                                           TrackerProperty  *predicate,
                                           const GValue     *object,
                                           GError          **error)
{
	GError          *actual_error = NULL;
	gboolean         change;
	TrackerRowid     pred_id = 0;

	g_return_if_fail (subject != 0);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);
	g_return_if_fail (data->in_transaction);

	g_assert (tracker_property_get_data_type (predicate) !=
	          TRACKER_PROPERTY_TYPE_RESOURCE);

	pred_id = tracker_property_get_id (predicate);
	data->has_persistent = TRUE;

	if (!resource_buffer_switch (data, graph, subject, error))
		return;

	/* add value to metadata database */
	change = cache_insert_metadata_decomposed (data, predicate, object, &actual_error);

	if (actual_error) {
		g_propagate_error (error, actual_error);
		return;
	}

	if (change) {
		tracker_data_dispatch_insert_statement_callbacks (data,
		                                                  pred_id,
		                                                  0 /* Always a literal */);
	}
}

void
tracker_data_update_statement (TrackerData      *data,
                               const gchar      *graph,
                               TrackerRowid      subject,
                               TrackerProperty  *predicate,
                               const GValue     *object,
                               GError          **error)
{
	TrackerOntologies *ontologies;
	GError *new_error = NULL;

	g_return_if_fail (subject != 0);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (data->in_transaction);

	ontologies = tracker_data_manager_get_ontologies (data->manager);

	if (object == NULL || !G_VALUE_TYPE (object)) {
		if (predicate == tracker_ontologies_get_rdf_type (ontologies)) {
			g_set_error (error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_UNSUPPORTED,
			             "Using 'null' with '%s' is not supported",
			             tracker_property_get_uri (predicate));
			return;
		}

		/* Flush upfront to make a null,x,null,y,z work: When x is set then
		 * if a null comes, we need to be flushed */

		tracker_data_update_buffer_flush (data, &new_error);
		if (new_error) {
			g_propagate_error (error, new_error);
			return;
		}

		if (!resource_buffer_switch (data, graph, subject, error))
			return;

		cache_delete_all_values (data,
		                         tracker_property_get_table_name (predicate),
		                         tracker_property_get_name (predicate));
		if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			if (!tracker_data_resource_unref_all (data, predicate, error))
				return;
		}
	} else {
		if (!resource_buffer_switch (data, graph, subject, error))
			return;

		if (!delete_single_valued (data, graph, subject, predicate,
		                           !tracker_property_get_multiple_values (predicate),
		                           error))
			return;

		tracker_data_update_buffer_flush (data, &new_error);
		if (new_error) {
			g_propagate_error (error, new_error);
			return;
		}

		if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			tracker_data_insert_statement_with_uri (data, graph, subject, predicate, object, error);
		} else {
			tracker_data_insert_statement_with_string (data, graph, subject, predicate, object, error);
		}
	}

	tracker_data_update_buffer_flush (data, &new_error);
	if (new_error) {
		g_propagate_error (error, new_error);
	}
}

void
tracker_data_begin_transaction (TrackerData  *data,
                                GError      **error)
{
	TrackerDBInterface *iface;
	TrackerDBManager *db_manager;

	g_return_if_fail (!data->in_transaction);

	db_manager = tracker_data_manager_get_db_manager (data->manager);

	if (!tracker_db_manager_has_enough_space (db_manager)) {
		g_set_error (error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_NO_SPACE,
			"There is not enough space on the file system for update operations");
		return;
	}

	if (!data->in_ontology_transaction &&
	    !tracker_data_update_initialize_modseq (data, error))
		return;

	data->resource_time = time (NULL);

	data->has_persistent = FALSE;

	if (data->update_buffer.resource_cache == NULL) {
		data->update_buffer.resource_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
		                                                            (GDestroyNotify) tracker_rowid_free);
		data->update_buffer.new_resources = g_hash_table_new_full (tracker_rowid_hash, tracker_rowid_equal,
		                                                           (GDestroyNotify) tracker_rowid_free, NULL);
		/* used for normal transactions */
		data->update_buffer.graphs = g_ptr_array_new_with_free_func ((GDestroyNotify) graph_buffer_free);
	}

	data->resource_buffer = NULL;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA cache_size = %d", TRACKER_DB_CACHE_SIZE_UPDATE);

	tracker_db_interface_start_transaction (iface);

	data->in_transaction = TRUE;
}

void
tracker_data_begin_ontology_transaction (TrackerData  *data,
                                         GError      **error)
{
	data->in_ontology_transaction = TRUE;
	tracker_data_begin_transaction (data, error);
}

void
tracker_data_commit_transaction (TrackerData  *data,
                                 GError      **error)
{
	TrackerDBInterface *iface;
	GError *actual_error = NULL;

	g_return_if_fail (data->in_transaction);

	iface = tracker_data_manager_get_writable_db_interface (data->manager);

	tracker_data_update_buffer_flush (data, &actual_error);
	if (actual_error) {
		tracker_data_rollback_transaction (data);
		g_propagate_error (error, actual_error);
		return;
	}

	tracker_db_interface_end_db_transaction (iface,
	                                         &actual_error);

	if (actual_error) {
		tracker_data_rollback_transaction (data);
		g_propagate_error (error, actual_error);
		return;
	}

	if (data->has_persistent && !data->in_ontology_transaction) {
		data->transaction_modseq++;
	}

	data->resource_time = 0;
	data->in_transaction = FALSE;
	data->in_ontology_transaction = FALSE;

	tracker_data_manager_commit_graphs (data->manager);

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA cache_size = %d", TRACKER_DB_CACHE_SIZE_DEFAULT);

	g_ptr_array_set_size (data->update_buffer.graphs, 0);
	g_hash_table_remove_all (data->update_buffer.resource_cache);

	tracker_data_dispatch_commit_statement_callbacks (data);
}

void
tracker_data_rollback_transaction (TrackerData *data)
{
	TrackerDBInterface *iface;
	GError *ignorable = NULL;

	g_return_if_fail (data->in_transaction);

	data->in_transaction = FALSE;
	data->in_ontology_transaction = FALSE;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);

	tracker_data_update_buffer_clear (data);

	tracker_db_interface_execute_query (iface, &ignorable, "ROLLBACK");

	if (ignorable) {
		g_warning ("Transaction rollback failed: %s\n", ignorable->message);
		g_clear_error (&ignorable);
	}

	tracker_data_manager_rollback_graphs (data->manager);

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA cache_size = %d", TRACKER_DB_CACHE_SIZE_DEFAULT);

	tracker_data_dispatch_rollback_statement_callbacks (data);
}

static GVariant *
update_sparql (TrackerData  *data,
               const gchar  *update,
               gboolean      blank,
               GError      **error)
{
	GError *actual_error = NULL;
	TrackerSparql *sparql_query;
	GVariant *blank_nodes;

	g_return_val_if_fail (update != NULL, NULL);

#ifdef G_ENABLE_DEBUG
	if (TRACKER_DEBUG_CHECK (SPARQL)) {
		gchar *update_to_print;

		update_to_print = g_strdup (update);
		g_strdelimit (update_to_print, "\n", ' ');
		g_message ("[SPARQL] %s", update_to_print);
		g_free (update_to_print);
	}
#endif

	tracker_data_begin_transaction (data, &actual_error);
	if (actual_error) {
		g_propagate_error (error, actual_error);
		return NULL;
	}

	sparql_query = tracker_sparql_new_update (data->manager, update);
	blank_nodes = tracker_sparql_execute_update (sparql_query, blank, NULL, &actual_error);
	g_object_unref (sparql_query);

	if (actual_error) {
		tracker_data_rollback_transaction (data);
		g_propagate_error (error, actual_error);
		return NULL;
	}

	tracker_data_commit_transaction (data, &actual_error);
	if (actual_error) {
		g_propagate_error (error, actual_error);
		return NULL;
	}

	return blank_nodes;
}

void
tracker_data_update_sparql (TrackerData  *data,
                            const gchar  *update,
                            GError      **error)
{
	update_sparql (data, update, FALSE, error);
}

GVariant *
tracker_data_update_sparql_blank (TrackerData  *data,
                                  const gchar  *update,
                                  GError      **error)
{
	return update_sparql (data, update, TRUE, error);
}

void
tracker_data_load_turtle_file (TrackerData  *data,
                               GFile        *file,
                               const gchar  *graph,
                               GError      **error)
{
	TrackerTurtleReader *reader = NULL;
	TrackerOntologies *ontologies;
	GError *inner_error = NULL;
	const gchar *subject_str, *predicate_str, *object_str, *langtag;
	gboolean object_is_uri;
	goffset last_parsed_line_no, last_parsed_column_no;
	gchar *ontology_uri;

	reader = tracker_turtle_reader_new_for_file (file, error);
	if (!reader)
		return;

	ontologies = tracker_data_manager_get_ontologies (data->manager);

	while (tracker_turtle_reader_next (reader,
	                                   &subject_str,
	                                   &predicate_str,
	                                   &object_str,
	                                   &langtag,
	                                   &object_is_uri,
	                                   &last_parsed_line_no,
	                                   &last_parsed_column_no,
	                                   &inner_error)) {
		TrackerProperty *predicate;
		GValue object = G_VALUE_INIT;
		TrackerRowid subject;

		predicate = tracker_ontologies_get_property_by_uri (ontologies, predicate_str);
		if (predicate == NULL) {
			g_set_error (&inner_error, TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_UNKNOWN_PROPERTY,
			             "Property '%s' not found in the ontology",
			             predicate_str);
			goto failed;
		}

		/* Skip nrl:added/nrl:modified when parsing */
		if (g_strcmp0 (tracker_property_get_name (predicate),
		               "nrl:modified") == 0 ||
		    g_strcmp0 (tracker_property_get_name (predicate),
		               "nrl:added") == 0)
			continue;

		if (!tracker_data_query_string_to_value (data->manager,
		                                         object_str,
		                                         langtag,
		                                         tracker_property_get_data_type (predicate),
		                                         &object,
		                                         &inner_error))
			goto failed;

		subject = tracker_data_update_ensure_resource (data,
		                                               subject_str,
		                                               &inner_error);
		if (inner_error)
			goto failed;

		if (object_is_uri) {
			tracker_data_insert_statement_with_uri (data, graph,
			                                        subject, predicate, &object,
			                                        &inner_error);
		} else {
			tracker_data_insert_statement_with_string (data, graph,
			                                           subject, predicate, &object,
			                                           &inner_error);
		}

		g_value_unset (&object);

		if (inner_error)
			goto failed;

		tracker_data_update_buffer_might_flush (data, &inner_error);

		if (inner_error)
			goto failed;
	}

	g_clear_object (&reader);

	return;

failed:
	g_clear_object (&reader);

	ontology_uri = g_file_get_uri (file);
	g_propagate_prefixed_error (error, inner_error,
	                            "%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT ": ",
	                            ontology_uri, last_parsed_line_no, last_parsed_column_no);
	g_free (ontology_uri);
}

TrackerRowid
tracker_data_ensure_graph (TrackerData  *data,
                           const gchar  *uri,
                           GError      **error)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerRowid id;

	id = tracker_data_update_ensure_resource (data, uri, error);
	if (id == 0)
		return 0;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);
	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE, error,
	                                              "INSERT OR IGNORE INTO Graph (ID) VALUES (?)");
	if (!stmt)
		return 0;

	tracker_db_statement_bind_int (stmt, 0, id);
	tracker_db_statement_execute (stmt, error);
	g_object_unref (stmt);

	return id;
}

gboolean
tracker_data_delete_graph (TrackerData  *data,
                           const gchar  *uri,
                           GError      **error)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerRowid id;

	id = query_resource_id (data, uri, error);
	if (id == 0)
		return FALSE;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);
	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE, error,
	                                              "DELETE FROM Graph WHERE ID = ?");
	if (!stmt)
		return FALSE;

	tracker_db_statement_bind_int (stmt, 0, id);
	tracker_db_statement_execute (stmt, error);
	g_object_unref (stmt);

	return TRUE;
}

static gboolean
resource_maybe_reset_property (TrackerData      *data,
                               const gchar      *graph,
                               TrackerResource  *resource,
                               TrackerRowid      subject,
                               const gchar      *property_uri,
                               GHashTable       *bnodes,
                               GError          **error)
{
	GError *inner_error = NULL;
	const gchar *subject_name;

	/* If the subject is a blank node, this is a whole new insertion.
	 * We don't need deleting anything then.
	 */
	subject_name = tracker_resource_get_identifier (resource);
	if (g_str_has_prefix (subject_name, "_:"))
		return TRUE;

	if (!tracker_data_delete_all (data,
	                              graph, subject, property_uri,
	                              &inner_error)) {
		if (inner_error)
			g_propagate_error (error, inner_error);

		return FALSE;
	}

	return TRUE;
}

static gboolean
update_resource_property (TrackerData      *data,
                          const gchar      *graph_uri,
                          TrackerResource  *resource,
                          TrackerRowid      subject,
                          const gchar      *property,
                          GHashTable       *visited,
                          GHashTable       *bnodes,
                          GError          **error)
{
	TrackerOntologies *ontologies;
	TrackerProperty *predicate;
	GList *values, *v;
	gchar *property_uri;
	GError *inner_error = NULL;

	values = tracker_resource_get_values (resource, property);
	tracker_data_manager_expand_prefix (data->manager,
	                                    property,
	                                    NULL, NULL,
	                                    &property_uri);

	ontologies = tracker_data_manager_get_ontologies (data->manager);
	predicate = tracker_ontologies_get_property_by_uri (ontologies, property_uri);
	if (predicate == NULL) {
		g_set_error (error, TRACKER_SPARQL_ERROR,
		             TRACKER_SPARQL_ERROR_UNKNOWN_PROPERTY,
		             "Property '%s' not found in the ontology",
		             property_uri);
		return FALSE;
	}

	for (v = values; v && !inner_error; v = v->next) {
		GValue *value, free_me = G_VALUE_INIT;
		TrackerRowid id;

		if (G_VALUE_HOLDS (v->data, TRACKER_TYPE_RESOURCE)) {

			if (!update_resource_single (data,
			                             graph_uri,
			                             g_value_get_object (v->data),
			                             visited,
			                             bnodes,
			                             &id,
			                             &inner_error))
				break;

			g_value_init (&free_me, G_TYPE_INT64);
			g_value_set_int64 (&free_me, id);
			value = &free_me;
		} else if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE &&
		           g_type_is_a (G_VALUE_TYPE (v->data), G_TYPE_STRING)) {
			gchar *object_str;

			tracker_data_manager_expand_prefix (data->manager,
			                                    g_value_get_string (v->data),
			                                    NULL, NULL,
			                                    &object_str);

			if (g_str_has_prefix (object_str, "_:")) {
				id = get_bnode_id (bnodes, data, object_str, error);
				if (inner_error)
					break;

				g_value_init (&free_me, G_TYPE_INT64);
				g_value_set_int64 (&free_me, id);
			} else if (!tracker_data_query_string_to_value (data->manager,
			                                                object_str,
			                                                NULL,
			                                                tracker_property_get_data_type (predicate),
			                                                &free_me,
			                                                &inner_error)) {
				break;
			}

			g_free (object_str);
			value = &free_me;
		} else {
			value = v->data;
		}

		tracker_data_insert_statement (data,
		                               graph_uri,
		                               subject,
		                               predicate,
		                               value,
		                               &inner_error);
		g_value_unset (&free_me);
	}

	g_list_free (values);
	g_free (property_uri);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}

static gboolean
update_resource_single (TrackerData      *data,
                        const gchar      *graph,
                        TrackerResource  *resource,
                        GHashTable       *visited,
                        GHashTable       *bnodes,
                        TrackerRowid     *id,
                        GError          **error)
{
	GList *properties = NULL, *l;
	GError *inner_error = NULL;
	const gchar *subject_str;
	TrackerRowid subject;
	gchar *graph_uri = NULL;
	gboolean is_bnode = FALSE;

	subject_str = tracker_resource_get_identifier (resource);
	if (!subject_str || g_str_has_prefix (subject_str, "_:")) {
		is_bnode = TRUE;
		subject = get_bnode_for_resource (bnodes, data, resource, error);
		if (!subject)
			return FALSE;
	} else {
		subject = tracker_data_update_ensure_resource (data, subject_str, error);
		if (subject == 0)
			return FALSE;
	}

	if (g_hash_table_lookup (visited, resource))
		goto out;

	g_hash_table_add (visited, resource);

	properties = tracker_resource_get_properties (resource);

	if (graph) {
		tracker_data_manager_expand_prefix (data->manager,
		                                    graph, NULL, NULL,
		                                    &graph_uri);
	}

	/* Handle rdf:type first */
	if (g_list_find_custom (properties, "rdf:type", (GCompareFunc) g_strcmp0)) {
		update_resource_property (data, graph_uri, resource,
		                          subject, "rdf:type",
		                          visited, bnodes,
		                          &inner_error);
		if (inner_error)
			goto out;
	}

	if (!is_bnode) {
		gboolean need_flush = FALSE;

		for (l = properties; l; l = l->next) {
			const gchar *property = l->data;

			if (tracker_resource_get_property_overwrite (resource, property)) {
				gchar *property_uri;

				tracker_data_manager_expand_prefix (data->manager,
				                                    property,
				                                    NULL, NULL,
				                                    &property_uri);

				if (resource_maybe_reset_property (data, graph_uri, resource,
				                                   subject, property_uri,
				                                   bnodes, &inner_error)) {
					need_flush = TRUE;
				} else if (inner_error) {
					g_free (property_uri);
					goto out;
				}

				g_free (property_uri);
			}
		}

		if (need_flush) {
			tracker_data_update_buffer_flush (data, &inner_error);
			if (inner_error)
				goto out;
		}
	}

	for (l = properties; l; l = l->next) {
		if (g_str_equal (l->data, "rdf:type"))
			continue;

		if (!update_resource_property (data, graph_uri, resource,
		                               subject, l->data,
		                               visited, bnodes,
		                               &inner_error))
			break;
	}

out:
	g_list_free (properties);
	g_free (graph_uri);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	if (id)
		*id = subject;

	return TRUE;
}

gboolean
tracker_data_update_resource (TrackerData      *data,
                              const gchar      *graph,
                              TrackerResource  *resource,
                              GHashTable       *bnodes,
                              GError          **error)
{
	GHashTable *visited;
	gboolean retval;

	visited = g_hash_table_new (NULL, NULL);

	if (bnodes)
		g_hash_table_ref (bnodes);
	else
		bnodes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) tracker_rowid_free);

	retval = update_resource_single (data, graph, resource, visited, bnodes, NULL, error);

	g_hash_table_unref (visited);
	g_hash_table_unref (bnodes);

	return retval;
}

TrackerRowid
tracker_data_generate_bnode (TrackerData  *data,
                             GError      **error)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt = NULL;
	GError *inner_error = NULL;
	TrackerRowid id;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_UPDATE,
	                                              &inner_error,
	                                              "INSERT INTO Resource (Uri, BlankNode) VALUES (?, ?)");
	if (!stmt) {
		g_propagate_error (error, inner_error);
		return 0;
	}

	tracker_db_statement_bind_null (stmt, 0);
	tracker_db_statement_bind_int (stmt, 1, TRUE);
	tracker_db_statement_execute (stmt, &inner_error);
	g_object_unref (stmt);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return 0;
	}

	id = tracker_db_interface_sqlite_get_last_insert_id (iface);
	g_hash_table_add (data->update_buffer.new_resources,
	                  tracker_rowid_copy (&id));

	return id;
}
