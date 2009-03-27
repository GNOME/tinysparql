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
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <gmodule.h>
#include <string.h>
#include <time.h>

#include "tracker-push.h"

typedef struct {
	TrackerConfig *config;
	TrackerIndexer *indexer;
	GList *modules;
} PushSupportPrivate;

typedef struct {
	void (*init) (TrackerConfig *config);
	void (*shutdown) (void);
	GModule *module;
} PushModule;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void
unload_modules (PushSupportPrivate *private)
{
	GList *copy = private->modules;

	while (copy) {
		PushModule *p_module = copy->data;

		p_module->shutdown ();

		g_module_close (p_module->module);
		g_slice_free (PushModule, p_module);

		copy = g_list_next (copy);
	}

	g_list_free (private->modules);
	private->modules = NULL;
}

static void
load_modules (PushSupportPrivate *private)
{
	GError *error = NULL;
	GDir *dir = g_dir_open (PUSH_MODULES_DIR, 0, &error);
	const gchar *name;

	if (error) {
		g_error_free (error);
		return;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		if (g_str_has_suffix (name, G_MODULE_SUFFIX)) {
			gchar *path = g_build_filename (PUSH_MODULES_DIR, name, NULL);
			PushModule *p_module = g_slice_new (PushModule);

			p_module->module = g_module_open (path, G_MODULE_BIND_LOCAL);

			if (!g_module_symbol (p_module->module, "tracker_push_module_shutdown",
					      (gpointer *) &p_module->shutdown) ||
			    !g_module_symbol (p_module->module, "tracker_push_module_init",
					      (gpointer *) &p_module->init)) {

				g_warning ("Could not load module symbols for '%s': %s",
					   path, g_module_error ());

				g_module_close (p_module->module);
				g_slice_free (PushModule, p_module);

			} else {
				g_module_make_resident (p_module->module);

				p_module->init (private->config);

				private->modules = g_list_prepend (private->modules,
								   p_module);
			}

			g_free (path);
		}
	}

	g_dir_close (dir);
}

static void
free_private (PushSupportPrivate *private)
{
	if (private->config)
		g_object_unref (private->config);
	if (private->indexer)
		g_object_unref (private->indexer);

	g_free (private);
}

void
tracker_push_init (TrackerConfig *config, TrackerIndexer *indexer)
{
	PushSupportPrivate *private;

	private = g_new0 (PushSupportPrivate, 1);

	g_static_private_set (&private_key,
			      private,
			      (GDestroyNotify) free_private);

	private->indexer = g_object_ref (indexer);
	private->config = g_object_ref (config);

	load_modules (private);
}

void
tracker_push_shutdown (void)
{
	PushSupportPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	unload_modules (private);

	g_static_private_set (&private_key, NULL, NULL);
}
