/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include "tracker-module-config.h"
#include "tracker-file-utils.h"
#include "tracker-type-utils.h"

#define GROUP_GENERAL  "General"
#define GROUP_MONITORS "Monitors"
#define GROUP_IGNORED  "Ignored"
#define GROUP_INDEX    "Index"
#define GROUP_SPECIFIC "Specific"

typedef struct {
	/* General */
	gchar	   *description;
	gboolean    enabled;

	/* Monitors */
	GHashTable *monitor_directories;
	GHashTable *monitor_recurse_directories;

	/* Ignored */
	GHashTable *ignored_directories;
	GHashTable *ignored_files;

	GList	   *ignored_directory_patterns;
	GList	   *ignored_file_patterns;

	/* Index */
	gchar	   *index_service;
	GHashTable *index_mime_types;
	GHashTable *index_files;
	GList	   *index_file_patterns;

	/* Specific Options, FIXME: Finish */

} ModuleConfig;

static gboolean      initiated;
static GHashTable   *modules;
static GFileMonitor *monitor;

static void
module_destroy_notify (gpointer data)
{
	ModuleConfig *mc;

	mc = (ModuleConfig*) data;

	g_list_foreach (mc->index_file_patterns,
			(GFunc) g_pattern_spec_free,
			NULL);
	g_list_free (mc->index_file_patterns);

	g_hash_table_unref (mc->index_files);
	g_hash_table_unref (mc->index_mime_types);
	g_free (mc->index_service);

	g_list_foreach (mc->ignored_file_patterns,
			(GFunc) g_pattern_spec_free,
			NULL);
	g_list_free (mc->ignored_file_patterns);

	g_list_foreach (mc->ignored_directory_patterns,
			(GFunc) g_pattern_spec_free,
			 NULL);
	g_list_free (mc->ignored_directory_patterns);

	g_hash_table_unref (mc->ignored_files);
	g_hash_table_unref (mc->ignored_directories);

	g_hash_table_unref (mc->monitor_recurse_directories);
	g_hash_table_unref (mc->monitor_directories);

	g_free (mc->description);

	g_slice_free (ModuleConfig, mc);
}

static void
check_for_monitor_directory_conflicts (ModuleConfig *mc)
{
	GHashTableIter iter1, iter2;
	gpointer       key;

	/* Make sure we don't have duplicates from the monitor
	 * directories in the monitor recurse directories hash table
	 * and also, make sure there are no recurse directories higher
	 * as parents of monitor directories, this would duplicate
	 * monitors unnecesarily.
	 */
	g_hash_table_iter_init (&iter1, mc->monitor_directories);
	while (g_hash_table_iter_next (&iter1, &key, NULL)) {
		const gchar *path;

		path = (const gchar*) key;

		if (g_hash_table_lookup (mc->monitor_recurse_directories, path)) {
			g_debug ("Removing path:'%s' from monitor directories, "
				 "ALREADY in monitor recurse directories",
				 path);

			g_hash_table_iter_remove (&iter1);
			g_hash_table_iter_init (&iter1, mc->monitor_directories);
			continue;
		}

		g_hash_table_iter_init (&iter2, mc->monitor_recurse_directories);
		while (g_hash_table_iter_next (&iter2, &key, NULL)) {
			const gchar *in_path;

			in_path = (const gchar*) key;

			if (path == in_path) {
				continue;
			}

			if (tracker_path_is_in_path (path, in_path)) {
				g_debug ("Removing path:'%s' from monitor directories, "
					 "ALREADY in monitor recurse directories HIERARCHY",
					 path);

				g_hash_table_iter_remove (&iter1);
				g_hash_table_iter_init (&iter1, mc->monitor_directories);
				break;
			}
		}
	}
}

static gchar *
get_directory (void)
{
	return g_build_path (G_DIR_SEPARATOR_S, SHAREDIR, "tracker", "modules", NULL);
}

static void
set_ignored_file_patterns (ModuleConfig *mc)
{
	GPatternSpec *spec;
	GList	     *ignored_files;
	GList	     *l;
	GList	     *patterns = NULL;

	g_list_foreach (mc->ignored_file_patterns,
			(GFunc) g_pattern_spec_free,
			NULL);
	g_list_free (mc->ignored_file_patterns);

	ignored_files = g_hash_table_get_keys (mc->ignored_files);

	for (l = ignored_files; l; l = l->next) {
		g_message ("  Adding file ignore pattern:'%s'",
			   (gchar *) l->data);
		spec = g_pattern_spec_new (l->data);
		patterns = g_list_prepend (patterns, spec);
	}

	g_list_free (ignored_files);

	mc->ignored_file_patterns = g_list_reverse (patterns);
}

static void
set_ignored_directory_patterns (ModuleConfig *mc)
{
	GPatternSpec *spec;
	GList	     *ignored_directories;
	GList	     *l;
	GList	     *patterns = NULL;

	g_list_foreach (mc->ignored_directory_patterns,
			(GFunc) g_pattern_spec_free,
			NULL);
	g_list_free (mc->ignored_directory_patterns);

	ignored_directories = g_hash_table_get_keys (mc->ignored_directories);

	for (l = ignored_directories; l; l = l->next) {
		g_message ("  Adding directory ignore pattern:'%s'",
			   (gchar *) l->data);
		spec = g_pattern_spec_new (l->data);
		patterns = g_list_prepend (patterns, spec);
	}

	g_list_free (ignored_directories);

	mc->ignored_directory_patterns = g_list_reverse (patterns);
}

static void
set_index_file_patterns (ModuleConfig *mc)
{
	GPatternSpec *spec;
	GList	     *index_files;
	GList	     *l;
	GList	     *patterns = NULL;

	g_list_foreach (mc->index_file_patterns,
			(GFunc) g_pattern_spec_free,
			NULL);
	g_list_free (mc->index_file_patterns);

	index_files = g_hash_table_get_keys (mc->index_files);

	for (l = index_files; l; l = l->next) {
		g_message ("  Adding file index pattern:'%s'",
			   (gchar *) l->data);
		spec = g_pattern_spec_new (l->data);
		patterns = g_list_prepend (patterns, spec);
	}

	g_list_free (index_files);

	mc->index_file_patterns = g_list_reverse (patterns);
}

static gboolean
load_boolean (GKeyFile	  *key_file,
	      const gchar *group,
	      const gchar *key)
{
	GError	 *error = NULL;
	gboolean  boolean;

	boolean = g_key_file_get_boolean (key_file, group, key, &error);

	if (error) {
		g_message ("Couldn't load module config boolean in "
			   "group:'%s' with key:'%s', %s",
			   group,
			   key,
			   error->message);

		g_error_free (error);
		g_key_file_free (key_file);

		return FALSE;
	}

	return boolean;
}

static gchar *
load_string (GKeyFile	 *key_file,
	      const gchar *group,
	      const gchar *key,
	      gboolean	   expand_string_as_path)
{
	GError *error = NULL;
	gchar  *str;

	str = g_key_file_get_string (key_file, group, key, &error);

	if (error) {
		g_message ("Couldn't load module config string in "
			   "group:'%s' with key:'%s', %s",
			   group,
			   key,
			   error->message);

		g_error_free (error);
		g_key_file_free (key_file);

		return NULL;
	}

	if (expand_string_as_path) {
		gchar *real_path;

		real_path = tracker_path_evaluate_name (str);
		g_free (str);

		return real_path;
	}

	return str;
}

static GHashTable *
load_string_list (GKeyFile    *key_file,
		  const gchar *group,
		  const gchar *key,
		  gboolean     expand_strings_as_paths,
		  gboolean     remove_hierarchy_dups)
{
	GError	    *error = NULL;
	GHashTable  *table;
	gchar	   **str;
	gchar	   **p;
	gsize	     size;

	table = g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       g_free,
				       NULL);

	str = g_key_file_get_string_list (key_file, group, key, &size, &error);

	if (error) {
		g_message ("Couldn't load module config string list in "
			   "group:'%s' with key:'%s', %s",
			   group,
			   key,
			   error->message);

		g_error_free (error);
		g_key_file_free (key_file);

		return table;
	}

	for (p = str; *p; p++) {
		gchar *real_path;

		if (!expand_strings_as_paths) {
			if (g_hash_table_lookup (table, *p)) {
				continue;
			}

			g_hash_table_insert (table,
					     g_strdup (*p),
					     GINT_TO_POINTER (1));
		} else {
			if (g_hash_table_lookup (table, *p)) {
				continue;
			}

			real_path = tracker_path_evaluate_name (*p);
			if (g_hash_table_lookup (table, real_path)) {
				g_free (real_path);
				continue;
			}

			g_hash_table_insert (table,
					     real_path,
					     GINT_TO_POINTER (1));
			g_debug ("Got real path:'%s' for '%s'", real_path, *p);
		}
	}

	g_strfreev (str);

	/* Go through again to make sure we don't have situations
	 * where /foo and / exist, because of course /foo is
	 * redundant here where 'remove_hierarchy_dups' is TRUE.
	 */
	if (remove_hierarchy_dups) {
		tracker_path_hash_table_filter_duplicates (table);
	}

	return table;
}

static ModuleConfig *
load_file (const gchar *filename)
{
	GKeyFile     *key_file;
	GError	     *error = NULL;
	ModuleConfig *mc;

	key_file = g_key_file_new ();

	/* Load options */
	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, &error);

	if (error) {
		g_message ("Couldn't load module config for '%s', %s",
			   filename,
			   error->message);

		g_error_free (error);
		g_key_file_free (key_file);

		return NULL;
	}

	g_message ("Loading module config:'%s'", filename);

	mc = g_slice_new0 (ModuleConfig);

	/* General */
	mc->description = load_string (key_file,
				       GROUP_GENERAL,
				       "Description",
				       FALSE);
	mc->enabled = load_boolean (key_file,
				    GROUP_GENERAL,
				    "Enabled");

	/* Monitors */
	mc->monitor_directories = load_string_list (key_file,
						    GROUP_MONITORS,
						    "Directories",
						    TRUE,
						    FALSE);
	mc->monitor_recurse_directories = load_string_list (key_file,
							   GROUP_MONITORS,
							   "RecurseDirectories",
							   TRUE,
							   TRUE);

	/* Ignored */
	mc->ignored_directories = load_string_list (key_file,
						    GROUP_IGNORED,
						    "Directories",
						    TRUE,
						    FALSE);
	mc->ignored_files = load_string_list (key_file,
					      GROUP_IGNORED,
					      "Files",
					      FALSE,
					      FALSE);

	/* Index */
	mc->index_service = load_string (key_file,
					 GROUP_INDEX,
					 "Service",
					 FALSE);
	mc->index_mime_types = load_string_list (key_file,
						 GROUP_INDEX,
						 "MimeTypes",
						 FALSE,
						 FALSE);
	mc->index_files = load_string_list (key_file,
					    GROUP_INDEX,
					    "Files",
					    FALSE,
					    FALSE);

	check_for_monitor_directory_conflicts (mc);

	/* FIXME: Specific options */

	set_ignored_file_patterns (mc);
	set_ignored_directory_patterns (mc);
	set_index_file_patterns (mc);

	g_key_file_free (key_file);

	return mc;
}

static gboolean
load_directory (void)
{
	GFile		*file;
	GFileEnumerator *enumerator;
	GFileInfo	*info;
	GError		*error = NULL;
	gchar		*path;
	gchar		*filename;
	const gchar	*name;
	const gchar	*extension;
	glong		 extension_len;

	path = get_directory ();
	file = g_file_new_for_path (path);

	enumerator = g_file_enumerate_children (file,
						G_FILE_ATTRIBUTE_STANDARD_NAME ","
						G_FILE_ATTRIBUTE_STANDARD_TYPE,
						G_PRIORITY_DEFAULT,
						NULL,
						&error);

	if (error) {
		g_warning ("Could not get module config from directory:'%s', %s",
			   path,
			   error->message);

		g_free (path);
		g_error_free (error);
		g_object_unref (file);

		return FALSE;
	}

	extension = ".module";
	extension_len = g_utf8_strlen (extension, -1);

	/* We should probably do this async */
	for (info = g_file_enumerator_next_file (enumerator, NULL, &error);
	     info && !error;
	     info = g_file_enumerator_next_file (enumerator, NULL, &error)) {
		GFile	     *child;
		ModuleConfig *mc;

		name = g_file_info_get_name (info);

		if (!g_str_has_suffix (name, extension)) {
			g_object_unref (info);
			continue;
		}

		child = g_file_get_child (file, name);
		filename = g_file_get_path (child);
		mc = load_file (filename);
		g_free (filename);

		if (mc) {
			gchar *name_stripped;

			name_stripped = g_strndup (name, g_utf8_strlen (name, -1) - extension_len);

			g_hash_table_insert (modules,
					     name_stripped,
					     mc);
		}

		g_object_unref (child);
		g_object_unref (info);
	}

	if (error) {
		g_warning ("Could not get module config information from directory:'%s', %s",
			   path,
			   error->message);
		g_error_free (error);
	}

	g_message ("Loaded module config, %d found",
		   g_hash_table_size (modules));

	g_object_unref (enumerator);
	g_object_unref (file);
	g_free (path);

	return TRUE;
}

static void
changed_cb (GFileMonitor     *monitor,
	    GFile	     *file,
	    GFile	     *other_file,
	    GFileMonitorEvent event_type,
	    gpointer	      user_data)
{
	gchar *filename;

	/* Do we recreate if the file is deleted? */

	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CHANGED:
	case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		filename = g_file_get_path (file);
		g_message ("Config file changed:'%s', reloading settings...",
			   filename);
		g_free (filename);

		load_directory ();
		break;

	default:
		break;
	}
}

gboolean
tracker_module_config_init (void)
{
	GFile *file;
	gchar *path;

	if (initiated) {
		return TRUE;
	}

	path = get_directory ();
	if (!g_file_test (path, G_FILE_TEST_IS_DIR | G_FILE_TEST_EXISTS)) {
		g_critical ("Module config directory:'%s' doesn't exist",
			    path);
		g_free (path);
		return FALSE;
	}

	modules = g_hash_table_new_full (g_str_hash,
					 g_str_equal,
					 g_free,
					 module_destroy_notify);

	/* Get modules */
	if (!load_directory ()) {
		g_hash_table_unref (modules);
		g_free (path);
		return FALSE;
	}

	/* Add file monitoring for changes */
	g_message ("Setting up monitor for changes to modules directory:'%s'",
		   path);

	file = g_file_new_for_path (path);
	monitor = g_file_monitor_directory (file,
					    G_FILE_MONITOR_NONE,
					    NULL,
					    NULL);

	g_signal_connect (monitor, "changed",
			  G_CALLBACK (changed_cb),
			  NULL);

	g_object_unref (file);
	g_free (path);

	initiated = TRUE;

	return TRUE;
}

void
tracker_module_config_shutdown (void)
{
	if (!initiated) {
		return;
	}

	g_signal_handlers_disconnect_by_func (monitor, changed_cb, NULL);

	g_object_unref (monitor);

	g_hash_table_unref (modules);

	initiated = FALSE;
}

GList *
tracker_module_config_get_modules (void)
{
	return g_hash_table_get_keys (modules);
}

const gchar *
tracker_module_config_get_description (const gchar *name)
{
	ModuleConfig *mc;

	g_return_val_if_fail (name != NULL, NULL);

	mc = g_hash_table_lookup (modules, name);
	g_return_val_if_fail (mc, NULL);

	return mc->description;
}

gboolean
tracker_module_config_get_enabled (const gchar *name)
{
	ModuleConfig *mc;

	g_return_val_if_fail (name != NULL, FALSE);

	mc = g_hash_table_lookup (modules, name);
	g_return_val_if_fail (mc, FALSE);

	return mc->enabled;
}

GList *
tracker_module_config_get_monitor_directories (const gchar *name)
{
	ModuleConfig *mc;

	g_return_val_if_fail (name != NULL, FALSE);

	mc = g_hash_table_lookup (modules, name);
	g_return_val_if_fail (mc, NULL);

	return g_hash_table_get_keys (mc->monitor_directories);
}

GList *
tracker_module_config_get_monitor_recurse_directories (const gchar *name)
{
	ModuleConfig *mc;

	g_return_val_if_fail (name != NULL, FALSE);

	mc = g_hash_table_lookup (modules, name);
	g_return_val_if_fail (mc, NULL);

	return g_hash_table_get_keys (mc->monitor_recurse_directories);
}

GList *
tracker_module_config_get_ignored_directories (const gchar *name)
{
	ModuleConfig *mc;

	g_return_val_if_fail (name != NULL, FALSE);

	mc = g_hash_table_lookup (modules, name);
	g_return_val_if_fail (mc, NULL);

	return g_hash_table_get_keys (mc->ignored_directories);
}

GList *
tracker_module_config_get_ignored_files (const gchar *name)
{
	ModuleConfig *mc;

	g_return_val_if_fail (name != NULL, FALSE);

	mc = g_hash_table_lookup (modules, name);
	g_return_val_if_fail (mc, NULL);

	return g_hash_table_get_keys (mc->ignored_files);
}

const gchar *
tracker_module_config_get_index_service (const gchar *name)
{
	ModuleConfig *mc;

	g_return_val_if_fail (name != NULL, NULL);

	mc = g_hash_table_lookup (modules, name);
	g_return_val_if_fail (mc, NULL);

	return mc->index_service;
}

GList *
tracker_module_config_get_index_mime_types (const gchar *name)
{
	ModuleConfig *mc;

	g_return_val_if_fail (name != NULL, FALSE);

	mc = g_hash_table_lookup (modules, name);
	g_return_val_if_fail (mc, NULL);

	return g_hash_table_get_keys (mc->index_mime_types);
}

GList *
tracker_module_config_get_index_files (const gchar *name)
{
	ModuleConfig *mc;

	g_return_val_if_fail (name != NULL, FALSE);

	mc = g_hash_table_lookup (modules, name);
	g_return_val_if_fail (mc, NULL);

	return g_hash_table_get_keys (mc->index_files);
}

/*
 * Convenience functions
 */

GList *
tracker_module_config_get_ignored_file_patterns (const gchar *name)
{
	ModuleConfig *mc;

	g_return_val_if_fail (name != NULL, NULL);

	mc = g_hash_table_lookup (modules, name);
	g_return_val_if_fail (mc, NULL);

	return g_list_copy (mc->ignored_file_patterns);
}

GList *
tracker_module_config_get_ignored_directory_patterns (const gchar *name)
{
	ModuleConfig *mc;

	g_return_val_if_fail (name != NULL, NULL);

	mc = g_hash_table_lookup (modules, name);
	g_return_val_if_fail (mc, NULL);

	return g_list_copy (mc->ignored_directory_patterns);
}

GList *
tracker_module_config_get_index_file_patterns (const gchar *name)
{
	ModuleConfig *mc;

	g_return_val_if_fail (name != NULL, NULL);

	mc = g_hash_table_lookup (modules, name);
	g_return_val_if_fail (mc, NULL);

	return g_list_copy (mc->index_file_patterns);
}
