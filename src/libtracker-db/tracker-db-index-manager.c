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
 */

#include <stdio.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-ontology.h>

#include "tracker-db-index-manager.h"

#define MIN_BUCKET_DEFAULT  10
#define MAX_BUCKET_DEFAULT  20

#define MAX_INDEX_FILE_SIZE 2000000000

typedef struct {
	TrackerDBIndexType  type;
	TrackerDBIndex	   *index;
	const gchar	   *file;
	const gchar	   *name;
	gchar		   *abs_filename;
} TrackerDBIndexDefinition;

static gboolean			 initialized;
static gchar			*data_dir;

static TrackerDBIndexDefinition  indexes[] = {
	{ TRACKER_DB_INDEX_UNKNOWN,
	  NULL,
	  NULL,
	  NULL,
	  NULL },
	{ TRACKER_DB_INDEX_FILE,
	  NULL,
	  "file-index.db",
	  "file-index",
	  NULL },
	{ TRACKER_DB_INDEX_FILE_UPDATE,
	  NULL,
	  "file-index-update.db",
	  "file-index-update",
	  NULL },
	{ TRACKER_DB_INDEX_EMAIL,
	  NULL,
	  "email-index.db",
	  "email-index",
	  NULL }
};

static gboolean
has_tmp_merge_files (TrackerDBIndexType type)
{
	GFile		*file;
	GFileEnumerator *enumerator;
	GFileInfo	*info;
	GError		*error = NULL;
	gchar		*prefix;
	gchar		*dirname;
	gboolean	 found;

	if (type == TRACKER_DB_INDEX_UNKNOWN) {
		return FALSE;
	}

	prefix = g_strconcat (indexes[type].name, ".tmp", NULL);

	dirname = g_path_get_dirname (indexes[type].abs_filename);
	file = g_file_new_for_path (dirname);

	enumerator = g_file_enumerate_children (file,
						G_FILE_ATTRIBUTE_STANDARD_NAME ","
						G_FILE_ATTRIBUTE_STANDARD_TYPE,
						G_PRIORITY_DEFAULT,
						NULL,
						&error);

	if (error) {
		g_warning ("Could not check for temporary index files in "
			   "directory:'%s', %s",
			   dirname,
			   error->message);

		g_error_free (error);
		g_object_unref (file);
		g_free (dirname);
		g_free (prefix);

		return FALSE;
	}

	found = FALSE;

	for (info = g_file_enumerator_next_file (enumerator, NULL, &error);
	     info && !error && !found;
	     info = g_file_enumerator_next_file (enumerator, NULL, &error)) {
		/* Check each file has or hasn't got the prefix */
		if (g_str_has_prefix (g_file_info_get_name (info), prefix)) {
			found = TRUE;
		}

		g_object_unref (info);
	}

	if (error) {
		g_warning ("Could not get file information for temporary "
			   "index files, %s",
			   error->message);
		g_error_free (error);
	}

	g_object_unref (enumerator);
	g_object_unref (file);
	g_free (dirname);
	g_free (prefix);

	return found;
}

gboolean
tracker_db_index_manager_init (TrackerDBIndexManagerFlags  flags,
			       gint			   min_bucket,
			       gint			   max_bucket)
{
	gchar	 *final_index_filename;
	gchar	 *name;
	gboolean  need_reindex = FALSE;
	gboolean  force_reindex;
	gboolean  readonly;
	guint	  i;

	g_return_val_if_fail (min_bucket >= 0, FALSE);
	g_return_val_if_fail (max_bucket >= min_bucket, FALSE);

	if (initialized) {
		return TRUE;
	}

	g_message ("Setting index database locations");

	data_dir = g_build_filename (g_get_user_cache_dir (),
				     "tracker",
				     NULL);

	g_message ("Checking index directories exist");

	g_mkdir_with_parents (data_dir, 00755);

	g_message ("Checking index files exist");

	for (i = 1; i < G_N_ELEMENTS (indexes); i++) {
		indexes[i].abs_filename = g_build_filename (data_dir, indexes[i].file, NULL);

		if (need_reindex) {
			continue;
		}

		if (!g_file_test (indexes[i].abs_filename, G_FILE_TEST_EXISTS)) {
			g_message ("Could not find index file:'%s'", indexes[i].abs_filename);
		}
	}

	g_message ("Merging old temporary indexes");

	i = TRACKER_DB_INDEX_FILE;
	name = g_strconcat (indexes[i].name, "-final", NULL);
	final_index_filename = g_build_filename (data_dir, name, NULL);
	g_free (name);

	if (g_file_test (final_index_filename, G_FILE_TEST_EXISTS) &&
	    !has_tmp_merge_files (i)) {
		g_message ("  Overwriting '%s' with '%s'",
			   indexes[i].abs_filename,
			   final_index_filename);

		g_rename (final_index_filename, indexes[i].abs_filename);
	}

	g_free (final_index_filename);

	i = TRACKER_DB_INDEX_EMAIL;
	name = g_strconcat (indexes[i].name, "-final", NULL);
	final_index_filename = g_build_filename (data_dir, name, NULL);
	g_free (name);

	if (g_file_test (final_index_filename, G_FILE_TEST_EXISTS) &&
	    !has_tmp_merge_files (i)) {
		g_message ("  Overwriting '%s' with '%s'",
			   indexes[i].abs_filename,
			   final_index_filename);

		g_rename (final_index_filename, indexes[i].abs_filename);
	}

	g_free (final_index_filename);

	/* Now we have cleaned up merge files, see if we are supposed
	 * to be reindexing.
	 */

	force_reindex = (flags & TRACKER_DB_INDEX_MANAGER_FORCE_REINDEX) != 0;

	if (force_reindex || need_reindex) {
		g_message ("Cleaning up index files for reindex");

		for (i = 1; i < G_N_ELEMENTS (indexes); i++) {
			g_unlink (indexes[i].abs_filename);
		}
	}

	g_message ("Creating index files, this may take a few moments...");

	readonly = (flags & TRACKER_DB_INDEX_MANAGER_READONLY) != 0;

	for (i = 1; i < G_N_ELEMENTS (indexes); i++) {
		indexes[i].index = tracker_db_index_new (indexes[i].abs_filename,
							 min_bucket,
							 max_bucket,
							 readonly);
	}

	initialized = TRUE;

	return TRUE;
}

void
tracker_db_index_manager_shutdown (void)
{
	guint i;

	if (!initialized) {
		return;
	}

	for (i = 1; i < G_N_ELEMENTS (indexes); i++) {
		g_object_unref (indexes[i].index);
		indexes[i].index = NULL;

		g_free (indexes[i].abs_filename);
		indexes[i].abs_filename = NULL;
	}

	g_free (data_dir);

	initialized = FALSE;
}

TrackerDBIndex *
tracker_db_index_manager_get_index (TrackerDBIndexType type)
{
	return indexes[type].index;
}

TrackerDBIndex *
tracker_db_index_manager_get_index_by_service (const gchar *service)
{
	TrackerDBType	   type;
	TrackerDBIndexType index_type;

	g_return_val_if_fail (initialized == TRUE, NULL);
	g_return_val_if_fail (service != NULL, NULL);

	type = tracker_ontology_get_service_db_by_name (service);

	switch (type) {
	case TRACKER_DB_TYPE_FILES:
		index_type = TRACKER_DB_INDEX_FILE;
		break;
	case TRACKER_DB_TYPE_EMAIL:
		index_type = TRACKER_DB_INDEX_EMAIL;
		break;
	default:
		/* How do we handle XESAM? and others? default to files? */
		index_type = TRACKER_DB_INDEX_UNKNOWN;
		break;
	}

	return indexes[index_type].index;
}

TrackerDBIndex *
tracker_db_index_manager_get_index_by_service_id (gint id)
{
	TrackerDBType	    type;
	TrackerDBIndexType  index_type;
	gchar		   *service;

	g_return_val_if_fail (initialized == TRUE, NULL);

	service = tracker_ontology_get_service_by_id (id);
	if (!service) {
		return NULL;
	}

	type = tracker_ontology_get_service_db_by_name (service);
	g_free (service);

	switch (type) {
	case TRACKER_DB_TYPE_FILES:
		index_type = TRACKER_DB_INDEX_FILE;
		break;
	case TRACKER_DB_TYPE_EMAIL:
		index_type = TRACKER_DB_INDEX_EMAIL;
		break;
	default:
		/* How do we handle XESAM? and others? default to files? */
		index_type = TRACKER_DB_INDEX_UNKNOWN;
		break;
	}

	if (index_type == TRACKER_DB_INDEX_UNKNOWN) {
		return NULL;
	}

	return indexes[index_type].index;
}

const gchar *
tracker_db_index_manager_get_filename (TrackerDBIndexType type)
{
	return indexes[type].abs_filename;
}

gboolean
tracker_db_index_manager_are_indexes_too_big (void)
{
	gboolean too_big;
	guint	 i;

	g_return_val_if_fail (initialized == TRUE, FALSE);

	for (i = 1, too_big = FALSE; i < G_N_ELEMENTS (indexes) && !too_big; i++) {
		too_big = tracker_file_get_size (indexes[i].abs_filename) > MAX_INDEX_FILE_SIZE;
	}

	if (too_big) {
		g_critical ("One or more index files are too big, indexing disabled");
		return TRUE;
	}

	return FALSE;
}
