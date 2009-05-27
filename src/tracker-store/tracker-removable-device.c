/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 *
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
 *
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"


#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gmodule.h>

#include <raptor.h>

#include <libtracker-data/tracker-data-query.h>
#include <libtracker-data/tracker-data-update.h>
#include <libtracker-db/tracker-db-manager.h>

#include <libtracker-data/tracker-turtle.h>

#include "tracker-removable-device.h"
#include "tracker-store.h"

typedef struct {
	gchar *last_subject;
	gchar *base;
	guint amount;
	GHashTable *metadata;
} TurtleStorerInfo;

typedef enum {
	REMOVAL,
	REPLACE,
	MOVE
} StorerTask;

typedef struct {
	gchar *destination;
	GHashTable *metadata;
} MoveInfo;

static void
foreach_in_metadata_set_metadata (gpointer      predicate,
				  gpointer      value,
				  gpointer      user_data)
{
	const gchar *uri = user_data;
	TrackerProperty *field;
	const gchar *pred_uri;

	field = tracker_ontology_get_property_by_uri (predicate);

	if (!field)
		return;

	pred_uri = tracker_property_get_uri (field);

	if (!value) {
		
		gchar *query = g_strdup_printf ("DELETE { <%s> <%s> ?o } WHERE { <%s> <%s> ?o }",
		                                uri, pred_uri,
		                                uri, pred_uri);
		tracker_store_queue_sparql_update (query, NULL, NULL, NULL);
		g_free (query);
	} else {
		tracker_store_queue_insert_statement (uri, pred_uri, value, 
		                                      NULL, NULL, NULL);
	}
}

static void 
replace_service (const gchar *uri,
		 GHashTable  *metadata)
{
	TrackerDBResultSet  *result_set;
	GError        *error = NULL;
	gchar         *escaped_uri;
	gchar         *query;

	g_return_if_fail (uri != NULL);
	g_return_if_fail (metadata != NULL);


	escaped_uri = tracker_escape_string (uri);

	query = g_strdup_printf ("SELECT ?t WHERE { <%s> tracker:modified ?t }",
	                         escaped_uri);

	result_set = tracker_store_sparql_query (query, &error);

	g_free (query);

	if (error) {
		g_error_free (error);
	}

	if (result_set) {
		GValue       vt_value = { 0, };
		const gchar *t_value;
		const gchar *modified;

		_tracker_db_result_set_get_value (result_set, 0, &vt_value);
		t_value = g_value_get_string (&vt_value);

		modified = g_hash_table_lookup (metadata, "resource:Modified");

		if (g_strcmp0 (t_value, modified) < 0) {
			g_hash_table_remove (metadata, "resource:Modified");

			g_hash_table_foreach (metadata, 
					      foreach_in_metadata_set_metadata,
					      (gpointer) uri);
		}

		g_value_unset (&vt_value);
		g_object_unref (result_set);

	} else {

		g_hash_table_remove (metadata, "resource:Modified");

		g_hash_table_foreach (metadata, 
				      foreach_in_metadata_set_metadata,
				      (gpointer) uri);
	}

	g_free (escaped_uri);

}


static void
free_move_info (gpointer user_data)
{
	MoveInfo *move_info = user_data;
	g_free (move_info->destination);
	g_hash_table_unref (move_info->metadata);
	g_slice_free (MoveInfo, move_info);
}

static void
move_on_deleted (GError *error, gpointer user_data)
{
	if (!error) {
		MoveInfo *move_info = user_data;
		replace_service (move_info->destination, 
		                 move_info->metadata);
	}
}

static void
commit_turtle_parse_info_storer (TurtleStorerInfo *info, 
				 gboolean          may_flush, 
				 StorerTask        task,
				 const gchar      *destination)
{
	if (info->last_subject) {
		MoveInfo *move_info;
		gchar *sparql = NULL;

		switch (task) {
		    case REMOVAL:
			sparql = g_strdup_printf ("DELETE { <%s> a rdfs:Resource }", info->last_subject);
			tracker_store_queue_sparql_update (sparql, NULL, NULL, NULL);
			g_free (sparql);
		    break;
		    case MOVE:

			move_info = g_slice_new (MoveInfo);
			move_info->destination = g_strdup (destination);
			move_info->metadata = g_hash_table_ref (info->metadata);

			sparql = g_strdup_printf ("DELETE { <%s> a rdfs:Resource }", info->last_subject);
			tracker_store_queue_sparql_update (sparql, move_on_deleted,
			                                   move_info, 
			                                   free_move_info);
			g_free (sparql);

		    break;
		    default:
		    case REPLACE:
			replace_service (info->last_subject, 
			                 info->metadata);
		    break;
		}

		info->amount++;

		g_hash_table_unref (info->metadata);
		g_free (info->last_subject);
		info->last_subject = NULL;
		info->metadata = NULL;
	}
}

static void
consume_triple_storer (const gchar *subject,
                       const gchar *predicate,
                       const gchar *object,
                       void        *user_data) 
{
	TurtleStorerInfo *info = user_data;

	if (!object || !predicate)
		return;

	if (g_strcmp0 (subject, info->last_subject) != 0) {
		/* Commit previous subject */
		commit_turtle_parse_info_storer (info, TRUE, REPLACE, NULL);

		/* Install next subject */
		info->last_subject = g_strdup (subject);
		info->metadata = g_hash_table_new_full (g_str_hash, g_str_equal,
							(GDestroyNotify) g_free,
							(GDestroyNotify) g_free);
	}

	if (object[0] == '\0' && predicate[0] == '\0') {
		/* <URI> <rdf:type> "Type" ;                    *
		 *       <> <>  -  is a removal of the resource */

		/* We commit this subject as a removal, the last_subject
		 * field will be cleared for the next subject to be set
		 * ready first next process loop. */

		commit_turtle_parse_info_storer (info, FALSE, REMOVAL, NULL);
	} else
	if (object[0] == '\0' && predicate[0] != '\0') {
		/* <URI> <rdf:type> "Type" ;                             *
		 *       <Pfx:Predicate> <>  -  is a removal of the      * 
		 *                              resource's Pfx:Predicate */

		/* We put NULL here, so that a null value goes into 
		 * SQLite. Perhaps we should change this? If so, Why? */

		g_hash_table_replace (info->metadata,
				      g_strdup (predicate),
				      NULL);
	} else
	if (object[0] != '\0' && predicate[0] == '\0') {
		/* <URI> <> <to-URI>  -  is a move of the subject */
		commit_turtle_parse_info_storer (info, FALSE, MOVE, object);
	} else {
		g_hash_table_replace  (info->metadata,
				       g_strdup (predicate),
				       g_strdup (object));
	}
}

static void
process_turtle_statement (const gchar     *subject,
                          const gchar     *predicate,
                          const gchar     *object_,
                          gpointer         user_data)
{
	if (subject && predicate && object_) {
		consume_triple_storer (subject, predicate, object_, 
		                       user_data);
	}
}

static void
process_turtle_destroy (gpointer user_data)
{
	TurtleStorerInfo *info = user_data;

	commit_turtle_parse_info_storer (info, FALSE, REPLACE, NULL);

	tracker_store_queue_commit (NULL, NULL, NULL);

	g_free (info->base);
	g_free (info);
}

void
tracker_removable_device_load (const gchar    *mount_point)
{
	gchar *filename;

	filename = g_build_filename (mount_point,
				     ".cache", 
				     "metadata",
				     "metadata.ttl", 
				     NULL);

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		TurtleStorerInfo *info;
		gchar            *base_uri, *tmp;
		GFile            *mountp_file;

		info = g_new0 (TurtleStorerInfo, 1);

		info->amount = 0;
		info->base = g_strdup (mount_point);

		mountp_file = g_file_new_for_path (mount_point);
		tmp = g_file_get_uri (mountp_file);
		base_uri = g_strconcat (tmp, "/", NULL);

		/*
		tracker_store_queue_turtle_import (filename, 
		                                   base_uri,
		                                   process_turtle_statement,
		                                   info,
		                                   process_turtle_destroy);
		*/

		g_debug ("Support for removable device cache not yet ported to new TrackerStore API");

		g_free (base_uri);
		g_free (tmp);
		g_object_unref (mountp_file);
	}

	g_free (filename);
}
