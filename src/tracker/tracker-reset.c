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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-data/tracker-data.h>

#include "tracker-reset.h"
#include "tracker-daemon.h"
#include "tracker-process.h"
#include "tracker-config.h"

static gboolean hard_reset;
static gboolean soft_reset;
static gboolean remove_config;

#define RESET_OPTIONS_ENABLED() \
	(hard_reset || \
	 soft_reset || \
	 remove_config)

static GOptionEntry entries[] = {
	{ "hard-reset", 'r', 0, G_OPTION_ARG_NONE, &hard_reset,
	  N_("Kill all Tracker processes and remove all databases"),
	  NULL },
	{ "soft-reset", 'e', 0, G_OPTION_ARG_NONE, &soft_reset,
	  N_("Same as --hard-reset but the backup & journal are restored after restart"),
	  NULL },
	{ "remove-config", 'c', 0, G_OPTION_ARG_NONE, &remove_config,
	  N_("Remove all configuration files so they are re-generated on next start"),
	  NULL },
	{ NULL }
};

static void
log_handler (const gchar    *domain,
             GLogLevelFlags  log_level,
             const gchar    *message,
             gpointer        user_data)
{
	switch (log_level) {
	case G_LOG_LEVEL_WARNING:
	case G_LOG_LEVEL_CRITICAL:
	case G_LOG_LEVEL_ERROR:
	case G_LOG_FLAG_RECURSION:
	case G_LOG_FLAG_FATAL:
		g_fprintf (stderr, "%s\n", message);
		fflush (stderr);
		break;
	case G_LOG_LEVEL_MESSAGE:
	case G_LOG_LEVEL_INFO:
	case G_LOG_LEVEL_DEBUG:
	case G_LOG_LEVEL_MASK:
	default:
		g_fprintf (stdout, "%s\n", message);
		fflush (stdout);
		break;
	}
}

static void
delete_file (GFile    *file,
             gpointer  user_data)
{
	if (g_file_delete (file, NULL, NULL)) {
		gchar *path;

		path = g_file_get_path (file);
		g_print ("  %s\n", path);
		g_free (path);
	}
}

static void
directory_foreach (GFile    *file,
                   gchar    *suffix,
                   GFunc     func,
                   gpointer  user_data)
{
	GFileEnumerator *enumerator;
	GFileInfo *info;
	GFile *child;

	enumerator = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
	                                        G_FILE_QUERY_INFO_NONE, NULL, NULL);

	while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {

		if (!suffix || g_str_has_suffix (g_file_info_get_name (info), suffix)) {
			child = g_file_enumerator_get_child (enumerator, info);
			(func) (child, user_data);
			g_object_unref (child);
		}

		g_object_unref (info);
	}

	g_object_unref (enumerator);
}

void
tracker_reset_run_default (void)
{
	/* Set options in here we want to run by default with no args */

	tracker_reset_run ();
}

gint
tracker_reset_run (void)
{
	TrackerProcessTypes kill_option = TRACKER_PROCESS_TYPE_NONE;
	GError *error = NULL;

	if (hard_reset && soft_reset) {
		g_printerr ("%s\n",
		            _("You can not use the --hard-reset and --soft-reset arguments together"));
		return EXIT_FAILURE;
	}

	if (hard_reset || soft_reset) {
		/* Imply --kill */
		kill_option = TRACKER_PROCESS_TYPE_ALL;
		/* FIXME: Kill all before doing reset ... */
	}

	if (hard_reset || soft_reset) {
		guint log_handler_id;
#ifndef DISABLE_JOURNAL
		gchar *rotate_to;
		TrackerDBConfig *db_config;
		gsize chunk_size;
		gint chunk_size_mb;
#endif /* DISABLE_JOURNAL */

		/* Set log handler for library messages */
		log_handler_id = g_log_set_handler (NULL,
		                                    G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL,
		                                    log_handler,
		                                    NULL);

		g_log_set_default_handler (log_handler, NULL);

#ifndef DISABLE_JOURNAL
		db_config = tracker_db_config_new ();

		chunk_size_mb = tracker_db_config_get_journal_chunk_size (db_config);
		chunk_size = (gsize) ((gsize) chunk_size_mb * (gsize) 1024 * (gsize) 1024);
		rotate_to = tracker_db_config_get_journal_rotate_destination (db_config);

		/* This call is needed to set the journal's filename */
		tracker_db_journal_set_rotating ((chunk_size_mb != -1),
		                                 chunk_size, rotate_to);

		g_free (rotate_to);
		g_object_unref (db_config);

#endif /* DISABLE_JOURNAL */

		/* Clean up (select_cache_size and update_cache_size don't matter here) */
		if (!tracker_db_manager_init (TRACKER_DB_MANAGER_REMOVE_ALL,
		                              NULL,
		                              FALSE,
		                              FALSE,
		                              100,
		                              100,
		                              NULL,
		                              NULL,
		                              NULL,
		                              &error)) {

			g_message ("Error initializing database: %s", error->message);
			g_free (error);

			return EXIT_FAILURE;
		}
#ifndef DISABLE_JOURNAL
		tracker_db_journal_init (NULL, FALSE, NULL);
#endif /* DISABLE_JOURNAL */

		tracker_db_manager_remove_all (hard_reset);
		tracker_db_manager_shutdown ();
#ifndef DISABLE_JOURNAL
		tracker_db_journal_shutdown (NULL);
#endif /* DISABLE_JOURNAL */

		/* Unset log handler */
		g_log_remove_handler (NULL, log_handler_id);
	}

	if (remove_config) {
		GFile *file;
		const gchar *home_conf_dir;
		gchar *path;
		GSList *all, *l;

		/* Check the default XDG_DATA_HOME location */
		home_conf_dir = g_getenv ("XDG_CONFIG_HOME");

		if (home_conf_dir && tracker_path_has_write_access_or_was_created (home_conf_dir)) {
			path = g_build_path (G_DIR_SEPARATOR_S, home_conf_dir, "tracker", NULL);
		} else {
			home_conf_dir = g_getenv ("HOME");

			if (!home_conf_dir || !tracker_path_has_write_access_or_was_created (home_conf_dir)) {
				home_conf_dir = g_get_home_dir ();
			}
			path = g_build_path (G_DIR_SEPARATOR_S, home_conf_dir, ".config", "tracker", NULL);
		}

		file = g_file_new_for_path (path);
		g_free (path);

		g_print ("%s\n", _("Removing configuration files…"));

		directory_foreach (file, ".cfg", (GFunc) delete_file, NULL);
		g_object_unref (file);

		g_print ("%s\n", _("Resetting existing configuration…"));

		all = tracker_gsettings_get_all (NULL);

		if (!all) {
			return EXIT_FAILURE;
		}

		for (l = all; l; l = l->next) {
			ComponentGSettings *c = l->data;
			gchar **keys, **p;

			if (!c) {
				continue;
			}

			g_print ("  %s\n", c->name);

			keys = g_settings_list_keys (c->settings);
			for (p = keys; p && *p; p++) {
				g_print ("    %s\n", *p);
				g_settings_reset (c->settings, *p);
			}

			if (keys) {
				g_strfreev (keys);
			}

			g_settings_apply (c->settings);
		}

		g_settings_sync ();

		tracker_gsettings_free (all);
	}

	/* All known options have their own exit points */
	g_warn_if_reached ();

	return EXIT_FAILURE;
}

GOptionGroup *
tracker_reset_get_option_group (void)
{
	GOptionGroup *group;

	/* Status options */
	group = g_option_group_new ("Reset",
	                            _("Reset options"),
	                            _("Show reset options"),
	                            NULL,
	                            NULL);
	g_option_group_add_entries (group, entries);

	return group;
}

gboolean
tracker_reset_options_enabled (void)
{
	return RESET_OPTIONS_ENABLED ();
}
