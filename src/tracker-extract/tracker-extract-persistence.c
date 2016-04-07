/*
 * Copyright (C) 2014 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "tracker-extract-persistence.h"

#define MAX_RETRIES 3

typedef struct _TrackerExtractPersistencePrivate TrackerExtractPersistencePrivate;

struct _TrackerExtractPersistencePrivate
{
	GFile *tmp_dir;
};

G_DEFINE_TYPE_WITH_PRIVATE (TrackerExtractPersistence, tracker_extract_persistence, G_TYPE_OBJECT)

static GQuark n_retries_quark = 0;

static void
tracker_extract_persistence_class_init (TrackerExtractPersistenceClass *klass)
{
	n_retries_quark = g_quark_from_static_string ("tracker-extract-n-retries-quark");
}

static void
tracker_extract_persistence_init (TrackerExtractPersistence *persistence)
{
	TrackerExtractPersistencePrivate *priv;
	gchar *dirname, *tmp_path;

	priv = tracker_extract_persistence_get_instance_private (persistence);

	dirname = g_strdup_printf ("tracker-extract-files.%d", getuid ());
	tmp_path = g_build_filename (g_get_tmp_dir (), dirname, NULL);
	g_free (dirname);

	if (g_mkdir_with_parents (tmp_path, 0700) != 0) {
		g_critical ("The directory %s could not be created, or has the wrong permissions",
		            tmp_path);
		g_assert_not_reached ();
	}

	priv->tmp_dir = g_file_new_for_path (tmp_path);
	g_free (tmp_path);
}

static void
increment_n_retries (GFile *file)
{
	guint n_retries;

	n_retries = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (file), n_retries_quark));
	g_object_set_qdata (G_OBJECT (file), n_retries_quark, GUINT_TO_POINTER (n_retries + 1));
}

static GFile *
persistence_create_symlink_file (TrackerExtractPersistence *persistence,
                                 GFile                     *file)
{
	TrackerExtractPersistencePrivate *priv;
	guint n_retries = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (file), n_retries_quark));
	gchar *link_name, *path, *md5;
	GFile *link_file;

	priv = tracker_extract_persistence_get_instance_private (persistence);
	path = g_file_get_path (file);
	md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, path, -1);
	link_name = g_strdup_printf ("%d-%s", n_retries, md5);
	link_file = g_file_get_child (priv->tmp_dir, link_name);

	g_free (link_name);
	g_free (path);
	g_free (md5);

	return link_file;
}

static GFile *
persistence_symlink_get_file (GFileInfo *info)
{
	const gchar *symlink_name, *symlink_target;
	gchar *md5, **items;
	GFile *file = NULL;
	guint n_retries;

	symlink_name = g_file_info_get_name (info);
	symlink_target = g_file_info_get_symlink_target (info);

	if (!g_path_is_absolute (symlink_target)) {
		g_critical ("Symlink paths must be absolute, '%s' points to '%s'",
		            symlink_name, symlink_target);
		return NULL;
	}

	md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, symlink_target, -1);
	items = g_strsplit (symlink_name, "-", 2);
	n_retries = g_strtod (items[0], NULL);

	if (g_strcmp0 (items[1], md5) == 0) {
		file = g_file_new_for_path (symlink_target);
		g_object_set_qdata (G_OBJECT (file), n_retries_quark,
		                    GUINT_TO_POINTER (n_retries));
	} else {
		g_critical ("path MD5 for '%s' doesn't match with symlink '%s'",
		            symlink_target, symlink_name);
	}

	g_strfreev (items);
	g_free (md5);

	return file;
}

static gboolean
persistence_store_file (TrackerExtractPersistence *persistence,
                        GFile                     *file)
{
	GError *error = NULL;
	gboolean success;
	GFile *link_file;
	gchar *path;

	increment_n_retries (file);
	path = g_file_get_path (file);
	link_file = persistence_create_symlink_file (persistence, file);

	success = g_file_make_symbolic_link (link_file, path, NULL, &error);

	if (!success) {
		g_warning ("Could not save '%s' into failsafe persistence store: %s",
		           path, error ? error->message : "no error given");
		g_clear_error (&error);
	}

	g_object_unref (link_file);
	g_free (path);

	return success;
}

static gboolean
persistence_remove_file (TrackerExtractPersistence *persistence,
                         GFile                     *file)
{
	GError *error = NULL;
	GFile *link_file;
	gboolean success;

	link_file = persistence_create_symlink_file (persistence, file);
	success = g_file_delete (link_file, NULL, &error);

	if (!success) {
		gchar *path = g_file_get_path (file);

		g_warning ("Could not delete '%s' from failsafe persistence store",
		           path);
		g_free (path);
	}

	g_object_unref (link_file);

	return success;
}

static void
persistence_retrieve_files (TrackerExtractPersistence *persistence,
                            TrackerFileRecoveryFunc    retry_func,
                            TrackerFileRecoveryFunc    ignore_func,
                            gpointer                   user_data)
{
	TrackerExtractPersistencePrivate *priv;
	GFileEnumerator *enumerator;
	GFileInfo *info;

	priv = tracker_extract_persistence_get_instance_private (persistence);
	enumerator = g_file_enumerate_children (priv->tmp_dir,
	                                        G_FILE_ATTRIBUTE_STANDARD_NAME ","
	                                        G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
	                                        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                                        NULL, NULL);
	if (!enumerator)
		return;

	while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {
		GFile *file, *symlink_file;
		guint n_retries;

		symlink_file = g_file_enumerator_get_child (enumerator, info);
		file = persistence_symlink_get_file (info);

		if (!file) {
			/* If we got here, persistence_symlink_get_file() already emitted a g_critical */
			g_object_unref (symlink_file);
			g_object_unref (info);
			continue;
		}

		/* Delete the symlink, it will get probably added back soon after,
		 * and n_retries incremented.
		 */
		g_file_delete (symlink_file, NULL, NULL);
		g_object_unref (symlink_file);

		n_retries = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (file), n_retries_quark));

		/* Trigger retry/ignore func for the symlink target */
		if (n_retries >= MAX_RETRIES) {
			ignore_func (file, user_data);
		} else {
			retry_func (file, user_data);
		}

		g_object_unref (file);
		g_object_unref (info);
	}

	g_file_enumerator_close (enumerator, NULL, NULL);
	g_object_unref (enumerator);
}

TrackerExtractPersistence *
tracker_extract_persistence_initialize (TrackerFileRecoveryFunc retry_func,
                                        TrackerFileRecoveryFunc ignore_func,
                                        gpointer                user_data)
{
	static TrackerExtractPersistence *persistence = NULL;

	if (!persistence) {
		persistence = g_object_new (TRACKER_TYPE_EXTRACT_PERSISTENCE,
		                            NULL);
		persistence_retrieve_files (persistence,
		                            retry_func, ignore_func,
		                            user_data);
	}

	return persistence;
}

void
tracker_extract_persistence_add_file (TrackerExtractPersistence *persistence,
                                      GFile                     *file)
{
	g_return_if_fail (TRACKER_IS_EXTRACT_PERSISTENCE (persistence));
	g_return_if_fail (G_IS_FILE (file));

	persistence_store_file (persistence, file);
}

void
tracker_extract_persistence_remove_file (TrackerExtractPersistence *persistence,
                                         GFile                     *file)
{
	g_return_if_fail (TRACKER_IS_EXTRACT_PERSISTENCE (persistence));
	g_return_if_fail (G_IS_FILE (file));

	persistence_remove_file (persistence, file);
}
