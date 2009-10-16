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

/* When merging the decomposed branch to trunk: this filed used to be called
 * tracker-indexer/tracker-turtle.c. What has happened in trunk is that this
 * file got renamed to tracker-indexer/tracker-removable-device.c and that
 * we created a new file libtracker-data/tracker-turtle.c which contains some
 * of the functions that used to be available in this file. 
 *
 * The reason for that is that Ivan's backup support, which runs in trackerd,
 * needed access to the same Turtle related routines. So we moved it to one of
 * our internally shared libraries (libtracker-data got elected for this). 
 *
 * When merging the decomposed branch, simply pick this file over the file 
 * tracker-indexer/tracker-turtle.c (in the decomposed branch). */

#include "config.h"


#ifdef HAVE_RAPTOR

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

#endif

#include <libtracker-data/tracker-turtle.h>

#include "tracker-removable-device.h"

#ifdef HAVE_RAPTOR
#include "tracker-module-metadata-private.h"

typedef struct {
	gchar *last_subject;
	gchar *base;
	guint amount;
	TrackerIndexer *indexer;
	TrackerModuleMetadata *metadata;
	gchar *rdf_type;
} TurtleStorerInfo;

typedef struct {
	raptor_serializer *serializer;
	gchar *about_uri;
} AddMetadataInfo;

typedef enum {
	REMOVAL,
	REPLACE,
	MOVE
} StorerTask;

static void
commit_turtle_parse_info_storer (TurtleStorerInfo *info, 
				 gboolean          may_flush, 
				 StorerTask        task,
				 const gchar      *destination)
{
	if (info->last_subject) {
		GHashTable *data;
		GFile *file, *dest_file = NULL;
		gchar *path, *dest_path = NULL;

		/* We have it as a URI, database api wants Paths. Update this when
		 * the database api becomes sane and uses URIs everywhere
		 */
		file = g_file_new_for_uri (info->last_subject);
		path = g_file_get_path (file);

		if (destination) {
			dest_file = g_file_new_for_uri (destination);
			dest_path = g_file_get_path (dest_file);
		}

		if (info->rdf_type) {
			TrackerHal *hal;
			const gchar *udi;

			/* We ignore records that didn't have an <rdf:type> 
			 * predicate. Those are just wrong anyway.
			 */

			switch (task) {
			case REMOVAL:
				tracker_data_update_delete_service_by_path (path, info->rdf_type);
				break;
			case MOVE:
				data = tracker_module_metadata_get_hash_table (info->metadata);

				hal = tracker_indexer_get_hal (info->indexer);
#ifdef HAVE_HAL
				udi = tracker_hal_udi_get_for_path (hal, dest_path);
#else
				udi = NULL;
#endif

				tracker_data_update_delete_service_by_path (path, info->rdf_type);
				tracker_data_update_replace_service (udi,
								     dest_path,
								     info->rdf_type,
								     data);
				g_hash_table_destroy (data);
				break;
			case REPLACE:
			default:
				data = tracker_module_metadata_get_hash_table (info->metadata);

				hal = tracker_indexer_get_hal (info->indexer);
#ifdef HAVE_HAL
				udi = tracker_hal_udi_get_for_path (hal, path);
#else
				udi = NULL;
#endif

				tracker_data_update_replace_service (udi, 
								     path,
								     info->rdf_type,
								     data);
				g_hash_table_destroy (data);
				break;
			}
		}

		info->amount++;

		g_object_unref (info->metadata);
		g_object_unref (file);

		if (dest_file) {
			g_object_unref (dest_file);
		}

		g_free (path);
		g_free (dest_path);
		g_free (info->last_subject);
		g_free (info->rdf_type);

		info->last_subject = NULL;
		info->metadata = NULL;
		info->rdf_type = NULL;
	}

	/* We commit per transaction of 100 here, and then we also iterate the
	 * mainloop so that the state-machine gets the opportunity to run for a
	 * moment */

	if (may_flush && info->amount > TRACKER_INDEXER_TRANSACTION_MAX) {
		tracker_indexer_transaction_commit (info->indexer);
		g_main_context_iteration (NULL, FALSE);
		tracker_indexer_transaction_open (info->indexer);
		info->amount = 0;
	}
}

static gchar *
get_uri_with_trailing_slash (GFile *file)
{
	gchar *uri, *str;

	uri = g_file_get_uri (file);
	str = g_strdup_printf ("%s/", uri);
	g_free (uri);

	return str;
}

static void
consume_triple_storer (void                   *user_data, 
		       const raptor_statement *triple) 
{
	TurtleStorerInfo *info = user_data;
	gchar            *subject;

	/* TODO: cope with multi-value values like User:Keywords */

	subject = raptor_uri_as_string ((raptor_uri *) triple->subject);

	if (g_strcmp0 (subject, info->last_subject) != 0) {
		/* Commit previous subject */
		commit_turtle_parse_info_storer (info, TRUE, REPLACE, NULL);

		/* Install next subject */
		info->last_subject = g_strdup (subject);
		info->metadata = tracker_module_metadata_new ();
	}

	if (triple->object_type == RAPTOR_IDENTIFIER_TYPE_LITERAL) {
		const gchar *predicate;

		predicate = raptor_uri_as_string ((raptor_uri *) triple->predicate);

		if (g_strcmp0 (predicate, "rdf:type") == 0) {
			g_free (info->rdf_type);
			info->rdf_type = g_strdup (triple->object);
		} else {
			tracker_module_metadata_add_string (info->metadata,
							    predicate,
							    triple->object);
		}
	} else if (triple->object_type == RAPTOR_IDENTIFIER_TYPE_RESOURCE) {
		GFile *file;
		gchar *key;

		file = g_file_new_for_path (info->base);
		key = get_uri_with_trailing_slash (file);

		if (g_strcmp0 (key, triple->object) == 0 &&
		    g_strcmp0 (key, triple->predicate) == 0) {
			/* <URI> <rdf:type> "Type" ;                    *
			 *       <> <>  -  is a removal of the resource */

			/* We commit this subject as a removal, the last_subject
			 * field will be cleared for the next subject to be set
			 * ready first next process loop. */

			commit_turtle_parse_info_storer (info, FALSE, REMOVAL, NULL);
		} else if (g_strcmp0 (key, triple->object) == 0 &&
			   g_strcmp0 (key, triple->predicate) != 0) {
			const gchar *predicate;

			/* <URI> <rdf:type> "Type" ;                             *
			 *       <Pfx:Predicate> <>  -  is a removal of the      * 
			 *                              resource's Pfx:Predicate */

			predicate = raptor_uri_as_string ((raptor_uri *) triple->predicate);

			/* We put NULL here, so that a null value goes into 
			 * SQLite. Perhaps we should change this? If so, Why? */

			tracker_module_metadata_add_string (info->metadata,
							    predicate,
							    NULL);
		} else if (g_strcmp0 (key, triple->object) != 0 &&
			   g_strcmp0 (key, triple->predicate) == 0) {
			gchar *object;

			/* <URI> <> <to-URI>  -  is a move of the subject */
			object = raptor_uri_as_string ((raptor_uri *) triple->object);
			commit_turtle_parse_info_storer (info, FALSE, MOVE, object);
		}

		g_free (key);
		g_object_unref (file);
	}
}

#endif /* HAVE_RAPTOR */

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
#ifdef HAVE_RAPTOR
	gchar *filename;

	filename = g_build_filename (mount_point,
				     ".cache", 
				     "metadata",
				     "metadata.ttl", 
				     NULL);

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		TurtleStorerInfo *info;
		GFile            *file;
		gchar            *base_uri;

		info = g_slice_new0 (TurtleStorerInfo);

		info->indexer = g_object_ref (indexer);
		info->amount = 0;

		info->base = g_strdup (mount_point);

		/* We need to open the transaction, during the parsing will the
		 * transaction be committed and reopened */

		tracker_indexer_transaction_open (info->indexer);

		file = g_file_new_for_path (mount_point);
		base_uri = get_uri_with_trailing_slash (file);

		tracker_turtle_process (filename, base_uri, consume_triple_storer, info);

		g_object_unref (file);
		g_free (base_uri);

		/* Commit final subject (our loop doesn't handle the very last) 
		 * It can't be a REMOVAL nor MOVE as those happen immediately. */

		commit_turtle_parse_info_storer (info, FALSE, REPLACE, NULL);

		/* We will (always) be left in open state, so we commit the 
		 * last opened transaction */

		tracker_indexer_transaction_commit (info->indexer);

		g_free (info->base);
		g_object_unref (info->indexer);
		g_slice_free (TurtleStorerInfo, info);
	}

	g_free (filename);
#endif /* HAVE_RAPTOR */
}

#ifdef HAVE_RAPTOR

static void
set_metadata (const gchar *key, 
	      const gchar *value, 
	      gpointer     user_data)
{
	raptor_statement    *statement;
	AddMetadataInfo     *item;
	const gchar         *about_uri;
	raptor_serializer   *serializer;

	item = user_data;
	
	g_return_if_fail (item != NULL);
	
	about_uri = item->about_uri;
	serializer = item->serializer;

	statement = g_new0 (raptor_statement, 1);

	statement->subject = (void *) raptor_new_uri ((const unsigned char *) about_uri);
	statement->subject_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;

	statement->predicate = (void *) raptor_new_uri ((const guchar *) (key ? key : ""));
	statement->predicate_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;

	if (value) {
		statement->object = (unsigned char *) g_strdup (value);
		statement->object_type = RAPTOR_IDENTIFIER_TYPE_LITERAL;
	} else {
		statement->object = (void *) raptor_new_uri ((const guchar *) (""));
		statement->object_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;
	}

	raptor_serialize_statement (serializer, 
				    statement);

	raptor_free_uri ((raptor_uri *) statement->subject);
	raptor_free_uri ((raptor_uri *) statement->predicate);

	if (value) {
		g_free ((unsigned char *) statement->object);
	} else {
		raptor_free_uri ((raptor_uri *) statement->object);
	}

	g_free (statement);
}

/* TODO URI branch: path -> uri */
static void
foreach_in_metadata_set_metadata (TrackerField *field,
				  gpointer      value,
				  gpointer      user_data)
{
	if (!tracker_field_get_multiple_values (field)) {
		set_metadata (tracker_field_get_name (field), value, user_data);
	} else {
		GList *l;

		for (l = value; l; l = l->next) {
			set_metadata (tracker_field_get_name (field), l->data, user_data);
		}
	}

}
#endif /* HAVE_RAPTOR */

void
tracker_removable_device_add_metadata (TrackerIndexer        *indexer, 
				       const gchar           *mount_point, 
				       const gchar           *path,
				       const gchar           *rdf_type,
				       TrackerModuleMetadata *metadata)
{
	g_return_if_fail (TRACKER_IS_INDEXER (indexer));
	g_return_if_fail (mount_point != NULL);
	g_return_if_fail (path != NULL);
	g_return_if_fail (rdf_type != NULL);
	
#ifdef HAVE_RAPTOR
	AddMetadataInfo *info;
	gchar           *filename, *muri;
	const gchar     *p;
	FILE            *target_file;
	raptor_uri      *suri;
	GFile           *file, *base_file;

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

	target_file = tracker_file_open (filename, "a+", FALSE);
	g_free (filename);

	if (!target_file) {
		return;
	}

	info = g_slice_new (AddMetadataInfo);

	info->serializer = raptor_new_serializer ("turtle");

	p = path + strlen (mount_point) + 1;
	info->about_uri = g_strdup (p);

	raptor_serializer_set_feature (info->serializer, 
				       RAPTOR_FEATURE_WRITE_BASE_URI, 0);

	raptor_serializer_set_feature (info->serializer, 
				       RAPTOR_FEATURE_ASSUME_IS_RDF, 1);

	raptor_serializer_set_feature (info->serializer, 
				       RAPTOR_FEATURE_ALLOW_NON_NS_ATTRIBUTES, 1);

	file = g_file_new_for_path (mount_point);
	base_file = g_file_get_child (file, "base");
	muri = g_file_get_uri (base_file);

	suri = raptor_new_uri ((const unsigned char *) muri);

	g_object_unref (file);
	g_object_unref (base_file);
	g_free (muri);

	raptor_serialize_start_to_file_handle (info->serializer, 
					       suri, 
					       target_file);

	set_metadata ("rdf:type", rdf_type, info);

	tracker_data_metadata_foreach (TRACKER_DATA_METADATA (metadata),
				       foreach_in_metadata_set_metadata,
				       info);

	g_free (info->about_uri);

	raptor_serialize_end (info->serializer);
	raptor_free_serializer (info->serializer);
	raptor_free_uri (suri);

	g_slice_free (AddMetadataInfo, info);

	tracker_file_close (target_file, FALSE);
#endif /* HAVE_RAPTOR */
}

/* TODO URI branch: path -> uri */

void
tracker_removable_device_add_removal (TrackerIndexer *indexer, 
				      const gchar    *mount_point, 
				      const gchar    *path,
				      const gchar    *rdf_type)
{
	g_return_if_fail (TRACKER_IS_INDEXER (indexer));
	g_return_if_fail (mount_point != NULL);
	g_return_if_fail (path != NULL);
	g_return_if_fail (rdf_type != NULL);

#ifdef HAVE_RAPTOR
	gchar               *filename, *about_uri, *muri;
	const gchar         *p;
	FILE                *target_file;
	raptor_uri          *suri;
	raptor_serializer   *serializer;
	AddMetadataInfo     *info;
	GFile               *file, *base_file;

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

	target_file = tracker_file_open (filename, "a+", FALSE);
	g_free (filename);

	if (!target_file) {
		return;
	}

	serializer = raptor_new_serializer ("turtle");

	p = path + strlen (mount_point) + 1;
	about_uri = g_strdup (p);

	raptor_serializer_set_feature (serializer, 
				       RAPTOR_FEATURE_WRITE_BASE_URI,
				       0);

	file = g_file_new_for_path (mount_point);
	base_file = g_file_get_child (file, "base");

	muri = g_file_get_uri (base_file);
	suri = raptor_new_uri ((const unsigned char *) muri);

	g_object_unref (base_file);
	g_object_unref (file);
	g_free (muri);

	raptor_serialize_start_to_file_handle (serializer, 
					       suri, 
					       target_file);

	info = g_slice_new (AddMetadataInfo);

	info->serializer = serializer;
	info->about_uri = about_uri;

	set_metadata ("rdf:type", rdf_type, info);

	set_metadata (NULL, NULL, info);

	raptor_free_uri (suri);

	g_slice_free (AddMetadataInfo, info);
	g_free (about_uri);

	raptor_serialize_end (serializer);
	raptor_free_serializer (serializer);

	tracker_file_close (target_file, FALSE);
#endif /* HAVE_RAPTOR */
}

/* TODO URI branch: path -> uri */

void
tracker_removable_device_add_move (TrackerIndexer *indexer, 
				   const gchar    *mount_point, 
				   const gchar    *from_path, 
				   const gchar    *to_path,
				   const gchar    *rdf_type)
{
	g_return_if_fail (TRACKER_IS_INDEXER (indexer));
	g_return_if_fail (mount_point != NULL);
	g_return_if_fail (from_path != NULL);
	g_return_if_fail (to_path != NULL);
	g_return_if_fail (rdf_type != NULL);

#ifdef HAVE_RAPTOR
	gchar               *filename, *about_uri, *to_uri, *muri;
	const gchar         *p;
	FILE                *target_file;
	raptor_uri          *suri;
	raptor_serializer   *serializer;
	AddMetadataInfo     *info;
	GFile               *file;

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

	target_file = tracker_file_open (filename, "a+", FALSE);
	g_free (filename);

	if (!target_file) {
		return;
	}

	serializer = raptor_new_serializer ("turtle");

	raptor_serializer_set_feature (serializer, 
				       RAPTOR_FEATURE_WRITE_BASE_URI, 0);

	p = from_path + strlen (mount_point) + 1;
	about_uri = g_strdup (p);

	p = to_path + strlen (mount_point) + 1;
	to_uri = g_strdup (p);

	file = g_file_new_for_path (mount_point);
	muri = get_uri_with_trailing_slash (file);
	suri = raptor_new_uri ((const unsigned char *) muri);

	g_object_unref (file);
	g_free (muri);

	raptor_serialize_start_to_file_handle (serializer, 
					       suri,
					       target_file);

	info = g_slice_new (AddMetadataInfo);

	info->serializer = serializer;
	info->about_uri = about_uri;

	set_metadata ("rdf:type", rdf_type, info);

	set_metadata (NULL, to_uri, info);

	g_slice_free (AddMetadataInfo, info);


	g_free (about_uri);
	g_free (to_uri);

	raptor_serialize_end (serializer);
	raptor_free_serializer (serializer);
	raptor_free_uri (suri);

	tracker_file_close (target_file, FALSE);
#endif /* HAVE_RAPTOR */
}

