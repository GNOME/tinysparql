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
#include <time.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-common/tracker-date-time.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-ontologies.h>

#include <libtracker-fts/tracker-fts.h>

#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-dbus.h>
#include <libtracker-db/tracker-db-journal.h>

#include "tracker-class.h"
#include "tracker-data-manager.h"
#include "tracker-data-update.h"
#include "tracker-data-query.h"
#include "tracker-ontologies.h"
#include "tracker-property.h"
#include "tracker-sparql-query.h"

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDFS_PREFIX TRACKER_RDFS_PREFIX
#define TRACKER_PREFIX TRACKER_TRACKER_PREFIX
#define NAO_PREFIX TRACKER_NAO_PREFIX
#define NAO_LAST_MODIFIED NAO_PREFIX "lastModified"
#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_PROPERTY RDF_PREFIX "Property"
#define RDF_TYPE RDF_PREFIX "type"

typedef struct _TrackerDataUpdateBuffer TrackerDataUpdateBuffer;
typedef struct _TrackerDataUpdateBufferResource TrackerDataUpdateBufferResource;
typedef struct _TrackerDataUpdateBufferPredicate TrackerDataUpdateBufferPredicate;
typedef struct _TrackerDataUpdateBufferProperty TrackerDataUpdateBufferProperty;
typedef struct _TrackerDataUpdateBufferTable TrackerDataUpdateBufferTable;
typedef struct _TrackerDataBlankBuffer TrackerDataBlankBuffer;
typedef struct _TrackerStatementDelegate TrackerStatementDelegate;
typedef struct _TrackerCommitDelegate TrackerCommitDelegate;

struct _TrackerDataUpdateBuffer {
	/* string -> integer */
	GHashTable *resource_cache;
	/* string -> TrackerDataUpdateBufferResource */
	GHashTable *resources;
	/* integer -> TrackerDataUpdateBufferResource */
	GHashTable *resources_by_id;

	/* the following two fields are valid per sqlite transaction, not just for same subject */
	gboolean fts_ever_updated;
	/* TrackerClass -> integer */
	GHashTable *class_counts;
};

struct _TrackerDataUpdateBufferResource {
	const gchar *subject;
	gint id;
	gboolean create;
	gboolean fts_updated;
	/* TrackerProperty -> GValueArray */
	GHashTable *predicates;
	/* string -> TrackerDataUpdateBufferTable */
	GHashTable *tables;
	/* TrackerClass */
	GPtrArray *types;
};

struct _TrackerDataUpdateBufferProperty {
	const gchar *name;
	GValue value;
	gint graph;
	gboolean fts : 1;
	gboolean date_time : 1;
};

struct _TrackerDataUpdateBufferTable {
	gboolean insert;
	gboolean delete_row;
	gboolean delete_value;
	gboolean multiple_values;
	TrackerClass *class;
	/* TrackerDataUpdateBufferProperty */
	GArray *properties;
};

/* buffer for anonymous blank nodes
 * that are not yet in the database */
struct _TrackerDataBlankBuffer {
	GHashTable *table;
	gchar *subject;
	/* string */
	GArray *predicates;
	/* string */
	GArray *objects;
	/* string */
	GArray *graphs;
};

struct _TrackerStatementDelegate {
	TrackerStatementCallback callback;
	gpointer user_data;
};

struct _TrackerCommitDelegate {
	TrackerCommitCallback callback;
	gpointer user_data;
};

typedef struct {
	gchar *graph;
	gchar *subject;
	gchar *predicate;
	gchar *object;
	gboolean is_uri;
} QueuedStatement;

static gboolean in_transaction = FALSE;
static gboolean in_journal_replay = FALSE;
static TrackerDataUpdateBuffer update_buffer;
/* current resource */
static TrackerDataUpdateBufferResource *resource_buffer;
static TrackerDataBlankBuffer blank_buffer;
static time_t resource_time = 0;

static GPtrArray *insert_callbacks = NULL;
static GPtrArray *delete_callbacks = NULL;
static GPtrArray *commit_callbacks = NULL;
static GPtrArray *rollback_callbacks = NULL;
static gint max_service_id = 0;
static gint max_modseq = 0;

static gint ensure_resource_id (const gchar *uri, gboolean    *create);

void
tracker_data_add_commit_statement_callback (TrackerCommitCallback    callback,
                                            gpointer                 user_data)
{
	TrackerCommitDelegate *delegate = g_new0 (TrackerCommitDelegate, 1);

	if (!commit_callbacks) {
		commit_callbacks = g_ptr_array_new ();
	}

	delegate->callback = callback;
	delegate->user_data = user_data;

	g_ptr_array_add (commit_callbacks, delegate);
}

void
tracker_data_remove_commit_statement_callback (TrackerCommitCallback callback,
                                               gpointer              user_data)
{
	TrackerCommitDelegate *delegate;
	guint i;
	gboolean found = FALSE;

	if (!commit_callbacks) {
		return;
	}

	for (i = 0; i < commit_callbacks->len; i++) {
		delegate = g_ptr_array_index (commit_callbacks, i);
		if (delegate->callback == callback && delegate->user_data == user_data) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		g_free (delegate);
		g_ptr_array_remove_index (commit_callbacks, i);
	}
}

void
tracker_data_add_rollback_statement_callback (TrackerCommitCallback    callback,
                                              gpointer                 user_data)
{
	TrackerCommitDelegate *delegate = g_new0 (TrackerCommitDelegate, 1);

	if (!rollback_callbacks) {
		rollback_callbacks = g_ptr_array_new ();
	}

	delegate->callback = callback;
	delegate->user_data = user_data;

	g_ptr_array_add (rollback_callbacks, delegate);
}


void
tracker_data_remove_rollback_statement_callback (TrackerCommitCallback callback,
                                                 gpointer              user_data)
{
	TrackerCommitDelegate *delegate;
	guint i;
	gboolean found = FALSE;

	if (!rollback_callbacks) {
		return;
	}

	for (i = 0; i < rollback_callbacks->len; i++) {
		delegate = g_ptr_array_index (rollback_callbacks, i);
		if (delegate->callback == callback && delegate->user_data == user_data) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		g_free (delegate);
		g_ptr_array_remove_index (rollback_callbacks, i);
	}
}

void
tracker_data_add_insert_statement_callback (TrackerStatementCallback callback,
                                            gpointer                 user_data)
{
	TrackerStatementDelegate *delegate = g_new0 (TrackerStatementDelegate, 1);

	if (!insert_callbacks) {
		insert_callbacks = g_ptr_array_new ();
	}

	delegate->callback = callback;
	delegate->user_data = user_data;

	g_ptr_array_add (insert_callbacks, delegate);
}

void
tracker_data_remove_insert_statement_callback (TrackerStatementCallback callback,
                                               gpointer                 user_data)
{
	TrackerStatementDelegate *delegate;
	guint i;
	gboolean found = FALSE;

	if (!insert_callbacks) {
		return;
	}

	for (i = 0; i < insert_callbacks->len; i++) {
		delegate = g_ptr_array_index (insert_callbacks, i);
		if (delegate->callback == callback && delegate->user_data == user_data) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		g_free (delegate);
		g_ptr_array_remove_index (insert_callbacks, i);
	}
}

void
tracker_data_add_delete_statement_callback (TrackerStatementCallback callback,
                                            gpointer                 user_data)
{
	TrackerStatementDelegate *delegate = g_new0 (TrackerStatementDelegate, 1);

	if (!delete_callbacks) {
		delete_callbacks = g_ptr_array_new ();
	}

	delegate->callback = callback;
	delegate->user_data = user_data;

	g_ptr_array_add (delete_callbacks, delegate);
}

void
tracker_data_remove_delete_statement_callback (TrackerStatementCallback callback,
                                               gpointer                 user_data)
{
	TrackerStatementDelegate *delegate;
	guint i;
	gboolean found = FALSE;

	if (!delete_callbacks) {
		return;
	}

	for (i = 0; i < delete_callbacks->len; i++) {
		delegate = g_ptr_array_index (delete_callbacks, i);
		if (delegate->callback == callback && delegate->user_data == user_data) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		g_free (delegate);
		g_ptr_array_remove_index (delete_callbacks, i);
	}
}

GQuark tracker_data_error_quark (void) {
	return g_quark_from_static_string ("tracker_data_error-quark");
}

static gint
tracker_data_update_get_new_service_id (void)
{
	TrackerDBCursor    *cursor = NULL;
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	GError *error = NULL;

	if (G_LIKELY (max_service_id != 0)) {
		return ++max_service_id;
	}

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "SELECT MAX(ID) AS A FROM Resource");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &error);
		g_object_unref (stmt);
	}

	if (cursor) {
		if (tracker_db_cursor_iter_next (cursor, &error)) {
			max_service_id = MAX (tracker_db_cursor_get_int (cursor, 0), max_service_id);
		}

		g_object_unref (cursor);
	}

	if (G_UNLIKELY (error)) {
		g_warning ("Could not get new resource ID: %s\n", error->message);
		g_error_free (error);
	}

	return ++max_service_id;
}

void
tracker_data_update_shutdown (void)
{
	max_service_id = 0;
	max_modseq = 0;
}

static gint
tracker_data_update_get_next_modseq (void)
{
	TrackerDBCursor    *cursor = NULL;
	TrackerDBInterface *temp_iface;
	TrackerDBStatement *stmt;
	GError             *error = NULL;

	if (G_LIKELY (max_modseq != 0)) {
		return ++max_modseq;
	}

	temp_iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (temp_iface, &error,
	                                              "SELECT MAX(\"tracker:modified\") AS A FROM \"rdfs:Resource\"");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &error);
		g_object_unref (stmt);
	}

	if (cursor) {
		if (tracker_db_cursor_iter_next (cursor, &error)) {
			max_modseq = MAX (tracker_db_cursor_get_int (cursor, 0), max_modseq);
		}

		g_object_unref (cursor);
	}

	if (G_UNLIKELY (error)) {
		g_warning ("Could not get new resource ID: %s\n", error->message);
		g_error_free (error);
	}

	return ++max_modseq;
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
	gint                            i;

	for (i = 0; i < table->properties->len; i++) {
		property = &g_array_index (table->properties, TrackerDataUpdateBufferProperty, i);
		g_value_unset (&property->value);
	}

	g_array_free (table->properties, TRUE);
	g_slice_free (TrackerDataUpdateBufferTable, table);
}

static TrackerDataUpdateBufferTable *
cache_ensure_table (const gchar            *table_name,
                    gboolean                multiple_values)
{
	TrackerDataUpdateBufferTable *table;

	table = g_hash_table_lookup (resource_buffer->tables, table_name);
	if (table == NULL) {
		table = cache_table_new (multiple_values);
		g_hash_table_insert (resource_buffer->tables, g_strdup (table_name), table);
		table->insert = multiple_values;
	}

	return table;
}

static void
cache_insert_row (TrackerClass *class)
{
	TrackerDataUpdateBufferTable    *table;

	table = cache_ensure_table (tracker_class_get_name (class), FALSE);
	table->class = class;
	table->insert = TRUE;
}

static void
cache_insert_value (const gchar            *table_name,
                    const gchar            *field_name,
                    GValue                 *value,
                    gint                    graph,
                    gboolean                multiple_values,
                    gboolean                fts,
                    gboolean                date_time)
{
	TrackerDataUpdateBufferTable    *table;
	TrackerDataUpdateBufferProperty  property;

	/* No need to strdup here, the incoming string is either always static, or
	 * long-standing as tracker_property_get_name return value. */
	property.name = field_name;

	property.value = *value;
	property.graph = graph;
	property.fts = fts;
	property.date_time = date_time;

	table = cache_ensure_table (table_name, multiple_values);
	g_array_append_val (table->properties, property);
}

static void
cache_delete_row (TrackerClass *class)
{
	TrackerDataUpdateBufferTable    *table;

	table = cache_ensure_table (tracker_class_get_name (class), FALSE);
	table->class = class;
	table->delete_row = TRUE;
}

static void
cache_delete_value (const gchar            *table_name,
                    const gchar            *field_name,
                    GValue                 *value,
                    gboolean                multiple_values,
                    gboolean                fts,
                    gboolean                date_time)
{
	TrackerDataUpdateBufferTable    *table;
	TrackerDataUpdateBufferProperty  property;

	property.name = field_name;
	property.value = *value;
	property.graph = 0;
	property.fts = fts;
	property.date_time = date_time;

	table = cache_ensure_table (table_name, multiple_values);
	table->delete_value = TRUE;
	g_array_append_val (table->properties, property);
}

static gint
query_resource_id (const gchar *uri)
{
	gint id;

	id = GPOINTER_TO_INT (g_hash_table_lookup (update_buffer.resource_cache, uri));

	if (id == 0) {
		id = tracker_data_query_resource_id (uri);

		if (id) {
			g_hash_table_insert (update_buffer.resource_cache, g_strdup (uri), GINT_TO_POINTER (id));
		}
	}

	return id;
}

static gint
ensure_resource_id (const gchar *uri,
                    gboolean    *create)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	GError *error = NULL;
	gint id;

	id = query_resource_id (uri);

	if (create) {
		*create = (id == 0);
	}

	if (id == 0) {
		iface = tracker_db_manager_get_db_interface ();

		id = tracker_data_update_get_new_service_id ();
		stmt = tracker_db_interface_create_statement (iface, &error, "INSERT INTO Resource (ID, Uri) VALUES (?, ?)");

		if (stmt) {
			tracker_db_statement_bind_int (stmt, 0, id);
			tracker_db_statement_bind_text (stmt, 1, uri);
			tracker_db_statement_execute (stmt, &error);
			g_object_unref (stmt);
		}

		if (error) {
			g_critical ("Could not ensure resource existence: %s", error->message);
			g_error_free (error);
		}

		if (!in_journal_replay) {
			tracker_db_journal_append_resource (id, uri);
		}

		g_hash_table_insert (update_buffer.resource_cache, g_strdup (uri), GINT_TO_POINTER (id));
	}

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
		tracker_db_statement_bind_int64 (stmt, (*idx)++, g_value_get_int64 (value));
		break;
	case G_TYPE_DOUBLE:
		tracker_db_statement_bind_double (stmt, (*idx)++, g_value_get_double (value));
		break;
	default:
		if (type == TRACKER_TYPE_DATE_TIME) {
			tracker_db_statement_bind_int64 (stmt, (*idx)++, tracker_date_time_get_time (value));
			tracker_db_statement_bind_int (stmt, (*idx)++, tracker_date_time_get_local_date (value));
			tracker_db_statement_bind_int (stmt, (*idx)++, tracker_date_time_get_local_time (value));
		} else {
			g_warning ("Unknown type for binding: %s\n", G_VALUE_TYPE_NAME (value));
		}
		break;
	}
}

static void
add_class_count (TrackerClass *class,
                 gint          count)
{
	gint old_count_entry;

	tracker_class_set_count (class, tracker_class_get_count (class) + count);

	/* update class_counts table so that the count change can be reverted in case of rollback */
	if (!update_buffer.class_counts) {
		update_buffer.class_counts = g_hash_table_new (g_direct_hash, g_direct_equal);
	}

	old_count_entry = GPOINTER_TO_INT (g_hash_table_lookup (update_buffer.class_counts, class));
	g_hash_table_insert (update_buffer.class_counts, class,
	                     GINT_TO_POINTER (old_count_entry + count));
}

static void
tracker_data_resource_buffer_flush (GError **error)
{
	TrackerDBInterface             *iface;
	TrackerDBStatement             *stmt;
	TrackerDataUpdateBufferTable    *table;
	TrackerDataUpdateBufferProperty *property;
	GHashTableIter                  iter;
	const gchar                    *table_name;
	GString                        *sql, *fts;
	gint                            i, param;
	GError                         *actual_error = NULL;

	iface = tracker_db_manager_get_db_interface ();

	g_hash_table_iter_init (&iter, resource_buffer->tables);
	while (g_hash_table_iter_next (&iter, (gpointer*) &table_name, (gpointer*) &table)) {
		if (table->multiple_values) {
			for (i = 0; i < table->properties->len; i++) {
				property = &g_array_index (table->properties, TrackerDataUpdateBufferProperty, i);

				if (table->delete_value) {
					/* delete rows for multiple value properties */
					stmt = tracker_db_interface_create_statement (iface, &actual_error,
					                                              "DELETE FROM \"%s\" WHERE ID = ? AND \"%s\" = ?",
					                                              table_name,
					                                              property->name);
				} else if (property->date_time) {
					stmt = tracker_db_interface_create_statement (iface, &actual_error,
					                                              "INSERT OR IGNORE INTO \"%s\" (ID, \"%s\", \"%s:localDate\", \"%s:localTime\", \"%s:graph\") VALUES (?, ?, ?, ?, ?)",
					                                              table_name,
					                                              property->name,
					                                              property->name,
					                                              property->name,
					                                              property->name);
				} else {
					stmt = tracker_db_interface_create_statement (iface, &actual_error,
					                                              "INSERT OR IGNORE INTO \"%s\" (ID, \"%s\", \"%s:graph\") VALUES (?, ?, ?)",
					                                              table_name,
					                                              property->name,
					                                              property->name);
				}

				if (actual_error) {
					g_propagate_error (error, actual_error);
					return;
				}

				param = 0;

				tracker_db_statement_bind_int (stmt, param++, resource_buffer->id);
				statement_bind_gvalue (stmt, &param, &property->value);

				if (property->graph != 0) {
					tracker_db_statement_bind_int (stmt, param++, property->graph);
				} else {
					tracker_db_statement_bind_null (stmt, param++);
				}

				tracker_db_statement_execute (stmt, &actual_error);
				g_object_unref (stmt);

				if (actual_error) {
					g_propagate_error (error, actual_error);
					return;
				}
			}
		} else {
			if (table->delete_row) {
				/* remove entry from rdf:type table */
				stmt = tracker_db_interface_create_statement (iface, &actual_error, "DELETE FROM \"rdfs:Resource_rdf:type\" WHERE ID = ? AND \"rdf:type\" = ?");

				if (stmt) {
					tracker_db_statement_bind_int (stmt, 0, resource_buffer->id);
					tracker_db_statement_bind_int (stmt, 1, ensure_resource_id (tracker_class_get_uri (table->class), NULL));
					tracker_db_statement_execute (stmt, &actual_error);
					g_object_unref (stmt);
				}

				if (actual_error) {
					g_propagate_error (error, actual_error);
					return;
				}

				if (table->class) {
					add_class_count (table->class, -1);
				}

				/* remove row from class table */
				stmt = tracker_db_interface_create_statement (iface, &actual_error, "DELETE FROM \"%s\" WHERE ID = ?", table_name);

				if (stmt) {
					tracker_db_statement_bind_int (stmt, 0, resource_buffer->id);
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
				if (strcmp (table_name, "rdfs:Resource") == 0) {
					/* ensure we have a row for the subject id */
					stmt = tracker_db_interface_create_statement (iface, &actual_error,
						                                      "INSERT OR IGNORE INTO \"%s\" (ID, \"tracker:added\", \"tracker:modified\", Available) VALUES (?, ?, ?, 1)",
						                                      table_name);

					if (stmt) {
						tracker_db_statement_bind_int (stmt, 0, resource_buffer->id);
						g_warn_if_fail	(resource_time != 0);
						tracker_db_statement_bind_int64 (stmt, 1, (gint64) resource_time);
						tracker_db_statement_bind_int (stmt, 3, tracker_data_update_get_next_modseq ());
						tracker_db_statement_execute (stmt, &actual_error);
						g_object_unref (stmt);
					}
				} else {
					/* ensure we have a row for the subject id */
					stmt = tracker_db_interface_create_statement (iface, &actual_error,
						                                      "INSERT OR IGNORE INTO \"%s\" (ID) VALUES (?)",
						                                      table_name);

					if (stmt) {
						tracker_db_statement_bind_int (stmt, 0, resource_buffer->id);
						tracker_db_statement_execute (stmt, &actual_error);
						g_object_unref (stmt);
					}
				}

				if (actual_error) {
					g_propagate_error (error, actual_error);
					return;
				}
			}

			if (table->properties->len == 0) {
				continue;
			}

			sql = g_string_new ("UPDATE ");
			g_string_append_printf (sql, "\"%s\" SET ", table_name);

			for (i = 0; i < table->properties->len; i++) {
				property = &g_array_index (table->properties, TrackerDataUpdateBufferProperty, i);
				if (i > 0) {
					g_string_append (sql, ", ");
				}
				g_string_append_printf (sql, "\"%s\" = ?", property->name);

				if (property->date_time) {
					g_string_append_printf (sql, ", \"%s:localDate\" = ?", property->name);
					g_string_append_printf (sql, ", \"%s:localTime\" = ?", property->name);
				}

				g_string_append_printf (sql, ", \"%s:graph\" = ?", property->name);
			}

			g_string_append (sql, " WHERE ID = ?");

			stmt = tracker_db_interface_create_statement (iface, &actual_error, "%s", sql->str);
			g_string_free (sql, TRUE);

			if (actual_error) {
				g_propagate_error (error, actual_error);
				return;
			}

			param = 0;
			for (i = 0; i < table->properties->len; i++) {
				property = &g_array_index (table->properties, TrackerDataUpdateBufferProperty, i);
				if (table->delete_value) {
					/* just set value to NULL for single value properties */
					tracker_db_statement_bind_null (stmt, param++);
					if (property->date_time) {
						/* also set localDate and localTime to NULL */
						tracker_db_statement_bind_null (stmt, param++);
						tracker_db_statement_bind_null (stmt, param++);
					}
				} else {
					statement_bind_gvalue (stmt, &param, &property->value);
				}
				if (property->graph != 0) {
					tracker_db_statement_bind_int (stmt, param++, property->graph);
				} else {
					tracker_db_statement_bind_null (stmt, param++);
				}
			}

			tracker_db_statement_bind_int (stmt, param++, resource_buffer->id);

			tracker_db_statement_execute (stmt, &actual_error);
			g_object_unref (stmt);

			if (actual_error) {
				g_propagate_error (error, actual_error);
				return;
			}
		}
	}

	if (resource_buffer->fts_updated) {
		TrackerProperty *prop;
		GValueArray *values;

		tracker_fts_update_init (resource_buffer->id);

		g_hash_table_iter_init (&iter, resource_buffer->predicates);
		while (g_hash_table_iter_next (&iter, (gpointer*) &prop, (gpointer*) &values)) {
			if (tracker_property_get_fulltext_indexed (prop)) {
				fts = g_string_new ("");
				for (i = 0; i < values->n_values; i++) {
					g_string_append (fts, g_value_get_string (g_value_array_get_nth (values, i)));
					g_string_append_c (fts, ' ');
				}
				tracker_fts_update_text (resource_buffer->id,
							 tracker_data_query_resource_id (tracker_property_get_uri (prop)),
							 fts->str, !tracker_property_get_fulltext_no_limit (prop));
				g_string_free (fts, TRUE);
			}
		}
	}
}

static void resource_buffer_free (TrackerDataUpdateBufferResource *resource)
{
	g_hash_table_unref (resource->predicates);
	g_hash_table_unref (resource->tables);
	resource->subject = NULL;

	g_ptr_array_free (resource->types, TRUE);
	resource->types = NULL;

	g_slice_free (TrackerDataUpdateBufferResource, resource);
}

void
tracker_data_update_buffer_flush (GError **error)
{
	GHashTableIter iter;
	GError *actual_error = NULL;

	if (in_journal_replay) {
		g_hash_table_iter_init (&iter, update_buffer.resources_by_id);
		while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &resource_buffer)) {
			tracker_data_resource_buffer_flush (&actual_error);
			if (actual_error) {
				g_propagate_error (error, actual_error);
				break;
			}
		}

		g_hash_table_remove_all (update_buffer.resources_by_id);
	} else {
		g_hash_table_iter_init (&iter, update_buffer.resources);
		while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &resource_buffer)) {
			tracker_data_resource_buffer_flush (&actual_error);
			if (actual_error) {
				g_propagate_error (error, actual_error);
				break;
			}
		}

		g_hash_table_remove_all (update_buffer.resources);
	}
	resource_buffer = NULL;
}

void
tracker_data_update_buffer_might_flush (GError **error)
{
	/* avoid high memory usage by update buffer */
	if (g_hash_table_size (update_buffer.resources) +
	    g_hash_table_size (update_buffer.resources_by_id) >= 1000) {
		tracker_data_update_buffer_flush (error);
	}
}

static void
tracker_data_update_buffer_clear (void)
{
	g_hash_table_remove_all (update_buffer.resources);
	g_hash_table_remove_all (update_buffer.resources_by_id);
	resource_buffer = NULL;

	tracker_fts_update_rollback ();
	update_buffer.fts_ever_updated = FALSE;

	if (update_buffer.class_counts) {
		/* revert class count changes */

		GHashTableIter iter;
		TrackerClass *class;
		gpointer count_ptr;

		g_hash_table_iter_init (&iter, update_buffer.class_counts);
		while (g_hash_table_iter_next (&iter, (gpointer*) &class, &count_ptr)) {
			gint count;

			count = GPOINTER_TO_INT (count_ptr);
			tracker_class_set_count (class, tracker_class_get_count (class) - count);
		}

		g_hash_table_remove_all (update_buffer.class_counts);
	}
}

static void
tracker_data_blank_buffer_flush (GError **error)
{
	/* end of blank node */
	gint i;
	gint id;
	gchar *subject;
	gchar *blank_uri;
	const gchar *sha1;
	GChecksum *checksum;
	GError *actual_error = NULL;

	subject = blank_buffer.subject;
	blank_buffer.subject = NULL;

	/* we share anonymous blank nodes with identical properties
	   to avoid blowing up the database with duplicates */

	checksum = g_checksum_new (G_CHECKSUM_SHA1);

	/* generate hash uri from data to find resource
	   assumes no collisions due to generally little contents of anonymous nodes */
	for (i = 0; i < blank_buffer.predicates->len; i++) {
		g_checksum_update (checksum, g_array_index (blank_buffer.predicates, guchar *, i), -1);
		g_checksum_update (checksum, g_array_index (blank_buffer.objects, guchar *, i), -1);
	}

	sha1 = g_checksum_get_string (checksum);

	/* generate name based uuid */
	blank_uri = g_strdup_printf ("urn:uuid:%.8s-%.4s-%.4s-%.4s-%.12s",
	                             sha1, sha1 + 8, sha1 + 12, sha1 + 16, sha1 + 20);

	id = tracker_data_query_resource_id (blank_uri);

	if (id == 0) {
		/* uri not found
		   replay piled up statements to create resource */
		for (i = 0; i < blank_buffer.predicates->len; i++) {
			tracker_data_insert_statement (g_array_index (blank_buffer.graphs, gchar *, i),
			                               blank_uri,
			                               g_array_index (blank_buffer.predicates, gchar *, i),
			                               g_array_index (blank_buffer.objects, gchar *, i),
			                               &actual_error);
			if (actual_error) {
				break;
			}
		}
	}

	/* free piled up statements */
	for (i = 0; i < blank_buffer.predicates->len; i++) {
		g_free (g_array_index (blank_buffer.graphs, gchar *, i));
		g_free (g_array_index (blank_buffer.predicates, gchar *, i));
		g_free (g_array_index (blank_buffer.objects, gchar *, i));
	}
	g_array_remove_range (blank_buffer.graphs, 0, blank_buffer.graphs->len);
	g_array_remove_range (blank_buffer.predicates, 0, blank_buffer.predicates->len);
	g_array_remove_range (blank_buffer.objects, 0, blank_buffer.objects->len);

	g_hash_table_insert (blank_buffer.table, subject, blank_uri);
	g_checksum_free (checksum);

	if (actual_error) {
		g_propagate_error (error, actual_error);
	}
}

static void
cache_create_service_decomposed (TrackerClass *cl,
                                 const gchar  *graph,
                                 gint          graph_id)
{
	TrackerClass       **super_classes;
	GValue              gvalue = { 0 };
	gint                i;

	/* also create instance of all super classes */
	super_classes = tracker_class_get_super_classes (cl);
	while (*super_classes) {
		cache_create_service_decomposed (*super_classes, graph, graph_id);
		super_classes++;
	}

	for (i = 0; i < resource_buffer->types->len; i++) {
		if (g_ptr_array_index (resource_buffer->types, i) == cl) {
			/* ignore duplicate statement */
			return;
		}
	}

	g_ptr_array_add (resource_buffer->types, cl);

	g_value_init (&gvalue, G_TYPE_INT);

	cache_insert_row (cl);

	g_value_set_int (&gvalue, ensure_resource_id (tracker_class_get_uri (cl), NULL));
	cache_insert_value ("rdfs:Resource_rdf:type", "rdf:type", &gvalue,
	                    graph != NULL ? ensure_resource_id (graph, NULL) : graph_id,
	                    TRUE, FALSE, FALSE);

	add_class_count (cl, 1);
	if (!in_journal_replay && insert_callbacks) {
		guint n;
		const gchar *class_uri;

		class_uri = tracker_class_get_uri (cl);

		for (n = 0; n < insert_callbacks->len; n++) {
			TrackerStatementDelegate *delegate;

			delegate = g_ptr_array_index (insert_callbacks, n);
			delegate->callback (graph, resource_buffer->subject,
			                    RDF_PREFIX "type", class_uri,
			                    resource_buffer->types,
			                    delegate->user_data);
		}
	}
}

static gboolean
value_equal (GValue *value1,
             GValue *value2)
{
	GType type = G_VALUE_TYPE (value1);

	if (type != G_VALUE_TYPE (value2)) {
		return FALSE;
	}

	switch (type) {
	case G_TYPE_STRING:
		return (strcmp (g_value_get_string (value1), g_value_get_string (value2)) == 0);
	case G_TYPE_INT:
		return g_value_get_int (value1) == g_value_get_int (value2);
	case G_TYPE_DOUBLE:
		/* does RDF define equality for floating point values? */
		return g_value_get_double (value1) == g_value_get_double (value2);
	default:
		if (type == TRACKER_TYPE_DATE_TIME) {
			/* ignore UTC offset for comparison, irrelevant for comparison according to xsd:dateTime spec
			 * http://www.w3.org/TR/xmlschema-2/#dateTime
			 */
			return tracker_date_time_get_time (value1) == tracker_date_time_get_time (value2);
		}
		g_assert_not_reached ();
	}
}

static gboolean
value_set_add_value (GValueArray *value_set,
                     GValue      *value)
{
	gint i;

	g_return_val_if_fail (G_VALUE_TYPE (value), FALSE);

	for (i = 0; i < value_set->n_values; i++) {
		if (value_equal (g_value_array_get_nth (value_set, i), value)) {
			/* no change, value already in set */
			return FALSE;
		}
	}

	g_value_array_append (value_set, value);

	return TRUE;
}

static gboolean
value_set_remove_value (GValueArray *value_set,
                        GValue      *value)
{
	gint i;

	g_return_val_if_fail (G_VALUE_TYPE (value), FALSE);

	for (i = 0; i < value_set->n_values; i++) {
		if (value_equal (g_value_array_get_nth (value_set, i), value)) {
			/* value found, remove from set */

			g_value_array_remove (value_set, i);

			return TRUE;
		}
	}

	/* no change, value not found */
	return FALSE;
}

static gboolean
check_property_domain (TrackerProperty *property)
{
	gint type_index;

	for (type_index = 0; type_index < resource_buffer->types->len; type_index++) {
		if (tracker_property_get_domain (property) == g_ptr_array_index (resource_buffer->types, type_index)) {
			return TRUE;
		}
	}
	return FALSE;
}

static GValueArray *
get_property_values (TrackerProperty *property)
{
	gboolean            multiple_values;
	GValueArray        *old_values;

	multiple_values = tracker_property_get_multiple_values (property);

	old_values = g_value_array_new (multiple_values ? 4 : 1);
	g_hash_table_insert (resource_buffer->predicates, g_object_ref (property), old_values);

	if (!resource_buffer->create) {
		TrackerDBInterface *iface;
		TrackerDBStatement *stmt;
		TrackerDBResultSet *result_set = NULL;
		const gchar        *table_name;
		const gchar        *field_name;
		GError             *error = NULL;

		table_name = tracker_property_get_table_name (property);
		field_name = tracker_property_get_name (property);

		iface = tracker_db_manager_get_db_interface ();

		stmt = tracker_db_interface_create_statement (iface, &error, "SELECT \"%s\" FROM \"%s\" WHERE ID = ?", field_name, table_name);

		if (stmt) {
			tracker_db_statement_bind_int (stmt, 0, resource_buffer->id);
			result_set = tracker_db_statement_execute (stmt, &error);
			g_object_unref (stmt);
		}

		if (error) {
			g_warning ("Could not get property values: %s\n", error->message);
			g_error_free (error);
		}

		/* We use a result_set instead of a cursor here because it's
		 * possible that otherwise the cursor would remain open during
		 * the call from delete_resource_description. In future we want
		 * to allow having the same query open on multiple cursors,
		 * right now we don't support this. Which is why this workaround */

		if (result_set) {
			gboolean valid = TRUE;

			while (valid) {
				GValue gvalue = { 0 };
				_tracker_db_result_set_get_value (result_set, 0, &gvalue);
				if (G_VALUE_TYPE (&gvalue)) {
					if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME) {
						gint time;

						time = g_value_get_int (&gvalue);
						g_value_unset (&gvalue);
						g_value_init (&gvalue, TRACKER_TYPE_DATE_TIME);
						/* UTC offset is irrelevant for comparison */
						tracker_date_time_set (&gvalue, time, 0);
					}
					g_value_array_append (old_values, &gvalue);
					g_value_unset (&gvalue);
				}
				valid = tracker_db_result_set_iter_next (result_set);
			}
			g_object_unref (result_set);
		}
	}

	return old_values;
}

static GValueArray *
get_old_property_values (TrackerProperty  *property,
                         GError          **error)
{
	gboolean            fts;
	TrackerProperty   **properties, *prop;
	GValueArray        *old_values;

	fts = tracker_property_get_fulltext_indexed (property);

	/* read existing property values */
	old_values = g_hash_table_lookup (resource_buffer->predicates, property);
	if (old_values == NULL) {
		if (!check_property_domain (property)) {
			g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_CONSTRAINT,
			             "Subject `%s' is not in domain `%s' of property `%s'",
			             resource_buffer->subject,
			             tracker_class_get_name (tracker_property_get_domain (property)),
			             tracker_property_get_name (property));
			return NULL;
		}

		if (fts && !resource_buffer->fts_updated && !resource_buffer->create) {
			guint i, n_props;

			/* first fulltext indexed property to be modified
			 * retrieve values of all fulltext indexed properties
			 */
			tracker_fts_update_init (resource_buffer->id);

			properties = tracker_ontologies_get_properties (&n_props);

			for (i = 0; i < n_props; i++) {
				prop = properties[i];

				if (tracker_property_get_fulltext_indexed (prop)
				    && check_property_domain (prop)) {
					gint i;

					old_values = get_property_values (prop);

					/* delete old fts entries */
					for (i = 0; i < old_values->n_values; i++) {
						tracker_fts_update_text (resource_buffer->id, -1,
						                         g_value_get_string (g_value_array_get_nth (old_values, i)),
									 !tracker_property_get_fulltext_no_limit (prop));
					}
				}
			}

			update_buffer.fts_ever_updated = TRUE;

			old_values = g_hash_table_lookup (resource_buffer->predicates, property);
		} else {
			old_values = get_property_values (property);
		}

		if (fts) {
			resource_buffer->fts_updated = TRUE;
		}
	}

	return old_values;
}

static void
string_to_gvalue (const gchar         *value,
                  TrackerPropertyType  type,
                  GValue              *gvalue,
                  GError             **error)
{
	gint object_id;

	switch (type) {
	case TRACKER_PROPERTY_TYPE_STRING:
		g_value_init (gvalue, G_TYPE_STRING);
		g_value_set_string (gvalue, value);
		break;
	case TRACKER_PROPERTY_TYPE_INTEGER:
		g_value_init (gvalue, G_TYPE_INT);
		g_value_set_int (gvalue, atoi (value));
		break;
	case TRACKER_PROPERTY_TYPE_BOOLEAN:
		/* use G_TYPE_INT to be compatible with value stored in DB
		   (important for value_equal function) */
		g_value_init (gvalue, G_TYPE_INT);
		g_value_set_int (gvalue, strcmp (value, "true") == 0);
		break;
	case TRACKER_PROPERTY_TYPE_DOUBLE:
		g_value_init (gvalue, G_TYPE_DOUBLE);
		g_value_set_double (gvalue, atof (value));
		break;
	case TRACKER_PROPERTY_TYPE_DATE:
	case TRACKER_PROPERTY_TYPE_DATETIME:
		g_value_init (gvalue, TRACKER_TYPE_DATE_TIME);
		tracker_date_time_set_from_string (gvalue, value, error);
		break;
	case TRACKER_PROPERTY_TYPE_RESOURCE:
		object_id = ensure_resource_id (value, NULL);
		g_value_init (gvalue, G_TYPE_INT);
		g_value_set_int (gvalue, object_id);
		break;
	default:
		g_warn_if_reached ();
		break;
	}
}

static gboolean
cache_set_metadata_decomposed (TrackerProperty  *property,
                               const gchar      *value,
                               gint              value_id,
                               const gchar      *graph,
                               gint              graph_id,
                               GError          **error)
{
	gboolean            multiple_values, fts;
	const gchar        *table_name;
	const gchar        *field_name;
	TrackerProperty   **super_properties;
	GValue              gvalue = { 0 };
	GValueArray        *old_values;
	GError             *new_error = NULL;
	gboolean            change = FALSE;

	/* also insert super property values */
	super_properties = tracker_property_get_super_properties (property);
	while (*super_properties) {
		change |= cache_set_metadata_decomposed (*super_properties, value, value_id,
		                               graph, graph_id, &new_error);
		if (new_error) {
			g_propagate_error (error, new_error);
			return FALSE;
		}
		super_properties++;
	}

	multiple_values = tracker_property_get_multiple_values (property);
	table_name = tracker_property_get_table_name (property);
	field_name = tracker_property_get_name (property);

	fts = tracker_property_get_fulltext_indexed (property);

	/* read existing property values */
	old_values = get_old_property_values (property, &new_error);
	if (new_error) {
		g_propagate_error (error, new_error);
		return FALSE;
	}

	if (value) {
		string_to_gvalue (value, tracker_property_get_data_type (property), &gvalue, &new_error);
		if (new_error) {
			g_propagate_error (error, new_error);
			return FALSE;
		}
	} else {
		g_value_init (&gvalue, G_TYPE_INT);
		g_value_set_int (&gvalue, value_id);
	}

	if (!value_set_add_value (old_values, &gvalue)) {
		/* value already inserted */
		g_value_unset (&gvalue);
	} else if (!multiple_values && old_values->n_values > 1) {
		/* trying to add second value to single valued property */
		GValue old_value = { 0 };
		GValue new_value = { 0 };
		const gchar *old_value_str = NULL;
		const gchar *new_value_str = NULL;

		g_value_init (&old_value, G_TYPE_STRING);
		g_value_init (&new_value, G_TYPE_STRING);

		/* Get both old and new values as strings letting glib do
		 * whatever transformation needed */
		if (g_value_transform (g_value_array_get_nth (old_values, 0), &old_value)) {
			old_value_str = g_value_get_string (&old_value);
		}
		if (g_value_transform (g_value_array_get_nth (old_values, 1), &new_value)) {
			new_value_str = g_value_get_string (&new_value);
		}

		g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_CONSTRAINT,
		             "Unable to insert multiple values for subject `%s' and single valued property `%s' "
		             "(old_value: '%s', new value: '%s')",
		             resource_buffer->subject,
		             field_name,
		             old_value_str ? old_value_str : "<untransformable>",
		             new_value_str ? new_value_str : "<untransformable>");

		g_value_unset (&old_value);
		g_value_unset (&new_value);
		g_value_unset (&gvalue);

	} else {
		cache_insert_value (table_name, field_name, &gvalue,
		                    graph != NULL ? ensure_resource_id (graph, NULL) : graph_id,
		                    multiple_values, fts,
		                    tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME);

		change = TRUE;
	}

	return change;
}

static gboolean
delete_metadata_decomposed (TrackerProperty  *property,
                            const gchar      *value,
                            gint              value_id,
                            GError          **error)
{
	gboolean            multiple_values, fts;
	const gchar        *table_name;
	const gchar        *field_name;
	TrackerProperty   **super_properties;
	GValue gvalue = { 0 };
	GValueArray        *old_values;
	GError             *new_error = NULL;
	gboolean            change = FALSE;

	multiple_values = tracker_property_get_multiple_values (property);
	table_name = tracker_property_get_table_name (property);
	field_name = tracker_property_get_name (property);

	fts = tracker_property_get_fulltext_indexed (property);

	/* read existing property values */
	old_values = get_old_property_values (property, &new_error);
	if (new_error) {
		g_propagate_error (error, new_error);
		return FALSE;
	}

	if (value) {
		string_to_gvalue (value, tracker_property_get_data_type (property), &gvalue, &new_error);
		if (new_error) {
			g_propagate_error (error, new_error);
			return FALSE;
		}
	} else {
		g_value_init (&gvalue, G_TYPE_INT);
		g_value_set_int (&gvalue, value_id);
	}

	if (!value_set_remove_value (old_values, &gvalue)) {
		/* value not found */
		g_value_unset (&gvalue);
	} else {
		cache_delete_value (table_name, field_name, &gvalue, multiple_values, fts,
		                    tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_DATETIME);

		change = TRUE;
	}

	/* also delete super property values */
	super_properties = tracker_property_get_super_properties (property);
	while (*super_properties) {
		change |= delete_metadata_decomposed (*super_properties, value, value_id, error);
		super_properties++;
	}

	return change;
}

static void
cache_delete_resource_type (TrackerClass *class,
                            const gchar  *graph,
                            gint          graph_id)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBResultSet *result_set = NULL;
	TrackerProperty   **properties, *prop;
	gboolean            found;
	gint                i;
	guint               p, n_props;
	GError             *error = NULL;

	iface = tracker_db_manager_get_db_interface ();

	found = FALSE;
	for (i = 0; i < resource_buffer->types->len; i++) {
		if (g_ptr_array_index (resource_buffer->types, i) == class) {
			found = TRUE;
		}
	}

	if (!found) {
		/* type not found, nothing to do */
		return;
	}

	/* retrieve all subclasses we need to remove from the subject
	 * before we can remove the class specified as object of the statement */
	stmt = tracker_db_interface_create_statement (iface, &error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"rdfs:Class_rdfs:subClassOf\".ID) "
	                                              "FROM \"rdfs:Resource_rdf:type\" INNER JOIN \"rdfs:Class_rdfs:subClassOf\" ON (\"rdf:type\" = \"rdfs:Class_rdfs:subClassOf\".ID) "
	                                              "WHERE \"rdfs:Resource_rdf:type\".ID = ? AND \"rdfs:subClassOf\" = (SELECT ID FROM Resource WHERE Uri = ?)");

	if (stmt) {
		tracker_db_statement_bind_int (stmt, 0, resource_buffer->id);
		tracker_db_statement_bind_text (stmt, 1, tracker_class_get_uri (class));
		result_set = tracker_db_statement_execute (stmt, &error);
		g_object_unref (stmt);
	}

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	if (result_set) {
		do {
			gchar *class_uri;

			tracker_db_result_set_get (result_set, 0, &class_uri, -1);
			cache_delete_resource_type (tracker_ontologies_get_class_by_uri (class_uri),
			                            graph, graph_id);
			g_free (class_uri);
		} while (tracker_db_result_set_iter_next (result_set));

		g_object_unref (result_set);
	}

	/* delete all property values */

	properties = tracker_ontologies_get_properties (&n_props);

	for (p = 0; p < n_props; p++) {
		gboolean            multiple_values, fts;
		const gchar        *table_name;
		const gchar        *field_name;
		GValueArray        *old_values;
		gint                i;

		prop = properties[p];

		if (tracker_property_get_domain (prop) != class) {
			continue;
		}

		multiple_values = tracker_property_get_multiple_values (prop);
		table_name = tracker_property_get_table_name (prop);
		field_name = tracker_property_get_name (prop);

		fts = tracker_property_get_fulltext_indexed (prop);

		old_values = get_old_property_values (prop, NULL);

		for (i = old_values->n_values - 1; i >= 0 ; i--) {
			GValue *old_gvalue;
			GValue  gvalue = { 0 };

			old_gvalue = g_value_array_get_nth (old_values, i);
			g_value_init (&gvalue, G_VALUE_TYPE (old_gvalue));
			g_value_copy (old_gvalue, &gvalue);

			value_set_remove_value (old_values, &gvalue);
			cache_delete_value (table_name, field_name, &gvalue, multiple_values, fts,
			                    tracker_property_get_data_type (prop) == TRACKER_PROPERTY_TYPE_DATETIME);
		}
	}

	cache_delete_row (class);

	if (!in_journal_replay && delete_callbacks) {
		guint n;
		for (n = 0; n < delete_callbacks->len; n++) {
			TrackerStatementDelegate *delegate;

			delegate = g_ptr_array_index (delete_callbacks, n);
			delegate->callback (graph, resource_buffer->subject,
			                    RDF_PREFIX "type", tracker_class_get_uri (class),
			                    resource_buffer->types, delegate->user_data);
		}
	}

}

static void
resource_buffer_switch (const gchar *graph,
                        gint         graph_id,
                        const gchar *subject,
                        gint         subject_id)
{
	if (in_journal_replay) {
		/* journal replay only provides subject id
		   resource_buffer->subject is only used in error messages and callbacks
		   both should never occur when in journal replay */
		if (resource_buffer == NULL || resource_buffer->id != subject_id) {
			/* switch subject */
			resource_buffer = g_hash_table_lookup (update_buffer.resources_by_id, GINT_TO_POINTER (subject_id));
		}
	} else {
		if (resource_buffer == NULL || strcmp (resource_buffer->subject, subject) != 0) {
			/* switch subject */
			resource_buffer = g_hash_table_lookup (update_buffer.resources, subject);
		}
	}

	if (resource_buffer == NULL) {
		GValue gvalue = { 0 };
		gchar *subject_dup = NULL;

		/* subject not yet in cache, retrieve or create ID */
		resource_buffer = g_slice_new0 (TrackerDataUpdateBufferResource);
		if (subject != NULL) {
			subject_dup = g_strdup (subject);
			resource_buffer->subject = subject_dup;
		}
		if (subject_id > 0) {
			resource_buffer->id = subject_id;
		} else {
			resource_buffer->id = ensure_resource_id (resource_buffer->subject, &resource_buffer->create);
		}
		resource_buffer->fts_updated = FALSE;
		if (resource_buffer->create) {
			resource_buffer->types = g_ptr_array_new ();
		} else {
			resource_buffer->types = tracker_data_query_rdf_type (resource_buffer->id);
		}
		resource_buffer->predicates = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, (GDestroyNotify) g_value_array_free);
		resource_buffer->tables = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) cache_table_free);

		if (in_journal_replay) {
			g_hash_table_insert (update_buffer.resources_by_id, GINT_TO_POINTER (subject_id), resource_buffer);
		} else {
			g_hash_table_insert (update_buffer.resources, subject_dup, resource_buffer);

			if (graph != NULL) {
				graph_id = ensure_resource_id (graph, NULL);
			}
		}

		g_value_init (&gvalue, G_TYPE_INT);
		g_value_set_int (&gvalue, tracker_data_update_get_next_modseq ());
		cache_insert_value ("rdfs:Resource", "tracker:modified", &gvalue,
		                    0,
		                    FALSE, FALSE, FALSE);
	}
}

void
tracker_data_delete_statement (const gchar  *graph,
                               const gchar  *subject,
                               const gchar  *predicate,
                               const gchar  *object,
                               GError      **error)
{
	TrackerClass       *class;
	TrackerProperty    *field;
	gint                subject_id;
	gboolean change = FALSE;

	g_return_if_fail (subject != NULL);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);
	g_return_if_fail (in_transaction);

	subject_id = query_resource_id (subject);

	if (subject_id == 0) {
		/* subject not in database */
		return;
	}

	resource_buffer_switch (graph, 0, subject, subject_id);

	if (object && g_strcmp0 (predicate, RDF_PREFIX "type") == 0) {
		class = tracker_ontologies_get_class_by_uri (object);
		if (class != NULL) {
			if (!in_journal_replay) {
				tracker_db_journal_append_delete_statement_id (
					(graph != NULL ? query_resource_id (graph) : 0),
					resource_buffer->id,
					tracker_data_query_resource_id (predicate),
					query_resource_id (object));
			}

			cache_delete_resource_type (class, graph, 0);
		} else {
			g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_UNKNOWN_CLASS,
			             "Class '%s' not found in the ontology", object);
		}
	} else {
		field = tracker_ontologies_get_property_by_uri (predicate);
		if (field != NULL) {
			gint id;

			change = delete_metadata_decomposed (field, object, 0, error);

			id = tracker_property_get_id (field);
			if (!in_journal_replay && change) {
				if (tracker_property_get_data_type (field) == TRACKER_PROPERTY_TYPE_RESOURCE) {
					tracker_db_journal_append_delete_statement_id (
						(graph != NULL ? query_resource_id (graph) : 0),
						resource_buffer->id,
						(id != 0) ? id : tracker_data_query_resource_id (predicate),
						query_resource_id (object));
				} else {
					tracker_db_journal_append_delete_statement (
						(graph != NULL ? query_resource_id (graph) : 0),
						resource_buffer->id,
						(id != 0) ? id : tracker_data_query_resource_id (predicate),
						object);
				}
			}
		} else {
			g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_UNKNOWN_PROPERTY,
			             "Property '%s' not found in the ontology", predicate);
		}

		if (delete_callbacks && change) {
			guint n;
			for (n = 0; n < delete_callbacks->len; n++) {
				TrackerStatementDelegate *delegate;

				delegate = g_ptr_array_index (delete_callbacks, n);
				delegate->callback (graph, subject, predicate, object,
				                    resource_buffer->types,
				                    delegate->user_data);
			}
		}
	}
}

static gboolean
tracker_data_insert_statement_common (const gchar            *graph,
                                      const gchar            *subject,
                                      const gchar            *predicate,
                                      const gchar            *object,
                                      GError                **error)
{
	if (g_str_has_prefix (subject, ":")) {
		/* blank node definition
		   pile up statements until the end of the blank node */
		gchar *value;
		GError *actual_error = NULL;

		if (blank_buffer.subject != NULL) {
			/* active subject in buffer */
			if (strcmp (blank_buffer.subject, subject) != 0) {
				/* subject changed, need to flush buffer */
				tracker_data_blank_buffer_flush (&actual_error);

				if (actual_error) {
					g_propagate_error (error, actual_error);
					return FALSE;
				}
			}
		}

		if (blank_buffer.subject == NULL) {
			blank_buffer.subject = g_strdup (subject);
			if (blank_buffer.graphs == NULL) {
				blank_buffer.graphs = g_array_sized_new (FALSE, FALSE, sizeof (char*), 4);
				blank_buffer.predicates = g_array_sized_new (FALSE, FALSE, sizeof (char*), 4);
				blank_buffer.objects = g_array_sized_new (FALSE, FALSE, sizeof (char*), 4);
			}
		}

		value = g_strdup (graph);
		g_array_append_val (blank_buffer.graphs, value);
		value = g_strdup (predicate);
		g_array_append_val (blank_buffer.predicates, value);
		value = g_strdup (object);
		g_array_append_val (blank_buffer.objects, value);

		return FALSE;
	}

	resource_buffer_switch (graph, 0, subject, 0);

	return TRUE;
}

void
tracker_data_insert_statement (const gchar            *graph,
                               const gchar            *subject,
                               const gchar            *predicate,
                               const gchar            *object,
                               GError                **error)
{
	TrackerProperty *property;

	g_return_if_fail (subject != NULL);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);
	g_return_if_fail (in_transaction);

	property = tracker_ontologies_get_property_by_uri (predicate);
	if (property != NULL) {
		if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			tracker_data_insert_statement_with_uri (graph, subject, predicate, object, error);
		} else {
			tracker_data_insert_statement_with_string (graph, subject, predicate, object, error);
		}
	} else {
		g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_UNKNOWN_PROPERTY,
		             "Property '%s' not found in the ontology", predicate);
	}
}

void
tracker_data_insert_statement_with_uri (const gchar            *graph,
                                        const gchar            *subject,
                                        const gchar            *predicate,
                                        const gchar            *object,
                                        GError                **error)
{
	GError          *actual_error = NULL;
	TrackerClass    *class;
	TrackerProperty *property;
	gint             prop_id = 0;
	gboolean change = FALSE;

	g_return_if_fail (subject != NULL);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);
	g_return_if_fail (in_transaction);

	property = tracker_ontologies_get_property_by_uri (predicate);
	if (property == NULL) {
		g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_UNKNOWN_PROPERTY,
		             "Property '%s' not found in the ontology", predicate);
		return;
	} else {
		if (tracker_property_get_data_type (property) != TRACKER_PROPERTY_TYPE_RESOURCE) {
			g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_INVALID_TYPE,
			             "Property '%s' does not accept URIs", predicate);
			return;
		}
		prop_id = tracker_property_get_id (property);
	}

	/* subjects and objects starting with `:' are anonymous blank nodes */
	if (g_str_has_prefix (object, ":")) {
		/* anonymous blank node used as object in a statement */
		const gchar *blank_uri;

		if (blank_buffer.subject != NULL) {
			if (strcmp (blank_buffer.subject, object) == 0) {
				/* object still in blank buffer, need to flush buffer */
				tracker_data_blank_buffer_flush (&actual_error);

				if (actual_error) {
					g_propagate_error (error, actual_error);
					return;
				}
			}
		}

		blank_uri = g_hash_table_lookup (blank_buffer.table, object);

		if (blank_uri != NULL) {
			/* now insert statement referring to blank node */
			tracker_data_insert_statement (graph, subject, predicate, blank_uri, &actual_error);

			g_hash_table_remove (blank_buffer.table, object);

			if (actual_error) {
				g_propagate_error (error, actual_error);
				return;
			}

			return;
		} else {
			g_critical ("Blank node '%s' not found", object);
		}
	}

	if (!tracker_data_insert_statement_common (graph, subject, predicate, object, &actual_error)) {
		if (actual_error) {
			g_propagate_error (error, actual_error);
			return;
		}

		return;
	}

	if (strcmp (predicate, RDF_PREFIX "type") == 0) {
		/* handle rdf:type statements specially to
		   cope with inference and insert blank rows */
		class = tracker_ontologies_get_class_by_uri (object);
		if (class != NULL) {
			cache_create_service_decomposed (class, graph, 0);
		} else {
			g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_UNKNOWN_CLASS,
			             "Class '%s' not found in the ontology", object);
			return;
		}

		change = TRUE;
	} else {
		/* add value to metadata database */
		change = cache_set_metadata_decomposed (property, object, 0, graph, 0, &actual_error);
		if (actual_error) {
			g_propagate_error (error, actual_error);
			return;
		}

		if (insert_callbacks && change) {
			guint n;
			for (n = 0; n < insert_callbacks->len; n++) {
				TrackerStatementDelegate *delegate;

				delegate = g_ptr_array_index (insert_callbacks, n);
				delegate->callback (graph, subject, predicate, object,
				                    resource_buffer->types,
				                    delegate->user_data);
			}
		}
	}

	if (!in_journal_replay && change) {
		tracker_db_journal_append_insert_statement_id (
			(graph != NULL ? query_resource_id (graph) : 0),
			resource_buffer->id,
			(prop_id != 0) ? prop_id : tracker_data_query_resource_id (predicate),
			query_resource_id (object));
	}
}

void
tracker_data_insert_statement_with_string (const gchar            *graph,
                                           const gchar            *subject,
                                           const gchar            *predicate,
                                           const gchar            *object,
                                           GError                **error)
{
	GError          *actual_error = NULL;
	TrackerProperty *property;
	gint             id = 0;
	gboolean change;


	g_return_if_fail (subject != NULL);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);
	g_return_if_fail (in_transaction);

	property = tracker_ontologies_get_property_by_uri (predicate);
	if (property == NULL) {
		g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_UNKNOWN_PROPERTY,
		             "Property '%s' not found in the ontology", predicate);
		return;
	} else {
		if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_INVALID_TYPE,
			             "Property '%s' only accepts URIs", predicate);
			return;
		}
		id = tracker_property_get_id (property);
	}

	if (!tracker_data_insert_statement_common (graph, subject, predicate, object, &actual_error)) {
		if (actual_error) {
			g_propagate_error (error, actual_error);
			return;
		}

		return;
	}

	/* add value to metadata database */
	change = cache_set_metadata_decomposed (property, object, 0, graph, 0, &actual_error);
	if (actual_error) {
		g_propagate_error (error, actual_error);
		return;
	}

	if (insert_callbacks && change) {
		guint n;
		for (n = 0; n < insert_callbacks->len; n++) {
			TrackerStatementDelegate *delegate;

			delegate = g_ptr_array_index (insert_callbacks, n);
			delegate->callback (graph, subject, predicate, object,
			                    resource_buffer->types,
			                    delegate->user_data);
		}
	}

	if (!in_journal_replay && change) {
		tracker_db_journal_append_insert_statement (
			(graph != NULL ? query_resource_id (graph) : 0),
			resource_buffer->id,
			(id != 0) ? id : tracker_data_query_resource_id (predicate),
			object);
	}
}

void
tracker_data_begin_db_transaction (void)
{
	TrackerDBInterface *iface;

	g_return_if_fail (!in_transaction);

	resource_time = time (NULL);

	if (update_buffer.resource_cache == NULL) {
		update_buffer.resource_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		/* used for normal transactions */
		update_buffer.resources = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) resource_buffer_free);
		/* used for journal replay */
		update_buffer.resources_by_id = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) resource_buffer_free);
	}

	resource_buffer = NULL;
	if (blank_buffer.table == NULL) {
		blank_buffer.table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	}

	iface = tracker_db_manager_get_db_interface ();

	tracker_db_interface_start_transaction (iface);

	in_transaction = TRUE;
}

void
tracker_data_begin_db_transaction_for_replay (time_t time)
{
	in_journal_replay = TRUE;
	tracker_data_begin_db_transaction ();
	resource_time = time;
}

void
tracker_data_commit_db_transaction (void)
{
	TrackerDBInterface *iface;

	g_return_if_fail (in_transaction);

	in_transaction = FALSE;

	tracker_data_update_buffer_flush (NULL);

	if (update_buffer.fts_ever_updated) {
		tracker_fts_update_commit ();
		update_buffer.fts_ever_updated = FALSE;
	}

	if (update_buffer.class_counts) {
		/* successful transaction, no need to rollback class counts,
		   so remove them */
		g_hash_table_remove_all (update_buffer.class_counts);
	}

	iface = tracker_db_manager_get_db_interface ();

	tracker_db_interface_end_db_transaction (iface);

	g_hash_table_remove_all (update_buffer.resources);
	g_hash_table_remove_all (update_buffer.resources_by_id);
	g_hash_table_remove_all (update_buffer.resource_cache);

	in_journal_replay = FALSE;
}

void
tracker_data_notify_db_transaction (void)
{
	if (commit_callbacks) {
		guint n;
		for (n = 0; n < commit_callbacks->len; n++) {
			TrackerCommitDelegate *delegate;
			delegate = g_ptr_array_index (commit_callbacks, n);
			delegate->callback (delegate->user_data);
		}
	}
}


static void
format_sql_value_as_string (GString         *sql,
                            TrackerProperty *property)
{
	switch (tracker_property_get_data_type (property)) {
	case TRACKER_PROPERTY_TYPE_RESOURCE:
		g_string_append_printf (sql, "(SELECT Uri FROM Resource WHERE ID = \"%s\")", tracker_property_get_name (property));
		break;
	case TRACKER_PROPERTY_TYPE_INTEGER:
	case TRACKER_PROPERTY_TYPE_DOUBLE:
		g_string_append_printf (sql, "CAST (\"%s\" AS TEXT)", tracker_property_get_name (property));
		break;
	case TRACKER_PROPERTY_TYPE_BOOLEAN:
		g_string_append_printf (sql, "CASE \"%s\" WHEN 1 THEN 'true' WHEN 0 THEN 'false' ELSE NULL END", tracker_property_get_name (property));
		break;
	case TRACKER_PROPERTY_TYPE_DATETIME:
		g_string_append_printf (sql, "strftime (\"%%Y-%%m-%%dT%%H:%%M:%%SZ\", \"%s\", \"unixepoch\")", tracker_property_get_name (property));
		break;
	default:
		g_string_append_printf (sql, "\"%s\"", tracker_property_get_name (property));
		break;
	}
}

/**
 * Removes the description of a resource (embedded metadata), but keeps
 * annotations (non-embedded/user metadata) stored about the resource.
 */
void
tracker_data_delete_resource_description (const gchar *graph,
                                          const gchar *url,
                                          GError **error)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBResultSet *result_set = NULL, *single_result, *multi_result = NULL;
	TrackerClass       *class;
	gchar              *urn;
	GString            *sql;
	TrackerProperty   **properties, *property;
	int                 i;
	gboolean            first, bail_out = FALSE;
	gint                resource_id;
	guint               p, n_props;
	GError             *actual_error = NULL;

	/* We use result_sets instead of cursors here because it's possible
	 * that otherwise the query of the outer cursor would be reused by the
	 * cursors of the inner queries. */

	iface = tracker_db_manager_get_db_interface ();

	/* DROP GRAPH <url> - url here is nie:url */

	stmt = tracker_db_interface_create_statement (iface, &actual_error,
	                                              "SELECT ID, (SELECT Uri FROM Resource WHERE ID = \"nie:DataObject\".ID) FROM \"nie:DataObject\" WHERE \"nie:DataObject\".\"nie:url\" = ?");

	if (stmt) {
		tracker_db_statement_bind_text (stmt, 0, url);
		result_set = tracker_db_statement_execute (stmt, &actual_error);
		g_object_unref (stmt);
	}

	if (actual_error) {
		g_propagate_error (error, actual_error);
		return;
	}

	if (result_set) {
		tracker_db_result_set_get (result_set, 0, &resource_id, -1);
		tracker_db_result_set_get (result_set, 1, &urn, -1);
		g_object_unref (result_set);
	} else {
		/* For fallback to the old behaviour, we could do this here:
		 * resource_id = tracker_data_query_resource_id (url); */
		return;
	}

	properties = tracker_ontologies_get_properties (&n_props);

	stmt = tracker_db_interface_create_statement (iface, &actual_error,
	                                              "SELECT (SELECT Uri FROM Resource WHERE ID = \"rdf:type\") FROM \"rdfs:Resource_rdf:type\" WHERE ID = ?");

	if (stmt) {
		tracker_db_statement_bind_int (stmt, 0, resource_id);
		result_set = tracker_db_statement_execute (stmt, &actual_error);
		g_object_unref (stmt);
	}

	if (actual_error) {
		g_propagate_error (error, actual_error);
		return;
	}

	if (result_set) {
		do {
			gchar *class_uri;

			tracker_db_result_set_get (result_set, 0, &class_uri, -1);

			class = tracker_ontologies_get_class_by_uri (class_uri);

			if (class == NULL) {
				g_warning ("Class '%s' not found in the ontology", class_uri);
				g_free (class_uri);
				continue;
			}
			g_free (class_uri);

			/* retrieve single value properties for current class */

			sql = g_string_new ("SELECT ");

			first = TRUE;

			for (p = 0; p < n_props; p++) {
				property = properties[p];

				if (tracker_property_get_domain (property) == class) {
					if (!tracker_property_get_embedded (property)) {
						continue;
					}

					if (!tracker_property_get_multiple_values (property)) {
						if (!first) {
							g_string_append (sql, ", ");
						}
						first = FALSE;

						format_sql_value_as_string (sql, property);
					}
				}
			}

			single_result = NULL;
			if (!first) {
				g_string_append_printf (sql, " FROM \"%s\" WHERE ID = ?", tracker_class_get_name (class));
				stmt = tracker_db_interface_create_statement (iface, &actual_error, "%s", sql->str);

				if (stmt) {
					tracker_db_statement_bind_int (stmt, 0, resource_id);
					single_result = tracker_db_statement_execute (stmt, &actual_error);
					g_object_unref (stmt);
				}

				if (actual_error) {
					g_propagate_error (error, actual_error);
					bail_out = TRUE;
					break;
				}
			}

			g_string_free (sql, TRUE);

			i = 0;
			for (p = 0; p < n_props; p++) {
				property = properties[p];

				if (tracker_property_get_domain (property) != class) {
					continue;
				}

				if (!tracker_property_get_embedded (property)) {
					continue;
				}

				if (strcmp (tracker_property_get_uri (property), RDF_PREFIX "type") == 0) {
					/* Do not delete rdf:type statements */
					continue;
				}

				if (!tracker_property_get_multiple_values (property)) {
					gchar *value;

					/* single value property, value in single_result_set */

					tracker_db_result_set_get (single_result, i++, &value, -1);

					if (value) {
						tracker_data_delete_statement (graph, urn,
						                               tracker_property_get_uri (property),
						                               value,
						                               &actual_error);
						if (actual_error) {
							g_propagate_error (error, actual_error);
							bail_out = TRUE;
							break;
						}
						g_free (value);
					}

				} else {
					/* multi value property, retrieve values from DB */

					sql = g_string_new ("SELECT ");

					format_sql_value_as_string (sql, property);

					g_string_append_printf (sql,
					                        " FROM \"%s\" WHERE ID = ?",
					                        tracker_property_get_table_name (property));

					stmt = tracker_db_interface_create_statement (iface, &actual_error, "%s", sql->str);

					if (stmt) {
						tracker_db_statement_bind_int (stmt, 0, resource_id);
						multi_result = tracker_db_statement_execute (stmt, NULL);
						g_object_unref (stmt);
					}

					if (actual_error) {
						g_propagate_error (error, actual_error);
						bail_out = TRUE;
						break;
					}

					if (multi_result) {
						do {
							gchar *value;

							tracker_db_result_set_get (multi_result, 0, &value, -1);

							tracker_data_delete_statement (graph, urn,
							                               tracker_property_get_uri (property),
							                               value,
							                               &actual_error);

							g_free (value);

							if (actual_error) {
								g_propagate_error (error, actual_error);
								bail_out = TRUE;
								break;
							}

						} while (tracker_db_result_set_iter_next (multi_result));

						g_object_unref (multi_result);
					}

					g_string_free (sql, TRUE);
				}
			}

			if (!first) {
				g_object_unref (single_result);
			}

		} while (!bail_out && tracker_db_result_set_iter_next (result_set));

		g_object_unref (result_set);
	}

	g_free (urn);
}

void
tracker_data_begin_transaction (GError **error)
{
	TrackerDBInterface *iface;

	if (!tracker_db_manager_has_enough_space ()) {
		g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_NO_SPACE,
			"There is not enough space on the file system for update operations");
		return;
	}

	iface = tracker_db_manager_get_db_interface ();

	resource_time = time (NULL);
	tracker_db_interface_execute_query (iface, NULL, "SAVEPOINT sparql");
	tracker_db_journal_start_transaction (resource_time);
}

void
tracker_data_commit_transaction (GError **error)
{
	TrackerDBInterface *iface;
	GError *actual_error = NULL;

	iface = tracker_db_manager_get_db_interface ();

	tracker_data_update_buffer_flush (&actual_error);
	if (actual_error) {
		g_propagate_error (error, actual_error);
		return;
	}

	tracker_db_journal_commit_db_transaction ();
	resource_time = 0;
	tracker_db_interface_execute_query (iface, NULL, "RELEASE sparql");

	if (update_buffer.class_counts) {
		/* successful transaction, no need to rollback class counts,
		   so remove them */
		g_hash_table_remove_all (update_buffer.class_counts);
	}
}

void
tracker_data_rollback_transaction (void)
{
	TrackerDBInterface *iface;

	iface = tracker_db_manager_get_db_interface ();

	tracker_data_update_buffer_clear ();
	tracker_db_interface_execute_query (iface, NULL, "ROLLBACK TO sparql");
	tracker_db_journal_rollback_transaction ();

	if (rollback_callbacks) {
		guint n;
		for (n = 0; n < rollback_callbacks->len; n++) {
			TrackerCommitDelegate *delegate;
			delegate = g_ptr_array_index (rollback_callbacks, n);
			delegate->callback (delegate->user_data);
		}
	}
}

static GPtrArray *
update_sparql (const gchar  *update,
               gboolean      blank,
               GError      **error)
{
	GError *actual_error = NULL;
	TrackerSparqlQuery *sparql_query;
	GPtrArray *blank_nodes;

	g_return_val_if_fail (update != NULL, NULL);

	tracker_data_begin_transaction (&actual_error);
	if (actual_error) {
		g_propagate_error (error, actual_error);
		return NULL;
	}

	sparql_query = tracker_sparql_query_new_update (update);
	blank_nodes = tracker_sparql_query_execute_update (sparql_query, blank, &actual_error);
	g_object_unref (sparql_query);

	if (actual_error) {
		tracker_data_rollback_transaction ();
		g_propagate_error (error, actual_error);
		return NULL;
	}

	tracker_data_commit_transaction (&actual_error);
	if (actual_error) {
		tracker_data_rollback_transaction ();
		g_propagate_error (error, actual_error);
		return NULL;
	}

	return blank_nodes;
}

void
tracker_data_update_sparql (const gchar  *update,
                            GError      **error)
{
	update_sparql (update, FALSE, error);
}

GPtrArray *
tracker_data_update_sparql_blank (const gchar  *update,
                                  GError      **error)
{
	return update_sparql (update, TRUE, error);
}

void
tracker_data_sync (void)
{
	tracker_db_journal_fsync ();
}

static void
free_queued_statement (QueuedStatement *queued)
{
	g_free (queued->subject);
	g_free (queued->predicate);
	g_free (queued->object);
	g_free (queued->graph);
	g_free (queued);
}

static GList*
queue_statement (GList *queue,
                 const gchar *graph,
                 const gchar *subject,
                 const gchar *predicate,
                 const gchar *object,
                 gboolean     is_uri)
{
	QueuedStatement *queued = g_new (QueuedStatement, 1);

	queued->subject = g_strdup (subject);
	queued->predicate = g_strdup (predicate);
	queued->object = g_strdup (object);
	queued->is_uri = is_uri;
	queued->graph = graph ? g_strdup (graph) : NULL;

	queue = g_list_append (queue, queued);

	return queue;
}

static void
ontology_transaction_end (GList *ontology_queue,
                          GPtrArray *seen_classes,
                          GPtrArray *seen_properties)
{
	GList *l;
	const gchar *ontology_uri = NULL;

	/* Perform ALTER-TABLE and CREATE-TABLE calls for all that are is_new */
	tracker_data_ontology_import_into_db (TRUE);

	tracker_data_ontology_process_changes (seen_classes, seen_properties);

	for (l = ontology_queue; l; l = l->next) {
		QueuedStatement *queued = ontology_queue->data;

		if (g_strcmp0 (queued->predicate, RDF_TYPE) == 0) {
			if (g_strcmp0 (queued->object, TRACKER_PREFIX "Ontology") == 0) {
				ontology_uri = queued->subject;
			}
		}

		/* store ontology in database */
		tracker_data_ontology_process_statement (queued->graph,
		                                         queued->subject,
		                                         queued->predicate,
		                                         queued->object,
		                                         queued->is_uri,
		                                         TRUE, TRUE);

	}

	/* Update the nao:lastModified in the database */
	if (ontology_uri) {
		TrackerOntology *ontology;
		ontology = tracker_ontologies_get_ontology_by_uri (ontology_uri);
		if (ontology) {
			gint last_mod = 0;
			TrackerDBInterface *iface;
			TrackerDBStatement *stmt;
			GError *error = NULL;

			iface = tracker_db_manager_get_db_interface ();

			/* We can't do better than this cast, it's stored as an int in the
			 * db. See tracker-data-manager.c for more info. */
			last_mod = (gint) tracker_ontology_get_last_modified (ontology);

			stmt = tracker_db_interface_create_statement (iface, &error,
			        "UPDATE \"rdfs:Resource\" SET \"nao:lastModified\"= ? "
			        "WHERE \"rdfs:Resource\".ID = "
			        "(SELECT Resource.ID FROM Resource INNER JOIN \"rdfs:Resource\" "
			        "ON \"rdfs:Resource\".ID = Resource.ID WHERE "
			        "Resource.Uri = ?)");

			if (stmt) {
				tracker_db_statement_bind_int (stmt, 0, last_mod);
				tracker_db_statement_bind_text (stmt, 1, ontology_uri);
				tracker_db_statement_execute (stmt, NULL);
			} else {
				g_warning ("%s", error->message);
				g_error_free (error);
			}
		}
	}

	/* Reset the is_new flag for all classes and properties */
	tracker_data_ontology_import_finished ();
}

static GList*
ontology_statement_insert (GList       *ontology_queue,
                           gint         graph_id,
                           gint         subject_id,
                           gint         predicate_id,
                           const gchar *object,
                           GHashTable  *classes,
                           GHashTable  *properties,
                           GHashTable  *id_uri_map,
                           gboolean     is_uri,
                           GPtrArray   *seen_classes,
                           GPtrArray   *seen_properties)
{
	const gchar *graph, *subject, *predicate;

	if (graph_id > 0) {
		graph = g_hash_table_lookup (id_uri_map, GINT_TO_POINTER (graph_id));
	} else {
		graph = NULL;
	}

	subject = g_hash_table_lookup (id_uri_map, GINT_TO_POINTER (subject_id));
	predicate = g_hash_table_lookup (id_uri_map, GINT_TO_POINTER (predicate_id));

	/* load ontology from journal into memory, set all new's is_new to TRUE */
	tracker_data_ontology_load_statement ("journal", subject_id, subject, predicate,
	                                      object, NULL, TRUE, classes, properties,
	                                      seen_classes, seen_properties);

	/* Queue the statement for processing after ALTER in ontology_transaction_end */
	ontology_queue = queue_statement (ontology_queue, graph, subject, predicate, object, is_uri);

	return ontology_queue;
}

void
tracker_data_replay_journal (GHashTable          *classes,
                             GHashTable          *properties,
                             GHashTable          *id_uri_map,
                             TrackerBusyCallback  busy_callback,
                             gpointer             busy_user_data,
                             const gchar         *busy_status)
{
	GError *journal_error = NULL;
	TrackerProperty *rdf_type = NULL;
	gint last_operation_type = 0;
	gboolean in_ontology = FALSE;
	GList *ontology_queue = NULL;
	GPtrArray *seen_classes = NULL;
	GPtrArray *seen_properties = NULL;

	tracker_data_begin_db_transaction_for_replay (0);

	rdf_type = tracker_ontologies_get_property_by_uri (RDF_PREFIX "type");

	tracker_db_journal_reader_init (NULL);

	while (tracker_db_journal_reader_next (&journal_error)) {
		TrackerDBJournalEntryType type;
		const gchar *object;
		gint graph_id, subject_id, predicate_id, object_id;

		type = tracker_db_journal_reader_get_type ();
		if (type == TRACKER_DB_JOURNAL_RESOURCE) {
			GError *new_error = NULL;
			TrackerDBInterface *iface;
			TrackerDBStatement *stmt;
			gint id;
			const gchar *uri;
			TrackerProperty *property = NULL;
			TrackerClass *class;

			tracker_db_journal_reader_get_resource (&id, &uri);

			if (in_ontology) {
				g_hash_table_insert (id_uri_map, GINT_TO_POINTER (id), (gpointer) uri);
				continue;
			}

			class = g_hash_table_lookup (classes, GINT_TO_POINTER (id));
			if (!class)
				property = g_hash_table_lookup (properties, GINT_TO_POINTER (id));

			if (property || class)
				continue;

			iface = tracker_db_manager_get_db_interface ();

			stmt = tracker_db_interface_create_statement (iface, &new_error,
					                              "INSERT "
					                              "INTO Resource "
					                              "(ID, Uri) "
					                              "VALUES (?, ?)");

			if (stmt) {
				tracker_db_statement_bind_int (stmt, 0, id);
				tracker_db_statement_bind_text (stmt, 1, uri);
				tracker_db_statement_execute (stmt, &new_error);
				g_object_unref (stmt);
			}

			if (new_error) {
				g_warning ("Journal replay error: '%s'", new_error->message);
				g_error_free (new_error);
			}

		} else if (type == TRACKER_DB_JOURNAL_START_ONTOLOGY_TRANSACTION) {
			in_ontology = TRUE;
		} else if (type == TRACKER_DB_JOURNAL_START_TRANSACTION) {
			resource_time = tracker_db_journal_reader_get_time ();
		} else if (type == TRACKER_DB_JOURNAL_END_TRANSACTION) {
			if (in_ontology) {
				ontology_transaction_end (ontology_queue, seen_classes, seen_properties);
				g_list_foreach (ontology_queue, (GFunc) free_queued_statement, NULL);
				g_list_free (ontology_queue);
				ontology_queue = NULL;
				in_ontology = FALSE;
				tracker_data_ontology_free_seen (seen_classes);
				seen_classes = NULL;
				tracker_data_ontology_free_seen (seen_properties);
				seen_properties = NULL;
			} else {
				GError *new_error = NULL;
				tracker_data_update_buffer_might_flush (&new_error);
				if (new_error) {
					g_warning ("Journal replay error: '%s'", new_error->message);
					g_clear_error (&new_error);
				}
			}
		} else if (type == TRACKER_DB_JOURNAL_INSERT_STATEMENT) {
			GError *new_error = NULL;
			TrackerProperty *property;

			tracker_db_journal_reader_get_statement (&graph_id, &subject_id, &predicate_id, &object);

			if (in_ontology) {

				if (!seen_classes)
					seen_classes = g_ptr_array_new ();
				if (!seen_properties)
					seen_properties = g_ptr_array_new ();

				ontology_queue = ontology_statement_insert (ontology_queue,
				                                            graph_id,
				                                            subject_id,
				                                            predicate_id,
				                                            object,
				                                            classes,
				                                            properties,
				                                            id_uri_map,
				                                            FALSE,
				                                            seen_classes,
				                                            seen_properties);
				continue;
			}

			if (last_operation_type == -1) {
				tracker_data_update_buffer_flush (&new_error);
				if (new_error) {
					g_warning ("Journal replay error: '%s'", new_error->message);
					g_clear_error (&new_error);
				}
			}
			last_operation_type = 1;

			property = g_hash_table_lookup (properties, GINT_TO_POINTER (predicate_id));

			if (property) {
				resource_buffer_switch (NULL, graph_id, NULL, subject_id);

				cache_set_metadata_decomposed (property, object, 0, NULL, graph_id, &new_error);

				if (new_error) {
					g_warning ("Journal replay error: '%s'", new_error->message);
					g_clear_error (&new_error);
				}

			} else {
				g_warning ("Journal replay error: 'property with ID %d doesn't exist'", predicate_id);
			}

		} else if (type == TRACKER_DB_JOURNAL_INSERT_STATEMENT_ID) {
			GError *new_error = NULL;
			TrackerClass *class = NULL;
			TrackerProperty *property;

			tracker_db_journal_reader_get_statement_id (&graph_id, &subject_id, &predicate_id, &object_id);

			if (in_ontology) {
				const gchar *object_n;
				object_n = g_hash_table_lookup (id_uri_map, GINT_TO_POINTER (object_id));

				if (!seen_classes)
					seen_classes = g_ptr_array_new ();
				if (!seen_properties)
					seen_properties = g_ptr_array_new ();

				ontology_queue = ontology_statement_insert (ontology_queue,
				                                            graph_id,
				                                            subject_id,
				                                            predicate_id,
				                                            object_n,
				                                            classes,
				                                            properties,
				                                            id_uri_map,
				                                            TRUE,
				                                            seen_classes,
				                                            seen_properties);
				continue;
			}

			if (last_operation_type == -1) {
				tracker_data_update_buffer_flush (&new_error);
				if (new_error) {
					g_warning ("Journal replay error: '%s'", new_error->message);
					g_clear_error (&new_error);
				}
			}
			last_operation_type = 1;

			property = g_hash_table_lookup (properties, GINT_TO_POINTER (predicate_id));

			if (property) {
				if (tracker_property_get_data_type (property) != TRACKER_PROPERTY_TYPE_RESOURCE) {
					g_warning ("Journal replay error: 'property with ID %d does not account URIs'", predicate_id);
				} else {
					resource_buffer_switch (NULL, graph_id, NULL, subject_id);

					if (property == rdf_type) {
						class = g_hash_table_lookup (classes, GINT_TO_POINTER (object_id));
						if (class) {
							cache_create_service_decomposed (class, NULL, graph_id);
						} else {
							g_warning ("Journal replay error: 'class with ID %d not found in the ontology'", object_id);
						}
					} else {
						GError *new_error = NULL;

						/* add value to metadata database */
						cache_set_metadata_decomposed (property, NULL, object_id, NULL, graph_id, &new_error);

						if (new_error) {
							g_warning ("Journal replay error: '%s'", new_error->message);
							g_error_free (new_error);
						}
					}
				}
			} else {
				g_warning ("Journal replay error: 'property with ID %d doesn't exist'", predicate_id);
			}

		} else if (type == TRACKER_DB_JOURNAL_DELETE_STATEMENT) {
			GError *new_error = NULL;
			TrackerProperty *property;

			tracker_db_journal_reader_get_statement (&graph_id, &subject_id, &predicate_id, &object);

			if (in_ontology) {
				continue;
			}

			if (last_operation_type == 1) {
				tracker_data_update_buffer_flush (&new_error);
				if (new_error) {
					g_warning ("Journal replay error: '%s'", new_error->message);
					g_clear_error (&new_error);
				}
			}
			last_operation_type = -1;

			resource_buffer_switch (NULL, graph_id, NULL, subject_id);

			property = g_hash_table_lookup (properties, GINT_TO_POINTER (predicate_id));

			if (property) {
				GError *new_error = NULL;

				if (object && rdf_type == property) {
					TrackerClass *class;

					class = tracker_ontologies_get_class_by_uri (object);
					if (class != NULL) {
						cache_delete_resource_type (class, NULL, graph_id);
					} else {
						g_warning ("Journal replay error: 'class with '%s' not found in the ontology'", object);
					}
				} else {
					delete_metadata_decomposed (property, object, 0, &new_error);
				}

				if (new_error) {
					g_warning ("Journal replay error: '%s'", new_error->message);
					g_error_free (new_error);
				}

			} else {
				g_warning ("Journal replay error: 'property with ID %d doesn't exist'", predicate_id);
			}

		} else if (type == TRACKER_DB_JOURNAL_DELETE_STATEMENT_ID) {
			GError *new_error = NULL;
			TrackerClass *class = NULL;
			TrackerProperty *property;

			tracker_db_journal_reader_get_statement_id (&graph_id, &subject_id, &predicate_id, &object_id);

			if (in_ontology) {
				continue;
			}

			if (last_operation_type == 1) {
				tracker_data_update_buffer_flush (&new_error);
				if (new_error) {
					g_warning ("Journal replay error: '%s'", new_error->message);
					g_clear_error (&new_error);
				}
			}
			last_operation_type = -1;

			property = g_hash_table_lookup (properties, GINT_TO_POINTER (predicate_id));

			if (property) {

				resource_buffer_switch (NULL, graph_id, NULL, subject_id);

				if (property == rdf_type) {
					class = g_hash_table_lookup (classes, GINT_TO_POINTER (object_id));
					if (class) {
						cache_delete_resource_type (class, NULL, graph_id);
					} else {
						g_warning ("Journal replay error: 'class with ID %d not found in the ontology'", object_id);
					}
				} else {
					GError *new_error = NULL;

					delete_metadata_decomposed (property, NULL, object_id, &new_error);

					if (new_error) {
						g_warning ("Journal replay error: '%s'", new_error->message);
						g_error_free (new_error);
					}
				}
			} else {
				g_warning ("Journal replay error: 'property with ID %d doesn't exist'", predicate_id);
			}
		}

		if (busy_callback) {
			busy_callback (busy_status,
			               tracker_db_journal_reader_get_progress (),
			               busy_user_data);
		}
	}

	if (journal_error) {
		gsize size;

		size = tracker_db_journal_reader_get_size_of_correct ();
		tracker_db_journal_reader_shutdown ();

		tracker_db_journal_init (NULL, FALSE);
		tracker_db_journal_truncate (size);
		tracker_db_journal_shutdown ();

		g_clear_error (&journal_error);
	} else {
		tracker_db_journal_reader_shutdown ();
	}

	tracker_data_commit_db_transaction ();
}
