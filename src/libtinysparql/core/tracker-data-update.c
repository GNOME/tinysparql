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

#include <tracker-common.h>

#include "tracker-deserializer-rdf.h"
#include "tracker-private.h"
#include "tracker-uri.h"

#include "core/tracker-class.h"
#include "core/tracker-data-manager.h"
#include "core/tracker-data-update.h"
#include "core/tracker-data-query.h"
#include "core/tracker-db-interface-sqlite.h"
#include "core/tracker-db-manager.h"
#include "core/tracker-ontologies.h"
#include "core/tracker-property.h"
#include "core/tracker-sparql.h"
#include "core/tracker-uuid.h"

typedef struct _TrackerDataUpdateBuffer TrackerDataUpdateBuffer;
typedef struct _TrackerDataUpdateBufferGraph TrackerDataUpdateBufferGraph;
typedef struct _TrackerDataUpdateBufferResource TrackerDataUpdateBufferResource;
typedef struct _TrackerDataBlankBuffer TrackerDataBlankBuffer;
typedef struct _TrackerStatementDelegate TrackerStatementDelegate;
typedef struct _TrackerTransactionDelegate TrackerTransactionDelegate;

#define UPDATE_LOG_SIZE 64

typedef enum {
	TRACKER_LOG_CLASS_INSERT,
	TRACKER_LOG_CLASS_UPDATE,
	TRACKER_LOG_CLASS_DELETE,
	TRACKER_LOG_MULTIVALUED_PROPERTY_INSERT,
	TRACKER_LOG_MULTIVALUED_PROPERTY_DELETE,
	TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR,
	TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_INSERT,
	TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_DELETE,
	TRACKER_LOG_REF_CHANGE_FOR_PROPERTY_CLEAR,
	TRACKER_LOG_REF_CHANGE_FOR_MULTIVALUED_PROPERTY_CLEAR,
	TRACKER_LOG_REF_DEC_FOR_PROPERTY,
	TRACKER_LOG_REF_DEC_FOR_MULTIVALUED_PROPERTY,
	TRACKER_LOG_REF_INC,
	TRACKER_LOG_REF_DEC,
	TRACKER_LOG_PROPERTY_PROPAGATE_INSERT,
	TRACKER_LOG_PROPERTY_PROPAGATE_DELETE,
	TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_INSERT,
	TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_DELETE,
} TrackerDataLogEntryType;

typedef enum
{
	TRACKER_BUS_OP_SPARQL,
	TRACKER_BUS_OP_RDF,
} TrackerBusOpType;

typedef enum
{
	TRACKER_INC_REF,
	TRACKER_DEC_REF,
} TrackerRefcountChange;

typedef struct {
	gint prev;
	TrackerPropertyOp type;
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
		struct {
			TrackerProperty *property;
			gint64 refcount;
		} prop_clear_refcount;
		struct {
			TrackerProperty *property;
			gint64 object_id;
		} prop_refcount;
		struct {
			gpointer dummy;
			gint64 refcount;
		} refcount;
		struct {
			TrackerProperty *source;
			TrackerProperty *dest;
		} propagation;
		struct {
			TrackerProperty *property;
			TrackerClass *dest_class;
		} domain_index;
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

	/* Array of TrackerDataPropertyEntry */
	GArray *properties;
	/* Array of GArrays of TrackerDataLogEntry */
	GPtrArray *update_log;
	/* Set of TrackerDataLogEntry. Used for class events lookups in order to
	 * coalesce single-valued property changes.
	 */
	GHashTable *class_updates;
	/* Set of TrackerDataLogEntry. Used to coalesce refcount */
	GHashTable *refcounts;

	TrackerDBStatementMru stmt_mru;
};

struct _TrackerDataUpdateBufferGraph {
	gchar *graph;

	/* id -> TrackerDataUpdateBufferResource */
	GHashTable *resources;

	TrackerDBStatement *query_rdf_types;
	TrackerDBStatement *fts_delete;
	TrackerDBStatement *fts_insert;
	TrackerDBStatementMru values_mru;
};

struct _TrackerDataUpdateBufferResource {
	TrackerDataUpdateBufferGraph *graph;
	TrackerRowid id;
	gboolean create;
	gboolean modified;
	/* TrackerClass */
	GPtrArray *types;

	guint fts_update : 1;
};

struct _TrackerStatementDelegate {
	TrackerStatementCallback callback;
	gpointer user_data;
};

struct _TrackerTransactionDelegate {
	TrackerTransactionCallback callback;
	gpointer user_data;
};

struct _TrackerData {
	GObject parent_instance;

	TrackerDataManager *manager;

	gboolean in_transaction;
	gboolean in_ontology_transaction;
	gboolean implicit_create;
	TrackerDataUpdateBuffer update_buffer;

	GTimeZone *tz;

	/* current resource */
	TrackerDataUpdateBufferResource *resource_buffer;
	time_t resource_time;
	gint transaction_modseq;
	gboolean has_persistent;

	GPtrArray *statement_callbacks;
	GPtrArray *transaction_callbacks;
};

struct _TrackerDataClass {
	GObjectClass parent_class;
};

enum {
	PROP_0,
	PROP_MANAGER
};

G_DEFINE_TYPE (TrackerData, tracker_data, G_TYPE_OBJECT)

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
	} else if (entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_INSERT ||
		   entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_DELETE ||
		   entry->type == TRACKER_LOG_PROPERTY_PROPAGATE_INSERT ||
		   entry->type == TRACKER_LOG_PROPERTY_PROPAGATE_DELETE) {
		hash ^= g_direct_hash (entry->table.propagation.dest);
	} else if (entry->type == TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_INSERT ||
		   entry->type == TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_DELETE) {
		hash ^= g_direct_hash (entry->table.domain_index.dest_class);
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
	} else if (entry1->type == TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_INSERT ||
		   entry1->type == TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_DELETE ||
		   entry1->type == TRACKER_LOG_PROPERTY_PROPAGATE_INSERT ||
		   entry1->type == TRACKER_LOG_PROPERTY_PROPAGATE_DELETE) {
		if (entry1->table.propagation.dest !=
		    entry2->table.propagation.dest)
			return FALSE;
	} else if (entry1->type == TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_INSERT ||
		   entry1->type == TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_DELETE) {
		if (entry1->table.domain_index.dest_class !=
		    entry2->table.domain_index.dest_class)
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

static gboolean
tracker_data_log_entry_has_property (TrackerDataLogEntry *entry,
                                     TrackerProperty     *property)
{
	if (entry->type == TRACKER_LOG_CLASS_INSERT ||
	    entry->type == TRACKER_LOG_CLASS_UPDATE) {
		TrackerDataPropertyEntry *prop;
		gint idx;

		/* Unite with hash of properties */
		idx = entry->table.class.last_property_idx;

		while (idx >= 0) {
			prop = &g_array_index (entry->properties_ptr,
			                       TrackerDataPropertyEntry, idx);
			if (prop->property == property)
				return TRUE;

			idx = prop->prev;
		}
	}

	return FALSE;
}

void
tracker_data_add_transaction_callback (TrackerData                *data,
                                       TrackerTransactionCallback  callback,
                                       gpointer                    user_data)
{
	TrackerTransactionDelegate *delegate = g_new0 (TrackerTransactionDelegate, 1);

	if (!data->transaction_callbacks) {
		data->transaction_callbacks = g_ptr_array_new_with_free_func (g_free);
	}

	delegate->callback = callback;
	delegate->user_data = user_data;

	g_ptr_array_add (data->transaction_callbacks, delegate);
}

void
tracker_data_remove_transaction_callback (TrackerData                *data,
                                          TrackerTransactionCallback  callback,
                                          gpointer                    user_data)
{
	TrackerTransactionDelegate *delegate;
	guint i;

	if (!data->transaction_callbacks) {
		return;
	}

	for (i = 0; i < data->transaction_callbacks->len; i++) {
		delegate = g_ptr_array_index (data->transaction_callbacks, i);
		if (delegate->callback == callback && delegate->user_data == user_data) {
			g_ptr_array_remove_index (data->transaction_callbacks, i);
			return;
		}
	}
}

void
tracker_data_dispatch_commit_statement_callbacks (TrackerData *data)
{
	if (data->transaction_callbacks) {
		guint n;
		for (n = 0; n < data->transaction_callbacks->len; n++) {
			TrackerTransactionDelegate *delegate;
			delegate = g_ptr_array_index (data->transaction_callbacks, n);
			delegate->callback (TRACKER_DATA_COMMIT,
			                    delegate->user_data);
		}
	}
}

void
tracker_data_dispatch_rollback_statement_callbacks (TrackerData *data)
{
	if (data->transaction_callbacks) {
		guint n;
		for (n = 0; n < data->transaction_callbacks->len; n++) {
			TrackerTransactionDelegate *delegate;
			delegate = g_ptr_array_index (data->transaction_callbacks, n);
			delegate->callback (TRACKER_DATA_ROLLBACK,
			                    delegate->user_data);
		}
	}
}

void
tracker_data_add_statement_callback (TrackerData              *data,
                                     TrackerStatementCallback  callback,
                                     gpointer                  user_data)
{
	TrackerStatementDelegate *delegate = g_new0 (TrackerStatementDelegate, 1);

	if (!data->statement_callbacks) {
		data->statement_callbacks = g_ptr_array_new_with_free_func (g_free);
	}

	delegate->callback = callback;
	delegate->user_data = user_data;

	g_ptr_array_add (data->statement_callbacks, delegate);
}

void
tracker_data_remove_statement_callback (TrackerData             *data,
                                        TrackerStatementCallback callback,
                                        gpointer                 user_data)
{
	TrackerStatementDelegate *delegate;
	guint i;

	if (!data->statement_callbacks) {
		return;
	}

	for (i = 0; i < data->statement_callbacks->len; i++) {
		delegate = g_ptr_array_index (data->statement_callbacks, i);
		if (delegate->callback == callback && delegate->user_data == user_data) {
			g_ptr_array_remove_index (data->statement_callbacks, i);
			return;
		}
	}
}

void
tracker_data_dispatch_insert_statement_callbacks (TrackerData  *data,
                                                  TrackerRowid  predicate_id,
                                                  TrackerRowid  class_id)
{
	if (data->statement_callbacks) {
		guint n;

		for (n = 0; n < data->statement_callbacks->len; n++) {
			TrackerStatementDelegate *delegate;

			delegate = g_ptr_array_index (data->statement_callbacks, n);
			delegate->callback (TRACKER_DATA_INSERT,
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
tracker_data_dispatch_delete_statement_callbacks (TrackerData  *data,
                                                  TrackerRowid  predicate_id,
                                                  TrackerRowid  class_id)
{
	if (data->statement_callbacks) {
		guint n;

		for (n = 0; n < data->statement_callbacks->len; n++) {
			TrackerStatementDelegate *delegate;

			delegate = g_ptr_array_index (data->statement_callbacks, n);
			delegate->callback (TRACKER_DATA_DELETE,
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
	TrackerDBInterface *temp_iface;
	TrackerDBStatement *stmt;
	TrackerOntologies *ontologies;
	TrackerProperty *property;
	GError *inner_error = NULL;
	gint64 max_modseq = 0;
	gboolean retval = FALSE, first = TRUE;

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
		retval = tracker_db_statement_next_integer (stmt, &first, &max_modseq, &inner_error);
		g_assert (tracker_db_statement_next_integer (stmt, &first, NULL, NULL) == FALSE);
		g_object_unref (stmt);
	}

	if (!retval) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	data->transaction_modseq = max_modseq + 1;
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
tracker_data_finalize (GObject *object)
{
	TrackerData *data = TRACKER_DATA (object);

	g_clear_pointer (&data->update_buffer.graphs, g_ptr_array_unref);
	g_clear_pointer (&data->update_buffer.new_resources, g_hash_table_unref);
	g_clear_pointer (&data->update_buffer.resource_cache, g_hash_table_unref);
	g_clear_pointer (&data->update_buffer.properties, g_array_unref);
	g_clear_pointer (&data->update_buffer.update_log, g_ptr_array_unref);
	g_clear_pointer (&data->update_buffer.class_updates, g_hash_table_unref);
	g_clear_pointer (&data->update_buffer.refcounts, g_hash_table_unref);
	g_clear_object (&data->update_buffer.insert_resource);
	tracker_db_statement_mru_finish (&data->update_buffer.stmt_mru);

	g_clear_pointer (&data->statement_callbacks, g_ptr_array_unref);
	g_clear_pointer (&data->transaction_callbacks, g_ptr_array_unref);

	g_clear_pointer (&data->tz, g_time_zone_unref);

	G_OBJECT_CLASS (tracker_data_parent_class)->finalize (object);
}

static void
tracker_data_class_init (TrackerDataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = tracker_data_set_property;
	object_class->finalize = tracker_data_finalize;

	g_object_class_install_property (object_class,
	                                 PROP_MANAGER,
	                                 g_param_spec_object ("manager",
	                                                      "manager",
	                                                      "manager",
	                                                      TRACKER_TYPE_DATA_MANAGER,
	                                                      G_PARAM_WRITABLE |
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

static TrackerDataLogEntry *
append_to_update_log (TrackerData         *data,
                      TrackerDataLogEntry  entry)
{
	TrackerDataLogEntry *entry_ptr;
	GArray *array = NULL;

	if (data->update_buffer.update_log->len > 0) {
		array = g_ptr_array_index (data->update_buffer.update_log,
		                           data->update_buffer.update_log->len - 1);

		/* This chunk is full */
		if (array->len == UPDATE_LOG_SIZE)
			array = NULL;
	}

	if (!array) {
		/* Reserve a new chunk */
		array = g_array_sized_new (FALSE, FALSE, sizeof (TrackerDataLogEntry), UPDATE_LOG_SIZE);
		g_ptr_array_add (data->update_buffer.update_log, array);
	}

	g_array_append_val (array, entry);
	entry_ptr = &g_array_index (array,
	                            TrackerDataLogEntry,
	                            array->len - 1);

	return entry_ptr;
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

	append_to_update_log (data, entry);
}

static void
log_entry_for_single_value_property (TrackerData       *data,
                                     TrackerClass      *class,
                                     TrackerPropertyOp  type,
                                     TrackerProperty   *property,
                                     const GValue      *value)
{
	TrackerDataLogEntry entry = { 0, }, *entry_ptr = NULL;
	TrackerDataPropertyEntry prop = { 0, };
	guint prop_idx;

	entry.type = TRACKER_LOG_CLASS_UPDATE;
	entry.graph = data->resource_buffer->graph;
	entry.id = data->resource_buffer->id;
	entry.table.class.class = class;
	entry.table.class.last_property_idx = -1;
	entry.properties_ptr = data->update_buffer.properties;

	if (tracker_property_get_data_type (property) != TRACKER_PROPERTY_TYPE_RESOURCE)
		entry_ptr = g_hash_table_lookup (data->update_buffer.class_updates, &entry);

	if (!entry_ptr || tracker_data_log_entry_has_property (entry_ptr, property)) {
		if (entry_ptr)
			g_hash_table_remove (data->update_buffer.class_updates, entry_ptr);
		entry_ptr = append_to_update_log (data, entry);
		g_hash_table_add (data->update_buffer.class_updates, entry_ptr);
	}

	prop.property = property;
	prop.type = type;
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
	entry_ptr = append_to_update_log (data, entry);

	if (type == TRACKER_LOG_CLASS_DELETE)
		g_hash_table_remove (data->update_buffer.class_updates, entry_ptr);
	else
		g_hash_table_add (data->update_buffer.class_updates, entry_ptr);
}

static void
log_entry_for_resource_property_clear_refcount (TrackerData             *data,
                                                TrackerDataLogEntryType  type,
                                                TrackerProperty         *property,
                                                TrackerRefcountChange    change)
{
	TrackerDataLogEntry entry = { 0, };
	gint64 inc;

	g_assert (type == TRACKER_LOG_REF_CHANGE_FOR_PROPERTY_CLEAR ||
	          type == TRACKER_LOG_REF_CHANGE_FOR_MULTIVALUED_PROPERTY_CLEAR);

	inc = change == TRACKER_INC_REF ? 1 : -1;

	entry.type = type;
	entry.graph = data->resource_buffer->graph;
	entry.id = data->resource_buffer->id;
	entry.table.prop_clear_refcount.property = property;
	entry.table.prop_clear_refcount.refcount = inc;
	entry.properties_ptr = data->update_buffer.properties;

	append_to_update_log (data, entry);
}

static void
log_entry_for_resource_property_refcount (TrackerData             *data,
                                          TrackerDataLogEntryType  type,
                                          TrackerProperty         *property,
                                          gint64                   object_id)
{
	TrackerDataLogEntry entry = { 0, };

	g_assert (type == TRACKER_LOG_REF_DEC_FOR_PROPERTY ||
	          type == TRACKER_LOG_REF_DEC_FOR_MULTIVALUED_PROPERTY);

	entry.type = type;
	entry.graph = data->resource_buffer->graph;
	entry.id = data->resource_buffer->id;
	entry.table.prop_refcount.property = property;
	entry.table.prop_refcount.object_id = object_id;
	entry.properties_ptr = data->update_buffer.properties;

	append_to_update_log (data, entry);
}

static void
log_entry_for_resource_refcount (TrackerData             *data,
                                 TrackerDataLogEntryType  type,
                                 TrackerRowid             rowid,
                                 TrackerRefcountChange    change)
{
	TrackerDataLogEntry entry = { 0, }, *entry_ptr;
	gint64 inc;

	g_assert (type == TRACKER_LOG_REF_INC ||
	          type == TRACKER_LOG_REF_DEC);

	inc = type == TRACKER_LOG_REF_INC ? 1 : -1;

	entry.type = type;
	entry.graph = data->resource_buffer->graph;
	entry.id = rowid;
	entry.properties_ptr = data->update_buffer.properties;

	entry_ptr = g_hash_table_lookup (data->update_buffer.refcounts, &entry);

	if (entry_ptr) {
		entry_ptr->table.refcount.refcount += inc;

		/* Change type if refcount changes sign */
		if (entry_ptr->table.refcount.refcount >= 0)
			entry_ptr->type = TRACKER_LOG_REF_INC;
		else
			entry_ptr->type = TRACKER_LOG_REF_DEC;
	} else {
		entry_ptr = append_to_update_log (data, entry);
		entry_ptr->table.refcount.refcount = inc;
		g_hash_table_add (data->update_buffer.refcounts, entry_ptr);
	}
}

static void
log_entry_for_property_propagation (TrackerData             *data,
                                    TrackerDataLogEntryType  type,
                                    TrackerProperty         *source,
                                    TrackerProperty         *dest)
{
	TrackerDataLogEntry entry = { 0, };

	g_assert (type == TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_INSERT ||
	          type == TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_DELETE ||
	          type == TRACKER_LOG_PROPERTY_PROPAGATE_INSERT ||
	          type == TRACKER_LOG_PROPERTY_PROPAGATE_DELETE);

	entry.type = type;
	entry.graph = data->resource_buffer->graph;
	entry.id = data->resource_buffer->id;
	entry.table.propagation.source = source;
	entry.table.propagation.dest = dest;
	entry.properties_ptr = data->update_buffer.properties;

	append_to_update_log (data, entry);
}

static void
log_entry_for_domain_index_propagation (TrackerData             *data,
                                        TrackerDataLogEntryType  type,
                                        TrackerProperty         *property,
                                        TrackerClass            *dest_class)
{
	TrackerDataLogEntry entry = { 0, };

	g_assert (type == TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_INSERT ||
	          type == TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_DELETE);

	entry.type = type;
	entry.graph = data->resource_buffer->graph;
	entry.id = data->resource_buffer->id;
	entry.table.domain_index.property = property;
	entry.table.domain_index.dest_class = dest_class;
	entry.properties_ptr = data->update_buffer.properties;

	append_to_update_log (data, entry);
}

static GPtrArray*
tracker_data_query_rdf_type (TrackerData                   *data,
                             TrackerDataUpdateBufferGraph  *graph,
                             TrackerRowid                   id,
                             GError                       **error)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	GPtrArray *ret = NULL;
	GError *inner_error = NULL;
	TrackerOntologies *ontologies;
	const gchar *class_uri;
	TrackerClass *cl;
	gboolean first = TRUE;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);
	ontologies = tracker_data_manager_get_ontologies (data->manager);

	stmt = graph->query_rdf_types;

	if (!stmt) {
		stmt = graph->query_rdf_types =
			tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &inner_error,
			                                        "SELECT (SELECT Uri FROM Resource WHERE ID = \"rdf:type\") "
			                                        "FROM \"%s%srdfs:Resource_rdf:type\" "
			                                        "WHERE ID = ?",
								graph->graph ? graph->graph : "",
								graph->graph ? "_" : "");
	}

	if (!stmt) {
		g_propagate_prefixed_error (error,
		                            inner_error,
		                            "Querying RDF type:");
		return NULL;
	}

	tracker_db_statement_bind_int (stmt, 0, id);
	ret = g_ptr_array_new ();

	while (tracker_db_statement_next_string (stmt, &first, &class_uri, &inner_error)) {
		cl = tracker_ontologies_get_class_by_uri (ontologies, class_uri);
		g_assert (cl != NULL);
		g_ptr_array_add (ret, cl);
	}

	if (G_UNLIKELY (inner_error)) {
		g_clear_pointer (&ret, g_ptr_array_unref);
		g_propagate_prefixed_error (error,
		                            inner_error,
		                            "Querying RDF type:");
		return NULL;
	}

	return ret;
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

	if (strchr (uri, ':') == NULL) {
		g_set_error (error,
		             TRACKER_DB_INTERFACE_ERROR,
		             TRACKER_DB_CONSTRAINT,
		             "«%s» is not an absolute IRI", uri);
		return 0;
	}

	value = g_hash_table_lookup (data->update_buffer.resource_cache, uri);

	if (value != NULL) {
		return *value;
	}

	ontologies = tracker_data_manager_get_ontologies (data->manager);
	if (ontologies) {
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
	}

	if (id != 0) {
		g_hash_table_insert (data->update_buffer.resource_cache,
		                     g_strdup (uri),
		                     tracker_rowid_copy (&id));
		return id;
	}

	db_manager = tracker_data_manager_get_db_manager (data->manager);
	db_flags = tracker_db_manager_get_flags (db_manager);

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

	iface = tracker_data_manager_get_writable_db_interface (data->manager);

	if (inserted) {
		id = tracker_db_interface_sqlite_get_last_insert_id (iface);
		g_hash_table_add (data->update_buffer.new_resources,
		                  tracker_rowid_copy (&id));
	} else {
		GError *inner_error = NULL;

		id = tracker_data_query_resource_id (data->manager, iface,
		                                     uri, &inner_error);

		if (id == 0) {
			if (inner_error) {
				g_propagate_error (error, inner_error);
			} else {
				/* Insert might have failed due to something else than
				 * constraints (say, no space). In that case we might
				 * fail look up here without further errors. Set one.
				 */
				g_set_error (error,
				             TRACKER_DB_INTERFACE_ERROR,
				             TRACKER_DB_QUERY_ERROR,
				             "Failed to insert URI '%s' with unspecified error",
				             uri);
			}
		}
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

	stmt = tracker_db_statement_mru_lookup (&data->update_buffer.stmt_mru, entry);
	if (stmt) {
		tracker_db_statement_mru_update (&data->update_buffer.stmt_mru, stmt);
		return g_object_ref (stmt);
	}

	iface = tracker_data_manager_get_writable_db_interface (data->manager);

	if (entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "DELETE FROM \"%s%s%s\" WHERE ID = ?",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               tracker_property_get_table_name (entry->table.multivalued.property));
	} else if (entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_DELETE) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "DELETE FROM \"%s%s%s\" WHERE ID = ? AND \"%s\" = ?",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               tracker_property_get_table_name (entry->table.multivalued.property),
		                                               tracker_property_get_name (entry->table.multivalued.property));
	} else if (entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_INSERT) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "INSERT OR IGNORE INTO \"%s%s%s\" (ID, \"%s\") VALUES (?, ?)",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               tracker_property_get_table_name (entry->table.multivalued.property),
		                                               tracker_property_get_name (entry->table.multivalued.property));
	} else if (entry->type == TRACKER_LOG_CLASS_DELETE) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "DELETE FROM \"%s%s%s\" WHERE ID = ?",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               tracker_class_get_name (entry->table.class.class));
	} else if (entry->type == TRACKER_LOG_REF_CHANGE_FOR_PROPERTY_CLEAR) {
		const char *table_name;

		table_name = tracker_property_get_table_name (entry->table.prop_clear_refcount.property);
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "INSERT INTO \"%s%sRefcount\" (ROWID, Refcount) "
		                                               "SELECT \"%s\", ?1 FROM \"%s%s%s\" WHERE \"%s\" IS NOT NULL AND ID = ?2 "
		                                               "ON CONFLICT(ROWID) DO "
		                                               "UPDATE SET Refcount = Refcount + excluded.Refcount WHERE ROWID = excluded.ROWID",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               tracker_property_get_name (entry->table.prop_clear_refcount.property),
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               table_name,
							       tracker_property_get_name (entry->table.prop_clear_refcount.property));
	} else if (entry->type == TRACKER_LOG_REF_CHANGE_FOR_MULTIVALUED_PROPERTY_CLEAR) {
		const char *table_name;

		table_name = tracker_property_get_table_name (entry->table.prop_clear_refcount.property);
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "UPDATE \"%s%sRefcount\" "
		                                               "SET Refcount = Refcount + (?1 * (SELECT COUNT (*) FROM \"%s%s%s\" WHERE ID = ?2)) "
		                                               "WHERE ROWID = ?2",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               table_name);
	} else if (entry->type == TRACKER_LOG_REF_DEC_FOR_PROPERTY) {
		const char *table_name;

		table_name = tracker_property_get_table_name (entry->table.prop_refcount.property);
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "INSERT INTO \"%s%sRefcount\" (ROWID, Refcount) "
		                                               "SELECT \"%s\", ?1 FROM \"%s%s%s\" WHERE \"%s\" = ?2 AND ID = ?3 "
		                                               "ON CONFLICT(ROWID) DO "
		                                               "UPDATE SET Refcount = Refcount + excluded.Refcount WHERE ROWID = excluded.ROWID",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               tracker_property_get_name (entry->table.prop_refcount.property),
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               table_name,
		                                               tracker_property_get_name (entry->table.prop_refcount.property));
	} else if (entry->type == TRACKER_LOG_REF_DEC_FOR_MULTIVALUED_PROPERTY) {
		const char *table_name;

		table_name = tracker_property_get_table_name (entry->table.prop_refcount.property);
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "UPDATE \"%s%sRefcount\" "
		                                               "SET Refcount = Refcount + (?1 * (SELECT COUNT (*) FROM \"%s%s%s\" WHERE \"%s\" = ?2 AND ID = ?3)) "
		                                               "WHERE ROWID = ?3",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               table_name,
		                                               tracker_property_get_name (entry->table.prop_refcount.property));
	} else if (entry->type == TRACKER_LOG_REF_INC) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "INSERT INTO \"%s%sRefcount\" (ROWID, Refcount) "
		                                               "VALUES ($1, $2) "
		                                               "ON CONFLICT(ROWID) DO "
		                                               "UPDATE SET Refcount = Refcount + excluded.Refcount WHERE ROWID = excluded.ROWID",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "");
	} else if (entry->type == TRACKER_LOG_REF_DEC) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "UPDATE \"%s%sRefcount\" SET Refcount = Refcount + $1 WHERE ROWID = ?2",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "");
	} else if (entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_INSERT) {
		const gchar *source_table, *dest_table;

		source_table = tracker_property_get_table_name (entry->table.propagation.source);
		dest_table = tracker_property_get_table_name (entry->table.propagation.dest);

		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "INSERT OR IGNORE INTO \"%s%s%s\" (ID, \"%s\") "
		                                               "SELECT ROWID, \"%s\" FROM \"%s%s%s\" WHERE ROWID = $1",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               dest_table,
		                                               tracker_property_get_name (entry->table.propagation.dest),
		                                               tracker_property_get_name (entry->table.propagation.source),
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               source_table);
	} else if (entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_DELETE) {
		const gchar *source_table, *dest_table;

		source_table = tracker_property_get_table_name (entry->table.propagation.source);
		dest_table = tracker_property_get_table_name (entry->table.propagation.dest);

		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "DELETE FROM \"%s%s%s\" WHERE ROWID = $1 AND \"%s\" IN ("
		                                               "SELECT \"%s\" FROM \"%s%s%s\" WHERE ROWID = $1)",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               dest_table,
		                                               tracker_property_get_name (entry->table.propagation.dest),
		                                               tracker_property_get_name (entry->table.propagation.source),
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               source_table);
	} else if (entry->type == TRACKER_LOG_PROPERTY_PROPAGATE_INSERT ||
	           entry->type == TRACKER_LOG_PROPERTY_PROPAGATE_DELETE) {
		const gchar *source_table, *dest_table;

		source_table = tracker_property_get_table_name (entry->table.propagation.source);
		dest_table = tracker_property_get_table_name (entry->table.propagation.dest);

		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "UPDATE \"%s%s%s\" "
		                                               "SET \"%s\" = SparqlUpdateValue('%s', $1, \"%s\", (SELECT \"%s\" FROM \"%s%s%s\" WHERE ROWID = $2))"
		                                               "WHERE ROWID = $2",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               dest_table,
		                                               tracker_property_get_name (entry->table.propagation.dest),
		                                               tracker_property_get_name (entry->table.propagation.dest),
		                                               tracker_property_get_name (entry->table.propagation.dest),
		                                               tracker_property_get_name (entry->table.propagation.source),
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               source_table);
	} else if (entry->type == TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_INSERT ||
	           entry->type == TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_DELETE) {
		stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
		                                               "UPDATE \"%s%s%s\" "
		                                               "SET \"%s\" = SparqlUpdateValue('%s', $1, \"%s\", (SELECT \"%s\" FROM \"%s%s%s\" WHERE ROWID = $2)) "
		                                               "WHERE ROWID = $2",
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               tracker_class_get_name (entry->table.domain_index.dest_class),
		                                               tracker_property_get_name (entry->table.domain_index.property),
		                                               tracker_property_get_name (entry->table.domain_index.property),
		                                               tracker_property_get_name (entry->table.domain_index.property),
		                                               tracker_property_get_name (entry->table.domain_index.property),
		                                               entry->graph->graph ? entry->graph->graph : "",
		                                               entry->graph->graph ? "_" : "",
		                                               tracker_property_get_table_name (entry->table.domain_index.property));
	} else if (entry->type == TRACKER_LOG_CLASS_INSERT ||
	           entry->type == TRACKER_LOG_CLASS_UPDATE) {
		TrackerDataPropertyEntry *property_entry;
		gint param, property_idx;
		GString *sql;

		sql = g_string_new (NULL);
		param = 2;

		if (entry->type == TRACKER_LOG_CLASS_INSERT) {
			GString *values_sql;

			g_string_append_printf (sql,
			                        "INSERT INTO \"%s%s%s\" (ID",
			                        entry->graph->graph ? entry->graph->graph : "",
			                        entry->graph->graph ? "_" : "",
			                        tracker_class_get_name (entry->table.class.class));
			values_sql = g_string_new ("VALUES (?1");

			property_idx = entry->table.class.last_property_idx;

			while (property_idx >= 0) {
				property_entry = &g_array_index (entry->properties_ptr,
				                                 TrackerDataPropertyEntry,
				                                 property_idx);
				property_idx = property_entry->prev;

				g_string_append_printf (sql, ", \"%s\"", tracker_property_get_name (property_entry->property));
				g_string_append_printf (values_sql, ", ?%d", param++);
			}

			g_string_append (sql, ")");
			g_string_append (values_sql, ")");

			stmt = tracker_db_interface_create_vstatement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
			                                               "%s %s", sql->str, values_sql->str);
			g_string_free (sql, TRUE);
			g_string_free (values_sql, TRUE);
		} else if (entry->type == TRACKER_LOG_CLASS_UPDATE) {
			g_string_append_printf (sql,
			                        "UPDATE \"%s%s%s\" SET ",
			                        entry->graph->graph ? entry->graph->graph : "",
			                        entry->graph->graph ? "_" : "",
			                        tracker_class_get_name (entry->table.class.class));
			property_idx = entry->table.class.last_property_idx;

			while (property_idx >= 0) {
				TrackerDataPropertyEntry *property_entry;

				property_entry = &g_array_index (entry->properties_ptr,
				                                 TrackerDataPropertyEntry,
				                                 property_idx);
				property_idx = property_entry->prev;

				if (param > 2)
					g_string_append (sql, ", ");

				g_string_append_printf (sql, "\"%s\" = SparqlUpdateValue('%s', ?%d, \"%s\", ?%d)",
				                        tracker_property_get_name (property_entry->property),
				                        tracker_property_get_name (property_entry->property),
				                        param,
				                        tracker_property_get_name (property_entry->property),
				                        param + 1);
				param += 2;
			}

			g_string_append (sql, " WHERE ID = ?1");

			stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, error,
			                                              sql->str);
			g_string_free (sql, TRUE);
		}
	}

	if (stmt) {
		tracker_db_statement_mru_insert (&data->update_buffer.stmt_mru,
		                                 tracker_data_log_entry_copy (entry),
		                                 stmt);
	}

	return stmt;
}

static gboolean
tracker_data_flush_log_chunk (TrackerData  *data,
                              GArray       *chunk,
                              GError      **error)
{
	TrackerDBStatement *stmt = NULL;
	TrackerDataPropertyEntry *property_entry;
	guint i;
	GError *inner_error = NULL;

	for (i = 0; i < chunk->len; i++) {
		TrackerDataLogEntry *entry;

		entry = &g_array_index (chunk, TrackerDataLogEntry, i);

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
		} else if (entry->type == TRACKER_LOG_REF_CHANGE_FOR_PROPERTY_CLEAR ||
			   entry->type == TRACKER_LOG_REF_CHANGE_FOR_MULTIVALUED_PROPERTY_CLEAR) {
			tracker_db_statement_bind_int (stmt, 0, entry->table.prop_clear_refcount.refcount);
			tracker_db_statement_bind_int (stmt, 1, entry->id);
		} else if (entry->type == TRACKER_LOG_REF_DEC_FOR_PROPERTY ||
			   entry->type == TRACKER_LOG_REF_DEC_FOR_MULTIVALUED_PROPERTY) {
			tracker_db_statement_bind_int (stmt, 0, -1);
			tracker_db_statement_bind_int (stmt, 1, entry->table.prop_refcount.object_id);
			tracker_db_statement_bind_int (stmt, 2, entry->id);
		} else if (entry->type == TRACKER_LOG_REF_INC) {
			tracker_db_statement_bind_int (stmt, 0, entry->id);
			tracker_db_statement_bind_int (stmt, 1,
			                               entry->table.refcount.refcount);
		} else if (entry->type == TRACKER_LOG_REF_DEC) {
			tracker_db_statement_bind_int (stmt, 0,
			                               entry->table.refcount.refcount);
			tracker_db_statement_bind_int (stmt, 1, entry->id);
		} else if (entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_INSERT ||
		           entry->type == TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_DELETE) {
			tracker_db_statement_bind_int (stmt, 0, entry->id);
		} else if (entry->type == TRACKER_LOG_PROPERTY_PROPAGATE_INSERT ||
		           entry->type == TRACKER_LOG_PROPERTY_PROPAGATE_DELETE) {
			tracker_db_statement_bind_int (stmt, 0,
			                               entry->type == TRACKER_LOG_PROPERTY_PROPAGATE_INSERT ?
			                               TRACKER_OP_INSERT_FAILABLE : TRACKER_OP_DELETE);
			tracker_db_statement_bind_int (stmt, 1, entry->id);
		} else if (entry->type == TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_INSERT ||
		           entry->type == TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_DELETE) {
			tracker_db_statement_bind_int (stmt, 0,
			                               entry->type == TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_INSERT ?
			                               TRACKER_OP_INSERT : TRACKER_OP_DELETE);
			tracker_db_statement_bind_int (stmt, 1, entry->id);
		} else if (entry->type == TRACKER_LOG_CLASS_INSERT ||
		           entry->type == TRACKER_LOG_CLASS_UPDATE) {
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

				if (entry->type == TRACKER_LOG_CLASS_UPDATE)
					tracker_db_statement_bind_int (stmt, param++, property_entry->type);

				if (G_VALUE_TYPE (&property_entry->value) == G_TYPE_INVALID) {
					/* just set value to NULL for single value properties */
					tracker_db_statement_bind_null (stmt, param++);
				} else {
					statement_bind_gvalue (stmt, param++, &property_entry->value);
				}
			}
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

static gboolean
tracker_data_flush_log (TrackerData  *data,
                        GError      **error)
{
	guint i;

	for (i = 0; i < data->update_buffer.update_log->len; i++) {
		GArray *chunk;

		chunk = g_ptr_array_index (data->update_buffer.update_log, i);

		if (!tracker_data_flush_log_chunk (data,
		                                   chunk,
		                                   error))
			return FALSE;
	}

	return TRUE;
}

static void
graph_buffer_free (TrackerDataUpdateBufferGraph *graph)
{
	g_clear_object (&graph->query_rdf_types);
	g_clear_object (&graph->fts_delete);
	g_clear_object (&graph->fts_insert);
	g_hash_table_unref (graph->resources);
	g_free (graph->graph);
	tracker_db_statement_mru_finish (&graph->values_mru);
	g_slice_free (TrackerDataUpdateBufferGraph, graph);
}

static void resource_buffer_free (TrackerDataUpdateBufferResource *resource)
{
	g_ptr_array_free (resource->types, TRUE);
	resource->types = NULL;

	g_slice_free (TrackerDataUpdateBufferResource, resource);
}

gchar *
get_fts_properties (TrackerData *data)
{
	TrackerOntologies *ontologies;
	TrackerProperty **properties;
	guint n_props, i;
	GString *str = NULL;

	ontologies = tracker_data_manager_get_ontologies (data->manager);
	properties = tracker_ontologies_get_properties (ontologies, &n_props);

	for (i = 0; i < n_props; i++) {
		if (!tracker_property_get_fulltext_indexed (properties[i]))
			continue;

		if (!str)
			str = g_string_new (NULL);
		else
			g_string_append_c (str, ',');

		g_string_append_printf (str, "\"%s\"", tracker_property_get_name (properties[i]));
	}

	return str ? g_string_free (str, FALSE) : NULL;
}

static gboolean
tracker_data_ensure_graph_fts_stmts (TrackerData                   *data,
                                     TrackerDataUpdateBufferGraph  *graph,
                                     GError                       **error)
{
	TrackerDBInterface *iface;
	gchar *fts_properties;

	if (G_LIKELY (graph->fts_insert && graph->fts_delete))
		return TRUE;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);
	fts_properties = get_fts_properties (data);

	if (!graph->fts_delete) {
		graph->fts_delete =
			tracker_db_interface_create_vstatement (iface,
			                                        TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
			                                        error,
			                                        "INSERT INTO \"%s%sfts5\" (\"%s%sfts5\", ROWID, %s) "
			                                        "SELECT 'delete', ROWID, %s FROM \"%s%sfts_view\" WHERE ROWID = ?",
			                                        graph->graph ? graph->graph : "",
			                                        graph->graph ? "_" : "",
			                                        graph->graph ? graph->graph : "",
			                                        graph->graph ? "_" : "",
			                                        fts_properties,
			                                        fts_properties,
			                                        graph->graph ? graph->graph : "",
			                                        graph->graph ? "_" : "");
	}

	if (graph->fts_delete && !graph->fts_insert) {
		graph->fts_insert =
			tracker_db_interface_create_vstatement (iface,
			                                        TRACKER_DB_STATEMENT_CACHE_TYPE_NONE,
			                                        error,
			                                        "INSERT INTO \"%s%sfts5\" (ROWID, %s) "
			                                        "SELECT ROWID, %s FROM \"%s%sfts_view\" WHERE ROWID = ?",
			                                        graph->graph ? graph->graph : "",
			                                        graph->graph ? "_" : "",
			                                        fts_properties,
			                                        fts_properties,
			                                        graph->graph ? graph->graph : "",
			                                        graph->graph ? "_" : "");
	}

	g_free (fts_properties);

	return graph->fts_insert && graph->fts_delete;
}

static void
tracker_data_update_buffer_clear (TrackerData *data)
{
	TrackerDataUpdateBufferGraph *graph;
	guint i;

	for (i = 0; i < data->update_buffer.graphs->len; i++) {
		graph = g_ptr_array_index (data->update_buffer.graphs, i);
		g_hash_table_remove_all (graph->resources);
	}

	g_hash_table_remove_all (data->update_buffer.new_resources);
	g_hash_table_remove_all (data->update_buffer.resource_cache);
	g_hash_table_remove_all (data->update_buffer.class_updates);
	g_hash_table_remove_all (data->update_buffer.refcounts);
	g_array_set_size (data->update_buffer.properties, 0);
	g_ptr_array_set_size (data->update_buffer.update_log, 0);
	data->resource_buffer = NULL;
}

void
tracker_data_update_buffer_flush (TrackerData  *data,
                                  GError      **error)
{
	TrackerDataUpdateBufferGraph *graph;
	TrackerDataUpdateBufferResource *resource;
	GHashTableIter iter;
	G_GNUC_UNUSED gboolean fts_updated = FALSE;
	guint i;

	if (data->update_buffer.update_log->len == 0)
		return;

	for (i = 0; i < data->update_buffer.graphs->len; i++) {
		graph = g_ptr_array_index (data->update_buffer.graphs, i);
		g_hash_table_iter_init (&iter, graph->resources);

		while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &resource)) {
			if (resource->fts_update && !resource->create) {
				fts_updated = TRUE;
				if (!tracker_data_ensure_graph_fts_stmts (data,
				                                          graph,
				                                          error))
					goto out;

				tracker_db_statement_bind_int (graph->fts_delete, 0, resource->id);

				if (!tracker_db_statement_execute (graph->fts_delete, error))
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
			if (resource->fts_update) {
				fts_updated = TRUE;
				if (!tracker_data_ensure_graph_fts_stmts (data,
				                                          graph,
				                                          error))
					goto out;

				tracker_db_statement_bind_int (graph->fts_insert, 0, resource->id);

				if (!tracker_db_statement_execute (graph->fts_insert, error))
					goto out;
			}
		}

		g_hash_table_remove_all (graph->resources);
	}

#ifdef G_ENABLE_DEBUG
	if (fts_updated && TRACKER_DEBUG_CHECK (FTS_INTEGRITY)) {
		TrackerDBInterface *iface;

		iface = tracker_data_manager_get_writable_db_interface (data->manager);

		for (i = 0; i < data->update_buffer.graphs->len; i++) {
			graph = g_ptr_array_index (data->update_buffer.graphs, i);

			if (!tracker_data_manager_fts_integrity_check (data->manager, iface, graph->graph)) {
				g_set_error (error,
					     TRACKER_DB_INTERFACE_ERROR,
					     TRACKER_DB_CORRUPT,
					     "FTS index is corrupt in %s",
				             graph->graph);
				goto out;
			}
		}
	}
#endif

out:
	tracker_data_update_buffer_clear (data);
}

void
tracker_data_update_buffer_might_flush (TrackerData  *data,
                                        GError      **error)
{
	if (data->update_buffer.update_log->len == 0) {
		return;
	} else if (data->update_buffer.update_log->len == 1) {
		GArray *ops;

		ops = g_ptr_array_index (data->update_buffer.update_log,
		                         data->update_buffer.update_log->len - 1);
		if (ops->len < UPDATE_LOG_SIZE)
			return;
	}

	tracker_data_update_buffer_flush (data, error);
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

	log_entry_for_resource_refcount (data, TRACKER_LOG_REF_INC,
	                                 data->resource_buffer->id,
	                                 TRACKER_INC_REF);

	class_id = tracker_class_get_id (cl);
	ontologies = tracker_data_manager_get_ontologies (data->manager);

	g_value_init (&gvalue, G_TYPE_INT64);
	g_value_set_int64 (&gvalue, class_id);
	log_entry_for_multi_value_property (data,
	                                    TRACKER_LOG_MULTIVALUED_PROPERTY_INSERT,
	                                    tracker_ontologies_get_rdf_type (ontologies),
	                                    &gvalue);

	log_entry_for_resource_refcount (data, TRACKER_LOG_REF_INC,
	                                 data->resource_buffer->id,
	                                 TRACKER_INC_REF);
	log_entry_for_resource_refcount (data, TRACKER_LOG_REF_INC, class_id,
	                                 TRACKER_INC_REF);

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
		                                     TRACKER_OP_RESET,
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
		                                     TRACKER_OP_INSERT,
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
		if (!tracker_property_get_multiple_values (*domain_indexes)) {
			log_entry_for_domain_index_propagation (data,
			                                        TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_INSERT,
			                                        *domain_indexes,
			                                        cl);
		}
		domain_indexes++;
	}

	return TRUE;
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

static TrackerRowid
get_bnode_id (GHashTable       *bnodes,
              TrackerData      *data,
              const gchar      *str,
              GError          **error)
{
	TrackerRowid *value, bnode_id;

	/* Skip blank node label prefixes */
	if (g_str_has_prefix (str, "_:"))
		str = &str[2];

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
                                TrackerProperty *property)
{
	TrackerClass **domain_index_classes;

	domain_index_classes = tracker_property_get_domain_indexes (property);
	while (*domain_index_classes) {
		if (resource_in_domain_index_class (data, *domain_index_classes)) {
			log_entry_for_domain_index_propagation (data,
			                                        TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_INSERT,
			                                        property,
			                                        *domain_index_classes);
		}

		domain_index_classes++;
	}
}

static void
delete_property_domain_indexes (TrackerData     *data,
                                TrackerProperty *property)
{
	TrackerClass **domain_index_classes;

	domain_index_classes = tracker_property_get_domain_indexes (property);
	while (*domain_index_classes) {
		if (resource_in_domain_index_class (data, *domain_index_classes)) {
			log_entry_for_domain_index_propagation (data,
			                                        TRACKER_LOG_DOMAIN_INDEX_PROPAGATE_DELETE,
			                                        property,
			                                        *domain_index_classes);
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

static inline void
maybe_update_fts (TrackerData     *data,
                  TrackerProperty *property)
{
	data->resource_buffer->fts_update |=
		tracker_property_get_fulltext_indexed (property);
}

static gboolean
cache_insert_metadata_decomposed (TrackerData        *data,
                                  TrackerProperty    *property,
                                  const GValue       *object,
                                  TrackerPropertyOp   op,
                                  GError            **error)
{
	gboolean multiple_values;
	TrackerProperty **super_properties;
	GError *new_error = NULL;

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

	/* also insert super property values */
	super_properties = tracker_property_get_super_properties (property);
	multiple_values = tracker_property_get_multiple_values (property);

	maybe_update_fts (data, property);

	while (*super_properties) {
		GValue converted = G_VALUE_INIT;
		const GValue *val;

		maybe_update_fts (data, *super_properties);

		if (maybe_convert_value (data,
		                         tracker_property_get_data_type (property),
		                         tracker_property_get_data_type (*super_properties),
		                         object,
		                         &converted))
			val = &converted;
		else
			val = object;

		cache_insert_metadata_decomposed (data, *super_properties, val,
		                                  TRACKER_OP_INSERT_FAILABLE,
		                                  &new_error);
		if (new_error) {
			g_value_unset (&converted);
			g_propagate_error (error, new_error);
			return FALSE;
		}

		g_value_unset (&converted);
		super_properties++;
	}

	if (!data->resource_buffer->create &&
	    tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
		/* Since we inconditionally increase references at the end of the function,
		 * issue a (maybe) reference decrease op for the given value, in order to
		 * balance this extra reference with idempotent operations.
		 */
		if (multiple_values) {
			log_entry_for_resource_property_refcount (data,
			                                          TRACKER_LOG_REF_DEC_FOR_MULTIVALUED_PROPERTY,
			                                          property,
			                                          g_value_get_int64 (object));
		}

		log_entry_for_resource_property_refcount (data,
		                                          TRACKER_LOG_REF_DEC_FOR_PROPERTY,
		                                          property,
		                                          g_value_get_int64 (object));
	}

	if (multiple_values) {
		log_entry_for_multi_value_property (data,
		                                    TRACKER_LOG_MULTIVALUED_PROPERTY_INSERT,
		                                    property, object);
	} else {
		log_entry_for_single_value_property (data,
		                                     tracker_property_get_domain (property),
		                                     op, property, object);
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
		                                     TRACKER_OP_RESET,
		                                     modified, &gvalue);
	}

	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
		if (multiple_values) {
			log_entry_for_resource_refcount (data,
							 TRACKER_LOG_REF_INC,
							 data->resource_buffer->id,
							 TRACKER_INC_REF);
		}

		log_entry_for_resource_refcount (data,
		                                 TRACKER_LOG_REF_INC,
		                                 g_value_get_int64 (object),
		                                 TRACKER_INC_REF);
	}

	if (!multiple_values)
		insert_property_domain_indexes (data, property);

	return TRUE;
}

static gboolean
delete_metadata_decomposed (TrackerData      *data,
                            TrackerProperty  *property,
                            const GValue     *object,
                            GError          **error)
{
	gboolean multiple_values;
	TrackerProperty **super_properties;

	multiple_values = tracker_property_get_multiple_values (property);

	maybe_update_fts (data, property);

	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
		if (multiple_values) {
			log_entry_for_resource_property_refcount (data,
			                                          TRACKER_LOG_REF_DEC_FOR_MULTIVALUED_PROPERTY,
			                                          property,
			                                          g_value_get_int64 (object));
		}

		log_entry_for_resource_property_refcount (data,
		                                          TRACKER_LOG_REF_DEC_FOR_PROPERTY,
		                                          property,
		                                          g_value_get_int64 (object));
	}

	if (multiple_values) {
		log_entry_for_multi_value_property (data,
		                                    TRACKER_LOG_MULTIVALUED_PROPERTY_DELETE,
		                                    property, object);
	} else {
		log_entry_for_single_value_property (data,
		                                     tracker_property_get_domain (property),
		                                     TRACKER_OP_DELETE,
		                                     property, object);
	}

	if (!multiple_values)
		delete_property_domain_indexes (data, property);

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

		delete_metadata_decomposed (data, *super_properties, val, error);
		super_properties++;
		g_value_unset (&converted);
	}

	return TRUE;
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

		prop = properties[p];

		if (prop == tracker_ontologies_get_rdf_type (ontologies))
			continue;
		if (tracker_property_get_domain (prop) != class)
			continue;

		multiple_values = tracker_property_get_multiple_values (prop);

		maybe_update_fts (data, prop);

		if (tracker_property_get_data_type (prop) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			if (multiple_values) {
				log_entry_for_resource_property_clear_refcount (data,
				                                                TRACKER_LOG_REF_CHANGE_FOR_MULTIVALUED_PROPERTY_CLEAR,
				                                                prop,
				                                                TRACKER_DEC_REF);
			}

			log_entry_for_resource_property_clear_refcount (data,
			                                                TRACKER_LOG_REF_CHANGE_FOR_PROPERTY_CLEAR,
			                                                prop,
			                                                TRACKER_DEC_REF);
		}

		if (!multiple_values &&
		    *tracker_property_get_domain_indexes (prop))
			delete_property_domain_indexes (data, prop);

		if (multiple_values) {
			log_entry_for_multi_value_property (data,
			                                    TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR,
			                                    prop, NULL);
		}
	}

	g_value_init (&gvalue, G_TYPE_INT64);
	g_value_set_int64 (&gvalue, tracker_class_get_id (class));

	log_entry_for_resource_refcount (data,
	                                 TRACKER_LOG_REF_DEC,
	                                 tracker_class_get_id (class),
	                                 TRACKER_DEC_REF);
	log_entry_for_resource_refcount (data,
	                                 TRACKER_LOG_REF_DEC,
	                                 data->resource_buffer->id,
	                                 TRACKER_DEC_REF);
	log_entry_for_multi_value_property (data, TRACKER_LOG_MULTIVALUED_PROPERTY_DELETE,
	                                    tracker_ontologies_get_rdf_type (ontologies),
	                                    &gvalue);

	log_entry_for_class (data, TRACKER_LOG_CLASS_DELETE, class);

	log_entry_for_resource_refcount (data,
	                                 TRACKER_LOG_REF_DEC,
	                                 data->resource_buffer->id,
	                                 TRACKER_DEC_REF);

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

	if (g_strcmp0 (graph, TRACKER_DEFAULT_GRAPH) == 0)
		graph = NULL;

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
		if (create) {
			rdf_types = g_ptr_array_new ();
		} else {
			rdf_types = tracker_data_query_rdf_type (data,
			                                         graph_buffer,
			                                         subject,
			                                         &inner_error);
			if (!rdf_types) {
				g_propagate_error (error, inner_error);
				return FALSE;
			}

			if (rdf_types->len == 0)
				create = TRUE;
		}

		resource_buffer = g_slice_new0 (TrackerDataUpdateBufferResource);
		resource_buffer->id = subject;
		resource_buffer->create = create;
		resource_buffer->types = rdf_types;
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
                   GError          **error)
{
	TrackerProperty **super_properties;

	if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
		if (tracker_property_get_multiple_values (property)) {
			log_entry_for_resource_property_clear_refcount (data,
			                                                TRACKER_LOG_REF_CHANGE_FOR_MULTIVALUED_PROPERTY_CLEAR,
			                                                property,
			                                                TRACKER_DEC_REF);
		}

		log_entry_for_resource_property_clear_refcount (data,
		                                                TRACKER_LOG_REF_CHANGE_FOR_PROPERTY_CLEAR,
		                                                property,
		                                                TRACKER_DEC_REF);
	}

	if (subproperty == property) {
		if (tracker_property_get_multiple_values (property)) {
			log_entry_for_multi_value_property (data,
			                                    TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR,
			                                    property, NULL);

		} else {
			log_entry_for_single_value_property (data,
			                                     tracker_property_get_domain (property),
			                                     TRACKER_OP_RESET,
			                                     property, NULL);
		}
	} else {
		log_entry_for_property_propagation (data,
		                                    tracker_property_get_multiple_values (property) ?
		                                    TRACKER_LOG_MULTIVALUED_PROPERTY_PROPAGATE_DELETE :
		                                    TRACKER_LOG_PROPERTY_PROPAGATE_DELETE,
		                                    subproperty,
		                                    property);
	}

	/* also delete super property values */
	super_properties = tracker_property_get_super_properties (property);
	while (*super_properties) {
		if (!delete_all_helper (data, graph, subject,
		                        subproperty, *super_properties,
		                        error))
			return FALSE;

		super_properties++;
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

	g_return_val_if_fail (subject != 0, FALSE);
	g_return_val_if_fail (predicate != NULL, FALSE);
	g_return_val_if_fail (data->in_transaction, FALSE);

	if (!resource_buffer_switch (data, graph, subject, error))
		return FALSE;

	ontologies = tracker_data_manager_get_ontologies (data->manager);
	property = tracker_ontologies_get_property_by_uri (ontologies,
	                                                   predicate);

	return delete_all_helper (data, graph, subject,
	                          property, property,
	                          error);
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
		if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			log_entry_for_resource_property_clear_refcount (data,
			                                                TRACKER_LOG_REF_CHANGE_FOR_MULTIVALUED_PROPERTY_CLEAR,
			                                                predicate,
			                                                TRACKER_DEC_REF);
			log_entry_for_resource_property_clear_refcount (data,
			                                                TRACKER_LOG_REF_CHANGE_FOR_PROPERTY_CLEAR,
			                                                predicate,
			                                                TRACKER_DEC_REF);
		}

		log_entry_for_multi_value_property (data,
		                                    TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR,
		                                    predicate, NULL);
	} else if (!multiple_values) {
		if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			log_entry_for_resource_property_clear_refcount (data,
			                                                TRACKER_LOG_REF_CHANGE_FOR_PROPERTY_CLEAR,
			                                                predicate,
			                                                TRACKER_DEC_REF);
		}

		log_entry_for_single_value_property (data,
		                                     tracker_property_get_domain (predicate),
		                                     TRACKER_OP_RESET,
		                                     predicate, NULL);
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
	TrackerClass    *class = NULL;
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

		if (object_str)
			class = tracker_ontologies_get_class_by_uri (ontologies, object_str);

		/* handle rdf:type statements specially to
		   cope with inference and insert blank rows */
		if (class != NULL) {
			if (!cache_create_service_decomposed (data, class, error))
				return;
		} else {
			gchar *id;

			id = tracker_data_query_resource_urn (data->manager,
			                                      tracker_data_manager_get_writable_db_interface (data->manager),
			                                      object_id);
			g_set_error (error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_UNKNOWN_CLASS,
			             "Class '%s' not found in the ontology", id);
			g_free (id);
			return;
		}
	} else {
		/* add value to metadata database */
		change = cache_insert_metadata_decomposed (data, predicate, object,
		                                           TRACKER_OP_INSERT,
		                                           &actual_error);

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
	change = cache_insert_metadata_decomposed (data, predicate, object,
	                                           TRACKER_OP_INSERT,
	                                           &actual_error);

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

		if (!resource_buffer_switch (data, graph, subject, error))
			return;

		maybe_update_fts (data, predicate);

		if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			if (tracker_property_get_multiple_values (predicate)) {
				log_entry_for_resource_property_clear_refcount (data,
				                                                TRACKER_LOG_REF_CHANGE_FOR_MULTIVALUED_PROPERTY_CLEAR,
				                                                predicate,
				                                                TRACKER_DEC_REF);
			}

			log_entry_for_resource_property_clear_refcount (data,
			                                                TRACKER_LOG_REF_CHANGE_FOR_PROPERTY_CLEAR,
			                                                predicate,
			                                                TRACKER_DEC_REF);
		}

		log_entry_for_multi_value_property (data,
		                                    TRACKER_LOG_MULTIVALUED_PROPERTY_CLEAR,
		                                    predicate, NULL);
	} else {
		if (!resource_buffer_switch (data, graph, subject, error))
			return;

		maybe_update_fts (data, predicate);

		if (!delete_single_valued (data, graph, subject, predicate,
		                           !tracker_property_get_multiple_values (predicate),
		                           error))
			return;

		if (tracker_property_get_data_type (predicate) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			tracker_data_insert_statement_with_uri (data, graph, subject, predicate, object, error);
		} else {
			tracker_data_insert_statement_with_string (data, graph, subject, predicate, object, error);
		}
	}
}

static void
clear_property (gpointer data)
{
	TrackerDataPropertyEntry *entry = data;

	g_value_unset (&entry->value);
}

gboolean
tracker_data_begin_transaction (TrackerData  *data,
                                GError      **error)
{
	TrackerDBInterface *iface;
	TrackerDBManager *db_manager;

	g_return_val_if_fail (!data->in_transaction, FALSE);

	db_manager = tracker_data_manager_get_db_manager (data->manager);

	if (!tracker_db_manager_has_enough_space (db_manager)) {
		g_set_error (error, TRACKER_SPARQL_ERROR, TRACKER_SPARQL_ERROR_NO_SPACE,
		             "There is not enough space on the file system for update operations");
		return FALSE;
	}

	if (!data->in_ontology_transaction &&
	    !tracker_data_update_initialize_modseq (data, error))
		return FALSE;

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
		data->update_buffer.update_log = g_ptr_array_sized_new (1);
		g_ptr_array_set_free_func (data->update_buffer.update_log, (GDestroyNotify) g_array_unref);
		data->update_buffer.class_updates = g_hash_table_new (tracker_data_log_entry_hash,
		                                                      tracker_data_log_entry_equal);
		data->update_buffer.refcounts = g_hash_table_new (tracker_data_log_entry_hash,
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

	return TRUE;
}

gboolean
tracker_data_begin_ontology_transaction (TrackerData  *data,
                                         GError      **error)
{
	data->in_ontology_transaction = TRUE;
	return tracker_data_begin_transaction (data, error);
}

gboolean
tracker_data_commit_transaction (TrackerData  *data,
                                 GError      **error)
{
	TrackerDBInterface *iface;
	GError *actual_error = NULL;

	g_return_val_if_fail (data->in_transaction, FALSE);

	iface = tracker_data_manager_get_writable_db_interface (data->manager);

	tracker_data_update_buffer_flush (data, &actual_error);
	if (actual_error) {
		tracker_data_rollback_transaction (data);
		g_propagate_error (error, actual_error);
		return FALSE;
	}

	tracker_db_interface_end_db_transaction (iface,
	                                         &actual_error);

	if (actual_error) {
		tracker_data_rollback_transaction (data);
		g_propagate_error (error, actual_error);
		return FALSE;
	}

	if (data->has_persistent && !data->in_ontology_transaction) {
		data->transaction_modseq++;
	}

	data->resource_time = 0;
	data->in_transaction = FALSE;
	data->in_ontology_transaction = FALSE;

	tracker_data_manager_commit_graphs (data->manager);

	tracker_db_interface_execute_query (iface, NULL, "PRAGMA cache_size = %d", TRACKER_DB_CACHE_SIZE_DEFAULT);

	tracker_data_dispatch_commit_statement_callbacks (data);

	g_clear_pointer (&data->tz, g_time_zone_unref);

	return TRUE;
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

	g_clear_pointer (&data->tz, g_time_zone_unref);
}

gboolean
tracker_data_savepoint (TrackerData         *data,
                        TrackerSavepointOp   op,
                        const char          *name,
                        GError             **error)
{
	TrackerDBInterface *iface;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);

	switch (op) {
	case TRACKER_SAVEPOINT_SET:
		return tracker_db_interface_execute_query (iface, error, "SAVEPOINT %s", name);
		break;
	case TRACKER_SAVEPOINT_RELEASE:
		return tracker_db_interface_execute_query (iface, error, "RELEASE SAVEPOINT %s", name);
		break;
	case TRACKER_SAVEPOINT_ROLLBACK:
		return tracker_db_interface_execute_query (iface, error, "ROLLBACK TRANSACTION TO SAVEPOINT %s", name);
		break;
	}

	g_assert_not_reached ();
}

static GVariant *
update_sparql (TrackerData  *data,
               const gchar  *update,
               gboolean      blank,
               GError      **error)
{
	GError *actual_error = NULL;
	TrackerSparql *sparql_query;
	GVariant *blank_nodes = NULL;

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

	if (!tracker_data_begin_transaction (data, error))
		return NULL;

	sparql_query = tracker_sparql_new_update (data->manager, update, &actual_error);

	if (sparql_query) {
		tracker_sparql_execute_update (sparql_query, NULL, NULL,
		                               blank ? &blank_nodes : NULL,
		                               &actual_error);
		g_object_unref (sparql_query);
	}

	if (actual_error) {
		tracker_data_rollback_transaction (data);
		g_propagate_error (error, actual_error);
		return NULL;
	}

	if (!tracker_data_commit_transaction (data, error))
		return NULL;

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
                                     GHashTable           *bnodes,
                                     GError              **error)
{
	TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (deserializer);
	TrackerOntologies *ontologies;
	GError *inner_error = NULL;
	const gchar *subject_str, *predicate_str, *object_str, *graph_str;
	goffset last_parsed_line_no = 0, last_parsed_column_no = 0;

	if (bnodes)
		g_hash_table_ref (bnodes);
	else
		bnodes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) tracker_rowid_free);

	ontologies = tracker_data_manager_get_ontologies (data->manager);
	data->implicit_create = TRUE;

	while (tracker_sparql_cursor_next (cursor, NULL, &inner_error)) {
		TrackerProperty *predicate;
		GValue object = G_VALUE_INIT;
		TrackerRowid subject;
		const gchar *object_langtag;

		/* Validate cursor format, and skip rows that would result in invalid
		 * production of triples.
		 */
		if (tracker_sparql_cursor_get_value_type (cursor, TRACKER_RDF_COL_GRAPH) !=
		    TRACKER_SPARQL_VALUE_TYPE_UNBOUND &&
		    tracker_sparql_cursor_get_value_type (cursor, TRACKER_RDF_COL_GRAPH) !=
		    TRACKER_SPARQL_VALUE_TYPE_URI)
			continue;
		if (tracker_sparql_cursor_get_value_type (cursor, TRACKER_RDF_COL_SUBJECT) !=
		    TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE &&
		    tracker_sparql_cursor_get_value_type (cursor, TRACKER_RDF_COL_SUBJECT) !=
		    TRACKER_SPARQL_VALUE_TYPE_URI)
			continue;
		if (tracker_sparql_cursor_get_value_type (cursor, TRACKER_RDF_COL_PREDICATE) !=
		    TRACKER_SPARQL_VALUE_TYPE_URI)
			continue;
		if (!tracker_sparql_cursor_is_bound (cursor, TRACKER_RDF_COL_OBJECT))
			continue;

		subject_str = tracker_sparql_cursor_get_string (cursor,
		                                                TRACKER_RDF_COL_SUBJECT,
		                                                NULL);
		predicate_str = tracker_sparql_cursor_get_string (cursor,
		                                                  TRACKER_RDF_COL_PREDICATE,
		                                                  NULL);
		object_str = tracker_sparql_cursor_get_langstring (cursor,
		                                                   TRACKER_RDF_COL_OBJECT,
		                                                   &object_langtag,
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

		if (tracker_sparql_cursor_get_value_type (cursor, TRACKER_RDF_COL_SUBJECT) ==
		    TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE) {
			subject = get_bnode_id (bnodes, data, subject_str, &inner_error);
		} else {
			subject = tracker_data_update_ensure_resource (data,
			                                               subject_str,
			                                               &inner_error);
		}

		if (inner_error)
			goto failed;

		if (tracker_sparql_cursor_get_value_type (cursor, TRACKER_RDF_COL_OBJECT) ==
		    TRACKER_SPARQL_VALUE_TYPE_BLANK_NODE) {
			TrackerRowid object_id;

			object_id = get_bnode_id (bnodes, data, object_str, &inner_error);
			if (inner_error)
				goto failed;

			g_value_init (&object, G_TYPE_INT64);
			g_value_set_int64 (&object, object_id);
		} else {
			if (!tracker_data_query_string_to_value (data->manager,
			                                         object_str,
			                                         object_langtag,
			                                         tracker_property_get_data_type (predicate),
			                                         &object,
			                                         &inner_error))
				goto failed;
		}

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
	g_hash_table_unref (bnodes);

	return TRUE;

failed:
	data->implicit_create = FALSE;
	g_hash_table_unref (bnodes);

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
	                                     NULL,
	                                     error);
	g_object_unref (deserializer);
	g_free (uri);
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
	default:
		g_assert_not_reached ();
	}
}

static gchar *
read_string (GDataInputStream  *istream,
             gsize             *len_out,
             GCancellable      *cancellable,
             GError           **error)
{
	gchar *buf;
	guint32 len;

	len = g_data_input_stream_read_int32 (istream, NULL, error);
	if (len == 0)
		return NULL;

	buf = g_new0 (gchar, len + 1);

	if (!g_input_stream_read_all (G_INPUT_STREAM (istream),
	                              buf,
	                              len,
	                              NULL,
	                              cancellable,
	                              error)) {
		g_free (buf);
		return NULL;
	}

	g_assert (buf[len] == '\0');

	if (len_out)
		*len_out = len;

	return buf;
}

static gboolean
value_from_variant (GValue   *value,
                    GVariant *variant)
{
	if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING)) {
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, g_variant_get_string (variant, NULL));
	} else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_BOOLEAN)) {
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, g_variant_get_boolean (variant));
	} else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_INT16)) {
		g_value_init (value, G_TYPE_INT64);
		g_value_set_int64 (value, (gint64) g_variant_get_int16 (variant));
	} else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_INT32)) {
		g_value_init (value, G_TYPE_INT64);
		g_value_set_int64 (value, (gint64) g_variant_get_int32 (variant));
	} else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_UINT16)) {
		g_value_init (value, G_TYPE_INT64);
		g_value_set_int64 (value, (gint64) g_variant_get_uint16 (variant));
	} else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_UINT32)) {
		g_value_init (value, G_TYPE_INT64);
		g_value_set_int64 (value, (gint64) g_variant_get_uint32 (variant));
	} else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_INT64)) {
		g_value_init (value, G_TYPE_INT64);
		g_value_set_int64 (value, g_variant_get_int64 (variant));
	} else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_DOUBLE)) {
		g_value_init (value, G_TYPE_DOUBLE);
		g_value_set_double (value, g_variant_get_double (variant));
	} else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_BYTESTRING)) {
		gconstpointer data;
		gsize n_elems;
		GBytes *bytes;

		data = g_variant_get_fixed_array (variant, &n_elems, sizeof (guint8));
		bytes = g_bytes_new (data, n_elems * sizeof (guint8));

		g_value_init (value, G_TYPE_BYTES);
		g_value_take_boxed (value, bytes);
	} else {
		return FALSE;
	}

	return TRUE;
}

static void
value_free (gpointer data)
{
	GValue *value = data;

	g_value_unset (value);
	g_free (value);
}

static GHashTable *
extract_parameters (const gchar  *str,
                    GError      **error)
{
	GHashTable *parameters;
	GVariant *variant, *value;
	GVariantIter iter;
	gchar *key;

	variant = g_variant_parse (G_VARIANT_TYPE ("a{sv}"),
	                           str, NULL,
	                           NULL,
	                           error);
	if (!variant)
		return FALSE;

	parameters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, value_free);

	g_variant_iter_init (&iter, variant);
	while (g_variant_iter_next (&iter, "{sv}", &key, &value)) {
		GValue *gvalue = g_new0 (GValue, 1);

		if (!value_from_variant (gvalue, value)) {
			g_free (gvalue);
			continue;
		}

		g_hash_table_insert (parameters, key, gvalue);
		g_variant_unref (value);
	}

	g_variant_unref (variant);

	return parameters;
}

static gboolean
handle_update_sparql (TrackerData       *data,
                      GDataInputStream  *istream,
                      GHashTable        *bnodes,
                      GHashTable        *update_cache,
                      GCancellable      *cancellable,
                      GError           **error)
{
	TrackerSparql *update;
	gchar *buffer = NULL, *parameters_str = NULL;
	GHashTable *parameters = NULL;
	GError *inner_error = NULL;

	buffer = read_string (istream, NULL, cancellable, &inner_error);
	if (!buffer)
		goto error;

	parameters_str = read_string (istream, NULL, cancellable, &inner_error);
	if (inner_error)
		goto error;

	if (parameters_str) {
		parameters = extract_parameters (parameters_str, &inner_error);
		if (!parameters)
			goto error;
	}

	update = g_hash_table_lookup (update_cache, buffer);
	if (!update) {
		update = tracker_sparql_new_update (data->manager, buffer, &inner_error);
		if (!update)
			goto error;

		g_hash_table_insert (update_cache, g_steal_pointer (&buffer), update);
	}

	tracker_sparql_execute_update (update,
	                               parameters,
	                               bnodes,
	                               NULL,
	                               &inner_error);

 error:
	g_clear_pointer (&parameters, g_hash_table_unref);
	g_free (buffer);
	g_free (parameters_str);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}

static gboolean
handle_update_rdf (TrackerData       *data,
                   GDataInputStream  *istream,
                   GHashTable        *bnodes,
                   GCancellable      *cancellable,
                   GError           **error)
{
	TrackerDeserializer *deserializer;
	G_GNUC_UNUSED TrackerDeserializeFlags flags;
	TrackerRdfFormat format;
	GInputStream *rdf_stream;
	gchar *graph = NULL, *rdf = NULL;
	gsize graph_len = 0, rdf_len = 0;
	GError *inner_error = NULL;

	flags = g_data_input_stream_read_uint32 (istream, cancellable, &inner_error);
	if (inner_error)
		goto error;

	format = g_data_input_stream_read_uint32 (istream, cancellable, &inner_error);
	if (inner_error)
		goto error;

	graph = read_string (istream, &graph_len, cancellable, &inner_error);
	if (inner_error)
		goto error;

	if (!g_utf8_validate (graph, graph_len, NULL)) {
		inner_error = g_error_new (TRACKER_SPARQL_ERROR,
					   TRACKER_SPARQL_ERROR_PARSE,
					   "Graph is invalid");
		goto error;
	}

	rdf = read_string (istream, &rdf_len, cancellable, &inner_error);
	if (inner_error)
		goto error;

	rdf_stream = g_memory_input_stream_new_from_data (g_steal_pointer (&rdf), rdf_len, g_free);
	deserializer = TRACKER_DESERIALIZER (tracker_deserializer_new (rdf_stream, NULL,
	                                                               convert_format (format)));
	g_object_unref (rdf_stream);
	if (!deserializer)
		goto error;

	tracker_data_load_from_deserializer (data,
	                                     deserializer,
	                                     graph,
	                                     "<stream>",
	                                     bnodes,
	                                     &inner_error);
	g_object_unref (deserializer);

 error:
	g_free (graph);
	g_free (rdf);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
}

gboolean
tracker_data_load_from_dbus_fd (TrackerData   *data,
                                GInputStream  *istream,
                                GHashTable    *bnodes,
                                GCancellable  *cancellable,
                                GError       **error)
{
	GDataInputStream *stream;
	gint num_queries, i;
	GError *inner_error = NULL;
	GHashTable *update_cache;

	stream = g_data_input_stream_new (istream);
	g_buffered_input_stream_set_buffer_size (G_BUFFERED_INPUT_STREAM (stream),
	                                         sysconf (_SC_PAGE_SIZE));
	g_data_input_stream_set_byte_order (stream,
	                                    G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN);

	num_queries = g_data_input_stream_read_int32 (stream, cancellable, &inner_error);
	if (inner_error)
		goto error;

	update_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	for (i = 0; i < num_queries; i++) {
		TrackerBusOpType op_type;

		op_type = g_data_input_stream_read_uint32 (stream, cancellable, &inner_error);
		if (inner_error)
			break;

		if (op_type == TRACKER_BUS_OP_SPARQL) {
			if (!handle_update_sparql (data, stream,
			                           bnodes, update_cache,
			                           cancellable, &inner_error))
				break;
		} else if (op_type == TRACKER_BUS_OP_RDF) {
			if (!handle_update_rdf (data, stream, bnodes,
			                        cancellable, &inner_error))
				break;
		} else {
			g_assert_not_reached ();
		}
	}

	g_hash_table_unref (update_cache);

 error:
	g_clear_object (&stream);

	if (inner_error) {
		g_propagate_error (error, inner_error);
		return FALSE;
	}

	return TRUE;
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
	TrackerRowid id = 0, *value;

	iface = tracker_data_manager_get_writable_db_interface (data->manager);

	value = g_hash_table_lookup (data->update_buffer.resource_cache, uri);
	if (value) {
		id = *value;
		g_hash_table_remove (data->update_buffer.resource_cache, uri);
	}

	if (id == 0)
		id = tracker_data_query_resource_id (data->manager, iface, uri, error);
	if (id == 0)
		return FALSE;

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

	/* New resources do not need their properties cleared */
	if (g_hash_table_contains (data->update_buffer.new_resources, &subject))
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
			gchar *free_str = NULL;
			const char *object_str;
			gboolean retval;

			object_str = tracker_data_manager_expand_prefix (data->manager,
			                                                 g_value_get_string (val),
			                                                 NULL, &free_str);

			retval = tracker_data_query_string_to_value (data->manager,
			                                             object_str,
			                                             NULL,
			                                             tracker_property_get_data_type (property),
			                                             &free_me,
			                                             error);
			g_free (free_str);

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
		const char *subject_uri;
		char *free_str = NULL;

		subject_str = tracker_resource_get_identifier (resource);

		subject_uri = tracker_data_manager_expand_prefix (data->manager, subject_str,
		                                                  NULL, &free_str);
		subject = tracker_data_update_ensure_resource (data, subject_uri, error);
		g_free (free_str);

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

	g_hash_table_remove (data->update_buffer.new_resources, &subject);

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
	const gchar *graph_uri = NULL;
	char *free_me = NULL;

	if (bnodes)
		g_hash_table_ref (bnodes);
	else
		bnodes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) tracker_rowid_free);

	if (graph) {
		graph_uri = tracker_data_manager_expand_prefix (data->manager,
		                                                graph, NULL, &free_me);
	}

	retval = update_resource_single (data, graph_uri, resource, visited, bnodes, NULL, error);

	g_hash_table_unref (bnodes);
	g_free (free_me);

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

GTimeZone *
tracker_data_get_time_zone (TrackerData *data)
{
	if (!data->tz)
		data->tz = g_time_zone_new_local ();

	return data->tz;
}
