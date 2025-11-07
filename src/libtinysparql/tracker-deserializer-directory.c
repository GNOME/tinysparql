/*
 * Copyright (C) 2025, Red Hat Inc.
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include "tracker-deserializer-directory.h"

struct _TrackerDeserializerDirectory {
	TrackerDeserializerMerger parent_instance;
	GFile *directory;
	GList *files;
	GList *current_file;
};

enum {
	PROP_0,
	PROP_DIRECTORY,
	N_PROPS,
};

static GParamSpec *props[N_PROPS] = { 0, };

G_DEFINE_TYPE (TrackerDeserializerDirectory,
               tracker_deserializer_directory,
               TRACKER_TYPE_DESERIALIZER_MERGER)

static void
tracker_deserializer_directory_finalize (GObject *object)
{
	TrackerDeserializerDirectory *deserializer =
		TRACKER_DESERIALIZER_DIRECTORY (object);

	g_clear_object (&deserializer->directory);
	g_list_free_full (deserializer->files, g_object_unref);
	deserializer->files = NULL;
	deserializer->current_file = NULL;

	G_OBJECT_CLASS (tracker_deserializer_directory_parent_class)->finalize (object);
}

static void
tracker_deserializer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	TrackerDeserializerDirectory *deserializer =
		TRACKER_DESERIALIZER_DIRECTORY (object);

	switch (prop_id) {
	case PROP_DIRECTORY:
		deserializer->directory = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static int
compare_file_names (GFile *file1,
                    GFile *file2)
{
	char *name1, *name2;
	int ret;

	name1 = g_file_get_basename (file1);
	name2 = g_file_get_basename (file2);
	ret = strcmp (name1, name2);
	g_free (name1);
	g_free (name2);

	return ret;
}

static gboolean
get_ontology_files (GFile   *directory,
                    GList  **ontologies,
                    GError **error)
{
	GFileEnumerator *enumerator;
	GList *sorted = NULL;

	enumerator = g_file_enumerate_children (directory,
	                                        G_FILE_ATTRIBUTE_STANDARD_NAME,
	                                        G_FILE_QUERY_INFO_NONE,
	                                        NULL, error);
	if (!enumerator)
		return FALSE;

	while (TRUE) {
		GFileInfo *info;
		GFile *child;
		const gchar *name;

		if (!g_file_enumerator_iterate (enumerator, &info, &child, NULL, error)) {
			g_list_free_full (sorted, g_object_unref);
			g_object_unref (enumerator);
			return FALSE;
		}

		if (!info)
			break;

		name = g_file_info_get_name (info);
		if (g_str_has_suffix (name, ".ontology") ||
		    tracker_rdf_format_pick_for_file (child, NULL)) {
			sorted = g_list_prepend (sorted, g_object_ref (child));
		}
	}

	*ontologies = g_list_sort (sorted, (GCompareFunc) compare_file_names);
	g_object_unref (enumerator);

	return TRUE;
}

static gboolean
add_deserializer_for_current_file (TrackerDeserializerDirectory  *deserializer,
                                   GError                       **error)
{
	TrackerSparqlCursor *child_cursor;

	if (!deserializer->current_file)
		return TRUE;

	child_cursor = tracker_deserializer_new_for_file (deserializer->current_file->data,
	                                                  NULL, error);
	if (!child_cursor)
		return FALSE;

	tracker_deserializer_merger_add_child (TRACKER_DESERIALIZER_MERGER (deserializer),
	                                       TRACKER_DESERIALIZER (child_cursor));
	g_object_unref (child_cursor);

	return TRUE;
}

static gboolean
tracker_deserializer_directory_next (TrackerSparqlCursor  *cursor,
                                     GCancellable         *cancellable,
                                     GError              **error)
{
	TrackerDeserializerDirectory *deserializer =
		TRACKER_DESERIALIZER_DIRECTORY (cursor);
	gboolean retval = FALSE;

	if (!deserializer->files) {
		if (!get_ontology_files (deserializer->directory,
		                         &deserializer->files,
		                         error))
			return FALSE;

		deserializer->current_file = deserializer->files;

		if (!add_deserializer_for_current_file (deserializer, error))
			return FALSE;
	}

	while (!retval && deserializer->current_file) {
		retval = TRACKER_SPARQL_CURSOR_CLASS (tracker_deserializer_directory_parent_class)->next (cursor, cancellable, error);

		if (!retval) {
			deserializer->current_file = deserializer->current_file->next;

			if (!add_deserializer_for_current_file (deserializer, error))
				return FALSE;
		}
	}

	return retval;
}

static void
tracker_deserializer_directory_class_init (TrackerDeserializerDirectoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerSparqlCursorClass *cursor_class = TRACKER_SPARQL_CURSOR_CLASS (klass);

	object_class->finalize = tracker_deserializer_directory_finalize;
	object_class->set_property = tracker_deserializer_set_property;

	cursor_class->next = tracker_deserializer_directory_next;

	props[PROP_DIRECTORY] =
		g_param_spec_object ("directory", NULL, NULL,
		                     G_TYPE_FILE,
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS |
		                     G_PARAM_WRITABLE);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
tracker_deserializer_directory_init (TrackerDeserializerDirectory *deserializer)
{
}

TrackerSparqlCursor *
tracker_deserializer_directory_new (GFile                   *directory,
                                    TrackerNamespaceManager *namespaces)
{
	g_return_val_if_fail (G_IS_FILE (directory), NULL);
	g_return_val_if_fail (!namespaces || TRACKER_IS_NAMESPACE_MANAGER (namespaces), NULL);

	return g_object_new (TRACKER_TYPE_DESERIALIZER_DIRECTORY,
	                     "namespace-manager", namespaces,
	                     "directory", directory,
	                     NULL);
}

void
tracker_deserializer_directory_reset (TrackerDeserializerDirectory *deserializer)
{
	g_list_free_full (deserializer->files, g_object_unref);
	deserializer->files = NULL;
	deserializer->current_file = NULL;
}
