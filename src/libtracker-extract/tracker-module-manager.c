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
	GStrv fallback_rdf_types;
} RuleInfo;

typedef struct {
	GModule *module;
	TrackerExtractMetadataFunc extract_func;
	TrackerExtractInitFunc init_func;
	TrackerExtractShutdownFunc shutdown_func;
} ModuleInfo;

static gboolean dummy_extract_func (TrackerExtractInfo *info);

static ModuleInfo dummy_module = {
	NULL, dummy_extract_func, NULL, NULL
};

static GHashTable *modules = NULL;
static GHashTable *mimetype_map = NULL;
static gboolean initialized = FALSE;
static GArray *rules = NULL;

struct _TrackerMimetypeInfo {
	const GList *rules;
	const GList *cur;

	ModuleInfo *cur_module_info;
};

static gboolean
dummy_extract_func (TrackerExtractInfo *info)
{
	return TRUE;
}

static gboolean
load_extractor_rule (GKeyFile  *key_file,
                     GError   **error)
{
	GError *local_error = NULL;
	gchar *module_path, **mimetypes;
	gsize n_mimetypes, i;
	RuleInfo rule = { 0 };

	module_path = g_key_file_get_string (key_file, "ExtractorRule", "ModulePath", &local_error);

	if (local_error) {
		if (!g_error_matches (local_error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
			g_propagate_error (error, local_error);
			return FALSE;
		} else {
			/* Ignore */
			g_clear_error (&local_error);
		}
	}

	if (module_path &&
	    !G_IS_DIR_SEPARATOR (module_path[0])) {
		gchar *tmp;
		const gchar *extractors_dir;

		extractors_dir = g_getenv ("TRACKER_EXTRACTORS_DIR");
		if (G_LIKELY (extractors_dir == NULL)) {
			extractors_dir = TRACKER_EXTRACTORS_DIR;
		} else {
			g_message ("Extractor rules directory is '%s' (set in env)", extractors_dir);
		}

		tmp = g_build_filename (extractors_dir, module_path, NULL);
		g_free (module_path);
		module_path = tmp;
	}

	mimetypes = g_key_file_get_string_list (key_file, "ExtractorRule", "MimeTypes", &n_mimetypes, &local_error);

	if (!mimetypes) {
		g_free (module_path);

		if (local_error) {
			g_propagate_error (error, local_error);
		}

		return FALSE;
	}

	rule.fallback_rdf_types = g_key_file_get_string_list (key_file, "ExtractorRule", "FallbackRdfTypes", NULL, NULL);

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

	g_message ("Loading extractor rules... (%s)", extractors_dir);

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
		} else {
			g_debug ("  Loaded rule '%s'", name);
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

static GList *
lookup_rules (const gchar *mimetype)
{
	GList *mimetype_rules = NULL;
	RuleInfo *info;
	gchar *reversed;
	gint len, i;

	if (!rules) {
		return NULL;
	}

	if (mimetype_map) {
		mimetype_rules = g_hash_table_lookup (mimetype_map, mimetype);

		if (mimetype_rules) {
			return mimetype_rules;
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
				mimetype_rules = g_list_prepend (mimetype_rules, info);
			}
		}
	}

	if (mimetype_rules) {
		mimetype_rules = g_list_reverse (mimetype_rules);
		g_hash_table_insert (mimetype_map, g_strdup (mimetype), mimetype_rules);
	}

	g_free (reversed);

	return mimetype_rules;
}

GStrv
tracker_extract_module_manager_get_fallback_rdf_types (const gchar *mimetype)
{
	GList *l, *list;
	GHashTable *rdf_types;
	gchar **types, *type;
	GHashTableIter iter;
	gint i;

	if (!initialized &&
	    !tracker_extract_module_manager_init ()) {
		return NULL;
	}

	list = lookup_rules (mimetype);
	rdf_types = g_hash_table_new (g_str_hash, g_str_equal);

	for (l = list; l; l = l->next) {
		RuleInfo *r_info = l->data;

		if (r_info->fallback_rdf_types == NULL)
			continue;

		for (i = 0; r_info->fallback_rdf_types[i]; i++) {
                        g_debug ("Adding RDF type: %s, for module: %s",
                                 r_info->fallback_rdf_types[i],
                                 r_info->module_path);
			g_hash_table_insert (rdf_types,
					     r_info->fallback_rdf_types[i],
					     r_info->fallback_rdf_types[i]);
		}

                /* We only want the first RDF types matching */
                break;
	}

	g_hash_table_iter_init (&iter, rdf_types);
	types = g_new0 (gchar*, g_hash_table_size (rdf_types) + 1);
	i = 0;

	while (g_hash_table_iter_next (&iter, (gpointer*) &type, NULL)) {
		types[i] = g_strdup (type);
		i++;
	}

	g_hash_table_unref (rdf_types);

	return types;
}

static ModuleInfo *
load_module (RuleInfo *info)
{
	ModuleInfo *module_info = NULL;

	if (!info->module_path) {
		return &dummy_module;
	}

	if (modules) {
		module_info = g_hash_table_lookup (modules, info->module_path);
	}

	if (!module_info) {
		GModule *module;
		GError *init_error = NULL;

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

		if (module_info->init_func &&
		    !(module_info->init_func) (&init_error)) {
			g_critical ("Could not initialize module %s: %s",
			            g_module_name (module_info->module),
			            (init_error) ? init_error->message : "No error given");

			g_clear_error (&init_error);
			g_slice_free (ModuleInfo, module_info);
			return NULL;
		}

		/* Add it to the cache */
		if (G_UNLIKELY (!modules)) {
			/* Key is an intern string, so
			 * pointer comparison suffices
			 */
			modules = g_hash_table_new (NULL, NULL);
		}

		g_hash_table_insert (modules, (gpointer) info->module_path, module_info);
	}

	return module_info;
}

static gboolean
initialize_first_module (TrackerMimetypeInfo *info)
{
	ModuleInfo *module_info = NULL;

	/* Actually iterates through the list loaded + initialized module */
	while (info->cur && !module_info) {
		module_info = load_module (info->cur->data);

		if (!module_info) {
			info->cur = info->cur->next;
		}
	}

	info->cur_module_info = module_info;
	return (info->cur_module_info != NULL);
}

/**
 * tracker_extract_module_manager_get_mimetype_handlers:
 * @mimetype: a mimetype string
 *
 * Returns a #TrackerMimetypeInfo struct containing information about
 * the modules that handle @mimetype, or %NULL if no modules handle
 * @mimetype.
 *
 * The modules are ordered from most to least specific, and the
 * returned #TrackerMimetypeInfo already points to the first
 * module.
 *
 * Returns: (transfer full): (free-function: tracker_mimetype_info_free): (allow-none):
 * A #TrackerMimetypeInfo holding the information about the different
 * modules handling @mimetype, or %NULL if no modules handle @mimetype.
 *
 * Since: 0.12
 **/
TrackerMimetypeInfo *
tracker_extract_module_manager_get_mimetype_handlers (const gchar *mimetype)
{
	TrackerMimetypeInfo *info;
	GList *mimetype_rules;

	g_return_val_if_fail (mimetype != NULL, NULL);

	mimetype_rules = lookup_rules (mimetype);

	if (!mimetype_rules) {
		return NULL;
	}

	info = g_slice_new0 (TrackerMimetypeInfo);
	info->rules = mimetype_rules;
	info->cur = info->rules;

	if (!initialize_first_module (info)) {
		tracker_mimetype_info_free (info);
		info = NULL;
	}

	return info;
}

/**
 * tracker_mimetype_info_get_module:
 * @info: a #TrackerMimetypeInfo
 * @extract_func: (out): (allow-none): return value for the extraction function
 *
 * Returns the #GModule that @info is currently pointing to, if @extract_func is
 * not %NULL, it will be filled in with the pointer to the metadata extraction
 * function.
 *
 * Returns: The %GModule currently pointed to by @info.
 *
 * Since: 0.12
 **/
GModule *
tracker_mimetype_info_get_module (TrackerMimetypeInfo          *info,
                                  TrackerExtractMetadataFunc   *extract_func)
{
	g_return_val_if_fail (info != NULL, NULL);

	if (!info->cur_module_info) {
		return NULL;
	}

	if (extract_func) {
		*extract_func = info->cur_module_info->extract_func;
	}

	return info->cur_module_info->module;
}

/**
 * tracker_mimetype_info_iter_next:
 * @info: a #TrackerMimetypeInfo
 *
 * Iterates to the next module handling the mimetype.
 *
 * Returns: %TRUE if there is a next module.
 *
 * Since: 0.12
 **/
gboolean
tracker_mimetype_info_iter_next (TrackerMimetypeInfo *info)
{
	g_return_val_if_fail (info != NULL, FALSE);

	if (info->cur->next) {
		info->cur = info->cur->next;
		return initialize_first_module (info);
	}

	return FALSE;
}

void
tracker_mimetype_info_free (TrackerMimetypeInfo *info)
{
	g_return_if_fail (info != NULL);

	g_slice_free (TrackerMimetypeInfo, info);
}

void
tracker_module_manager_load_modules (void)
{
	RuleInfo *rule_info;
	guint i;

	g_return_if_fail (initialized == TRUE);

	for (i = 0; i < rules->len; i++) {
		rule_info = &g_array_index (rules, RuleInfo, i);
		load_module (rule_info);
	}
}
