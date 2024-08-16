/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdio.h>

#include "tracker-docgen-md.h"
#include "tracker-ontology-model.h"
#include "tracker-utils.h"

static gchar *ontology_dir = NULL;
static gchar *ontology_desc_dir = NULL;
static gchar *output_dir = NULL;
static gchar *introduction_dir = NULL;
static gboolean markdown = TRUE;

static GOptionEntry   entries[] = {
	{ "ontology-dir", 'd', 0, G_OPTION_ARG_FILENAME, &ontology_dir,
	  "Ontology directory",
	  NULL
	},
	{ "ontology-description-dir", 'e', 0, G_OPTION_ARG_FILENAME, &ontology_desc_dir,
	  "Ontology description directory",
	  NULL
	},
	{ "output-dir", 'o', 0, G_OPTION_ARG_FILENAME, &output_dir,
	  "File to write the output (default stdout)",
	  NULL
	},
	{ "introduction-dir", 'i', 0, G_OPTION_ARG_FILENAME, &introduction_dir,
	  "Directory to find ontology introduction",
	  NULL
	},
	{ "md", 'm', 0, G_OPTION_ARG_NONE, &markdown,
	  "Whether to produce markdown",
	  NULL
	},
	{ NULL }
};

gint
main (gint argc, gchar **argv)
{
	GOptionContext *context;
	TrackerOntologyDescription *description = NULL;
	TrackerOntologyModel *model = NULL;
	g_autoptr(GFile) ontology_file = NULL, output_file = NULL, ontology_desc_file = NULL;
	gchar *path;
	GStrv prefixes = NULL;
	gint i;
	g_autoptr(GError) error = NULL;
	gboolean retval;

	/* Translators: this messagge will apper immediately after the  */
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>     */
	context = g_option_context_new ("- Generates HTML doc for a TTL file");

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.                              */
	g_option_context_add_main_entries (context, entries, NULL);

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("%s\n", error->message);
		return -1;
	}

	if (!ontology_dir || !output_dir) {
		gchar *help;

		g_printerr ("%s\n\n",
		            "Ontology and output dirs are mandatory");

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return -1;
	}

	ontology_file = g_file_new_for_commandline_arg (ontology_dir);
	output_file = g_file_new_for_commandline_arg (output_dir);

	if (ontology_desc_dir)
		ontology_desc_file = g_file_new_for_commandline_arg (ontology_desc_dir);
	else
		ontology_desc_file = g_object_ref (ontology_file);

	model = tracker_ontology_model_new (ontology_file, ontology_desc_file, &error);
	if (error) {
		g_printerr ("Error loading ontology: %s\n", error->message);
		return -1;
	}

	path = g_file_get_path (output_file);
	retval = g_mkdir_with_parents (path, 0755);
	g_free (path);

	if (!retval && errno != EEXIST) {
		g_printerr ("Could not create output directory: %m\n");
		return -1;
	}

	prefixes = tracker_ontology_model_get_prefixes (model);

	for (i = 0; prefixes[i]; i++) {
		description = tracker_ontology_model_get_description (model, prefixes[i]);
		if (!description)
			continue;

		if (markdown)
			ttl_md_print (description, model, prefixes[i], output_file, introduction_dir);

		ttl_generate_dot_files (description, model, prefixes[i], output_file);
	}

	g_strfreev (prefixes);
	tracker_ontology_model_free (model);
	g_option_context_free (context);

	return 0;
}
