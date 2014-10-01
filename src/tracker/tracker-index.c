/*
 * Copyright (C) 2014, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#include <stdlib.h>
#include <errno.h>

#ifdef __sun
#include <procfs.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libtracker-control/tracker-control.h>

#include "tracker-index.h"

static const gchar **reindex_mime_types;
static gchar *index_file;

#define INDEX_OPTIONS_ENABLED() \
	(reindex_mime_types || index_file)

static GOptionEntry entries[] = {
	{ "reindex-mime-type", 'm', 0, G_OPTION_ARG_STRING_ARRAY, &reindex_mime_types,
	  N_("Tell miners to reindex files which match the mime type supplied (for new extractors), use -m MIME1 -m MIME2"),
	  N_("MIME") },
	{ "index-file", 'f', 0, G_OPTION_ARG_FILENAME, &index_file,
	  N_("Tell miners to (re)index a given file"),
	  N_("FILE") },
	{ NULL }
};

static gint
miner_reindex_mime_types (const gchar **mime_types)
{
	GError *error = NULL;
	TrackerMinerManager *manager;

	/* Auto-start the miners here if we need to */
	manager = tracker_miner_manager_new_full (TRUE, &error);
	if (!manager) {
		g_printerr (_("Could not reindex mimetypes, manager could not be created, %s"),
		            error ? error->message : _("No error given"));
		g_printerr ("\n");
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	tracker_miner_manager_reindex_by_mimetype (manager, (GStrv)reindex_mime_types, &error);
	if (error) {
		g_printerr ("%s: %s\n",
		            _("Could not reindex mimetypes"),
		            error->message);
		g_error_free (error);
		return EXIT_FAILURE;
	}

	g_print ("%s\n", _("Reindexing mime types was successful"));
	g_object_unref (manager);

	return EXIT_SUCCESS;
}

static gint
miner_index_file (const gchar *filepath)
{
	TrackerMinerManager *manager;
	GError *error = NULL;
	GFile *file;

	/* Auto-start the miners here if we need to */
	manager = tracker_miner_manager_new_full (TRUE, &error);
	if (!manager) {
		g_printerr (_("Could not (re)index file, manager could not be created, %s"),
		            error ? error->message : _("No error given"));
		g_printerr ("\n");
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	file = g_file_new_for_commandline_arg (index_file);

	tracker_miner_manager_index_file (manager, file, &error);

	if (error) {
		g_printerr ("%s: %s\n",
		            _("Could not (re)index file"),
		            error->message);
		g_error_free (error);
		return EXIT_FAILURE;
	}

	g_print ("%s\n", _("(Re)indexing file was successful"));

	g_object_unref (manager);
	g_object_unref (file);

	return EXIT_SUCCESS;
}

void
tracker_index_run_default (void)
{
	/* Set options in here we want to run by default with no args */

	tracker_index_run ();
}

gint
tracker_index_run (void)
{
	if (reindex_mime_types) {
		return miner_reindex_mime_types (reindex_mime_types);
	}

	if (index_file) {
		return miner_index_file (index_file);
	}

	/* All known options have their own exit points */
	g_warn_if_reached ();

	return EXIT_FAILURE;
}

GOptionGroup *
tracker_index_get_option_group (void)
{
	GOptionGroup *group;

	/* Status options */
	group = g_option_group_new ("Index",
	                            _("Index options"),
	                            _("Show index options"),
	                            NULL,
	                            NULL);
	g_option_group_add_entries (group, entries);

	return group;
}

gboolean
tracker_index_options_enabled (void)
{
	return INDEX_OPTIONS_ENABLED ();
}
