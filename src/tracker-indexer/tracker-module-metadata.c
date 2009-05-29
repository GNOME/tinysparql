/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib.h>
#include <string.h>
#include <time.h>
#include <libtracker-common/tracker-sparql-builder.h>
#include <libtracker-common/tracker-type-utils.h>
#include "tracker-module-metadata-private.h"

typedef struct _Statement Statement;

struct _Statement {
	gchar *subject;
	gchar *predicate;
	gchar *object;
};

struct TrackerModuleMetadata {
	GObject parent_instance;
	GArray *statements;
};

struct TrackerModuleMetadataClass {
	GObjectClass parent_class;
};


static void   tracker_module_metadata_finalize   (GObject *object);


G_DEFINE_TYPE (TrackerModuleMetadata, tracker_module_metadata, G_TYPE_OBJECT)

static void
tracker_module_metadata_class_init (TrackerModuleMetadataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_module_metadata_finalize;
}

static void
tracker_module_metadata_init (TrackerModuleMetadata *metadata)
{
	metadata->statements = g_array_new (FALSE, TRUE, sizeof (Statement));
}

static void
tracker_module_metadata_finalize (GObject *object)
{
	TrackerModuleMetadata *metadata;
	gint i;

	metadata = TRACKER_MODULE_METADATA (object);

	for (i = 0; i < metadata->statements->len; i++) {
		Statement *stmt;

		stmt = &g_array_index (metadata->statements, Statement, i);
		g_free (stmt->subject);
		g_free (stmt->predicate);
		g_free (stmt->object);
	}

	g_array_free (metadata->statements, TRUE);

	G_OBJECT_CLASS (tracker_module_metadata_parent_class)->finalize (object);
}

/**
 * tracker_module_metadata_add_take_string:
 * @metadata: A #TrackerModuleMetadata
 * @subject: The subject this metadata applies to (for example, a URI).
 * @predicate: The key to which the value will be assigned.
 * @value: Value for the metadata
 *
 * Adds a new metadata element as a string into @metadata.
 * For ontology fields that can take several values, this
 * function will allow adding several elements to the same
 * field name.
 *
 * If the function returns #TRUE, @metadata will take
 * ownership on @value, else you are responsible of
 * freeing it.
 *
 * Returns: #TRUE if the value was added successfully.
 **/
gboolean
tracker_module_metadata_add_take_string (TrackerModuleMetadata *metadata,
					 const gchar           *subject,
					 const gchar           *predicate,
					 gchar                 *value)
{
	Statement stmt;

	g_return_val_if_fail (metadata != NULL, FALSE);
	g_return_val_if_fail (subject != NULL, FALSE);
	g_return_val_if_fail (predicate != NULL, FALSE);

	if (!value) {
		return FALSE;
	}

	stmt.subject = g_strdup (subject);
	stmt.predicate = g_strdup (predicate);
	stmt.object = value;

	g_array_append_val (metadata->statements, stmt);

	return TRUE;
}

/**
 * tracker_module_metadata_add_string:
 * @metadata: A #TrackerModuleMetadata
 * @subject: The subject this metadata applies to (for example, a URI).
 * @predicate: The key to which the value will be assigned.
 * @value: Value for the metadata
 *
 * Adds a new metadata element as a string into @metadata.
 * see tracker_module_metadata_add_take_string().
 **/
void
tracker_module_metadata_add_string (TrackerModuleMetadata *metadata,
				    const gchar           *subject,
				    const gchar           *predicate,
				    const gchar           *value)
{
	gchar *str;

	str = g_strdup (value);

	if (!tracker_module_metadata_add_take_string (metadata, subject, predicate, str)) {
		g_free (str);
	}
}

/**
 * tracker_module_metadata_add_int:
 * @metadata: A #TrackerModuleMetadata
 * @subject: The subject this metadata applies to (for example, a URI).
 * @predicate: The key to which the value will be assigned.
 * @value: Value for the metadata
 *
 * Adds a new metadata element as a integer into @metadata.
 * see tracker_module_metadata_add_take_string().
 **/
void
tracker_module_metadata_add_int (TrackerModuleMetadata *metadata,
				 const gchar           *subject,
				 const gchar           *predicate,
				 gint                   value)
{
	gchar *str;

	str = tracker_gint_to_string (value);

	if (!tracker_module_metadata_add_take_string (metadata, subject, predicate, str)) {
		g_free (str);
	}
}

/**
 * tracker_module_metadata_add_uint:
 * @metadata: A #TrackerModuleMetadata
 * @subject: The subject this metadata applies to (for example, a URI).
 * @predicate: The key to which the value will be assigned.
 * @value: Value for the metadata
 *
 * Adds a new metadata element as an unsigned integer into @metadata.
 * see tracker_module_metadata_add_take_string().
 **/
void
tracker_module_metadata_add_uint (TrackerModuleMetadata *metadata,
				  const gchar           *subject,
				  const gchar           *predicate,
				  guint                  value)
{
	gchar *str;

	str = tracker_guint_to_string (value);

	if (!tracker_module_metadata_add_take_string (metadata, subject, predicate, str)) {
		g_free (str);
	}
}

/**
 * tracker_module_metadata_add_double:
 * @metadata: A #TrackerModuleMetadata
 * @subject: The subject this metadata applies to (for example, a URI).
 * @predicate: The key to which the value will be assigned.
 * @value: Value for the metadata
 *
 * Adds a new metadata element as a double into @metadata.
 * see tracker_module_metadata_add_take_string().
 **/
void
tracker_module_metadata_add_double (TrackerModuleMetadata *metadata,
				    const gchar           *subject,
				    const gchar           *predicate,
				    gdouble                value)
{
	gchar *str;

	str = g_strdup_printf ("%f", value);

	if (!tracker_module_metadata_add_take_string (metadata, subject, predicate, str)) {
		g_free (str);
	}
}

/**
 * tracker_module_metadata_add_float:
 * @metadata: A #TrackerModuleMetadata
 * @subject: The subject this metadata applies to (for example, a URI).
 * @predicate: The key to which the value will be assigned.
 * @value: Value for the metadata
 *
 * Adds a new metadata element as a float into @metadata.
 * see tracker_module_metadata_add_take_string().
 **/
void
tracker_module_metadata_add_float (TrackerModuleMetadata *metadata,
				   const gchar           *subject,
				   const gchar           *predicate,
				   gfloat                 value)
{
	gchar *str;

	str = g_strdup_printf ("%f", value);

	if (!tracker_module_metadata_add_take_string (metadata, subject, predicate, str)) {
		g_free (str);
	}
}

/**
 * tracker_module_metadata_add_date:
 * @metadata: A #TrackerModuleMetadata
 * @subject: The subject this metadata applies to (for example, a URI).
 * @predicate: The key to which the value will be assigned.
 * @value: Value for the metadata
 *
 * Adds a new metadata element as a time_t into @metadata.
 * see tracker_module_metadata_add_take_string().
 **/
void
tracker_module_metadata_add_date (TrackerModuleMetadata *metadata,
				  const gchar           *subject,
				  const gchar           *predicate,
				  time_t                 value)
{
	struct tm t;
	gchar *str;

	gmtime_r (&value, &t);
	str = g_strdup_printf ("%04d-%02d-%02dT%02d:%02d:%02d",
	                       t.tm_year + 1900,
	                       t.tm_mon + 1,
	                       t.tm_mday,
	                       t.tm_hour,
	                       t.tm_min,
	                       t.tm_sec);

	if (!tracker_module_metadata_add_take_string (metadata, subject, predicate, str)) {
		g_free (str);
	}
}

/**
 * tracker_module_metadata_foreach:
 * @metadata: A #TrackerModuleMetadata.
 * @func: The function to call with each metadata.
 * @user_data: user data to pass to the function.
 *
 * Calls a function for each element in @metadata.
 **/
void
tracker_module_metadata_foreach (TrackerModuleMetadata        *metadata,
				 TrackerModuleMetadataForeach  func,
				 gpointer		       user_data)
{
	gint i;

	for (i = 0; i < metadata->statements->len; i++) {
		Statement *stmt;

		stmt = &g_array_index (metadata->statements, Statement, i);
		func (stmt->subject, stmt->predicate, stmt->object, user_data);
	}
}

gchar *
tracker_module_metadata_get_sparql (TrackerModuleMetadata        *metadata)
{
	TrackerSparqlBuilder *sparql;
	gint     i;
	gchar *result;

	sparql = tracker_sparql_builder_new_update ();
	tracker_sparql_builder_insert_open (sparql);

	for (i = 0; i < metadata->statements->len; i++) {
		Statement *stmt;
		gchar     *object;

		stmt = &g_array_index (metadata->statements, Statement, i);

		tracker_sparql_builder_subject_iri (sparql, stmt->subject);
		tracker_sparql_builder_predicate_iri (sparql, stmt->predicate);
		tracker_sparql_builder_object_string (sparql, stmt->object);
	}

	tracker_sparql_builder_insert_close (sparql);

	result = g_strdup (tracker_sparql_builder_get_result (sparql));

	g_object_unref (sparql);

	return result;
}

/**
 * tracker_module_metadata_new:
 *
 * Creates a new #TrackerModuleMetadata
 *
 * Returns: A newly created #TrackerModuleMetadata
 **/
TrackerModuleMetadata *
tracker_module_metadata_new (void)
{
	return g_object_new (TRACKER_TYPE_MODULE_METADATA, NULL);
}
