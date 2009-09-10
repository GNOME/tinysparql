/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-ontology.h>

#include <tracker-fts/tracker-fts.h>

#include <libtracker-db/tracker-db-manager.h>
#include <libtracker-db/tracker-db-dbus.h>

#include "tracker-data-manager.h"
#include "tracker-data-update.h"
#include "tracker-data-query.h"
#include "tracker-sparql-query.h"

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDFS_PREFIX TRACKER_RDFS_PREFIX
#define TRACKER_PREFIX TRACKER_TRACKER_PREFIX

typedef struct _TrackerDataUpdateBuffer TrackerDataUpdateBuffer;
typedef struct _TrackerDataUpdateBufferPredicate TrackerDataUpdateBufferPredicate;
typedef struct _TrackerDataUpdateBufferProperty TrackerDataUpdateBufferProperty;
typedef struct _TrackerDataUpdateBufferTable TrackerDataUpdateBufferTable;
typedef struct _TrackerDataBlankBuffer TrackerDataBlankBuffer;

struct _TrackerDataUpdateBuffer {
	GHashTable *resource_cache;
	gchar *subject;
	gchar *new_subject;
	guint32 id;
	gboolean create;
	gboolean fts_updated;
	/* TrackerProperty -> GValueArray */
	GHashTable *predicates;
	GHashTable *tables;
	GPtrArray *types;
	/* valid per sqlite transaction, not just for same subject */
	gboolean fts_ever_updated;
};

struct _TrackerDataUpdateBufferProperty {
	gchar *name;
	GValue value;
	gboolean fts;
};

struct _TrackerDataUpdateBufferTable {
	gboolean insert;
	gboolean multiple_values;
	GArray *properties;
};

/* buffer for anonymous blank nodes
 * that are not yet in the database */
struct _TrackerDataBlankBuffer {
	GHashTable *table;
	gchar *subject;
	GArray *predicates;
	GArray *objects;
};


static gint transaction_level = 0;
static TrackerDataUpdateBuffer update_buffer;
static TrackerDataBlankBuffer blank_buffer;

static TrackerStatementCallback insert_callback = NULL;
static gpointer insert_data;
static TrackerStatementCallback delete_callback = NULL;
static gpointer delete_data;
static TrackerCommitCallback commit_callback = NULL;
static gpointer commit_data;

void 
tracker_data_set_commit_statement_callback (TrackerCommitCallback    callback,
					    gpointer                 user_data)
{
	commit_callback = callback;
	commit_data = user_data;
}

void
tracker_data_set_insert_statement_callback (TrackerStatementCallback callback,
					    gpointer                 user_data)
{
	insert_callback = callback;
	insert_data = user_data;
}

void 
tracker_data_set_delete_statement_callback (TrackerStatementCallback callback,
					    gpointer                 user_data)
{
	delete_callback = callback;
	delete_data = user_data;
}

GQuark tracker_data_error_quark (void) {
	return g_quark_from_static_string ("tracker_data_error-quark");
}

static guint32
tracker_data_update_get_new_service_id (TrackerDBInterface *iface)
{
	TrackerDBCursor    *cursor;
	TrackerDBInterface *temp_iface;
	TrackerDBStatement *stmt;

	static guint32	    max = 0;

	if (G_LIKELY (max != 0)) {
		return ++max;
	}

	temp_iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (temp_iface,
	                                             "SELECT MAX(ID) AS A FROM \"rdfs:Resource\"");
	cursor = tracker_db_statement_start_cursor (stmt, NULL);
	g_object_unref (stmt);

	if (cursor) {
		tracker_db_cursor_iter_next (cursor);
		max = MAX (tracker_db_cursor_get_int (cursor, 0), max);
		g_object_unref (cursor);
	}

	return ++max;
}

static guint32
tracker_data_update_get_next_modseq (void)
{
	TrackerDBCursor    *cursor;
	TrackerDBInterface *temp_iface;
	TrackerDBStatement *stmt;
	static guint32	    max = 0;

	if (G_LIKELY (max != 0)) {
		return ++max;
	}

	temp_iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (temp_iface, 
	                                              "SELECT MAX(\"tracker:modified\") AS A FROM \"rdfs:Resource\"");
	cursor = tracker_db_statement_start_cursor (stmt, NULL);
	g_object_unref (stmt);

	if (cursor) {
		tracker_db_cursor_iter_next (cursor);
		max = MAX (tracker_db_cursor_get_int (cursor, 0), max);
		g_object_unref (cursor);
	}

	return ++max;
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
		g_free (property->name);
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

	table = g_hash_table_lookup (update_buffer.tables, table_name);
	if (table == NULL) {
		table = cache_table_new (multiple_values);
		g_hash_table_insert (update_buffer.tables, g_strdup (table_name), table);
		table->insert = multiple_values;
	}

	return table;
}

static void
cache_insert_row (const gchar            *table_name)
{
	TrackerDataUpdateBufferTable    *table;

	table = cache_ensure_table (table_name, FALSE);
	table->insert = TRUE;
}

static void
cache_insert_value (const gchar            *table_name,
		    const gchar            *field_name,
		    GValue                 *value,
		    gboolean                multiple_values,
		    gboolean                fts)
{
	TrackerDataUpdateBufferTable    *table;
	TrackerDataUpdateBufferProperty  property;

	property.name = g_strdup (field_name);
	property.value = *value;
	property.fts = fts;

	table = cache_ensure_table (table_name, multiple_values);
	g_array_append_val (table->properties, property);
}

static guint32
query_resource_id (const gchar *uri)
{
	guint32 id;

	id = GPOINTER_TO_UINT (g_hash_table_lookup (update_buffer.resource_cache, uri));

	if (id == 0) {
		id = tracker_data_query_resource_id (uri);

		if (id) {
			g_hash_table_insert (update_buffer.resource_cache, g_strdup (uri), GUINT_TO_POINTER (id));
		}
	}

	return id;
}

static guint32
ensure_resource_id (const gchar *uri)
{
	TrackerDBInterface *iface, *common;
	TrackerDBStatement *stmt;

	guint32 id;

	id = query_resource_id (uri);

	if (id == 0) {
		/* object resource not yet in the database */
		common = tracker_db_manager_get_db_interface ();
		iface = tracker_db_manager_get_db_interface ();

		id = tracker_data_update_get_new_service_id (common);
		stmt = tracker_db_interface_create_statement (iface, "INSERT INTO \"rdfs:Resource\" (ID, Uri, \"tracker:modified\", Available) VALUES (?, ?, ?, 1)");
		tracker_db_statement_bind_int (stmt, 0, id);
		tracker_db_statement_bind_text (stmt, 1, uri);
		tracker_db_statement_bind_int (stmt, 2, tracker_data_update_get_next_modseq ());
		tracker_db_statement_execute (stmt, NULL);
		g_object_unref (stmt);

		stmt = tracker_db_interface_create_statement (iface, "INSERT INTO \"fts\" (rowid) VALUES (?)");
		tracker_db_statement_bind_int (stmt, 0, id);
		tracker_db_statement_execute (stmt, NULL);
		g_object_unref (stmt);

		g_hash_table_insert (update_buffer.resource_cache, g_strdup (uri), GUINT_TO_POINTER (id));
	}

	return id;
}

static void
statement_bind_gvalue (TrackerDBStatement *stmt,
		       gint                idx,
		       const GValue       *value)
{
	switch (G_VALUE_TYPE (value)) {
	case G_TYPE_STRING:
		tracker_db_statement_bind_text (stmt, idx, g_value_get_string (value));
		break;
	case G_TYPE_INT:
		tracker_db_statement_bind_int (stmt, idx, g_value_get_int (value));
		break;
	case G_TYPE_INT64:
		tracker_db_statement_bind_int64 (stmt, idx, g_value_get_int64 (value));
		break;
	case G_TYPE_DOUBLE:
		tracker_db_statement_bind_double (stmt, idx, g_value_get_double (value));
		break;
	default:
		g_warning ("Unknown type for binding: %s\n", G_VALUE_TYPE_NAME (value));
		break;
	}
}

static void
tracker_data_update_buffer_flush (void)
{
	TrackerDBInterface             *iface;
	TrackerDBStatement             *stmt;
	TrackerDataUpdateBufferTable    *table;
	TrackerDataUpdateBufferProperty *property;
	GHashTableIter                  iter;
	const gchar                    *table_name;
	GString                        *sql, *fts;
	int                             i;

	iface = tracker_db_manager_get_db_interface ();

	if (update_buffer.subject == NULL) {
		/* nothing to flush */
		return;
	}

	if (update_buffer.new_subject != NULL) {
		/* change uri of resource */
		stmt = tracker_db_interface_create_statement (iface,
			"UPDATE \"rdfs:Resource\" SET Uri = ? WHERE ID = ?");
		tracker_db_statement_bind_text (stmt, 0, update_buffer.new_subject);
		tracker_db_statement_bind_int (stmt, 1, update_buffer.id);
		tracker_db_statement_execute (stmt, NULL);
		g_object_unref (stmt);

		g_free (update_buffer.new_subject);
		update_buffer.new_subject = NULL;
	}

	g_hash_table_iter_init (&iter, update_buffer.tables);
	while (g_hash_table_iter_next (&iter, (gpointer*) &table_name, (gpointer*) &table)) {
		if (table->multiple_values) {
			for (i = 0; i < table->properties->len; i++) {
				property = &g_array_index (table->properties, TrackerDataUpdateBufferProperty, i);

				stmt = tracker_db_interface_create_statement (iface,
					"INSERT OR IGNORE INTO \"%s\" (ID, \"%s\") VALUES (?, ?)",
					table_name,
					property->name);
				tracker_db_statement_bind_int (stmt, 0, update_buffer.id);
				statement_bind_gvalue (stmt, 1, &property->value);
				tracker_db_statement_execute (stmt, NULL);
				g_object_unref (stmt);
			}
		} else {
			if (table->insert) {
				/* ensure we have a row for the subject id */
				stmt = tracker_db_interface_create_statement (iface,
					"INSERT OR IGNORE INTO \"%s\" (ID) VALUES (?)",
					table_name);
				tracker_db_statement_bind_int (stmt, 0, update_buffer.id);
				tracker_db_statement_execute (stmt, NULL);
				g_object_unref (stmt);
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
			}

			g_string_append (sql, " WHERE ID = ?");

			stmt = tracker_db_interface_create_statement (iface, "%s", sql->str);
			tracker_db_statement_bind_int (stmt, i, update_buffer.id);

			for (i = 0; i < table->properties->len; i++) {
				property = &g_array_index (table->properties, TrackerDataUpdateBufferProperty, i);
				statement_bind_gvalue (stmt, i, &property->value);
			}

			tracker_db_statement_execute (stmt, NULL);
			g_object_unref (stmt);

			g_string_free (sql, TRUE);
		}
	}

	if (update_buffer.fts_updated) {
		TrackerProperty *prop;
		GValueArray *values;

		fts = g_string_new ("");

		g_hash_table_iter_init (&iter, update_buffer.predicates);
		while (g_hash_table_iter_next (&iter, (gpointer*) &prop, (gpointer*) &values)) {
			if (tracker_property_get_fulltext_indexed (prop)) {
				for (i = 0; i < values->n_values; i++) {
					g_string_append (fts, g_value_get_string (g_value_array_get_nth (values, i)));
					g_string_append_c (fts, ' ');
				}
			}
		}

		tracker_fts_update_init (update_buffer.id);
		tracker_fts_update_text (update_buffer.id, 0, fts->str);
		g_string_free (fts, TRUE);
	}

	g_hash_table_remove_all (update_buffer.predicates);
	g_hash_table_remove_all (update_buffer.tables);
	g_free (update_buffer.subject);
	update_buffer.subject = NULL;

	g_ptr_array_foreach (update_buffer.types, (GFunc) g_free, NULL);
	g_ptr_array_free (update_buffer.types, TRUE);
	update_buffer.types = NULL;
}

static void
tracker_data_blank_buffer_flush (void)
{
	/* end of blank node */
	gint i, id;
	gchar *subject;
	gchar *blank_uri;
	const gchar *sha1;
	GChecksum *checksum;

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
			tracker_data_insert_statement (blank_uri,
				g_array_index (blank_buffer.predicates, gchar *, i),
				g_array_index (blank_buffer.objects, gchar *, i),
				NULL);
		}
		tracker_data_update_buffer_flush ();
	}

	/* free piled up statements */
	for (i = 0; i < blank_buffer.predicates->len; i++) {
		g_free (g_array_index (blank_buffer.predicates, gchar *, i));
		g_free (g_array_index (blank_buffer.objects, gchar *, i));
	}
	g_array_free (blank_buffer.predicates, TRUE);
	g_array_free (blank_buffer.objects, TRUE);

	g_hash_table_insert (blank_buffer.table, subject, blank_uri);
	g_checksum_free (checksum);
}

static void
cache_create_service_decomposed (TrackerClass           *cl)
{
	TrackerDBInterface *iface;
	TrackerClass       **super_classes;
	GValue              gvalue = { 0 };
	gint                i;
	const gchar        *class_uri;

	iface = tracker_db_manager_get_db_interface ();

	/* also create instance of all super classes */
	super_classes = tracker_class_get_super_classes (cl);
	while (*super_classes) {
		cache_create_service_decomposed (*super_classes);
		super_classes++;
	}

	class_uri = tracker_class_get_uri (cl);

	for (i = 0; i < update_buffer.types->len; i++) {
		if (strcmp (g_ptr_array_index (update_buffer.types, i), class_uri) == 0) {
			/* ignore duplicate statement */
			return;
		}
	}

	tracker_class_set_count (cl, tracker_class_get_count (cl) + 1);

	g_ptr_array_add (update_buffer.types, g_strdup (class_uri));

	g_value_init (&gvalue, G_TYPE_INT);

	cache_insert_row (tracker_class_get_name (cl));

	g_value_set_int (&gvalue, ensure_resource_id (tracker_class_get_uri (cl)));
	cache_insert_value ("rdfs:Resource_rdf:type", "rdf:type", &gvalue, TRUE, FALSE);
}

gboolean
tracker_data_update_resource_uri (const gchar *old_uri,
				  const gchar *new_uri)
{
	guint32 resource_id;
	TrackerDBInterface  *iface;
	TrackerDBStatement  *stmt;

	resource_id = tracker_data_query_resource_id (old_uri);
	if (resource_id == 0) {
		/* old uri does not exist */
		return FALSE;
	}

	if (tracker_data_query_resource_id (new_uri) > 0) {
		/* new uri already exists */
		return FALSE;
	}

	iface = tracker_db_manager_get_db_interface ();

	/* update URI in rdfs:Resource table */

	stmt = tracker_db_interface_create_statement (iface, "UPDATE \"rdfs:Resource\" SET Uri = ? WHERE ID = ?");
	tracker_db_statement_bind_text (stmt, 0, new_uri);
	tracker_db_statement_bind_int (stmt, 1, resource_id);
	tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	return TRUE;
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
check_property_domain (TrackerProperty *property)
{
	gint type_index;

	for (type_index = 0; type_index < update_buffer.types->len; type_index++) {
		if (strcmp (tracker_class_get_uri (tracker_property_get_domain (property)),
		            g_ptr_array_index (update_buffer.types, type_index)) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

static GValueArray *
get_property_values (TrackerProperty *property)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBCursor    *cursor;
	gchar              *table_name;
	const gchar        *field_name;
	gboolean            multiple_values;
	GValue gvalue = { 0 };
	GValueArray        *old_values;

	multiple_values = tracker_property_get_multiple_values (property);

	old_values = g_value_array_new (multiple_values ? 4 : 1);
	g_hash_table_insert (update_buffer.predicates, g_object_ref (property), old_values);

	if (multiple_values) {
		table_name = g_strdup_printf ("%s_%s",
			tracker_class_get_name (tracker_property_get_domain (property)),
			tracker_property_get_name (property));
	} else {
		table_name = g_strdup (tracker_class_get_name (tracker_property_get_domain (property)));
	}
	field_name = tracker_property_get_name (property);

	if (!update_buffer.create) {
		iface = tracker_db_manager_get_db_interface ();

		stmt = tracker_db_interface_create_statement (iface, "SELECT \"%s\" FROM \"%s\" WHERE ID = ?", field_name, table_name);
		tracker_db_statement_bind_int (stmt, 0, update_buffer.id);
		cursor = tracker_db_statement_start_cursor (stmt, NULL);
		g_object_unref (stmt);

		if (cursor) {
			while (tracker_db_cursor_iter_next (cursor)) {
				tracker_db_cursor_get_value (cursor, 0, &gvalue);
				if (G_VALUE_TYPE (&gvalue)) {
					g_value_array_append (old_values, &gvalue);
					g_value_unset (&gvalue);
				}
			}
			g_object_unref (cursor);
		}
	}

	g_free (table_name);

	return old_values;
}

static void
cache_set_metadata_decomposed (TrackerProperty	*property,
			       const gchar	*value,
			       GError          **error)
{
	guint32		    object_id;
	gboolean            multiple_values, fts;
	gchar              *table_name;
	const gchar        *field_name;
	TrackerProperty   **properties, **super_properties, **prop;
	GValue gvalue = { 0 };
	GValueArray        *old_values;

	/* also insert super property values */
	super_properties = tracker_property_get_super_properties (property);
	while (*super_properties) {
		cache_set_metadata_decomposed (*super_properties, value, error);
		if (*error) {
			return;
		}
		super_properties++;
	}

	multiple_values = tracker_property_get_multiple_values (property);
	if (multiple_values) {
		table_name = g_strdup_printf ("%s_%s",
			tracker_class_get_name (tracker_property_get_domain (property)),
			tracker_property_get_name (property));
	} else {
		table_name = g_strdup (tracker_class_get_name (tracker_property_get_domain (property)));
	}
	field_name = tracker_property_get_name (property);

	fts = tracker_property_get_fulltext_indexed (property);

	/* read existing property values */
	old_values = g_hash_table_lookup (update_buffer.predicates, property);
	if (old_values == NULL) {
		if (!check_property_domain (property)) {
			g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_CONSTRAINT,
				     "Subject `%s' is not in domain `%s' of property `%s'",
				     update_buffer.subject,
				     tracker_class_get_name (tracker_property_get_domain (property)),
				     field_name);
			g_free (table_name);
			return;
		}

		if (fts && !update_buffer.fts_updated && !update_buffer.create) {
			/* first fulltext indexed property to be modified
			 * retrieve values of all fulltext indexed properties
			 */
			tracker_fts_update_init (update_buffer.id);

			properties = tracker_ontology_get_properties ();

			for (prop = properties; *prop; prop++) {
				if (tracker_property_get_fulltext_indexed (*prop)
				    && check_property_domain (*prop)) {
					gint i;

					old_values = get_property_values (*prop);

					/* delete old fts entries */
					for (i = 0; i < old_values->n_values; i++) {
						tracker_fts_update_text (update_buffer.id, -1,
						                         g_value_get_string (g_value_array_get_nth (old_values, i)));
					}
				}
			}

			update_buffer.fts_ever_updated = TRUE;

			old_values = g_hash_table_lookup (update_buffer.predicates, property);
		} else {
			old_values = get_property_values (property);
		}

		if (fts) {
			update_buffer.fts_updated = TRUE;
		}
	}

	switch (tracker_property_get_data_type (property)) {
	case TRACKER_PROPERTY_TYPE_STRING:
		g_value_init (&gvalue, G_TYPE_STRING);
		g_value_set_string (&gvalue, value);
		break;
	case TRACKER_PROPERTY_TYPE_INTEGER:
		g_value_init (&gvalue, G_TYPE_INT);
		g_value_set_int (&gvalue, atoi (value));
		break;
	case TRACKER_PROPERTY_TYPE_BOOLEAN:
		/* use G_TYPE_INT to be compatible with value stored in DB
		   (important for value_equal function) */
		g_value_init (&gvalue, G_TYPE_INT);
		g_value_set_int (&gvalue, strcmp (value, "true") == 0);
		break;
	case TRACKER_PROPERTY_TYPE_DOUBLE:
		g_value_init (&gvalue, G_TYPE_DOUBLE);
		g_value_set_double (&gvalue, atof (value));
		break;
	case TRACKER_PROPERTY_TYPE_DATE:
	case TRACKER_PROPERTY_TYPE_DATETIME:
		g_value_init (&gvalue, G_TYPE_INT);
		g_value_set_int (&gvalue, tracker_string_to_date (value));
		break;
	case TRACKER_PROPERTY_TYPE_RESOURCE:
		object_id = ensure_resource_id (value);
		g_value_init (&gvalue, G_TYPE_INT);
		g_value_set_int (&gvalue, object_id);
		break;
	case TRACKER_PROPERTY_TYPE_BLOB:
	case TRACKER_PROPERTY_TYPE_STRUCT:
	case TRACKER_PROPERTY_TYPE_FULLTEXT:
	default:
		return;
	}

	if (!value_set_add_value (old_values, &gvalue)) {
		/* value already inserted */
		g_value_unset (&gvalue);
	} else if (!multiple_values && old_values->n_values > 1) {
		/* trying to add second value to single valued property */

		g_value_unset (&gvalue);

		g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_CONSTRAINT,
		             "Unable to insert multiple values for subject `%s' and single valued property `%s'",
		             update_buffer.subject,
		             field_name);
	} else {
		cache_insert_value (table_name, field_name, &gvalue, multiple_values, fts);
	}

	g_free (table_name);
}

static void
delete_resource_type (gint resource_id,
		      TrackerClass           *cl)
{
	TrackerDBInterface  *iface;
	TrackerDBStatement  *stmt;

	iface = tracker_db_manager_get_db_interface ();

	/* remove entry from rdf:type table */
	stmt = tracker_db_interface_create_statement (iface, "DELETE FROM \"rdfs:Resource_rdf:type\" WHERE ID = ? AND \"rdf:type\" = ?");
	tracker_db_statement_bind_int (stmt, 0, resource_id);
	tracker_db_statement_bind_int (stmt, 1, ensure_resource_id (tracker_class_get_uri (cl)));
	tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	/* remove row from class table */
	stmt = tracker_db_interface_create_statement (iface, "DELETE FROM \"%s\" WHERE ID = ?", tracker_class_get_name (cl));
	tracker_db_statement_bind_int (stmt, 0, resource_id);
	tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);
}

static void
delete_metadata_decomposed (gint resource_id,
			    TrackerProperty	*property,
			    const gchar	*value)
{
	TrackerDBInterface  *iface;
	TrackerDBStatement  *stmt;
	guint32		    object_id;
	gboolean            multiple_values;
	const gchar *class_name, *property_name;
	TrackerProperty   **super_properties;

	iface = tracker_db_manager_get_db_interface ();

	multiple_values = tracker_property_get_multiple_values (property);
	class_name = tracker_class_get_name (tracker_property_get_domain (property));
	property_name = tracker_property_get_name (property);
	if (multiple_values) {
		/* delete rows for multiple value properties */
		stmt = tracker_db_interface_create_statement (iface,
			"DELETE FROM \"%s_%s\" WHERE ID = ? AND \"%s\" = ?",
			class_name, property_name, property_name);
	} else {
		/* just set value to NULL for single value properties */
		stmt = tracker_db_interface_create_statement (iface,
			"UPDATE \"%s\" SET \"%s\" = NULL WHERE ID = ? AND \"%s\" = ?",
			class_name, property_name, property_name);
	}

	tracker_db_statement_bind_int (stmt, 0, resource_id);

	switch (tracker_property_get_data_type (property)) {
	case TRACKER_PROPERTY_TYPE_STRING:
		tracker_db_statement_bind_text (stmt, 1, value);
		break;
	case TRACKER_PROPERTY_TYPE_INTEGER:
		tracker_db_statement_bind_int (stmt, 1, atoi (value));
		break;
	case TRACKER_PROPERTY_TYPE_BOOLEAN:
		tracker_db_statement_bind_int (stmt, 1, strcmp (value, "true") == 0);
		break;
	case TRACKER_PROPERTY_TYPE_DOUBLE:
		tracker_db_statement_bind_double (stmt, 1, atof (value));
		break;
	case TRACKER_PROPERTY_TYPE_DATE:
	case TRACKER_PROPERTY_TYPE_DATETIME:
		tracker_db_statement_bind_int (stmt, 1, tracker_string_to_date (value));
		break;
	case TRACKER_PROPERTY_TYPE_RESOURCE:
		object_id = ensure_resource_id (value);
		tracker_db_statement_bind_int (stmt, 1, object_id);
		break;
	case TRACKER_PROPERTY_TYPE_BLOB:
	case TRACKER_PROPERTY_TYPE_STRUCT:
	case TRACKER_PROPERTY_TYPE_FULLTEXT:
	default:
		g_assert_not_reached ();
	}

	tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	/* also delete super property values */
	super_properties = tracker_property_get_super_properties (property);
	while (*super_properties) {
		delete_metadata_decomposed (resource_id, *super_properties, value);
		super_properties++;
	}
}


void
tracker_data_delete_statement (const gchar            *subject,
			       const gchar            *predicate,
			       const gchar            *object,
			       GError                **error)
{
	TrackerClass       *class;
	TrackerProperty    *field;
	gint		    subject_id;
	GPtrArray          *types;

	g_return_if_fail (subject != NULL);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);

	tracker_data_begin_transaction ();

	subject_id = query_resource_id (subject);
	
	if (subject_id == 0) {
		/* subject not in database */

		tracker_data_commit_transaction ();

		return;
	}

	if (update_buffer.subject != NULL) {
		/* delete does not use update buffer, flush it */
		tracker_data_update_buffer_flush ();
	}

	types = tracker_data_query_rdf_type (subject_id);

	/* TODO we need to retrieve all existing (FTS indexed) property values for
	 * this resource to properly support incremental FTS updates
	 * (like calling deleteTerms and then calling insertTerms)
	 */
	tracker_fts_update_init (subject_id);

	if (object && g_strcmp0 (predicate, RDF_PREFIX "type") == 0) {
		class = tracker_ontology_get_class_by_uri (object);
		if (class != NULL) {
			TrackerDBInterface *iface;
			TrackerDBStatement *stmt;
			TrackerDBResultSet *result_set;
			TrackerProperty   **properties, **prop;
			GString *projection = NULL;

			iface = tracker_db_manager_get_db_interface ();

			/* retrieve all subclasses we need to remove from the subject
			 * before we can remove the class specified as object of the statement */
			stmt = tracker_db_interface_create_statement (iface,
				"SELECT (SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"rdfs:Class_rdfs:subClassOf\".ID) "
				"FROM \"rdfs:Resource_rdf:type\" INNER JOIN \"rdfs:Class_rdfs:subClassOf\" ON (\"rdf:type\" = \"rdfs:Class_rdfs:subClassOf\".ID) "
				"WHERE \"rdfs:Resource_rdf:type\".ID = ? AND \"rdfs:subClassOf\" = (SELECT ID FROM \"rdfs:Resource\" WHERE Uri = ?)");
			tracker_db_statement_bind_int (stmt, 0, subject_id);
			tracker_db_statement_bind_text (stmt, 1, object);
			result_set = tracker_db_statement_execute (stmt, NULL);
			g_object_unref (stmt);

			if (result_set) {
				do {
					gchar *class_uri;

					tracker_db_result_set_get (result_set, 0, &class_uri, -1);
					tracker_data_delete_statement (subject, predicate, class_uri, NULL);
					g_free (class_uri);
				} while (tracker_db_result_set_iter_next (result_set));

				g_object_unref (result_set);
			}

			properties = tracker_ontology_get_properties ();

			for (prop = properties; *prop; prop++) {

				if (tracker_property_get_domain (*prop) != class) {
					continue;
				}

				if (!tracker_property_get_multiple_values (*prop)) {

					if (tracker_property_get_fulltext_indexed (*prop)) {
						if (!projection) {
							projection = g_string_new ("");
						} else {
							g_string_append_c (projection, ',');
						}

						g_string_append_c (projection, '\'');
						g_string_append (projection, tracker_property_get_name (*prop));
						g_string_append (projection, "',");
						g_string_append_c (projection, '"');
						g_string_append (projection, tracker_property_get_name (*prop));
						g_string_append_c (projection, '"');
					}
				} else {

					if (tracker_property_get_fulltext_indexed (*prop)) {

						/* Removing all fulltext properties from fts for 
						 * none nrl:maxCardinality */

						stmt = tracker_db_interface_create_statement (iface, "SELECT \"%s\" FROM \"%s_%s\" WHERE ID = ?",
								tracker_property_get_name (*prop),
								tracker_class_get_name (class),
								tracker_property_get_name (*prop));
						tracker_db_statement_bind_int (stmt, 0, subject_id);
						result_set = tracker_db_statement_execute (stmt, NULL);
						g_object_unref (stmt);

						if (result_set) {
							gchar *prop_object = NULL;

							tracker_db_result_set_get (result_set, 0, &prop_object, -1);
							g_object_unref (result_set);

							if (prop_object) {
								tracker_fts_update_text (subject_id, -1, prop_object);

								g_free (prop_object);
							}
						}
					}

					/* multi-valued property, delete values from DB */
					stmt = tracker_db_interface_create_statement (iface, "DELETE FROM \"%s_%s\" WHERE ID = ?",
								tracker_class_get_name (class),
								tracker_property_get_name (*prop));
					tracker_db_statement_bind_int (stmt, 0, subject_id);
					tracker_db_statement_execute (stmt, NULL);
					g_object_unref (stmt);
				}
			}

			/* delete single-valued properties for current class */

			if (projection) {

				/* Removing all fulltext properties from fts for
				 * nrl:maxCardinality 1 */

				stmt = tracker_db_interface_create_statement (iface, "SELECT %s FROM \"%s\" WHERE ID = ?",
						projection->str,
						tracker_class_get_name (class));
				g_string_free (projection, TRUE);

				tracker_db_statement_bind_int (stmt, 0, subject_id);
				result_set = tracker_db_statement_execute (stmt, NULL);
				g_object_unref (stmt);

				if (result_set) {
					gchar *prop_object = NULL;
					gchar *prop_name = NULL;
					guint columns, c;

					do {
						columns = tracker_db_result_set_get_n_columns (result_set);
						for (c = 0; c < columns; c += 2) {
							tracker_db_result_set_get (result_set, c, &prop_name, -1);
							tracker_db_result_set_get (result_set, c + 1, &prop_object, -1);

							if (prop_object && prop_name) {
								tracker_fts_update_text (subject_id, -1, prop_object);
							}

							g_free (prop_object);
							prop_object = NULL;
							g_free (prop_name);
							prop_name = NULL;

						}
					} while (tracker_db_result_set_iter_next (result_set));

					g_object_unref (result_set);
				}
			}

			stmt = tracker_db_interface_create_statement (iface, "DELETE FROM \"%s\" WHERE ID = ?",
			                                              tracker_class_get_name (class));
			tracker_db_statement_bind_int (stmt, 0, subject_id);
			tracker_db_statement_execute (stmt, NULL);
			g_object_unref (stmt);

			if (strcmp (tracker_class_get_name (class), "rdfs:Resource") == 0) {
				stmt = tracker_db_interface_create_statement (iface, "DELETE FROM \"fts\" WHERE rowid = ?");
				tracker_db_statement_bind_int (stmt, 0, subject_id);
				tracker_db_statement_execute (stmt, NULL);
				g_object_unref (stmt);
			}

			/* delete rows from class tables */
			delete_resource_type (subject_id, class);

			tracker_class_set_count (class, tracker_class_get_count (class) - 1);
		} else {
			g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_UNKNOWN_CLASS,
				     "Class '%s' not found in the ontology", object);
		}
	} else {
		field = tracker_ontology_get_property_by_uri (predicate);
		if (field != NULL) {

			delete_metadata_decomposed (subject_id, field, object);

			if (object && tracker_property_get_fulltext_indexed (field)) {
				tracker_fts_update_text (subject_id, -1, object);
			}
		} else {
			g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_UNKNOWN_PROPERTY,
				     "Property '%s' not found in the ontology", predicate);
		}
	}

	update_buffer.fts_ever_updated = TRUE;

	if (delete_callback) {
		delete_callback (subject, predicate, object, types, delete_data);
	}

	if (types) {
		g_ptr_array_foreach (types, (GFunc) g_free, NULL);
		g_ptr_array_free (types, TRUE);
	}

	tracker_data_commit_transaction ();
}

static gboolean
tracker_data_insert_statement_common (const gchar            *subject,
				      const gchar            *predicate,
				      const gchar            *object)
{
	if (g_str_has_prefix (subject, ":")) {
		/* blank node definition
		   pile up statements until the end of the blank node */
		gchar *value;

		if (blank_buffer.subject != NULL) {
			/* active subject in buffer */
			if (strcmp (blank_buffer.subject, subject) != 0) {
				/* subject changed, need to flush buffer */
				tracker_data_blank_buffer_flush ();
			}
		}

		if (blank_buffer.subject == NULL) {
			blank_buffer.subject = g_strdup (subject);
			blank_buffer.predicates = g_array_sized_new (FALSE, FALSE, sizeof (char*), 4);
			blank_buffer.objects = g_array_sized_new (FALSE, FALSE, sizeof (char*), 4);
		}

		value = g_strdup (predicate);
		g_array_append_val (blank_buffer.predicates, value);
		value = g_strdup (object);
		g_array_append_val (blank_buffer.objects, value);

		return FALSE;
	}

	if (update_buffer.subject != NULL) {
		/* active subject in cache */
		if (strcmp (update_buffer.subject, subject) != 0) {
			/* subject changed, need to flush cache */
			tracker_data_update_buffer_flush ();
		}
	}

	if (update_buffer.subject == NULL) {
		GValue gvalue = { 0 };

		g_value_init (&gvalue, G_TYPE_INT);

		/* subject not yet in cache, retrieve or create ID */
		update_buffer.subject = g_strdup (subject);
		update_buffer.id = query_resource_id (update_buffer.subject);
		update_buffer.create = (update_buffer.id == 0);
		update_buffer.fts_updated = FALSE;
		if (update_buffer.create) {
			update_buffer.id = ensure_resource_id (update_buffer.subject);
			update_buffer.types = g_ptr_array_new ();
		} else {
			update_buffer.types = tracker_data_query_rdf_type (update_buffer.id);
		}

		g_value_set_int (&gvalue, tracker_data_update_get_next_modseq ());
		cache_insert_value ("rdfs:Resource", "tracker:modified", &gvalue, FALSE, FALSE);
	}

	return TRUE;
}

void
tracker_data_insert_statement (const gchar            *subject,
			       const gchar            *predicate,
			       const gchar            *object,
			       GError                **error)
{
	TrackerProperty *property;

	g_return_if_fail (subject != NULL);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);

	property = tracker_ontology_get_property_by_uri (predicate);
	if (property != NULL) {
		if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
			tracker_data_insert_statement_with_uri (subject, predicate, object, error);
		} else {
			tracker_data_insert_statement_with_string (subject, predicate, object, error);
		}
	} else {
		g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_UNKNOWN_PROPERTY,
		             "Property '%s' not found in the ontology", predicate);
	}
}

void
tracker_data_insert_statement_with_uri (const gchar            *subject,
					const gchar            *predicate,
					const gchar            *object,
					GError                **error)
{
	GError          *actual_error = NULL;
	TrackerClass    *class;
	TrackerProperty *property;

	g_return_if_fail (subject != NULL);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);

	property = tracker_ontology_get_property_by_uri (predicate);
	if (property == NULL) {
		g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_UNKNOWN_PROPERTY,
		             "Property '%s' not found in the ontology", predicate);
		return;
	} else if (tracker_property_get_data_type (property) != TRACKER_PROPERTY_TYPE_RESOURCE) {
		g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_INVALID_TYPE,
		             "Property '%s' does not accept URIs", predicate);
		return;
	}

	tracker_data_begin_transaction ();

	/* subjects and objects starting with `:' are anonymous blank nodes */
	if (g_str_has_prefix (object, ":")) {
		/* anonymous blank node used as object in a statement */
		const gchar *blank_uri;

		if (blank_buffer.subject != NULL) {
			if (strcmp (blank_buffer.subject, object) == 0) {
				/* object still in blank buffer, need to flush buffer */
				tracker_data_blank_buffer_flush ();
			}
		}

		blank_uri = g_hash_table_lookup (blank_buffer.table, object);

		if (blank_uri != NULL) {
			/* now insert statement referring to blank node */
			tracker_data_insert_statement (subject, predicate, blank_uri, error);

			g_hash_table_remove (blank_buffer.table, object);

			tracker_data_commit_transaction ();

			return;
		} else {
			g_critical ("Blank node '%s' not found", object);
		}
	}

	if (!tracker_data_insert_statement_common (subject, predicate, object)) {
		tracker_data_commit_transaction ();
		return;
	}

	if (strcmp (predicate, RDF_PREFIX "type") == 0) {
		/* handle rdf:type statements specially to
		   cope with inference and insert blank rows */
		class = tracker_ontology_get_class_by_uri (object);
		if (class != NULL) {
			cache_create_service_decomposed (class);
		} else {
			g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_UNKNOWN_CLASS,
				     "Class '%s' not found in the ontology", object);
		}
	} else if (strcmp (predicate, TRACKER_PREFIX "uri") == 0) {
		/* internal property tracker:uri, used to change uri of existing element */
		update_buffer.new_subject = g_strdup (object);
	} else {
		/* add value to metadata database */
		cache_set_metadata_decomposed (property, object, &actual_error);
		if (actual_error) {
			/* FIXME rollback instead of commit */
			tracker_data_commit_transaction ();
			g_propagate_error (error, actual_error);
			return;
		}
	}

	if (insert_callback) {
		insert_callback (subject, predicate, object, update_buffer.types, insert_data);
	}

	tracker_data_commit_transaction ();
}

void
tracker_data_insert_statement_with_string (const gchar            *subject,
					   const gchar            *predicate,
					   const gchar            *object,
					   GError                **error)
{
	GError          *actual_error = NULL;
	TrackerProperty *property;

	g_return_if_fail (subject != NULL);
	g_return_if_fail (predicate != NULL);
	g_return_if_fail (object != NULL);

	property = tracker_ontology_get_property_by_uri (predicate);
	if (property == NULL) {
		g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_UNKNOWN_PROPERTY,
		             "Property '%s' not found in the ontology", predicate);
		return;
	} else if (tracker_property_get_data_type (property) == TRACKER_PROPERTY_TYPE_RESOURCE) {
		g_set_error (error, TRACKER_DATA_ERROR, TRACKER_DATA_ERROR_INVALID_TYPE,
		             "Property '%s' only accepts URIs", predicate);
		return;
	}

	tracker_data_begin_transaction ();

	if (!tracker_data_insert_statement_common (subject, predicate, object)) {
		tracker_data_commit_transaction ();
		return;
	}

	/* add value to metadata database */
	cache_set_metadata_decomposed (property, object, &actual_error);
	if (actual_error) {
		/* FIXME rollback instead of commit */
		tracker_data_commit_transaction ();
		g_propagate_error (error, actual_error);
		return;
	}

	if (insert_callback) {
		insert_callback (subject, predicate, object, update_buffer.types, insert_data);
	}

	tracker_data_commit_transaction ();
}

void
tracker_data_delete_resource (const gchar     *uri)
{
	g_return_if_fail (uri != NULL);

	tracker_data_delete_statement (uri, RDF_PREFIX "type", RDFS_PREFIX "Resource", NULL);
}

static void
db_set_volume_available (const gchar *uri,
                         gboolean     available)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface, "UPDATE \"rdfs:Resource\" SET Available = ? WHERE ID IN (SELECT ID FROM \"nie:DataObject\" WHERE \"nie:dataSource\" IN (SELECT ID FROM \"rdfs:Resource\" WHERE Uri = ?))");
	tracker_db_statement_bind_int (stmt, 0, available ? 1 : 0);
	tracker_db_statement_bind_text (stmt, 1, uri);
	tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);
}

void
tracker_data_update_enable_volume (const gchar *udi,
                                   const gchar *mount_path)
{
	gchar		   *removable_device_urn;
	gchar *delete_q;
	gchar *set_q;
	gchar *mount_path_uri;
	GFile *mount_path_file;
	GError *error = NULL;

	g_return_if_fail (udi != NULL);
	g_return_if_fail (mount_path != NULL);

	removable_device_urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s", udi);

	db_set_volume_available (removable_device_urn, TRUE);

	mount_path_file = g_file_new_for_path (mount_path);
	mount_path_uri = g_file_get_uri (mount_path_file);

	delete_q = g_strdup_printf ("DELETE { <%s> tracker:mountPoint ?d } WHERE { <%s> tracker:mountPoint ?d }", 
				    removable_device_urn, removable_device_urn);
	set_q = g_strdup_printf ("INSERT { <%s> a tracker:Volume; tracker:mountPoint <%s> }", 
				 removable_device_urn, mount_path_uri);

	tracker_data_update_sparql (delete_q, &error);

	if (error) {
		g_critical ("%s", error->message);
		g_error_free (error);
		error = NULL;
	}

	tracker_data_update_sparql (set_q, &error);

	if (error) {
		g_critical ("%s", error->message);
		g_error_free (error);
	}

	g_free (set_q);
	g_free (delete_q);

	delete_q = g_strdup_printf ("DELETE { <%s> tracker:isMounted ?d } WHERE { <%s> tracker:isMounted ?d }", 
				    removable_device_urn, removable_device_urn);
	set_q = g_strdup_printf ("INSERT { <%s> a tracker:Volume; tracker:isMounted true }", 
				 removable_device_urn);

	tracker_data_update_sparql (delete_q, &error);

	if (error) {
		g_critical ("%s", error->message);
		g_error_free (error);
		error = NULL;
	}

	tracker_data_update_sparql (set_q, &error);

	if (error) {
		g_critical ("%s", error->message);
		g_error_free (error);
	}

	g_free (set_q);
	g_free (delete_q);

	g_free (mount_path_uri);
	g_object_unref (mount_path_file);
	g_free (removable_device_urn);
}

void
tracker_data_update_reset_volume (const gchar *uri)
{
	time_t mnow;
	gchar *now_as_string;
	gchar *delete_q;
	gchar *set_q;
	GError *error = NULL;

	mnow = time (NULL);
	now_as_string = tracker_date_to_string (mnow);
	delete_q = g_strdup_printf ("DELETE { <%s> tracker:unmountDate ?d } WHERE { <%s> tracker:unmountDate ?d }", uri, uri);
	set_q = g_strdup_printf ("INSERT { <%s> a tracker:Volume; tracker:unmountDate \"%s\" }", 
				 uri, now_as_string);

	tracker_data_update_sparql (delete_q, &error);

	if (error) {
		g_critical ("%s", error->message);
		g_error_free (error);
		error = NULL;
	}

	tracker_data_update_sparql (set_q, &error);

	if (error) {
		g_critical ("%s", error->message);
		g_error_free (error);
	}

	g_free (now_as_string);
	g_free (set_q);
	g_free (delete_q);
}

void
tracker_data_update_disable_volume (const gchar *udi)
{
	gchar *removable_device_urn;
	gchar *delete_q;
	gchar *set_q;
	GError *error = NULL;

	g_return_if_fail (udi != NULL);

	removable_device_urn = g_strdup_printf (TRACKER_DATASOURCE_URN_PREFIX "%s", udi);

	db_set_volume_available (removable_device_urn, FALSE);

	tracker_data_update_reset_volume (removable_device_urn);

	delete_q = g_strdup_printf ("DELETE { <%s> tracker:isMounted ?d } WHERE { <%s> tracker:isMounted ?d }", 
				    removable_device_urn, removable_device_urn);
	set_q = g_strdup_printf ("INSERT { <%s> a tracker:Volume; tracker:isMounted false }", 
				 removable_device_urn);

	tracker_data_update_sparql (delete_q, &error);

	if (error) {
		g_critical ("%s", error->message);
		g_error_free (error);
		error = NULL;
	}

	tracker_data_update_sparql (set_q, &error);

	if (error) {
		g_critical ("%s", error->message);
		g_error_free (error);
	}

	g_free (set_q);
	g_free (delete_q);

	g_free (removable_device_urn);
}

void
tracker_data_update_disable_all_volumes (void)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	gchar *delete_q, *set_q;
	GError *error = NULL;

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface,
		"UPDATE \"rdfs:Resource\" SET Available = 0 "
		"WHERE ID IN ("
			"SELECT ID FROM \"nie:DataObject\" "
			"WHERE \"nie:dataSource\" IN ("
				"SELECT ID FROM \"rdfs:Resource\" WHERE Uri != ?"
			")"
		")");
	tracker_db_statement_bind_text (stmt, 0, TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN);
	tracker_db_statement_execute (stmt, NULL);
	g_object_unref (stmt);

	delete_q = g_strdup_printf ("DELETE { ?o tracker:isMounted ?d } WHERE { ?o tracker:isMounted ?d  FILTER (?o != <"TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN"> ) }");
	set_q = g_strdup_printf ("INSERT { ?o a tracker:Volume; tracker:isMounted false } WHERE { ?o a tracker:Volume FILTER (?o != <"TRACKER_NON_REMOVABLE_MEDIA_DATASOURCE_URN"> ) }");

	tracker_data_update_sparql (delete_q, &error);

	if (error) {
		g_critical ("%s", error->message);
		g_error_free (error);
		error = NULL;
	}

	tracker_data_update_sparql (set_q, &error);

	if (error) {
		g_critical ("%s", error->message);
		g_error_free (error);
	}

	g_free (set_q);
	g_free (delete_q);
}

void
tracker_data_begin_transaction (void)
{
	TrackerDBInterface *iface;

	if (transaction_level == 0) {
		update_buffer.resource_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		update_buffer.predicates = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, (GDestroyNotify) g_value_array_free);
		update_buffer.tables = g_hash_table_new_full (g_str_hash, g_str_equal,
							      g_free, (GDestroyNotify) cache_table_free);
		if (blank_buffer.table == NULL) {
			blank_buffer.table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
		}

		iface = tracker_db_manager_get_db_interface ();

		tracker_db_interface_start_transaction (iface);
	}

	transaction_level++;
}

void
tracker_data_commit_transaction (void)
{
	TrackerDBInterface *iface;

	transaction_level--;

	if (transaction_level == 0) {
		tracker_data_update_buffer_flush ();

		if (update_buffer.fts_ever_updated) {
			tracker_fts_update_commit ();
			update_buffer.fts_ever_updated = FALSE;
		}

		iface = tracker_db_manager_get_db_interface ();

		tracker_db_interface_end_transaction (iface);

		g_hash_table_unref (update_buffer.resource_cache);
		g_hash_table_unref (update_buffer.predicates);
		g_hash_table_unref (update_buffer.tables);

		if (commit_callback) {
			commit_callback (commit_data);
		}
	}
}

static void
format_sql_value_as_string (GString         *sql,
                            TrackerProperty *property)
{
	switch (tracker_property_get_data_type (property)) {
	case TRACKER_PROPERTY_TYPE_RESOURCE:
		g_string_append_printf (sql, "(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s\")", tracker_property_get_name (property));
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
tracker_data_delete_resource_description (const gchar *uri)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBCursor    *cursor, *single_cursor, *multi_cursor;
	TrackerClass	   *class;
	GString		   *sql;
	TrackerProperty	  **properties, **property;
	const gchar	   *class_uri, *value;
	int		    i;
	gboolean            first;
	gint                resource_id;

	resource_id = tracker_data_query_resource_id (uri);

	iface = tracker_db_manager_get_db_interface ();

	properties = tracker_ontology_get_properties ();

	stmt = tracker_db_interface_create_statement (iface, "SELECT (SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"rdf:type\") FROM \"rdfs:Resource_rdf:type\" WHERE ID = ?");
	tracker_db_statement_bind_int (stmt, 0, resource_id);
	cursor = tracker_db_statement_start_cursor (stmt, NULL);
	g_object_unref (stmt);

	if (cursor) {
		while (tracker_db_cursor_iter_next (cursor)) {
			class_uri = tracker_db_cursor_get_string (cursor, 0);

			class = tracker_ontology_get_class_by_uri (class_uri);
			if (class == NULL) {
				g_warning ("Class '%s' not found in the ontology", class_uri);
				continue;
			}

			/* retrieve single value properties for current class */

			sql = g_string_new ("SELECT ");

			first = TRUE;
			for (property = properties; *property; property++) {
				if (tracker_property_get_domain (*property) == class) {
					if (!tracker_property_get_embedded (*property)) {
						continue;
					}

					if (!tracker_property_get_multiple_values (*property)) {
						if (!first) {
							g_string_append (sql, ", ");
						}
						first = FALSE;

						format_sql_value_as_string (sql, *property);
					}
				}
			}

			if (!first) {
				g_string_append_printf (sql, " FROM \"%s\" WHERE ID = ?", tracker_class_get_name (class));
				stmt = tracker_db_interface_create_statement (iface, "%s", sql->str);
				tracker_db_statement_bind_int (stmt, 0, resource_id);
				single_cursor = tracker_db_statement_start_cursor (stmt, NULL);
				tracker_db_cursor_iter_next (single_cursor);
				g_object_unref (stmt);
			}

			g_string_free (sql, TRUE);

			i = 0;
			for (property = properties; *property; property++) {
				if (tracker_property_get_domain (*property) != class) {
					continue;
				}

				if (!tracker_property_get_embedded (*property)) {
					continue;
				}

				if (strcmp (tracker_property_get_uri (*property), RDF_PREFIX "type") == 0) {
					/* Do not delete rdf:type statements */
					continue;
				}

				if (!tracker_property_get_multiple_values (*property)) {
					/* single value property, value in single_result_set */

					value = tracker_db_cursor_get_string (single_cursor, i++);

					if (value) {
						tracker_data_delete_statement (uri, 
						                               tracker_property_get_uri (*property), 
						                               value, 
						                               NULL);
					}

				} else {
					/* multi value property, retrieve values from DB */

					sql = g_string_new ("SELECT ");

					format_sql_value_as_string (sql, *property);

					g_string_append_printf (sql,
								" FROM \"%s_%s\" WHERE ID = ?",
								tracker_class_get_name (tracker_property_get_domain (*property)),
								tracker_property_get_name (*property));

					stmt = tracker_db_interface_create_statement (iface, "%s", sql->str);
					tracker_db_statement_bind_int (stmt, 0, resource_id);
					multi_cursor = tracker_db_statement_start_cursor (stmt, NULL);
					g_object_unref (stmt);

					if (multi_cursor) {
						while (tracker_db_cursor_iter_next (multi_cursor)) {

							value = tracker_db_cursor_get_string (multi_cursor, 0);

							tracker_data_delete_statement (uri, 
							                               tracker_property_get_uri (*property), 
							                               value,
							                               NULL);

						}

						g_object_unref (multi_cursor);
					}

					g_string_free (sql, TRUE);
				}
			}

			if (!first) {
				g_object_unref (single_cursor);
			}

		}

		g_object_unref (cursor);
	}
}


void
tracker_data_update_sparql (const gchar  *update,
			    GError      **error)
{
	TrackerSparqlQuery *sparql_query;

	g_return_if_fail (update != NULL);

	sparql_query = tracker_sparql_query_new_update (update);

	tracker_sparql_query_execute (sparql_query, error);

	g_object_unref (sparql_query);
}

