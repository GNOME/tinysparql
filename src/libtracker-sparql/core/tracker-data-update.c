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

#include <libtracker-sparql/tracker-deserializer-rdf.h>
#include <libtracker-sparql/tracker-private.h>
#include <libtracker-sparql/tracker-uri.h>

#include "tracker-class.h"
#include "tracker-data-manager.h"
#include "tracker-data-update.h"
#include "tracker-data-query.h"
#include "tracker-db-interface-sqlite.h"
#include "tracker-db-manager.h"
#include "tracker-ontologies.h"
#include "tracker-property.h"
#include "tracker-sparql.h"
#include "tracker-uuid.h"

typedef struct _TrackerDataUpdateBuffer TrackerDataUpdateBuffer;
typedef struct _TrackerDataUpdateBufferGraph TrackerDataUpdateBufferGraph;
typedef struct _TrackerDataUpdateBufferResource TrackerDataUpdateBufferResource;
typedef struct _TrackerDataBlankBuffer TrackerDataBlankBuffer;
typedef struct _TrackerStatementDelegate TrackerStatementDelegate;
typedef struct _TrackerCommitDelegate TrackerCommitDelegate;

#define UPDATE_LOG_SIZE 64

typedef enum {
	TRACKER_LOG_CLASS_INSERT,
	TRACKER_LOG_CLASS_UPDATE,
	TRACKER_LOG_CLASS_DELETE,
	TRACKER_LOG_MULTIVALUED_PROPERTY_INSERT,
	TRACKER_LOG_MULTIVALUED_PROPERTY_DELETE,
	TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR,
} TrackerDataLogEntryType;

typedef struct {
	gint prev;
	TrackerProperty *property;
	GValue value;
} TrackerDataPropertyEntry;

typedef struct {
	TrackerDataLogEntryType type;
	const TrackerDataUpdateBufferGraph *graph;
	TrackerRowid id;
	union {
		struct {
			TrackerClass *class;
			gint last_property_idx; /* Index in properties_ptr array */
		} class;
		struct {
			TrackerProperty *property;
			gint change_idx; /* Index in properties_ptr array */
		} multivalued;
		GObject *any;
	} table;
	GArray *properties_ptr;
} TrackerDataLogEntry;

struct _TrackerDataUpdateBuffer {
	/* string -> ID */
	GHashTable *resource_cache;
	/* set of IDs, key is same pointer than resource_cache, and owned there */
	GHashTable *new_resources;
	/* TrackerDataUpdateBufferGraph */
	GPtrArray *graphs;
	/* Statement to insert in Resource table */
	TrackerDBStatement *insert_resource;
	TrackerDBStatement *query_resource;

	/* Array of TrackerDataPropertyEntry */
	GArray *properties;
	/* Array of TrackerDataLogEntry */
	GArray *update_log;
	/* Set of TrackerDataLogEntry. Used for class events lookups in order to
	 * coalesce single-valued property changes.
	 */
	GHashTable *class_updates;

	TrackerDBStatementMru stmt_mru;
};

typedef struct {
	TrackerRowid rowid;
	gint refcount_change;
} RefcountEntry;

struct _TrackerDataUpdateBufferGraph {
	gchar *graph;

	/* id -> TrackerDataUpdateBufferResource */
	GHashTable *resources;
	/* id -> integer */
	GArray *refcounts;

	TrackerDBStatement *insert_ref;
	TrackerDBStatement *update_ref;
	TrackerDBStatement *delete_ref;
	TrackerDBStatement *query_rdf_types;
	TrackerDBStatementMru values_mru;
};

struct _TrackerDataUpdateBufferResource {
	TrackerDataUpdateBufferGraph *graph;
	TrackerRowid id;
	gboolean create;
	gboolean modified;
	/* TrackerProperty -> GArray of GValue */
	GHashTable *predicates;
	/* TrackerClass */
	GPtrArray *types;

	GList *fts_properties;
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
	gboolean implicit_create;
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

static GArray      *get_property_values (TrackerData      *data,
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

static void tracker_data_insert_statement_with_uri    (TrackerData      *data,
                                                       const gchar      *graph,
                                                       TrackerRowid      subject,
                                                       TrackerProperty  *predicate,
                                                       const GValue     *object,
                                                       GError          **error);
static void tracker_data_insert_statement_with_string (TrackerData      *data,
                                                       const gchar      *graph,
                                                       TrackerRowid      subject,
                                                       TrackerProperty  *predicate,
                                                       const GValue     *object,
                                                       GError          **error);

static guint
tracker_data_log_entry_hash (gconstpointer value)
{
	const TrackerDataLogEntry *entry = value;

	return (g_direct_hash (entry->graph) ^
		tracker_rowid_hash (&entry->id) ^
		g_direct_hash (entry->table.any));
}

static gboolean
tracker_data_log_entry_equal (gconstpointer value1,
			      gconstpointer value2)
{
	const TrackerDataLogEntry *entry1 = value1, *entry2 = value2;

	return (entry1->graph == entry2->graph &&
	        entry1->id == entry2->id &&
		entry1->table.any == entry2->table.any);
}

static guint
tracker_data_log_entry_schema_hash (gconstpointer value)
{
	const TrackerDataLogEntry *entry = value;
	guint hash = 0;

	hash = (entry->type ^
	        g_direct_hash (entry->graph) ^
	        g_direct_hash (entry->table.any));

	if (entry->type == TRACKER_LOG_CLASS_INSERT ||
	    entry->type == TRACKER_LOG_CLASS_UPDATE) {
		TrackerDataPropertyEntry *prop;
		gint idx;

		/* Unite with hash of properties */
		idx = entry->table.class.last_property_idx;

		while (idx >= 0) {
			prop = &g_array_index (entry->properties_ptr,
			                       TrackerDataPropertyEntry, idx);
			hash ^= g_direct_hash (prop->property);
			idx = prop->prev;
		}
	}

	return hash;
}

static gboolean
tracker_data_log_entry_schema_equal (gconstpointer value1,
                                     gconstpointer value2)
{
	const TrackerDataLogEntry *entry1 = value1, *entry2 = value2;

	if (value1 == value2)
		return TRUE;

	if (entry1->type != entry2->type ||
	    entry1->graph != entry2->graph ||
	    entry1->table.any != entry2->table.any)
		return FALSE;

	if (entry1->type == TRACKER_LOG_CLASS_INSERT ||
	    entry1->type == TRACKER_LOG_CLASS_UPDATE) {
		TrackerDataPropertyEntry *prop1, *prop2;
		gint idx1, idx2;

		/* Compare properties */
		idx1 = entry1->table.class.last_property_idx;
		idx2 = entry2->table.class.last_property_idx;

		while (idx1 >= 0 && idx2 >= 0) {
			prop1 = &g_array_index (entry1->properties_ptr,
			                        TrackerDataPropertyEntry, idx1);
			prop2 = &g_array_index (entry2->properties_ptr,
			                        TrackerDataPropertyEntry, idx2);

			if (prop1->property != prop2->property)
				return FALSE;

			idx1 = prop1->prev;
			idx2 = prop2->prev;
		}

		if (idx1 >= 0 || idx2 >= 0)
			return FALSE;
	}

	return TRUE;
}

static TrackerDataLogEntry *
tracker_data_log_entry_copy (TrackerDataLogEntry *entry)
{
	TrackerDataLogEntry *copy;

	copy = g_slice_copy (sizeof (TrackerDataLogEntry), entry);

	if (entry->type == TRACKER_LOG_CLASS_INSERT ||
	    entry->type == TRACKER_LOG_CLASS_UPDATE) {
		TrackerDataPropertyEntry prop;
		GArray *properties_copy;
		gint idx;

		properties_copy = g_array_new (FALSE, TRUE, sizeof (TrackerDataPropertyEntry));
		idx = entry->table.class.last_property_idx;

		while (idx >= 0) {
			prop = g_array_index (entry->properties_ptr, TrackerDataPropertyEntry, idx);
			idx = prop.prev;
			if (idx >= 0)
				prop.prev = properties_copy->len + 1;
			g_array_append_val (properties_copy, prop);
		}

		copy->properties_ptr = properties_copy;

		if (properties_copy->len > 0)
			copy->table.class.last_property_idx = 0;
	}

	return copy;
}

static void
tracker_data_log_entry_free (TrackerDataLogEntry *entry)
{
	if (entry->type == TRACKER_LOG_CLASS_INSERT ||
	    entry->type == TRACKER_LOG_CLASS_UPDATE)
		g_array_unref (entry->properties_ptr);

	g_slice_free (TrackerDataLogEntry, entry);
}

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
			delegate->callback (data->resource_buffer->graph->graph,
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
			delegate->callback (data->resource_buffer->graph->graph,
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
	TrackerDBInterface *temp_iface;
	TrackerDBStatement *stmt;
	TrackerOntologies *ontologies;
	TrackerProperty *property;
	GArray *res = NULL;
	GError *inner_error = NULL;
	gint max_modseq = 0;

	/* Is it already initialized? */
	if (data->transaction_modseq != 0)
		return TRUE;

	temp_iface = tracker_data_manager_get_writable_db_interface (data->manager);
	ontologies = tracker_data_manager_get_ontologies (data->manager);
	property = tracker_ontologies_get_nrl_modified (ontologies);

	stmt = tracker_db_interface_create_vstatement (temp_iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &inner_error,
	                                               "SELECT MAX(object) FROM tracker_triples "
	                                               "WHERE predicate = %" G_GINT64_FORMAT,
	                                               tracker_property_get_id (property));

	if (stmt) {
		res = tracker_db_statement_get_values (stmt,
		                                       TRACKER_PROPERTY_TYPE_INTEGER,
		                                       &inner_error);
		g_object_unref (stmt);
	}

	if (res) {
		max_modseq = g_value_get_int64 (&g_array_index (res, GValue, 0));
		data->transaction_modseq = max_modseq + 1;
		g_array_unref (res);
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
tracker_data_finalize (GObject *object)
{
	TrackerData *data = TRACKER_DATA (object);

	g_clear_pointer (&data->update_buffer.graphs, g_ptr_array_unref);
	g_clear_pointer (&data->update_buffer.new_resources, g_hash_table_unref);
	g_clear_pointer (&data->update_buffer.resource_cache, g_hash_table_unref);
	g_clear_pointer (&data->update_buffer.properties, g_array_unref);
	g_clear_pointer (&data->update_buffer.update_log, g_array_unref);
	g_clear_pointer (&data->update_buffer.class_updates, g_hash_table_unref);
	g_clear_object (&data->update_buffer.insert_resource);
	g_clear_object (&data->update_buffer.query_resource);
	tracker_db_statement_mru_finish (&data->update_buffer.stmt_mru);

	g_clear_pointer (&data->insert_callbacks, g_ptr_array_unref);
	g_clear_pointer (&data->delete_callbacks, g_ptr_array_unref);
	g_clear_pointer (&data->commit_callbacks, g_ptr_array_unref);
	g_clear_pointer (&data->rollback_callbacks, g_ptr_array_unref);

	G_OBJECT_CLASS (tracker_data_parent_class)->finalize (object);
}

static void
tracker_data_class_init (TrackerDataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = tracker_data_set_property;
	object_class->get_property = tracker_data_get_property;
	object_class->finalize = tracker_data_finalize;

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

static void
log_entry_for_multi_value_property (TrackerData             *data,
                                    TrackerDataLogEntryType  type,
                                    TrackerProperty         *property,
				    const GValue            *value)
{
	TrackerDataLogEntry entry = { 0, };
	TrackerDataPropertyEntry prop = { 0, };
	guint prop_idx;

	prop.property = property;
	prop.prev = -1;

	if (type != TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR) {
		g_value_init (&prop.value, G_VALUE_TYPE (value));
		g_value_copy (value, &prop.value);
	}

	g_array_append_val (data->update_buffer.properties, prop);
	prop_idx = data->update_buffer.properties->len - 1;

	entry.type = type;
	entry.graph = data->resource_buffer->graph;
	entry.id = data->resource_buffer->id;
	entry.table.multivalued.property = property;
	entry.table.multivalued.change_idx = prop_idx;
	entry.properties_ptr = data->update_buffer.properties;
	g_array_append_val (data->update_buffer.update_log, entry);
}

static void
log_entry_for_single_value_property (TrackerData     *data,
                                     TrackerClass    *class,
                                     TrackerProperty *property,
				     const GValue    *value)
{
	TrackerDataLogEntry entry = { 0, }, *entry_ptr;
	TrackerDataPropertyEntry prop = { 0, };
	guint prop_idx;

	entry.type = TRACKER_LOG_CLASS_UPDATE;
	entry.graph = data->resource_buffer->graph;
	entry.id = data->resource_buffer->id;
	entry.table.class.class = class;
	entry.table.class.last_property_idx = -1;
	entry.properties_ptr = data->update_buffer.properties;

	entry_ptr = g_hash_table_lookup (data->update_buffer.class_updates, &entry);

	if (!entry_ptr) {
		g_array_append_val (data->update_buffer.update_log, entry);
		entry_ptr = &g_array_index (data->update_buffer.update_log,
		                            TrackerDataLogEntry,
		                            data->update_buffer.update_log->len - 1);
		g_hash_table_add (data->update_buffer.class_updates, entry_ptr);
	}

	prop.property = property;
	prop.prev = entry_ptr->table.class.last_property_idx;
	if (value) {
		g_value_init (&prop.value, G_VALUE_TYPE (value));
		g_value_copy (value, &prop.value);
	}
	g_array_append_val (data->update_buffer.properties, prop);
	prop_idx = data->update_buffer.properties->len - 1;

	entry_ptr->table.class.last_property_idx = prop_idx;
}

static void
log_entry_for_class (TrackerData             *data,
                     TrackerDataLogEntryType  type,
                     TrackerClass            *class)
{
	TrackerDataLogEntry entry = { 0, }, *entry_ptr;

	entry.type = type;
	entry.graph = data->resource_buffer->graph;
	entry.id = data->resource_buffer->id;
	entry.table.class.class = class;
	entry.table.class.last_property_idx = -1;
	entry.properties_ptr = data->update_buffer.properties;

	entry_ptr = g_hash_table_lookup (data->update_buffer.class_updates, &entry);

	if (entry_ptr && entry_ptr->type == type)
		return;

	entry.properties_ptr = data->update_buffer.properties;
	g_array_append_val (data->update_buffer.update_log, entry);

	entry_ptr = &g_array_index (data->update_buffer.update_log,
	                            TrackerDataLogEntry,
	                            data->update_buffer.update_log->len - 1);

	if (type == TRACKER_LOG_CLASS_DELETE)
		g_hash_table_remove (data->update_buffer.class_updates, entry_ptr);
	else
		g_hash_table_add (data->update_buffer.class_updates, entry_ptr);
}

static GPtrArray*
tracker_data_query_rdf_type (TrackerData                   *data,
                             TrackerDataUpdateBufferGraph  *graph,
                             TrackerRowid                   id,
                             GError                       **error)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	GArray *classes = NULL;
	GPtrArray *ret = NULL;
	GError *inner_error = NULL;
	TrackerOntologies *ontologies;
	const gchar *class_uri;
	guint i;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);
	ontologies = tracker_data_manager_get_ontologies (data->manager);

	stmt = graph->query_rdf_types;

	if (!stmt) {
		stmt = graph->query_rdf_types =
			tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &inner_error,
			                                        "SELECT (SELECT Uri FROM Resource WHERE ID = \"rdf:type\") "
			                                        "FROM \"%s\".\"rdfs:Resource_rdf:type\" "
			                                        "WHERE ID = ?",
			                                        graph->graph ? graph->graph : "main");
	}

	if (stmt) {
		tracker_db_statement_bind_int (stmt, 0, id);
		classes = tracker_db_statement_get_values (stmt,
		                                           TRACKER_PROPERTY_TYPE_STRING,
		                                           &inner_error);
	}

	if (G_UNLIKELY (inner_error)) {
		g_propagate_prefixed_error (error,
		                            inner_error,
		                            "Querying RDF type:");
		return NULL;
	}

	if (classes) {
		ret = g_ptr_array_sized_new (classes->len);

		for (i = 0; i < classes->len; i++) {
			TrackerClass *cl;

			class_uri = g_value_get_string (&g_array_index (classes, GValue, i));
			cl = tracker_ontologies_get_class_by_uri (ontologies, class_uri);
			if (!cl) {
				g_critical ("Unknown class %s", class_uri);
				continue;
			}
			g_ptr_array_add (ret, cl);
		}

		g_array_unref (classes);
	}

	return ret;
}

static TrackerRowid
query_resource_id (TrackerData  *data,
                   const gchar  *uri,
                   GError      **error)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerRowid *value, id = 0;
	GError *inner_error = NULL;
	GArray *res = NULL;

	value = g_hash_table_lookup (data->update_buffer.resource_cache, uri);
	if (value)
		return *value;

	stmt = data->update_buffer.query_resource;
	if (!stmt) {
		iface = tracker_data_manager_get_writable_db_interface (data->manager);
		stmt = data->update_buffer.query_resource =
			tracker_db_interface_create_statement (iface,
			                                       TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
			                                       &inner_error,
			                                       "SELECT ID FROM Resource WHERE Uri = ?");
	}

	if (stmt) {
		tracker_db_statement_bind_text (stmt, 0, uri);
		res = tracker_db_statement_get_values (stmt,
		                                       TRACKER_PROPERTY_TYPE_INTEGER,
		                                       &inner_error);
	}

	if (G_UNLIKELY (inner_error)) {
		g_propagate_prefixed_error (error,
		                            inner_error,
		                            "Querying resource ID:");
		return 0;
	}

	if (res && res->len == 1) {
		id = g_value_get_int64 (&g_array_index (res, GValue, 0));
		g_hash_table_insert (data->update_buffer.resource_cache, g_strdup (uri),
		                     tracker_rowid_copy (&id));
	}

	g_clear_pointer (&res, g_array_unref);

	return id;
}

static gboolean
tracker_data_ensure_insert_resource_stmt (TrackerData  *data,
                                          GError      **error)
{
	TrackerDBInterface *iface;

	if (G_LIKELY (data->update_buffer.insert_resource))
		return TRUE;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);

	data->update_buffer.insert_resource =
		tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                       "INSERT INTO Resource (Uri, BlankNode) VALUES (?, ?)");

	return data->update_buffer.insert_resource != NULL;
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
	gboolean inserted;
	TrackerRowid *value, id = 0;
	TrackerOntologies *ontologies;
	TrackerClass *class;

	if (strchr (uri, ':') == NULL)
		g_warning ("«%s» is not an absolute IRI", uri);

	value = g_hash_table_lookup (data->update_buffer.resource_cache, uri);

	if (value != NULL) {
		return *value;
	}

	ontologies = tracker_data_manager_get_ontologies (data->manager);
	class = tracker_ontologies_get_class_by_uri (ontologies, uri);

	/* Fast path, look up classes/properties directly */
	if (class) {
		id = tracker_class_get_id (class);
	} else {
		TrackerProperty *property;

		property = tracker_ontologies_get_property_by_uri (ontologies, uri);
		if (property)
			id = tracker_property_get_id (property);
	}

	if (id != 0) {
		g_hash_table_insert (data->update_buffer.resource_cache,
		                     g_strdup (uri),
		                     tracker_rowid_copy (&id));
		return id;
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

	if (!tracker_data_ensure_insert_resource_stmt (data, error))
		return 0;

	stmt = data->update_buffer.insert_resource;
	tracker_db_statement_bind_text (stmt, 0, uri);
	tracker_db_statement_bind_int (stmt, 1, FALSE);
	inserted = tracker_db_statement_execute (stmt, NULL);

	if (inserted) {
		iface = tracker_data_manager_get_writable_db_interface (data->manager);
		id = tracker_db_interface_sqlite_get_last_insert_id (iface);
		g_hash_table_add (data->update_buffer.new_resources,
		                  tracker_rowid_copy (&id));
	} else {
		id = query_resource_id (data, uri, error);
	}

	if (id != 0) {
		g_hash_table_insert (data->update_buffer.resource_cache,
		                     g_strdup (uri),
		                     tracker_rowid_copy (&id));
	}

	return id;
}

static void
statement_bind_gvalue (TrackerDBStatement *stmt,
                       gint                idx,
                       const GValue       *value)
{
	GType type;

	type = G_VALUE_TYPE (value);
	switch (type) {
	case G_TYPE_STRING:
		tracker_db_statement_bind_text (stmt, idx, g_value_get_string (value));
		break;
	case G_TYPE_INT:
		tracker_db_statement_bind_int (stmt, idx, g_value_get_int (value));
		break;
	case G_TYPE_INT64:
		tracker_db_statement_bind_int (stmt, idx, g_value_get_int64 (value));
		break;
	case G_TYPE_DOUBLE:
		tracker_db_statement_bind_double (stmt, idx, g_value_get_double (value));
		break;
	case G_TYPE_BOOLEAN:
		tracker_db_statement_bind_int (stmt, idx, g_value_get_boolean (value));
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
				tracker_db_statement_bind_text (stmt, idx, str);
				g_free (str);
			} else {
				tracker_db_statement_bind_int (stmt, idx,
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
				tracker_db_statement_bind_text (stmt, idx, data);
			} else {
				/* String with langtag */
				tracker_db_statement_bind_bytes (stmt, idx, bytes);
			}
		} else if (type == TRACKER_TYPE_URI) {
			tracker_db_statement_bind_text (stmt, idx, g_value_get_string (value));
		} else {
			g_warning ("Unknown type for binding: %s\n", G_VALUE_TYPE_NAME (value));
		}
		break;
	}
}

static TrackerDBStatement *
tracker_data_ensure_update_statement (TrackerData          *data,
                                      TrackerDataLogEntry  *entry,
                                      GError              **error)
{
	TrackerDBStatement *stmt;
	TrackerDBInterface *iface;
	const gchar *database;

	stmt = tracker_db_statement_mru_lookup (&data->update_buffer.stmt_mru, entry);
	if (stmt) {
		tracker_db_statement_mru_update (&data->update_buffer.stmt_mru, stmt);
		return g_object_ref (stmt);
	}

	iface = tracker_data_manager_get_writable_db_interface (data->manager);
	database = entry->graph->graph ? entry->graph->graph : "main";

	if (entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "DELETE FROM \"%s\".\"%s\" WHERE ID = ?",
		                                               database,
		                                               tracker_property_get_table_name (entry->table.multivalued.property));
	} else if (entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_DELETE) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "DELETE FROM \"%s\".\"%s\" WHERE ID = ? AND \"%s\" = ?",
		                                               database,
		                                               tracker_property_get_table_name (entry->table.multivalued.property),
		                                               tracker_property_get_name (entry->table.multivalued.property));
	} else if (entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_INSERT) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "INSERT OR IGNORE INTO \"%s\".\"%s\" (ID, \"%s\") VALUES (?, ?)",
		                                               database,
		                                               tracker_property_get_table_name (entry->table.multivalued.property),
		                                               tracker_property_get_name (entry->table.multivalued.property));
	} else if (entry->type == TRACKER_LOG_CLASS_DELETE) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "DELETE FROM \"%s\".\"%s\" WHERE ID = ?",
		                                               database,
		                                               tracker_class_get_name (entry->table.class.class));
	} else {
		GHashTable *visited_properties;
		TrackerDataPropertyEntry *property_entry;
		gint param, property_idx;
		GString *sql;

		sql = g_string_new (NULL);
		visited_properties = g_hash_table_new (NULL, NULL);
		param = 2;

		if (entry->type == TRACKER_LOG_CLASS_INSERT) {
			GString *values_sql;

			g_string_append_printf (sql,
			                        "INSERT INTO \"%s\".\"%s\" (ID",
			                        database,
			                        tracker_class_get_name (entry->table.class.class));
			values_sql = g_string_new ("VALUES (?1");

			property_idx = entry->table.class.last_property_idx;

			while (property_idx >= 0) {
				property_entry = &g_array_index (entry->properties_ptr,
				                                 TrackerDataPropertyEntry,
				                                 property_idx);
				property_idx = property_entry->prev;

				if (g_hash_table_contains (visited_properties, property_entry->property))
					continue;

				g_string_append_printf (sql, ", \"%s\"", tracker_property_get_name (property_entry->property));
				g_string_append_printf (values_sql, ", ?%d", param++);
				g_hash_table_add (visited_properties, property_entry->property);
			}

			g_string_append (sql, ")");
			g_string_append (values_sql, ")");

			stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
			                                               "%s %s", sql->str, values_sql->str);
			g_string_free (sql, TRUE);
			g_string_free (values_sql, TRUE);
		} else if (entry->type == TRACKER_LOG_CLASS_UPDATE) {
			g_string_append_printf (sql,
			                        "UPDATE \"%s\".\"%s\" SET ",
			                        database,
			                        tracker_class_get_name (entry->table.class.class));
			property_idx = entry->table.class.last_property_idx;

			while (property_idx >= 0) {
				TrackerDataPropertyEntry *property_entry;

				property_entry = &g_array_index (entry->properties_ptr,
				                                 TrackerDataPropertyEntry,
				                                 property_idx);
				property_idx = property_entry->prev;

				if (g_hash_table_contains (visited_properties, property_entry->property))
					continue;

				if (param > 2)
					g_string_append (sql, ", ");

				g_string_append_printf (sql, "\"%s\" = ?%d",
				                        tracker_property_get_name (property_entry->property),
				                        param++);
				g_hash_table_add (visited_properties, property_entry->property);
			}

			g_string_append (sql, " WHERE ID = ?1");

			stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
			                                              sql->str);
			g_string_free (sql, TRUE);
		}

		g_hash_table_unref (visited_properties);
	}

	if (stmt) {
		tracker_db_statement_mru_insert (&data->update_buffer.stmt_mru,
		                                 tracker_data_log_entry_copy (entry),
		                                 stmt);
	}

	return stmt;
}

static gboolean
tracker_data_flush_log (TrackerData  *data,
                        GError      **error)
{
	TrackerDBStatement *stmt = NULL;
	TrackerDataPropertyEntry *property_entry;
	guint i;
	GError *inner_error = NULL;

	for (i = 0; i < data->update_buffer.update_log->len; i++) {
		TrackerDataLogEntry *entry;

		entry = &g_array_index (data->update_buffer.update_log,
		                        TrackerDataLogEntry, i);

		stmt = tracker_data_ensure_update_statement (data, entry, error);
		if (!stmt)
			return FALSE;

		if (entry->type == TRACKER_LOG_CLASS_DELETE ||
		    entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR) {
			tracker_db_statement_bind_int (stmt, 0, entry->id);
		} else if (entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_DELETE ||
		           entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_INSERT) {
			tracker_db_statement_bind_int (stmt, 0, entry->id);

			property_entry = &g_array_index (entry->properties_ptr,
			                                 TrackerDataPropertyEntry,
			                                 entry->table.multivalued.change_idx);
			statement_bind_gvalue (stmt, 1, &property_entry->value);
		} else {
			GList *visited_properties = NULL;
			gint param, property_idx;

			tracker_db_statement_bind_int (stmt, 0, entry->id);
			param = 1;

			property_idx = entry->table.class.last_property_idx;

			while (property_idx >= 0) {
				TrackerDataPropertyEntry *property_entry;

				property_entry = &g_array_index (entry->properties_ptr,
				                                 TrackerDataPropertyEntry,
				                                 property_idx);
				property_idx = property_entry->prev;

				if (g_list_find (visited_properties, property_entry->property))
					continue;

				if (G_VALUE_TYPE (&property_entry->value) == G_TYPE_INVALID) {
					/* just set value to NULL for single value properties */
					tracker_db_statement_bind_null (stmt, param++);
				} else {
					statement_bind_gvalue (stmt, param++, &property_entry->value);
				}

				visited_properties = g_list_prepend (visited_properties, property_entry->property);
			}

			g_list_free (visited_properties);
		}

		tracker_db_statement_execute (stmt, &inner_error);
		g_object_unref (stmt);

		if (inner_error) {
			g_propagate_error (error, inner_error);
			return FALSE;
		}
	}

	return TRUE;
}

static void
tracker_data_update_refcount (TrackerData  *data,
                              TrackerRowid  id,
                              gint          refcount)
{
	const TrackerDataUpdateBufferGraph *graph;
	RefcountEntry entry;
	guint i;

	g_assert (data->resource_buffer != NULL);
	graph = data->resource_buffer->graph;

	for (i = 0; i < graph->refcounts->len; i++) {
		RefcountEntry *ptr;

		ptr = &g_array_index (graph->refcounts, RefcountEntry, i);
		if (ptr->rowid == id) {
			ptr->refcount_change += refcount;
			return;
		}
	}

	entry.rowid = id;
	entry.refcount_change = refcount;
	g_array_append_val (graph->refcounts, entry);
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

	old_values = get_property_values (data, property, error);
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
	TrackerRowid id;
	gint refcount;
	guint i;
	GError *inner_error = NULL;
	const gchar *database;
	gchar *query;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);
	database = graph->graph ? graph->graph : "main";

	for (i = 0; i < graph->refcounts->len; i++) {
		RefcountEntry *entry;

		entry = &g_array_index (graph->refcounts, RefcountEntry, i);
		id = entry->rowid;
		refcount = entry->refcount_change;

		if (refcount > 0) {
			if (!graph->insert_ref) {
				query = g_strdup_printf ("INSERT OR IGNORE INTO \"%s\".Refcount (ROWID, Refcount) VALUES (?1, 0)",
				                         database);
				graph->insert_ref =
					tracker_db_interface_create_statement (iface,
					                                       TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
					                                       &inner_error, query);
				g_free (query);

				if (inner_error) {
					g_propagate_error (error, inner_error);
					break;
				}
			}

			tracker_db_statement_bind_int (graph->insert_ref, 0, id);
			tracker_db_statement_execute (graph->insert_ref, &inner_error);

			if (inner_error) {
				g_propagate_error (error, inner_error);
				break;
			}
		}

		if (refcount != 0) {
			if (!graph->update_ref) {
				query = g_strdup_printf ("UPDATE \"%s\".Refcount SET Refcount = Refcount + ?2 WHERE Refcount.ROWID = ?1",
				                         database);
				graph->update_ref =
					tracker_db_interface_create_statement (iface,
					                                       TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
					                                       &inner_error, query);
				g_free (query);

				if (inner_error) {
					g_propagate_error (error, inner_error);
					break;
				}
			}

			tracker_db_statement_bind_int (graph->update_ref, 0, id);
			tracker_db_statement_bind_int (graph->update_ref, 1, refcount);
			tracker_db_statement_execute (graph->update_ref, &inner_error);

			if (inner_error) {
				g_propagate_error (error, inner_error);
				break;
			}
		}

		if (refcount < 0) {
			if (!graph->delete_ref) {
				query = g_strdup_printf ("DELETE FROM \"%s\".Refcount WHERE Refcount.ROWID = ?1 AND Refcount.Refcount = 0",
				                         database);
				graph->delete_ref =
					tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
					                                       &inner_error, query);
				g_free (query);

				if (inner_error) {
					g_propagate_error (error, inner_error);
					break;
				}
			}

			tracker_db_statement_bind_int (graph->delete_ref, 0, id);
			tracker_db_statement_execute (graph->delete_ref, &inner_error);

			if (inner_error) {
				g_propagate_error (error, inner_error);
				break;
			}
		}
	}
}

static void
graph_buffer_free (TrackerDataUpdateBufferGraph *graph)
{
	g_clear_object (&graph->insert_ref);
	g_clear_object (&graph->update_ref);
	g_clear_object (&graph->delete_ref);
	g_clear_object (&graph->query_rdf_types);
	g_hash_table_unref (graph->resources);
	g_array_unref (graph->refcounts);
	g_free (graph->graph);
	tracker_db_statement_mru_finish (&graph->values_mru);
	g_slice_free (TrackerDataUpdateBufferGraph, graph);
}

static void resource_buffer_free (TrackerDataUpdateBufferResource *resource)
{
	g_clear_pointer (&resource->predicates, g_hash_table_unref);

	g_list_free (resource->fts_properties);
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
	TrackerDBInterface *iface;
	GHashTableIter iter;
	GError *actual_error = NULL;
	const gchar *database;
	GList *l;
	guint i;

	if (data->update_buffer.update_log->len == 0)
		return;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);

	for (i = 0; i < data->update_buffer.graphs->len; i++) {
		graph = g_ptr_array_index (data->update_buffer.graphs, i);
		g_hash_table_iter_init (&iter, graph->resources);

		while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &resource)) {
			if (resource->fts_properties && !resource->create) {
				GPtrArray *properties;
				gboolean retval;

				properties = g_ptr_array_sized_new (8);
				database = resource->graph->graph ? resource->graph->graph : "main";

				for (l = resource->fts_properties; l; l = l->next)
					g_ptr_array_add (properties, (gpointer) tracker_property_get_name (l->data));

				g_ptr_array_add (properties, NULL);

				retval = tracker_db_interface_sqlite_fts_delete_text (iface,
				                                                      database,
				                                                      resource->id,
				                                                      (const gchar **) properties->pdata,
				                                                      error);
				g_ptr_array_free (properties, TRUE);

				if (!retval)
					goto out;
			}
		}
	}

	if (!tracker_data_flush_log (data, error))
		goto out;

	for (i = 0; i < data->update_buffer.graphs->len; i++) {
		graph = g_ptr_array_index (data->update_buffer.graphs, i);
		g_hash_table_iter_init (&iter, graph->resources);

		while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &resource)) {
			if (resource->fts_properties) {
				GPtrArray *properties;
				gboolean retval;

				properties = g_ptr_array_sized_new (8);
				database = resource->graph->graph ? resource->graph->graph : "main";

				for (l = resource->fts_properties; l; l = l->next)
					g_ptr_array_add (properties, (gpointer) tracker_property_get_name (l->data));

				g_ptr_array_add (properties, NULL);

				retval = tracker_db_interface_sqlite_fts_update_text (iface,
				                                                      database,
				                                                      resource->id,
				                                                      (const gchar **) properties->pdata,
				                                                      error);
				g_ptr_array_free (properties, TRUE);

				if (!retval)
					goto out;
			}
		}

		tracker_data_flush_graph_refcounts (data, graph, &actual_error);
		if (actual_error) {
			g_propagate_error (error, actual_error);
			goto out;
		}

		g_hash_table_remove_all (graph->resources);
		g_array_set_size (graph->refcounts, 0);
	}

out:
	g_hash_table_remove_all (data->update_buffer.new_resources);
	g_hash_table_remove_all (data->update_buffer.class_updates);
	g_array_set_size (data->update_buffer.properties, 0);
	g_array_set_size (data->update_buffer.update_log, 0);
	data->resource_buffer = NULL;
}

void
tracker_data_update_buffer_might_flush (TrackerData  *data,
                                        GError      **error)
{
	if (data->update_buffer.update_log->len > UPDATE_LOG_SIZE - 10)
		tracker_data_update_buffer_flush (data, error);
}

static void
tracker_data_update_buffer_clear (TrackerData *data)
{
	TrackerDataUpdateBufferGraph *graph;
	guint i;

	for (i = 0; i < data->update_buffer.graphs->len; i++) {
		graph = g_ptr_array_index (data->update_buffer.graphs, i);
		g_hash_table_remove_all (graph->resources);
		g_array_set_size (graph->refcounts, 0);
	}

	g_hash_table_remove_all (data->update_buffer.new_resources);
	g_hash_table_remove_all (data->update_buffer.resource_cache);
	g_hash_table_remove_all (data->update_buffer.class_updates);
	g_array_set_size (data->update_buffer.properties, 0);
	g_array_set_size (data->update_buffer.update_log, 0);
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

	log_entry_for_class (data, TRACKER_LOG_CLASS_INSERT, cl);
	tracker_data_resource_ref (data, data->resource_buffer->id, FALSE);

	class_id = tracker_class_get_id (cl);
	ontologies = tracker_data_manager_get_ontologies (data->manager);

	g_value_init (&gvalue, G_TYPE_INT64);
	g_value_set_int64 (&gvalue, class_id);
	log_entry_for_multi_value_property (data,
	                                    TRACKER_LOG_MULTIVALUED_PROPERTY_INSERT,
	                                    tracker_ontologies_get_rdf_type (ontologies),
	                                    &gvalue);

	tracker_data_resource_ref (data, class_id, TRUE);

	if (!data->resource_buffer->modified) {
		/* first modification of this particular resource, update nrl:modified */
		TrackerOntologies *ontologies;
		TrackerProperty *modified;
		GValue gvalue = { 0 };

		data->resource_buffer->modified = TRUE;
		ontologies = tracker_data_manager_get_ontologies (data->manager);
		modified = tracker_ontologies_get_nrl_modified (ontologies);

		g_value_init (&gvalue, G_TYPE_INT64);
		g_value_set_int64 (&gvalue, get_transaction_modseq (data));
		log_entry_for_single_value_property (data,
		                                     tracker_property_get_domain (modified),
		                                     modified, &gvalue);
	}

	if (data->resource_buffer->create &&
	    strcmp (tracker_class_get_uri (cl), TRACKER_PREFIX_RDFS "Resource") == 0) {
		/* Add nrl:added for the new rdfs:Resource */
		TrackerOntologies *ontologies;
		TrackerProperty *added;
		GValue gvalue = { 0 };

		ontologies = tracker_data_manager_get_ontologies (data->manager);
		added = tracker_ontologies_get_nrl_added (ontologies);

		g_value_init (&gvalue, G_TYPE_INT64);
		g_value_set_int64 (&gvalue, data->resource_time);
		log_entry_for_single_value_property (data,
		                                     tracker_property_get_domain (added),
		                                     added, &gvalue);
	}

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
		old_values = get_property_values (data, *domain_indexes, &inner_error);
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
			log_entry_for_single_value_property (data, cl, *domain_indexes, v);
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
	TrackerDataUpdateBufferGraph *graph;
	const gchar *database;
	GArray *old_values;

	if (!data->resource_buffer->predicates) {
		data->resource_buffer->predicates =
			g_hash_table_new_full (NULL, NULL, g_object_unref,
			                       (GDestroyNotify) g_array_unref);
	}

	old_values = g_hash_table_lookup (data->resource_buffer->predicates, property);
	if (old_values != NULL)
		return old_values;

	graph = data->resource_buffer->graph;
	database = graph->graph ? graph->graph : "main";

	if (!data->resource_buffer->create) {
		TrackerDBStatement *stmt;

		stmt = tracker_db_statement_mru_lookup (&graph->values_mru, property);

		if (stmt) {
			tracker_db_statement_mru_update (&graph->values_mru, stmt);
			g_object_ref (stmt);
		} else {
			TrackerDBInterface *iface;
			const gchar *table_name;
			const gchar *field_name;

			table_name = tracker_property_get_table_name (property);
			field_name = tracker_property_get_name (property);

			iface = tracker_data_manager_get_writable_db_interface (data->manager);
			stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
			                                               "SELECT \"%s\" FROM \"%s\".\"%s\" WHERE ID = ?",
			                                               field_name, database, table_name);
			if (!stmt)
				return NULL;

			tracker_db_statement_mru_insert (&graph->values_mru, property, stmt);
		}

		if (stmt) {
			tracker_db_statement_bind_int (stmt, 0, data->resource_buffer->id);
			old_values = tracker_db_statement_get_values (stmt,
			                                              tracker_property_get_data_type (property),
			                                              error);
			g_object_unref (stmt);
		}

		if (!old_values)
			return NULL;
	}

	if (!old_values) {
		old_values = g_array_new (FALSE, FALSE, sizeof (GValue));
		g_array_set_clear_func (old_values, (GDestroyNotify) g_value_unset);
	}

	g_hash_table_insert (data->resource_buffer->predicates, g_object_ref (property), old_values);

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
                        GHashTable       *visited,
                        TrackerData      *data,
                        TrackerResource  *resource,
                        GError          **error)
{
	const gchar *identifier;
	TrackerRowid *bnode_id;

	bnode_id = g_hash_table_lookup (visited, resource);
	if (bnode_id)
		return *bnode_id;

	identifier = tracker_resource_get_identifier_internal (resource);

	if (identifier) {
		/* If the resource has a blank node identifier string already,
		 * then it has been likely referenced in other user SPARQL.
		 * Make sure this blank node label is cached for future
		 * references.
		 */
		return get_bnode_id (bnodes, data, identifier, error);
	} else {
		return tracker_data_generate_bnode (data, error);
	}
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
insert_property_domain_indexes (TrackerData     *data,
                                TrackerProperty *property,
                                const GValue    *gvalue)
{
	TrackerClass **domain_index_classes;

	domain_index_classes = tracker_property_get_domain_indexes (property);
	while (*domain_index_classes) {
		if (resource_in_domain_index_class (data, *domain_index_classes)) {
			log_entry_for_single_value_property (data,
			                                     *domain_index_classes,
			                                     property,
			                                     gvalue);
		}
		domain_index_classes++;
	}
}

static void
delete_property_domain_indexes (TrackerData     *data,
                                TrackerProperty *property,
                                const GValue    *gvalue)
{
	TrackerClass **domain_index_classes;

	domain_index_classes = tracker_property_get_domain_indexes (property);
	while (*domain_index_classes) {
		if (resource_in_domain_index_class (data, *domain_index_classes)) {
			log_entry_for_single_value_property (data,
			                                     *domain_index_classes,
			                                     property, NULL);
		}
		domain_index_classes++;
	}
}

static gboolean
maybe_convert_value (TrackerData         *data,
                     TrackerPropertyType  source,
                     TrackerPropertyType  target,
                     const GValue        *value,
                     GValue              *value_out)
{
	if (source == TRACKER_PROPERTY_TYPE_RESOURCE &&
	    target == TRACKER_PROPERTY_TYPE_STRING &&
	    G_VALUE_HOLDS_INT64 (value)) {
		TrackerDBInterface *iface;
		gchar *str;

		iface = tracker_data_manager_get_writable_db_interface (data->manager);
		str = tracker_data_query_resource_urn (data->manager, iface,
		                                       g_value_get_int64 (value));

		if (!str) {
			str = g_strdup_printf ("urn:bnode:%" G_GINT64_FORMAT,
			                       g_value_get_int64 (value));
		}

		g_value_init (value_out, G_TYPE_STRING);
		g_value_take_string (value_out, str);
		return TRUE;
	}

	return FALSE;
}

static void
maybe_append_fts_property (TrackerData     *data,
                           TrackerProperty *property)
{
	if (!tracker_property_get_fulltext_indexed (property))
		return;

	if (g_list_find (data->resource_buffer->fts_properties, property))
		return;

	data->resource_buffer->fts_properties =
		g_list_prepend (data->resource_buffer->fts_properties, property);
}

static gboolean
cache_insert_metadata_decomposed (TrackerData      *data,
                                  TrackerProperty  *property,
                                  const GValue     *object,
                                  GError          **error)
{
	gboolean            multiple_values;
	TrackerProperty   **super_properties;
	GArray             *old_values;
	GError             *new_error = NULL;
	gboolean            change = FALSE;

	if (!check_property_domain (data, property)) {
		if (data->implicit_create) {
			if (!cache_create_service_decomposed (data,
							      tracker_property_get_domain (property),
							      error))
				return FALSE;
		} else {
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

			return FALSE;
		}
	}

	/* read existing property values */
	old_values = get_property_values (data, property, &new_error);
	if (new_error) {
		g_propagate_error (error, new_error);
		return FALSE;
	}

	/* also insert super property values */
	super_properties = tracker_property_get_super_properties (property);
	multiple_values = tracker_property_get_multiple_values (property);

	maybe_append_fts_property (data, property);

	while (*super_properties) {
		gboolean super_is_multi;
		GArray *super_old_values;
		GValue converted = G_VALUE_INIT;
		const GValue *val;

		super_is_multi = tracker_property_get_multiple_values (*super_properties);
		super_old_values = get_property_values (data, *super_properties, &new_error);
		if (new_error) {
			g_propagate_error (error, new_error);
			return FALSE;
		}

		maybe_append_fts_property (data, *super_properties);

		if (maybe_convert_value (data,
		                         tracker_property_get_data_type (property),
		                         tracker_property_get_data_type (*super_properties),
		                         object,
		                         &converted))
			val = &converted;
		else
			val = object;

		if (super_is_multi || super_old_values->len == 0) {
			change |= cache_insert_metadata_decomposed (data, *super_properties, val,
			                                            &new_error);
			if (new_error) {
				g_value_unset (&converted);
				g_propagate_error (error, new_error);
				return FALSE;
			}
		}
		g_value_unset (&converted);
		super_properties++;
	}

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
		             tracker_property_get_name (property),
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
		if (multiple_values) {
			log_entry_for_multi_value_property (data,
			                                    TRACKER_LOG_MULTIVALUED_PROPERTY_INSERT,
			                                    property, object);
		} else {
			log_entry_for_single_value_property (data,
			                                     tracker_property_get_domain (property),
			                                     property, object);
		}

		if (!data->resource_buffer->modified) {
			/* first modification of this particular resource, update nrl:modified */
			TrackerOntologies *ontologies;
			TrackerProperty *modified;
			GValue gvalue = { 0 };

			data->resource_buffer->modified = TRUE;
			ontologies = tracker_data_manager_get_ontologies (data->manager);
			modified = tracker_ontologies_get_nrl_modified (ontologies);

			g_value_init (&gvalue, G_TYPE_INT64);
			g_value_set_int64 (&gvalue, get_transaction_modseq (data));
			log_entry_for_single_value_property (data,
			                                     tracker_property_get_domain (modified),
			                                     modified, &gvalue);
		}

		if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE)
			tracker_data_resource_ref (data, g_value_get_int64 (object), multiple_values);

		if (!multiple_values)
			insert_property_domain_indexes (data, property, object);

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
	TrackerProperty   **super_properties;
	GArray             *old_values;
	GError             *new_error = NULL;
	gboolean            change = FALSE;

	multiple_values = tracker_property_get_multiple_values (property);

	maybe_append_fts_property (data, property);

	/* read existing property values */
	old_values = get_property_values (data, property, &new_error);
	if (new_error) {
		/* no need to error out if statement does not exist for any reason */
		g_clear_error (&new_error);
		return FALSE;
	}

	if (!value_set_remove_value (old_values, object)) {
		/* value not found */
	} else {
		if (multiple_values) {
			log_entry_for_multi_value_property (data,
			                                    TRACKER_LOG_MULTIVALUED_PROPERTY_DELETE,
			                                    property, object);
		} else {
			log_entry_for_single_value_property (data,
			                                     tracker_property_get_domain (property),
			                                     property, NULL);
		}

		if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE)
			tracker_data_resource_unref (data, g_value_get_int64 (object), multiple_values);

		if (!multiple_values)
			delete_property_domain_indexes (data, property, object);

		change = TRUE;
	}

	/* also delete super property values */
	super_properties = tracker_property_get_super_properties (property);
	while (*super_properties) {
		GValue converted = G_VALUE_INIT;
		const GValue *val;

		if (maybe_convert_value (data,
		                         tracker_property_get_data_type (property),
		                         tracker_property_get_data_type (*super_properties),
		                         object,
		                         &converted))
			val = &converted;
		else
			val = object;

		change |= delete_metadata_decomposed (data, *super_properties, val, error);
		super_properties++;
		g_value_unset (&converted);
	}

	return change;
}

static gboolean
cache_delete_resource_type_full (TrackerData   *data,
                                 TrackerClass  *class,
                                 gboolean       single_type,
                                 GError       **error)
{
	TrackerProperty **properties, *prop;
	gboolean found;
	guint i, j, p, n_props;
	TrackerOntologies *ontologies;
	GValue gvalue = G_VALUE_INIT;

	ontologies = tracker_data_manager_get_ontologies (data->manager);

	if (!single_type) {
		if (strcmp (tracker_class_get_uri (class), TRACKER_PREFIX_RDFS "Resource") == 0) {
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
		for (i = 0; i < data->resource_buffer->types->len; i++) {
			TrackerClass *type, **super_classes;

			type = g_ptr_array_index (data->resource_buffer->types, i);
			super_classes = tracker_class_get_super_classes (type);

			for (j = 0; super_classes[j]; j++) {
				if (super_classes[j] != class)
					continue;

				if (!cache_delete_resource_type_full (data, type, FALSE, error))
					return FALSE;
			}
		}

		/* Once done deleting subclasses, fall through */
	}

	/* Delete property values, only properties that:
	 * - Have ?prop rdfs:range rdfs:Resource
	 * - Are domain indexes in other classes
	 *
	 * Do need inspection of the previous content. All multivalued properties
	 * can be cleared through TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR, and all
	 * values for other single-valued properties will eventually disappear when
	 * deleting the row from the table representing the given TrackerClass.
	 */
	properties = tracker_ontologies_get_properties (ontologies, &n_props);

	for (p = 0; p < n_props; p++) {
		gboolean multiple_values;
		GArray *old_values;
		gint y;

		prop = properties[p];

		if (prop == tracker_ontologies_get_rdf_type (ontologies))
			continue;
		if (tracker_property_get_domain (prop) != class)
			continue;

		multiple_values = tracker_property_get_multiple_values (prop);

		maybe_append_fts_property (data, prop);

		if (*tracker_property_get_domain_indexes (prop) ||
		    tracker_property_get_data_type (prop) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			old_values = get_property_values (data, prop, error);
			if (!old_values)
				return FALSE;

			for (y = old_values->len - 1; y >= 0 ; y--) {
				GValue *old_gvalue, copy = G_VALUE_INIT;

				old_gvalue = &g_array_index (old_values, GValue, y);
				g_value_init (&copy, G_VALUE_TYPE (old_gvalue));
				g_value_copy (old_gvalue, &copy);

				value_set_remove_value (old_values, old_gvalue);

				if (!multiple_values) {
					log_entry_for_single_value_property (data,
					                                     tracker_property_get_domain (prop),
					                                     prop, NULL);
				}

				if (tracker_property_get_data_type (prop) == TRACKER_PROPERTY_TYPE_RESOURCE)
					tracker_data_resource_unref (data, g_value_get_int64 (&copy), multiple_values);

				if (!multiple_values)
					delete_property_domain_indexes (data, prop, &copy);

				g_value_unset (&copy);
			}
		}

		if (multiple_values) {
			log_entry_for_multi_value_property (data,
			                                    TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR,
			                                    prop, NULL);
		}

		if (data->resource_buffer->predicates)
			g_hash_table_remove (data->resource_buffer->predicates, prop);
	}

	g_value_init (&gvalue, G_TYPE_INT64);
	g_value_set_int64 (&gvalue, tracker_class_get_id (class));
	log_entry_for_multi_value_property (data, TRACKER_LOG_MULTIVALUED_PROPERTY_DELETE,
	                                    tracker_ontologies_get_rdf_type (ontologies),
	                                    &gvalue);
	tracker_data_resource_unref (data, tracker_class_get_id (class), TRUE);

	log_entry_for_class (data, TRACKER_LOG_CLASS_DELETE, class);
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
	graph_buffer->refcounts = g_array_sized_new (FALSE, FALSE, sizeof (RefcountEntry), UPDATE_LOG_SIZE);
	graph_buffer->graph = g_strdup (name);

	graph_buffer->resources =
		g_hash_table_new_full (tracker_rowid_hash, tracker_rowid_equal, NULL,
		                       (GDestroyNotify) resource_buffer_free);

	tracker_db_statement_mru_init (&graph_buffer->values_mru, 20,
	                               g_direct_hash,
	                               g_direct_equal,
	                               NULL);

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

	/* large INSERTs with thousands of resources could lead to
	   high peak memory usage due to the update buffer
	   flush the buffer if it already contains 1000 resources */
	tracker_data_update_buffer_might_flush (data, &inner_error);
	if (inner_error)
		return FALSE;

	if (data->resource_buffer != NULL &&
	    g_strcmp0 (data->resource_buffer->graph->graph, graph) == 0 &&
	    data->resource_buffer->id == subject) {
		/* Resource buffer stays the same */
		return TRUE;
	}

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
			rdf_types = tracker_data_query_rdf_type (data,
			                                         graph_buffer,
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

		if (resource_buffer->create) {
			resource_buffer->types = g_ptr_array_new ();
		} else {
			resource_buffer->types = rdf_types;
		}
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
			log_entry_for_multi_value_property (data,
			                                    TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR,
			                                    property, NULL);

			if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
				if (!tracker_data_resource_unref_all (data, property, error))
					return FALSE;
			}
		} else {
			value = &g_array_index (old_values, GValue, 0);
			log_entry_for_single_value_property (data,
			                                     tracker_property_get_domain (property),
			                                     property, NULL);

			if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE)
				tracker_data_resource_unref (data, g_value_get_int64 (value), FALSE);
		}
	} else {
		super_old_values = get_property_values (data, property, error);
		if (!super_old_values)
			return FALSE;

		for (i = 0; i < old_values->len; i++) {
			value = &g_array_index (old_values, GValue, i);

			if (!value_set_remove_value (super_old_values, value))
				continue;

			if (tracker_property_get_multiple_values (property)) {
				log_entry_for_multi_value_property (data,
				                                    TRACKER_LOG_MULTIVALUED_PROPERTY_DELETE,
				                                    property, value);
			} else {
				log_entry_for_single_value_property (data,
				                                     tracker_property_get_domain (property),
				                                     property, NULL);
			}

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
	old_values = get_property_values (data, property, &inner_error);
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
		log_entry_for_multi_value_property (data,
		                                    TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR,
		                                    predicate, NULL);

		if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			if (!tracker_data_resource_unref_all (data, predicate, error))
				return FALSE;
		}
	} else if (!multiple_values) {
		GError *inner_error = NULL;
		GArray *old_values;

		log_entry_for_single_value_property (data,
		                                     tracker_property_get_domain (predicate),
		                                     predicate, NULL);

		if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			old_values = get_property_values (data, predicate, &inner_error);

			if (old_values && old_values->len == 1) {
				GValue *value;

				value = &g_array_index (old_values, GValue, 0);
				tracker_data_resource_unref (data, g_value_get_int64 (value), multiple_values);
				g_array_remove_index (old_values, 0);
			}
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

static void
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

static void
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

		maybe_append_fts_property (data, predicate);

		log_entry_for_multi_value_property (data,
		                                    TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR,
		                                    predicate, NULL);

		if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			if (!tracker_data_resource_unref_all (data, predicate, error))
				return;
		}
	} else {
		if (!resource_buffer_switch (data, graph, subject, error))
			return;

		maybe_append_fts_property (data, predicate);

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

static void
clear_property (gpointer data)
{
	TrackerDataPropertyEntry *entry = data;

	g_value_unset (&entry->value);
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

		data->update_buffer.properties = g_array_sized_new (FALSE, TRUE, sizeof (TrackerDataPropertyEntry), UPDATE_LOG_SIZE);
		g_array_set_clear_func (data->update_buffer.properties, clear_property);
		data->update_buffer.update_log = g_array_sized_new (FALSE, TRUE, sizeof (TrackerDataLogEntry), UPDATE_LOG_SIZE);
		data->update_buffer.class_updates = g_hash_table_new (tracker_data_log_entry_hash,
								      tracker_data_log_entry_equal);
		tracker_db_statement_mru_init (&data->update_buffer.stmt_mru, 100,
		                               tracker_data_log_entry_schema_hash,
		                               tracker_data_log_entry_schema_equal,
		                               (GDestroyNotify) tracker_data_log_entry_free);
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

gboolean
tracker_data_load_from_deserializer (TrackerData          *data,
                                     TrackerDeserializer  *deserializer,
                                     const gchar          *graph,
                                     const gchar          *location,
                                     GError              **error)
{
	TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (deserializer);
	TrackerOntologies *ontologies;
	GError *inner_error = NULL;
	const gchar *subject_str, *predicate_str, *object_str, *graph_str;
	goffset last_parsed_line_no = 0, last_parsed_column_no = 0;

	ontologies = tracker_data_manager_get_ontologies (data->manager);
	data->implicit_create = TRUE;

	while (tracker_sparql_cursor_next (cursor, NULL, &inner_error)) {
		TrackerProperty *predicate;
		GValue object = G_VALUE_INIT;
		TrackerRowid subject;

		subject_str = tracker_sparql_cursor_get_string (cursor,
		                                                TRACKER_RDF_COL_SUBJECT,
		                                                NULL);
		predicate_str = tracker_sparql_cursor_get_string (cursor,
		                                                  TRACKER_RDF_COL_PREDICATE,
		                                                  NULL);
		object_str = tracker_sparql_cursor_get_string (cursor,
		                                               TRACKER_RDF_COL_OBJECT,
		                                               NULL);
		graph_str = tracker_sparql_cursor_get_string (cursor,
		                                              TRACKER_RDF_COL_GRAPH,
		                                              NULL);

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
		                                         NULL, /* FIXME: Missing langtag */
		                                         tracker_property_get_data_type (predicate),
		                                         &object,
		                                         &inner_error))
			goto failed;

		subject = tracker_data_update_ensure_resource (data,
		                                               subject_str,
		                                               &inner_error);
		if (inner_error)
			goto failed;

		tracker_data_insert_statement (data,
		                               graph_str ? graph_str : graph,
		                               subject, predicate, &object,
		                               &inner_error);
		g_value_unset (&object);

		if (inner_error)
			goto failed;

		tracker_data_update_buffer_might_flush (data, &inner_error);

		if (inner_error)
			goto failed;
	}

	if (inner_error)
		goto failed;

	data->implicit_create = FALSE;

	return TRUE;

failed:
	data->implicit_create = FALSE;

	tracker_deserializer_get_parser_location (deserializer,
						  &last_parsed_line_no,
						  &last_parsed_column_no);

	g_propagate_prefixed_error (error, inner_error,
	                            "%s:%" G_GOFFSET_FORMAT ":%" G_GOFFSET_FORMAT ": ",
	                            location, last_parsed_line_no, last_parsed_column_no);

	return FALSE;
}

void
tracker_data_load_rdf_file (TrackerData  *data,
			    GFile        *file,
			    const gchar  *graph,
			    GError      **error)
{
	TrackerSparqlCursor *deserializer;
	gchar *uri;

	deserializer = tracker_deserializer_new_for_file (file, NULL, error);
	if (!deserializer)
		return;

	uri = g_file_get_uri (file);
	tracker_data_load_from_deserializer (data,
	                                     TRACKER_DESERIALIZER (deserializer),
	                                     graph,
	                                     uri,
	                                     error);
	g_object_unref (deserializer);
	g_free (uri);
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
	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
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
	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
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

	/* If the subject is a blank node, this is a whole new insertion.
	 * We don't need deleting anything then.
	 */
	if (tracker_resource_is_blank_node (resource))
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
                          TrackerProperty  *property,
                          const GValue     *val,
                          GHashTable       *visited,
                          GHashTable       *bnodes,
                          GError          **error)
{
	GError *inner_error = NULL;
	GValue free_me = G_VALUE_INIT;
	const GValue *value;
	TrackerRowid id;

	if (G_VALUE_HOLDS (val, TRACKER_TYPE_RESOURCE)) {
		if (!update_resource_single (data,
		                             graph_uri,
		                             g_value_get_object (val),
		                             visited,
		                             bnodes,
		                             &id,
		                             error))
			return FALSE;

		g_value_init (&free_me, G_TYPE_INT64);
		g_value_set_int64 (&free_me, id);
		value = &free_me;
	} else if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE &&
	           g_type_is_a (G_VALUE_TYPE (val), G_TYPE_STRING)) {
		if (g_str_has_prefix (g_value_get_string (val), "_:")) {
			id = get_bnode_id (bnodes, data, g_value_get_string (val), error);
			if (id == 0)
				return FALSE;

			g_value_init (&free_me, G_TYPE_INT64);
			g_value_set_int64 (&free_me, id);
		} else {
			gchar *object_str;
			gboolean retval;

			tracker_data_manager_expand_prefix (data->manager,
			                                    g_value_get_string (val),
			                                    NULL, NULL,
			                                    &object_str);

			retval = tracker_data_query_string_to_value (data->manager,
			                                             object_str,
			                                             NULL,
			                                             tracker_property_get_data_type (property),
			                                             &free_me,
			                                             error);
			g_free (object_str);

			if (!retval)
				return FALSE;
		}

		value = &free_me;
	} else {
		value = val;
	}

	tracker_data_insert_statement (data,
	                               graph_uri,
	                               subject,
	                               property,
	                               value,
	                               &inner_error);
	g_value_unset (&free_me);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}

static gboolean
update_resource_single (TrackerData      *data,
                        const gchar      *graph_uri,
                        TrackerResource  *resource,
                        GHashTable       *visited,
                        GHashTable       *bnodes,
                        TrackerRowid     *id,
                        GError          **error)
{
	TrackerOntologies *ontologies;
	TrackerResourceIterator iter;
	GList *types, *l;
	GError *inner_error = NULL;
	const gchar *subject_str, *property;
	TrackerRowid subject;
	const GValue *value;
	gboolean is_bnode = FALSE;

	if (tracker_resource_is_blank_node (resource)) {
		is_bnode = TRUE;
		subject = get_bnode_for_resource (bnodes, visited, data, resource, error);
		if (!subject)
			return FALSE;
	} else {
		gchar *subject_uri;

		subject_str = tracker_resource_get_identifier (resource);
		tracker_data_manager_expand_prefix (data->manager,
		                                    subject_str, NULL, NULL,
		                                    &subject_uri);
		subject = tracker_data_update_ensure_resource (data, subject_uri, error);
		g_free (subject_uri);
		if (subject == 0)
			return FALSE;
	}

	if (id)
		*id = subject;

	if (g_hash_table_lookup (visited, resource))
		return TRUE;

	g_hash_table_insert (visited, resource, tracker_rowid_copy (&subject));
	ontologies = tracker_data_manager_get_ontologies (data->manager);

	/* Handle rdf:type first */
	types = tracker_resource_get_values (resource, "rdf:type");

	for (l = types; l; l = l->next) {
		if (!update_resource_property (data, graph_uri, resource,
		                               subject,
		                               tracker_ontologies_get_rdf_type (ontologies),
		                               l->data,
		                               visited, bnodes,
		                               error)) {
			g_list_free (types);
			return FALSE;
		}
	}

	g_list_free (types);

	tracker_resource_iterator_init (&iter, resource);

	while (tracker_resource_iterator_next (&iter, &property, &value)) {
		TrackerProperty *predicate;

		if (g_str_equal (property, "rdf:type"))
			continue;

		predicate = tracker_ontologies_get_property_by_uri (ontologies, property);
		if (predicate == NULL) {
			g_set_error (error, TRACKER_SPARQL_ERROR,
			             TRACKER_SPARQL_ERROR_UNKNOWN_PROPERTY,
			             "Property '%s' not found in the ontology",
			             property);
			return FALSE;
		}

		if (!is_bnode && tracker_resource_get_property_overwrite (resource, property)) {
			resource_maybe_reset_property (data, graph_uri, resource,
			                               subject, property,
			                               bnodes, &inner_error);

			if (inner_error)
				return FALSE;
		}

		if (!update_resource_property (data, graph_uri, resource,
		                               subject, predicate, value,
		                               visited, bnodes,
		                               error))
			return FALSE;
	}


	return TRUE;
}

gboolean
tracker_data_update_resource (TrackerData      *data,
                              const gchar      *graph,
                              TrackerResource  *resource,
                              GHashTable       *bnodes,
                              GHashTable       *visited,
                              GError          **error)
{
	gboolean retval;
	gchar *graph_uri = NULL;

	if (bnodes)
		g_hash_table_ref (bnodes);
	else
		bnodes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) tracker_rowid_free);

	if (graph) {
		tracker_data_manager_expand_prefix (data->manager,
		                                    graph, NULL, NULL,
		                                    &graph_uri);
	}

	retval = update_resource_single (data, graph_uri, resource, visited, bnodes, NULL, error);

	g_hash_table_unref (bnodes);
	g_free (graph_uri);

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

	if (!tracker_data_ensure_insert_resource_stmt (data, error))
		return 0;

	stmt = data->update_buffer.insert_resource;
	tracker_db_statement_bind_null (stmt, 0);
	tracker_db_statement_bind_int (stmt, 1, TRUE);
	tracker_db_statement_execute (stmt, &inner_error);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return 0;
	}

	iface = tracker_data_manager_get_writable_db_interface (data->manager);
	id = tracker_db_interface_sqlite_get_last_insert_id (iface);
	g_hash_table_add (data->update_buffer.new_resources,
	                  tracker_rowid_copy (&id));

	return id;
}
