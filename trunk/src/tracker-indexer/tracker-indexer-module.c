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
#include "tracker-indexer-module.h"


static gboolean tracker_indexer_module_load   (GTypeModule *module);
static void     tracker_indexer_module_unload (GTypeModule *module);


G_DEFINE_TYPE (TrackerIndexerModule, tracker_indexer_module, G_TYPE_TYPE_MODULE)

static void
tracker_indexer_module_class_init (TrackerIndexerModuleClass *klass)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (klass);

  module_class->load = tracker_indexer_module_load;
  module_class->unload = tracker_indexer_module_unload;
}

static void
tracker_indexer_module_init (TrackerIndexerModule *module)
{
}

static gboolean
tracker_indexer_module_load (GTypeModule *module)
{
	TrackerIndexerModule *indexer_module;
	gchar *full_name, *path;

	indexer_module = TRACKER_INDEXER_MODULE (module);

	full_name = g_strdup_printf ("libtracker-module-%s", indexer_module->name);
	path = g_build_filename (INDEXER_MODULES_DIR, full_name, NULL);

	indexer_module->module = g_module_open (path, G_MODULE_BIND_LOCAL);

	g_free (full_name);
	g_free (path);

	if (G_UNLIKELY (!indexer_module->module)) {
		g_warning ("Could not load indexer module '%s': %s\n",
			   indexer_module->name,
			   g_module_error ());

		return FALSE;
	}

	g_module_make_resident (indexer_module->module);

	if (!g_module_symbol (indexer_module->module, "indexer_module_initialize",
			      (gpointer *) &indexer_module->initialize) ||
	    !g_module_symbol (indexer_module->module, "indexer_module_shutdown",
			      (gpointer *) &indexer_module->shutdown) ||
	    !g_module_symbol (indexer_module->module, "indexer_module_create_file",
			      (gpointer *) &indexer_module->create_file)) {
		g_warning ("Could not load module symbols for '%s': %s",
			   indexer_module->name,
			   g_module_error ());

		return FALSE;
	}

	indexer_module->initialize (module);

	return TRUE;
}

static void
tracker_indexer_module_unload (GTypeModule *module)
{
	TrackerIndexerModule *indexer_module;

	indexer_module = TRACKER_INDEXER_MODULE (module);
	indexer_module->shutdown ();

	g_module_close (indexer_module->module);
	indexer_module->module = NULL;

	indexer_module->initialize = NULL;
	indexer_module->shutdown = NULL;
	indexer_module->create_file = NULL;
}

TrackerIndexerModule *
tracker_indexer_module_get (const gchar *name)
{
	static GHashTable *modules;
	TrackerIndexerModule *module;

	g_return_val_if_fail (name != NULL, NULL);

	if (G_UNLIKELY (!modules)) {
		modules = g_hash_table_new (g_str_hash, g_str_equal);
	}

	module = g_hash_table_lookup (modules, name);

	if (G_UNLIKELY (!module)) {
		module = g_object_new (TRACKER_TYPE_INDEXER_MODULE, NULL);
		g_type_module_set_name (G_TYPE_MODULE (module), name);
		module->name = g_strdup (name);

		g_hash_table_insert (modules, module->name, module);
	}

	if (!g_type_module_use (G_TYPE_MODULE (module))) {
		return NULL;
	}

	return module;
}

TrackerModuleFile *
tracker_indexer_module_create_file (TrackerIndexerModule *module,
				    GFile                *file)
{
	return module->create_file (file);
}
