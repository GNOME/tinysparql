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

#include <gio/gio.h>
#include <glib/gstdio.h>

#include "tracker-turtle.h"

static gboolean initialized = FALSE;

struct TurtleFile {
	FILE              *file;
#ifdef HAVE_RAPTOR
	raptor_uri        *uri;
	raptor_serializer *serializer;
#endif /* HAVE_RAPTOR */
};

#ifdef HAVE_RAPTOR
typedef struct {
	gchar             *last_subject;
	raptor_serializer *serializer;
	GHashTable        *hash;
} TurtleOptimizerInfo;
#endif /* HAVE_RAPTOR */

void
tracker_turtle_init (void)
{
	if (!initialized) {
#ifdef HAVE_RAPTOR
		raptor_init ();
#endif /* HAVE_RAPTOR */
		initialized = TRUE;
	}
}

void
tracker_turtle_shutdown (void)
{
	if (initialized) {
#ifdef HAVE_RAPTOR
		raptor_finish ();
#endif /* HAVE_RAPTOR */
		initialized = FALSE;
	}
}

#ifdef HAVE_RAPTOR

static void
foreach_in_hash (gpointer key,
		 gpointer value, 
		 gpointer user_data)
{
	raptor_statement    *statement;
	TurtleOptimizerInfo *item = user_data;
	const guchar        *about_uri = (const guchar *) item->last_subject;
	raptor_serializer   *serializer = item->serializer;

	statement = g_new0 (raptor_statement, 1);

	statement->subject = (void *) raptor_new_uri (about_uri);
	statement->subject_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;

	statement->predicate = (void *) raptor_new_uri (key);
	statement->predicate_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;

	statement->object = (unsigned char *) g_strdup (value);
	statement->object_type = RAPTOR_IDENTIFIER_TYPE_LITERAL;

	raptor_serialize_statement (serializer, 
				    statement);

	raptor_free_uri ((raptor_uri *) statement->subject);
	raptor_free_uri ((raptor_uri *) statement->predicate);
	g_free ((unsigned char *) statement->object);

	g_free (statement);
}

static void
commit_turtle_parse_info_optimizer (TurtleOptimizerInfo *info)
{
	if (info->last_subject) {
		g_hash_table_foreach (info->hash, 
				      foreach_in_hash,
				      info);

		g_hash_table_destroy (info->hash);
		g_free (info->last_subject);
		info->last_subject = NULL;
		info->hash = NULL;
	}
}

static void
consume_triple_optimizer (void                   *user_data, 
			  const raptor_statement *triple) 
{
	TurtleOptimizerInfo *info = user_data;
	gchar               *subject;
	gchar               *predicate;

	subject = (gchar *) raptor_uri_as_string ((raptor_uri *) triple->subject);
	predicate = (gchar *) raptor_uri_as_string ((raptor_uri *) triple->predicate);

	if (!info->last_subject || strcmp (subject, info->last_subject) != 0) {
		/* Commit previous subject */
		commit_turtle_parse_info_optimizer (info);
		info->last_subject = g_strdup (subject);
		info->hash = g_hash_table_new_full (g_str_hash, g_str_equal,
						    (GDestroyNotify) g_free,
						    (GDestroyNotify) g_free);
	}

	if (triple->object_type == RAPTOR_IDENTIFIER_TYPE_RESOURCE) {
		/* TODO: these checks for removals of resources and predicates 
		 * are incorrect atm */
		if (g_str_has_suffix (predicate, ":")) {
			/* <URI> <> <> */
			g_hash_table_destroy (info->hash);
			g_free (info->last_subject);
			info->last_subject = NULL;
			info->hash = NULL;
		} else {
			/* <URI> <Pfx:Predicate> <> */
			g_hash_table_remove (info->hash, predicate);
		}
	} else {
		/* TODO: Add conflict resolution here (if any is needed) */
		g_hash_table_replace (info->hash,
				      g_strdup (predicate),
				      g_strdup (triple->object));
	}
}

static void
foreach_in_metadata (TrackerField *field, 
		     gpointer      value, 
		     gpointer      user_data)
{
	raptor_statement          *statement;
	TrackerTurtleMetadataItem *item = user_data;
	const guchar              *about_uri = (const guchar *) item->about_uri;
	TurtleFile                *turtle = item->turtle;
	raptor_serializer         *serializer = turtle->serializer;

	/* TODO: cope with group values
	 *
	 * If you want to reuse the importer of tracker-indexer (for the remov-
	 * able devices), then you'll need to ensure that the predicates 
	 * File:Modified and rdf:type are added per record (you separate triples
	 * using a ; and you end a record using a . (a dot).
	 *
	 * Also look at tracker-indexer/tracker-removable-device.c */

	statement = g_new0 (raptor_statement, 1);

	statement->subject = (void *) raptor_new_uri (about_uri);
	statement->subject_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;

	statement->predicate = (void *) raptor_new_uri ((unsigned char *) tracker_field_get_name (field));
	statement->predicate_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;

	statement->object = (unsigned char *) g_strdup (value);
	statement->object_type = RAPTOR_IDENTIFIER_TYPE_LITERAL;

	raptor_serialize_statement (serializer, 
				    statement);

	raptor_free_uri ((raptor_uri *) statement->subject);
	raptor_free_uri ((raptor_uri *) statement->predicate);
	g_free ((unsigned char *) statement->object);

	g_free (statement);
}
#endif /* HAVE_RAPTOR */

TurtleFile *
tracker_turtle_open (const gchar *turtle_file)
{
#ifdef HAVE_RAPTOR
	TurtleFile *turtle;

	g_return_val_if_fail (initialized, NULL);

	turtle = g_new0 (TurtleFile, 1);

	turtle->file = tracker_file_open (turtle_file, "a+",  TRUE);
	if (!turtle->file) {
		return NULL;
	}

	turtle->serializer = raptor_new_serializer ("turtle");
	turtle->uri = raptor_new_uri ((unsigned char *) "/");
	raptor_serialize_start_to_file_handle (turtle->serializer, 
					       turtle->uri, 
					       turtle->file);

	return turtle;
#else 
	g_return_val_if_fail (initialized, NULL);

	return NULL;
#endif
}

void 
tracker_turtle_add_metadata (TurtleFile          *turtle,
			     const gchar         *uri,
			     TrackerDataMetadata *metadata)
{
	if (!initialized) {
		g_critical ("Using tracker_turtle module without initialization");
	}

#ifdef HAVE_RAPTOR
	g_return_if_fail (turtle != NULL);
	TrackerTurtleMetadataItem *info = g_slice_new (TrackerTurtleMetadataItem);

	info->about_uri = (gchar *) uri;
	info->metadata = metadata;
	info->turtle = turtle;

	tracker_data_metadata_foreach (metadata, 
				       foreach_in_metadata,
				       info);

	g_slice_free (TrackerTurtleMetadataItem, info);
#endif /* HAVE_RAPTOR */
}

void 
tracker_turtle_add_metadatas (TurtleFile *turtle,
			      GPtrArray  *metadata_items)
{
	if (!initialized) {
		g_critical ("Using tracker_turtle module without initialization");
	}

#ifdef HAVE_RAPTOR
	guint count;
	g_return_if_fail (turtle != NULL);
	g_return_if_fail (metadata_items != NULL);

	for (count = 0; count < metadata_items->len; count++) {
		TrackerTurtleMetadataItem *item = g_ptr_array_index (metadata_items, count);

		item->turtle = turtle;

		tracker_data_metadata_foreach (item->metadata, 
					       foreach_in_metadata,
					       item);
	}
#endif /* HAVE_RAPTOR */
}

void
tracker_turtle_add_triple (TurtleFile   *turtle,
			   const gchar  *uri,
			   TrackerField *property,
			   const gchar  *value)
{

	if (!initialized) {
		g_critical ("Using tracker_turtle module without initialization");
	}
	
#ifdef HAVE_RAPTOR
	g_return_if_fail (turtle != NULL); 
	g_return_if_fail (uri != NULL);
	g_return_if_fail (property != NULL);
	g_return_if_fail (value != NULL);

	TrackerTurtleMetadataItem *item = g_slice_new (TrackerTurtleMetadataItem);
	item->about_uri = (gchar *) uri;
	item->turtle = turtle;
	foreach_in_metadata (property, (gpointer) value, item);
	g_slice_free (TrackerTurtleMetadataItem, item);
#endif
}


void
tracker_turtle_close (TurtleFile *turtle)
{
#ifdef HAVE_RAPTOR
	g_return_if_fail (turtle != NULL);

	raptor_free_uri (turtle->uri);
	raptor_serialize_end (turtle->serializer);
	raptor_free_serializer(turtle->serializer);

	if (turtle->file) {
		tracker_file_close (turtle->file, FALSE);
	}

	g_free (turtle);
#endif
}

#ifdef HAVE_RAPTOR
static void 
raptor_error (void           *user_data, 
	      raptor_locator *locator, 
	      const gchar    *message)
{
	g_message ("RAPTOR parse error: %s for %s\n", 
		   message, 
		   (gchar *) user_data);
}
#endif

void
tracker_turtle_process (const gchar          *turtle_file,
			const gchar          *base_uri,
			TurtleTripleCallback  callback,
			void                 *user_data)
{
#ifdef HAVE_RAPTOR
	unsigned char  *uri_string;
	raptor_uri     *uri, *buri;
	raptor_parser  *parser;
#endif
	if (!initialized) {
		g_critical ("Using tracker_turtle module without initialization");
	}

#ifdef HAVE_RAPTOR

	parser = raptor_new_parser ("turtle");

	raptor_set_statement_handler (parser, user_data, (raptor_statement_handler) callback);
	raptor_set_fatal_error_handler (parser, (void *)turtle_file, raptor_error);
	raptor_set_error_handler (parser, (void *)turtle_file, raptor_error);
	raptor_set_warning_handler (parser, (void *)turtle_file, raptor_error);

	uri_string = raptor_uri_filename_to_uri_string (turtle_file);
	uri = raptor_new_uri (uri_string);
	buri = raptor_new_uri ((unsigned char *) base_uri);

	raptor_parse_file (parser, uri, buri);

	raptor_free_uri (uri);
	raptor_free_memory (uri_string);
	raptor_free_uri (buri);

	raptor_free_parser (parser);
#endif 	
}

void
tracker_turtle_optimize (const gchar *turtle_file)
{
#ifdef HAVE_RAPTOR
	raptor_uri          *suri;
	TurtleOptimizerInfo *info;
	gchar               *tmp_file, *base_uri;
	FILE                *target_file;
#endif

	if (!initialized) {
		g_critical ("Using tracker_turtle module without initialization");
	}

#ifdef HAVE_RAPTOR
	tmp_file = g_strdup_printf ("%s.tmp", turtle_file);

	target_file = g_fopen (tmp_file, "a");
	/* Similar to a+ */
	if (!target_file) 
		target_file = g_fopen (tmp_file, "w");

	if (!target_file) {
		g_free (target_file);
		g_free (tmp_file);
		return;
	}

	info = g_slice_new0 (TurtleOptimizerInfo);
	info->serializer = raptor_new_serializer ("turtle");
	suri = raptor_new_uri ((unsigned char *) "/");

	base_uri = g_filename_to_uri (turtle_file, NULL, NULL);

	raptor_serialize_start_to_file_handle (info->serializer, 
					       suri, target_file);

	tracker_turtle_process (turtle_file, base_uri, consume_triple_optimizer, info);

	g_free (base_uri);

	/* Commit final subject (or loop doesn't handle the very last) */
	commit_turtle_parse_info_optimizer (info);

	raptor_serialize_end (info->serializer);
	raptor_free_serializer (info->serializer);
	fclose (target_file);

	g_slice_free (TurtleOptimizerInfo, info);

	raptor_free_uri (suri);

	/* When we are finished we atomicly overwrite the original with
	 * our newly created .tmp file */

	g_rename (tmp_file, turtle_file);

	g_free (tmp_file);
#endif /* HAVE_RAPTOR */
}
