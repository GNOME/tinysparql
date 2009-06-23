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

#include <libtracker-data/tracker-turtle.h>

#include "tracker-removable-device.h"

#include "tracker-module-metadata-private.h"

typedef struct {
	gchar *last_subject;
	gchar *base;
	guint amount;
	TrackerIndexer *indexer;
	GHashTable *metadata;
} TurtleStorerInfo;

typedef enum {
	REMOVAL,
	REPLACE,
	MOVE
} StorerTask;

typedef struct {
	raptor_serializer *serializer;
	gchar *about_uri;
} AddMetadataInfo;

static void
commit_turtle_parse_info_storer (TurtleStorerInfo *info, 
				 gboolean          may_flush, 
				 StorerTask        task,
				 const gchar      *destination)
{
	if (info->last_subject) {
		switch (task) {
		    case REMOVAL:
			tracker_data_delete_resource (info->last_subject);
		    break;
		    case MOVE:
			tracker_data_delete_resource (info->last_subject);
#if 0
			tracker_data_update_replace_service (destination, 
							     info->metadata);
#endif
		    break;
		    default:
		    case REPLACE:
#if 0
			tracker_data_update_replace_service (info->last_subject, 
							     info->metadata);
#endif
		    break;
		}

		info->amount++;

		g_hash_table_destroy (info->metadata);
		g_free (info->last_subject);
		info->last_subject = NULL;
		info->metadata = NULL;
	}

	/* We commit per transaction of TRACKER_INDEXER_TRANSACTION_MAX here, 
	 * and then we also iterate the mainloop so that the state-machine 
	 * gets the opportunity to run for a moment */

	if (may_flush && info->amount > TRACKER_INDEXER_TRANSACTION_MAX) {
		tracker_indexer_transaction_commit (info->indexer);
		g_main_context_iteration (NULL, FALSE);
		tracker_indexer_transaction_open (info->indexer);
		info->amount = 0;
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

void
tracker_removable_device_optimize (TrackerIndexer *indexer,
				   const gchar    *mount_point)
{
	gchar *file;

	file = g_build_filename (mount_point,
				 ".cache", 
				 "metadata",
				 "metadata.ttl", 
				 NULL);

	if (g_file_test (file, G_FILE_TEST_EXISTS)) {
		tracker_turtle_optimize (file);
	}

	g_free (file);
}

void
tracker_removable_device_load (TrackerIndexer *indexer,
			       const gchar    *mount_point)
{
	gchar *filename;

	filename = g_build_filename (mount_point,
				     ".cache", 
				     "metadata",
				     "metadata.ttl", 
				     NULL);

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		TurtleStorerInfo  info;
		gchar            *base_uri, *tmp;
		GFile            *mountp_file;

		info.indexer = g_object_ref (indexer);
		info.amount = 0;

		info.base = g_strdup (mount_point);

		/* We need to open the transaction, during the parsing will the
		 * transaction be committed and reopened */

		tracker_indexer_transaction_open (info.indexer);

		mountp_file = g_file_new_for_path (mount_point);
		tmp = g_file_get_uri (mountp_file);
		base_uri = g_strconcat (tmp, "/", NULL);

		tracker_turtle_process (filename, base_uri, consume_triple_storer, &info);

		g_free (base_uri);
		g_free (tmp);
		g_object_unref (mountp_file);

		/* Commit final subject (our loop doesn't handle the very last) 
		 * It can't be a REMOVAL nor MOVE as those happen immediately. */

		commit_turtle_parse_info_storer (&info, FALSE, REPLACE, NULL);

		/* We will (always) be left in open state, so we commit the 
		 * last opened transaction */

		tracker_indexer_transaction_commit (info.indexer);

		g_free (info.base);
		g_object_unref (info.indexer);
	}

	g_free (filename);
}

static void
set_metadata (const gchar *key, 
	      const gchar *value, 
	      gpointer     user_data)
{
	raptor_statement     statement;
	AddMetadataInfo     *item;
	const gchar         *about_uri;
	raptor_serializer   *serializer;

	item = user_data;

	g_return_if_fail (item != NULL);

	about_uri = item->about_uri;
	serializer = item->serializer;

	statement.subject = (void *) raptor_new_uri ((const guchar *) about_uri);
	statement.subject_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;

	statement.predicate = (void *) raptor_new_uri ((const guchar *) (key?key:""));
	statement.predicate_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;

	if (value) {
		statement.object = (unsigned char *) g_strdup (value);
		statement.object_type = RAPTOR_IDENTIFIER_TYPE_LITERAL;
	} else {
		statement.object = (void *) raptor_new_uri ((const guchar *) (""));
		statement.object_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;
	}

	raptor_serialize_statement (serializer, 
				    &statement);

	raptor_free_uri ((raptor_uri *) statement.subject);
	raptor_free_uri ((raptor_uri *) statement.predicate);

	if (value) {
		g_free ((unsigned char *) statement.object);
	} else  {
		raptor_free_uri ((raptor_uri *) statement.object);
	}
}

static void
foreach_in_metadata_set_metadata (const gchar     *subject,
				  const gchar     *key,
				  const gchar     *value,
				  gpointer      user_data)
{
	raptor_statement     statement;
	AddMetadataInfo     *item = user_data;
	raptor_serializer   *serializer = item->serializer;

	statement.subject = (void *) raptor_new_uri ((const guchar *) subject);
	statement.subject_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;

	statement.predicate = (void *) raptor_new_uri ((const guchar *) (key?key:""));
	statement.predicate_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;

	if (value) {
		statement.object = (unsigned char *) g_strdup (value);
		statement.object_type = RAPTOR_IDENTIFIER_TYPE_LITERAL;
	} else {
		statement.object = (void *) raptor_new_uri ((const guchar *) (""));
		statement.object_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;
	}

	raptor_serialize_statement (serializer, 
				    &statement);

	raptor_free_uri ((raptor_uri *) statement.subject);
	raptor_free_uri ((raptor_uri *) statement.predicate);

	if (value)
		g_free ((unsigned char *) statement.object);
	else 
		raptor_free_uri ((raptor_uri *) statement.object);
}

void
tracker_removable_device_add_metadata (TrackerIndexer        *indexer, 
				       const gchar           *mount_point, 
				       const gchar           *uri,
				       TrackerModuleMetadata *metadata)
{
	g_return_if_fail (TRACKER_IS_INDEXER (indexer));
	g_return_if_fail (mount_point != NULL);
	g_return_if_fail (uri != NULL);

	AddMetadataInfo  info;
	gchar           *filename, *muri, *tmp;
	const gchar     *p;
	FILE            *target_file;
	raptor_uri      *suri;
	GFile           *mountp_file;

	filename = g_build_filename (mount_point, 
				     ".cache",
				     "metadata",
				     NULL);
	g_mkdir_with_parents (filename, 0700);
	g_free (filename);

	filename = g_build_filename (mount_point, 
				     ".cache",
				     "metadata", 
				     "metadata.ttl", 
				     NULL);

	target_file = fopen (filename, "a");
	/* Similar to a+ */
	if (!target_file) 
		target_file = fopen (filename, "w");
	g_free (filename);

	if (!target_file) {
		return;
	}

	info.serializer = raptor_new_serializer ("turtle");

	/* TODO: paths to URIs: this is wrong */
	p = uri + strlen (mount_point) + 1 + 7;
	info.about_uri = g_strdup (p);

	raptor_serializer_set_feature (info.serializer, 
				       RAPTOR_FEATURE_WRITE_BASE_URI, 0);

	raptor_serializer_set_feature (info.serializer, 
				       RAPTOR_FEATURE_ASSUME_IS_RDF, 1);

	raptor_serializer_set_feature (info.serializer, 
				       RAPTOR_FEATURE_ALLOW_NON_NS_ATTRIBUTES, 1);


	mountp_file = g_file_new_for_path (mount_point);
	tmp = g_file_get_uri (mountp_file);
	muri = g_strconcat (tmp, "/base", NULL);

	suri = raptor_new_uri ((const guchar *) muri);

	g_free (muri);
	g_free (tmp);
	g_object_unref (mountp_file);

	raptor_serialize_start_to_file_handle (info.serializer, 
					       suri,
					       target_file);

	/* TODO: Concatenate SPARQL queries instead
	tracker_module_metadata_foreach (metadata, 
					 foreach_in_metadata_set_metadata,
					 &info);
	 */

	g_free (info.about_uri);

	raptor_serialize_end (info.serializer);
	raptor_free_serializer (info.serializer);
	raptor_free_uri (suri);

	fclose (target_file);
}

/* TODO URI branch: path -> uri */

void
tracker_removable_device_add_removal (TrackerIndexer *indexer, 
				      const gchar *mount_point, 
				      const gchar *uri)
{
	g_return_if_fail (TRACKER_IS_INDEXER (indexer));
	g_return_if_fail (mount_point != NULL);
	g_return_if_fail (uri != NULL);

	gchar               *filename, *about_uri, *muri, *tmp;
	const gchar         *p;
	FILE                *target_file;
	raptor_uri          *suri;
	raptor_serializer   *serializer;
	AddMetadataInfo      info;
	GFile               *mountp_file;

	filename = g_build_filename (mount_point,
				     ".cache",
				     "metadata",
				     NULL);
	g_mkdir_with_parents (filename, 0700);
	g_free (filename);

	filename = g_build_filename (mount_point, ".cache",
				     "metadata",
				     "metadata.ttl", 
				     NULL);

	target_file = fopen (filename, "a");
	/* Similar to a+ */
	if (!target_file) 
		target_file = fopen (filename, "w");
	g_free (filename);

	if (!target_file) {
		return;
	}

	serializer = raptor_new_serializer ("turtle");

	/* TODO: paths to URIs: this is wrong */
	p = uri + strlen (mount_point) + 1 + 7;
	about_uri = g_strdup (p);

	raptor_serializer_set_feature (serializer, 
				       RAPTOR_FEATURE_WRITE_BASE_URI,
				       0);

	mountp_file = g_file_new_for_path (mount_point);
	tmp = g_file_get_uri (mountp_file);
	muri = g_strconcat (tmp, "/base", NULL);

	suri = raptor_new_uri ((const unsigned char *) muri);

	g_free (muri);
	g_free (tmp);
	g_object_unref (mountp_file);

	raptor_serialize_start_to_file_handle (serializer, 
					       suri, 
					       target_file);

	info.serializer = serializer;
	info.about_uri = about_uri;

	set_metadata (NULL, NULL, &info);

	raptor_free_uri (suri);
	g_free (about_uri);

	raptor_serialize_end (serializer);
	raptor_free_serializer (serializer);

	fclose (target_file);
}

/* TODO URI branch: path -> uri */

void
tracker_removable_device_add_move (TrackerIndexer *indexer, 
				   const gchar *mount_point, 
				   const gchar *from_uri, 
				   const gchar *to_uri)
{
	g_return_if_fail (TRACKER_IS_INDEXER (indexer));
	g_return_if_fail (mount_point != NULL);
	g_return_if_fail (from_uri != NULL);
	g_return_if_fail (to_uri != NULL);

	gchar               *filename, *about_uri, *to_urip, *muri, *tmp;
	const gchar         *p;
	FILE                *target_file;
	raptor_uri          *suri;
	raptor_serializer   *serializer;
	AddMetadataInfo      info;
	GFile               *mountp_file;

	filename = g_build_filename (mount_point,
				     ".cache",
				     "metadata",
				     NULL);
	g_mkdir_with_parents (filename, 0700);
	g_free (filename);

	filename = g_build_filename (mount_point, 
				     ".cache",
				     "metadata",
				     "metadata.ttl", 
				     NULL);

	target_file = fopen (filename, "a");
	/* Similar to a+ */
	if (!target_file) 
		target_file = fopen (filename, "w");
	g_free (filename);

	if (!target_file) {
		return;
	}

	serializer = raptor_new_serializer ("turtle");

	raptor_serializer_set_feature (serializer, 
				       RAPTOR_FEATURE_WRITE_BASE_URI, 0);

	/* TODO: paths to URIs: this is wrong */
	p = from_uri + strlen (mount_point) + 1 + 7;
	about_uri = g_strdup (p);

	/* TODO: paths to URIs: this is wrong */
	p = to_uri + strlen (mount_point) + 1 + 7;
	to_urip = g_strdup (p);

	mountp_file = g_file_new_for_path (mount_point);
	tmp = g_file_get_uri (mountp_file);
	muri = g_strconcat (tmp, "/", NULL);

	suri = raptor_new_uri ((const guchar *) muri);

	g_free (muri);
	g_free (tmp);
	g_object_unref (mountp_file);

	raptor_serialize_start_to_file_handle (serializer, 
					       suri,
					       target_file);

	info.serializer = serializer;
	info.about_uri = about_uri;

	set_metadata (NULL, to_urip, &info);

	g_free (about_uri);
	g_free (to_urip);

	raptor_serialize_end (serializer);
	raptor_free_serializer (serializer);
	raptor_free_uri (suri);

	fclose (target_file);
}

