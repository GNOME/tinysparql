/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <gmodule.h>
#include "tracker-writeback-module.h"


static gboolean tracker_writeback_module_load   (GTypeModule *module);
static void     tracker_writeback_module_unload (GTypeModule *module);


G_DEFINE_TYPE (TrackerWritebackModule, tracker_writeback_module, G_TYPE_TYPE_MODULE)

static void
tracker_writeback_module_class_init (TrackerWritebackModuleClass *klass)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (klass);

  module_class->load = tracker_writeback_module_load;
  module_class->unload = tracker_writeback_module_unload;
}

static void
tracker_writeback_module_init (TrackerWritebackModule *module)
{
}

static gboolean
tracker_writeback_module_load (GTypeModule *module)
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

	if (!g_module_symbol (writeback_module->module, "writeback_module_initialize",
			      (gpointer *) &writeback_module->initialize) ||
	    !g_module_symbol (writeback_module->module, "writeback_module_shutdown",
			      (gpointer *) &writeback_module->shutdown)) {
		g_warning ("Could not load module symbols for '%s': %s",
			   writeback_module->name,
			   g_module_error ());

		return FALSE;
	}

	writeback_module->initialize (module);

	return TRUE;
}

static void
tracker_writeback_module_unload (GTypeModule *module)
{
	TrackerWritebackModule *writeback_module;

	writeback_module = TRACKER_WRITEBACK_MODULE (module);
	writeback_module->shutdown ();

	g_module_close (writeback_module->module);
	writeback_module->module = NULL;

	writeback_module->initialize = NULL;
	writeback_module->shutdown = NULL;
}

TrackerWritebackModule *
tracker_writeback_module_get (const gchar *name)
{
	static GHashTable *modules;
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

        return list;
}
