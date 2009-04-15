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

#include <uuid.h>

#include "tracker-turtle.h"

static gboolean  initialized = FALSE;
static GMutex   *turtle_mutex;
static GCond    *turtle_cond;

static gboolean turtle_first;
static gchar * volatile turtle_subject;
static gchar * volatile turtle_predicate;
static char * volatile turtle_object;
static volatile gboolean     turtle_eof;

typedef struct {
	gchar                *about_uri;
	TurtleFile           *turtle; /* For internal use only */
} TrackerTurtleMetadataItem;

struct TurtleFile {
	FILE              *file;
	raptor_uri        *uri;
	raptor_serializer *serializer;
};

typedef struct {
	gchar             *last_subject;
	raptor_serializer *serializer;
	GHashTable        *hash;
} TurtleOptimizerInfo;

typedef struct {
	gchar *file;
	gchar *base_uri;
} TurtleThreadData;

void
tracker_turtle_init (void)
{
	if (!initialized) {
		raptor_init ();
		initialized = TRUE;
	}
}

void
tracker_turtle_shutdown (void)
{
	if (initialized) {
		raptor_finish ();
		initialized = FALSE;
	}
}


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
foreach_in_metadata (TrackerProperty *field, 
		     gpointer         value, 
		     gpointer         user_data)
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

	statement->predicate = (void *) raptor_new_uri ((unsigned char *) tracker_property_get_name (field));
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



TurtleFile *
tracker_turtle_open (const gchar *turtle_file)
{
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
}

void
tracker_turtle_add_triple (TurtleFile   *turtle,
			   const gchar  *uri,
			   TrackerProperty *property,
			   const gchar  *value)
{
	TrackerTurtleMetadataItem *item = g_slice_new (TrackerTurtleMetadataItem);
	item->about_uri = (gchar *) uri;
	item->turtle = turtle;
	foreach_in_metadata (property, (gpointer) value, item);
	g_slice_free (TrackerTurtleMetadataItem, item);
}


void
tracker_turtle_close (TurtleFile *turtle)
{
	raptor_free_uri (turtle->uri);
	raptor_serialize_end (turtle->serializer);
	raptor_free_serializer(turtle->serializer);

	if (turtle->file) {
		tracker_file_close (turtle->file, FALSE);
	}

	g_free (turtle);
}

static void 
raptor_error (void           *user_data, 
	      raptor_locator *locator, 
	      const gchar    *message)
{
	g_message ("RAPTOR parse error: %s:%d:%d: %s\n", 
		   (gchar *) user_data,
		   locator->line,
		   locator->column,
		   message);
}

static unsigned char*
turtle_generate_id (void              *user_data,
                    raptor_genid_type  type,
                    unsigned char     *user_id)
{
	static gint id = 0;

	/* user_id is NULL for anonymous nodes */
	if (user_id == NULL) {
		return (guchar *) g_strdup_printf (":%d", ++id);
	} else {
		GChecksum   *checksum;
		const gchar *sha1;

		checksum = g_checksum_new (G_CHECKSUM_SHA1);
		/* base UUID, unique per file */
		g_checksum_update (checksum, user_data, 16);
		/* node ID */
		g_checksum_update (checksum, user_id, -1);

		sha1 = g_checksum_get_string (checksum);

		/* generate name based uuid */
		return (guchar *) g_strdup_printf (
			"urn:uuid:%.8s-%.4s-%.4s-%.4s-%.12s",
			sha1, sha1 + 8, sha1 + 12, sha1 + 16, sha1 + 20);
	}
}

static void
turtle_statement_handler (void                   *user_data,
                          const raptor_statement *triple) 
{
	g_mutex_lock (turtle_mutex);

	/* wait until last statement has been released */
	while (turtle_subject != NULL) {
		g_cond_wait (turtle_cond, turtle_mutex);
	}

	/* set new statement */
	turtle_subject = g_strdup ((const gchar *) raptor_uri_as_string ((raptor_uri *) triple->subject));
	turtle_predicate = g_strdup ((const gchar *) raptor_uri_as_string ((raptor_uri *) triple->predicate));
	turtle_object = g_strdup ((const gchar *) triple->object);

	/* signal main thread to pull statement */
	g_cond_signal (turtle_cond);

	g_mutex_unlock (turtle_mutex);
}

static gboolean
turtle_next (void)
{
	g_mutex_lock (turtle_mutex);

	/* release last statement */
	if (turtle_first) {
		/* never release first statement */
		turtle_first = FALSE;
	} else if (turtle_subject != NULL) {
		g_free (turtle_subject);
		g_free (turtle_predicate);
		g_free (turtle_object);

		turtle_subject = NULL;
		turtle_predicate = NULL;
		turtle_object = NULL;

		/* notify thread that last statement has been cleared */
		g_cond_signal (turtle_cond);
	}

	/* wait for new statement or EOF */
	while (turtle_subject == NULL && !turtle_eof) {
		g_cond_wait (turtle_cond, turtle_mutex);
	}

	g_mutex_unlock (turtle_mutex);

	return !turtle_eof;
}

static gpointer
turtle_thread_func (gpointer data)
{
	TurtleThreadData *thread_data;
	unsigned char  *uri_string;
	raptor_uri     *uri, *buri;
	raptor_parser  *parser;
	uuid_t          base_uuid;

	thread_data = (TurtleThreadData *) data;

	parser = raptor_new_parser ("turtle");

	/* generate UUID as base for blank nodes */
	uuid_generate (base_uuid);

	raptor_set_statement_handler (parser, NULL, (raptor_statement_handler) turtle_statement_handler);
	raptor_set_generate_id_handler (parser, base_uuid, turtle_generate_id);
	raptor_set_fatal_error_handler (parser, (void *)thread_data->file, raptor_error);
	raptor_set_error_handler (parser, (void *)thread_data->file, raptor_error);
	raptor_set_warning_handler (parser, (void *)thread_data->file, raptor_error);

	uri_string = raptor_uri_filename_to_uri_string (thread_data->file);
	uri = raptor_new_uri (uri_string);
	if (thread_data->base_uri != NULL) {
		buri = raptor_new_uri ((unsigned char *) thread_data->base_uri);
	} else {
		buri = NULL;
	}

	raptor_parse_file (parser, uri, buri);

	g_mutex_lock (turtle_mutex);

	/* wait until last statement has been released */
	while (turtle_subject != NULL) {
		g_cond_wait (turtle_cond, turtle_mutex);
	}

	turtle_eof = TRUE;

	/* signal main thread to pull eof */
	g_cond_signal (turtle_cond);

	g_mutex_unlock (turtle_mutex);

	raptor_free_uri (uri);
	raptor_free_memory (uri_string);
	if (buri != NULL) {
		raptor_free_uri (buri);
	}

	raptor_free_parser (parser);

	g_free (thread_data->file);
	g_free (thread_data->base_uri);
	g_free (thread_data);

	return NULL;
}

void
tracker_turtle_process (const gchar          *turtle_file,
			const gchar          *base_uri,
			TurtleTripleCallback  callback,
			void                 *user_data)
{
	unsigned char  *uri_string;
	raptor_uri     *uri, *buri;
	raptor_parser  *parser;

	if (!initialized) {
		g_critical ("Using tracker_turtle module without initialization");
	}

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
}

void
tracker_turtle_optimize (const gchar *turtle_file)
{
	raptor_uri          *suri;
	TurtleOptimizerInfo *info;
	gchar               *tmp_file, *base_uri;
	FILE                *target_file;

	if (!initialized) {
		g_critical ("Using tracker_turtle module without initialization");
	}

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
}

void
tracker_turtle_reader_init (const gchar *turtle_file,
                            const gchar *base_uri)
{
	GThread        *parser_thread;
	TurtleThreadData *thread_data;

	if (!initialized) {
		g_critical ("Using tracker_turtle module without initialization");
	}

	turtle_mutex = g_mutex_new ();
	turtle_cond = g_cond_new ();

	thread_data = g_new0 (TurtleThreadData, 1);
	thread_data->file = g_strdup (turtle_file);
	thread_data->base_uri = g_strdup (base_uri);

	turtle_first = TRUE;

	parser_thread = g_thread_create (turtle_thread_func, thread_data, FALSE, NULL);
}

gboolean
tracker_turtle_reader_next (void)
{
	if (turtle_next ()) {
		return TRUE;
	} else {
		/* EOF, cleanup */

		turtle_eof = FALSE;

		g_mutex_free (turtle_mutex);
		g_cond_free (turtle_cond);

		return FALSE;
	}
}

const gchar *
tracker_turtle_reader_get_subject (void)
{
	return turtle_subject;
}

const gchar *
tracker_turtle_reader_get_predicate (void)
{
	return turtle_predicate;
}

const gchar *
tracker_turtle_reader_get_object (void)
{
	return turtle_object;
}

