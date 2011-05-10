/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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
 */

#include "config.h"

#include <string.h>

#include "tracker-module-manager.h"

#define EXTRACTOR_FUNCTION "tracker_extract_get_metadata"
#define INIT_FUNCTION      "tracker_extract_module_init"
#define SHUTDOWN_FUNCTION  "tracker_extract_module_shutdown"

typedef struct {
	const gchar *module_path; /* intern string */
	GList *patterns;
} RuleInfo;

typedef struct {
	GModule *module;
	TrackerModuleThreadAwareness thread_awareness;
	TrackerExtractMetadataFunc extract_func;
	TrackerExtractInitFunc init_func;
	TrackerExtractShutdownFunc shutdown_func;
} ModuleInfo;

static GHashTable *modules = NULL;
static GHashTable *mimetype_map = NULL;
static gboolean initialized = FALSE;
static GArray *rules = NULL;

static gboolean
load_extractor_rule (GKeyFile  *key_file,
                     GError   **error)
{
	gchar *module_path, **mimetypes;
	gsize n_mimetypes, i;
	RuleInfo rule = { 0 };

	module_path = g_key_file_get_string (key_file, "ExtractorRule", "ModulePath", error);

	if (!module_path) {
		return FALSE;
	}

	mimetypes = g_key_file_get_string_list (key_file, "ExtractorRule", "MimeTypes", &n_mimetypes, error);

	if (!mimetypes) {
		g_free (module_path);
		return FALSE;
	}

	/* Construct the rule */
	rule.module_path = g_intern_string (module_path);

	for (i = 0; i < n_mimetypes; i++) {
		GPatternSpec *pattern;

		pattern = g_pattern_spec_new (mimetypes[i]);
		rule.patterns = g_list_prepend (rule.patterns, pattern);
	}

	if (G_UNLIKELY (!rules)) {
		rules = g_array_new (FALSE, TRUE, sizeof (RuleInfo));
	}

	g_array_append_val (rules, rule);
	g_strfreev (mimetypes);
	g_free (module_path);

	return TRUE;
}

gboolean
tracker_extract_module_manager_init (void)
{
	const gchar *extractors_dir, *name;
	GList *files = NULL, *l;
	GError *error = NULL;
	GDir *dir;

	if (initialized) {
		return TRUE;
	}

	if (!g_module_supported ()) {
		g_error ("Modules are not supported for this platform");
		return FALSE;
	}

	extractors_dir = g_getenv ("TRACKER_EXTRACTOR_RULES_DIR");
	if (G_LIKELY (extractors_dir == NULL)) {
		extractors_dir = TRACKER_EXTRACTOR_RULES_DIR;
	} else {
		g_message ("Extractor rules directory is '%s' (set in env)", extractors_dir);
	}

	dir = g_dir_open (extractors_dir, 0, &error);

	if (!dir) {
		g_error ("Error opening extractor rules directory: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		files = g_list_insert_sorted (files, (gpointer) name, (GCompareFunc) g_strcmp0);
	}

	g_message ("Loading extractor rules...");

	for (l = files; l; l = l->next) {
		GKeyFile *key_file;
		const gchar *name;
		gchar *path;

		name = l->data;

		if (!g_str_has_suffix (l->data, ".rule")) {
			g_message ("  Skipping file '%s', no '.rule' suffix", name);
			continue;
		}

		path = g_build_filename (extractors_dir, name, NULL);
		key_file = g_key_file_new ();

		if (!g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, &error) ||
		    !load_extractor_rule (key_file, &error)) {
			g_warning ("  Could not load extractor rule file '%s': %s", name, error->message);
			g_clear_error (&error);
			continue;
		}

		g_key_file_free (key_file);
		g_free (path);
	}

	g_message ("Extractor rules loaded");
	g_list_free (files);
	g_dir_close (dir);

	/* Initialize miscellaneous data */
	mimetype_map = g_hash_table_new_full (g_str_hash,
	                                      g_str_equal,
	                                      (GDestroyNotify) g_free,
	                                      NULL);
	initialized = TRUE;

	return TRUE;
}

static RuleInfo *
lookup_rule (const gchar *mimetype)
{
	RuleInfo *info;
	gchar *reversed;
	gint len, i;

	if (!rules) {
		return NULL;
	}

	if (!mimetype_map) {
		info = g_hash_table_lookup (mimetype_map, mimetype);

		if (info) {
			return info;
		}
	}

	reversed = g_strdup (mimetype);
	g_strreverse (reversed);
	len = strlen (mimetype);

	/* Apply the rules! */
	for (i = 0; i < rules->len; i++) {
		GList *l;

		info = &g_array_index (rules, RuleInfo, i);

		for (l = info->patterns; l; l = l->next) {
			if (g_pattern_match (l->data, len, mimetype, reversed)) {
				/* Match, store for future queries and return */
				g_hash_table_insert (mimetype_map, g_strdup (mimetype), info);
				g_free (reversed);
				return info;
			}
		}
	}

	g_free (reversed);

	return NULL;
}

GModule *
tracker_extract_module_manager_get_for_mimetype (const gchar                  *mimetype,
                                                 TrackerExtractInitFunc       *init_func,
                                                 TrackerExtractShutdownFunc   *shutdown_func,
                                                 TrackerExtractMetadataFunc   *extract_func)
{
	ModuleInfo *module_info = NULL;
	RuleInfo *info;

	if (init_func) {
		*init_func = NULL;
	}

	if (shutdown_func) {
		*shutdown_func = NULL;
	}

	if (extract_func) {
		*extract_func = NULL;
	}

	if (!initialized &&
	    !tracker_extract_module_manager_init ()) {
		return NULL;
	}

	info = lookup_rule (mimetype);

	if (!info) {
		return NULL;
	}

	if (modules) {
		module_info = g_hash_table_lookup (modules, info->module_path);
	}

	if (!module_info) {
		GModule *module;

		/* Load the module */
		module = g_module_open (info->module_path, G_MODULE_BIND_LOCAL);

		if (!module) {
			g_warning ("Could not load module '%s': %s",
			           info->module_path,
			           g_module_error ());
			return NULL;
		}

		g_module_make_resident (module);

		module_info = g_slice_new0 (ModuleInfo);
		module_info->module = module;

		if (!g_module_symbol (module, EXTRACTOR_FUNCTION, (gpointer *) &module_info->extract_func)) {
			g_warning ("Could not load module '%s': Function %s() was not found, is it exported?",
			           g_module_name (module), EXTRACTOR_FUNCTION);
			g_slice_free (ModuleInfo, module_info);
			return NULL;
		}

		g_module_symbol (module, INIT_FUNCTION, (gpointer *) &module_info->init_func);
		g_module_symbol (module, SHUTDOWN_FUNCTION, (gpointer *) &module_info->shutdown_func);

		/* Add it to the cache */
		if (G_UNLIKELY (!modules)) {
			/* Key is an intern string, so
			 * pointer comparison suffices
			 */
			modules = g_hash_table_new (NULL, NULL);
		}

		g_hash_table_insert (modules, (gpointer) info->module_path, module_info);
	}

	if (extract_func) {
		*extract_func = module_info->extract_func;
	}

	if (init_func) {
		*init_func = module_info->init_func;
	}

	if (shutdown_func) {
		*shutdown_func = module_info->shutdown_func;
	}

	return module_info->module;
}

gboolean
tracker_extract_module_manager_mimetype_is_handled (const gchar *mimetype)
{
	RuleInfo *info;

	if (!initialized &&
	    !tracker_extract_module_manager_init ()) {
		return FALSE;
	}

	info = lookup_rule (mimetype);

	return info != NULL;
}
