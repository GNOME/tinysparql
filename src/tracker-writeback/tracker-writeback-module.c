/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#include <gmodule.h>

#include "tracker-writeback-module.h"

static TrackerMinerManager *manager = NULL;

static gboolean writeback_module_load   (GTypeModule *module);
static void     writeback_module_unload (GTypeModule *module);

G_DEFINE_TYPE (TrackerWritebackModule, tracker_writeback_module, G_TYPE_TYPE_MODULE)

G_DEFINE_ABSTRACT_TYPE (TrackerWriteback, tracker_writeback, G_TYPE_OBJECT)


static void
tracker_writeback_module_class_init (TrackerWritebackModuleClass *klass)
{
	GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (klass);

	module_class->load = writeback_module_load;
	module_class->unload = writeback_module_unload;
}

static void
tracker_writeback_module_init (TrackerWritebackModule *module)
{
}

static gboolean
writeback_module_load (GTypeModule *module)
{
	TrackerWritebackModule *writeback_module;
	gchar *path;

	writeback_module = TRACKER_WRITEBACK_MODULE (module);

	path = g_build_filename (WRITEBACK_MODULES_DIR, writeback_module->name, NULL);
	writeback_module->module = g_module_open (path, G_MODULE_BIND_LOCAL);
	g_free (path);

	if (G_UNLIKELY (!writeback_module->module)) {
		g_warning ("Could not load writeback module '%s': %s\n",
		           writeback_module->name,
		           g_module_error ());

		return FALSE;
	}

	g_module_make_resident (writeback_module->module);

	if (!g_module_symbol (writeback_module->module, "writeback_module_create",
	                      (gpointer *) &writeback_module->create) ||
	    !g_module_symbol (writeback_module->module, "writeback_module_get_rdf_types",
	                      (gpointer *) &writeback_module->get_rdf_types)) {
		g_warning ("Could not load module symbols for '%s': %s",
		           writeback_module->name,
		           g_module_error ());

		return FALSE;
	}

	g_message ("Loaded module:'%s'", writeback_module->name);

	return TRUE;
}

static void
writeback_module_unload (GTypeModule *module)
{
	TrackerWritebackModule *writeback_module;

	writeback_module = TRACKER_WRITEBACK_MODULE (module);

	g_module_close (writeback_module->module);
	writeback_module->module = NULL;

	g_message ("Unloaded module:'%s'", writeback_module->name);
}

TrackerWritebackModule *
tracker_writeback_module_get (const gchar *name)
{
	static GHashTable *modules = NULL;
	TrackerWritebackModule *module;

	g_return_val_if_fail (name != NULL, NULL);

	if (G_UNLIKELY (!modules)) {
		modules = g_hash_table_new (g_str_hash, g_str_equal);
	}

	module = g_hash_table_lookup (modules, name);

	if (G_UNLIKELY (!module)) {
		module = g_object_new (TRACKER_TYPE_WRITEBACK_MODULE, NULL);
		g_type_module_set_name (G_TYPE_MODULE (module), name);
		module->name = g_strdup (name);

		g_hash_table_insert (modules, module->name, module);
	}

	if (!g_type_module_use (G_TYPE_MODULE (module))) {
		return NULL;
	}

	return module;
}

GList *
tracker_writeback_modules_list (void)
{
	GError *error = NULL;
	const gchar *name;
	GList *list = NULL;
	GDir *dir;

	dir = g_dir_open (WRITEBACK_MODULES_DIR, 0, &error);

	if (error) {
		g_critical ("Could not get writeback modules list: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		if (!g_str_has_suffix (name, G_MODULE_SUFFIX)) {
			continue;
		}

		list = g_list_prepend (list, g_strdup (name));
	}

	g_dir_close (dir);

	return g_list_reverse (list);
}

TrackerWriteback *
tracker_writeback_module_create (TrackerWritebackModule *module)
{
	return (module->create) (G_TYPE_MODULE (module));
}

const gchar * const *
tracker_writeback_module_get_rdf_types (TrackerWritebackModule *module)
{
	return (module->get_rdf_types) ();
}

static void
tracker_writeback_class_init (TrackerWritebackClass *klass)
{
}

static void
tracker_writeback_init (TrackerWriteback *writeback)
{
}

gboolean
tracker_writeback_update_metadata (TrackerWriteback        *writeback,
                                   GPtrArray               *values,
                                   TrackerSparqlConnection *connection)
{
	g_return_val_if_fail (TRACKER_IS_WRITEBACK (writeback), FALSE);
	g_return_val_if_fail (values != NULL, FALSE);

	if (TRACKER_WRITEBACK_GET_CLASS (writeback)->update_metadata) {
		return TRACKER_WRITEBACK_GET_CLASS (writeback)->update_metadata (writeback, values, connection);
	}

	return FALSE;
}

TrackerMinerManager*
tracker_writeback_get_miner_manager (void)
{
	if (!manager) {
		manager = tracker_miner_manager_new ();
	}

	return manager;
}
