/*
 * Copyright (C) 2010 Nokia <ivan.frade@nokia.com>
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

#include <glib/gi18n.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-miner/tracker-miner.h>

#include "tracker-control.h"

/* Note:
 * Every time a new option is added, make sure it is considered in the
 * 'MINERS_OPTIONS_ENABLED' macro below
 */
static const gchar **reindex_mime_types;
static gchar *index_file;
static gchar *miner_name;
static gchar *pause_reason;
static gint resume_cookie = -1;
static gboolean list_miners_running;
static gboolean list_miners_available;
static gboolean pause_details;

#define MINERS_OPTIONS_ENABLED() \
	(reindex_mime_types || \
	 index_file || \
	 miner_name || \
	 pause_reason || \
	 resume_cookie != -1 || \
	 list_miners_running || \
	 list_miners_available || \
	 pause_details)

static GOptionEntry entries[] = {
	{ "reindex-mime-type", 'm', 0, G_OPTION_ARG_STRING_ARRAY, &reindex_mime_types,
	  N_("Tell miners to reindex files which match the mime type supplied (for new extractors), use -m MIME1 -m MIME2"),
	  N_("MIME") },
	{ "index-file", 'f', 0, G_OPTION_ARG_FILENAME, &index_file,
	  N_("Tell miners to (re)index a given file"),
	  N_("FILE") },
	{ "pause", 0 , 0, G_OPTION_ARG_STRING, &pause_reason,
	  N_("Pause a miner (you must use this with --miner)"),
	  N_("REASON")
	},
	{ "resume", 0 , 0, G_OPTION_ARG_INT, &resume_cookie,
	  N_("Resume a miner (you must use this with --miner)"),
	  N_("COOKIE")
	},
	{ "miner", 0 , 0, G_OPTION_ARG_STRING, &miner_name,
	  N_("Miner to use with --resume or --pause (you can use suffixes, e.g. Files or Applications)"),
	  N_("MINER")
	},
	{ "list-miners-running", 'l', 0, G_OPTION_ARG_NONE, &list_miners_running,
	  N_("List all miners currently running"),
	  NULL
	},
	{ "list-miners-available", 'a', 0, G_OPTION_ARG_NONE, &list_miners_available,
	  N_("List all miners installed"),
	  NULL
	},
	{ "pause-details", 'i', 0, G_OPTION_ARG_NONE, &pause_details,
	  N_("List pause reasons"),
	  NULL
	},
	{ NULL }
};

gboolean
tracker_control_miners_options_enabled (void)
{
	return MINERS_OPTIONS_ENABLED ();
}

static gint
miner_pause (const gchar *miner,
             const gchar *reason)
{
	TrackerMinerManager *manager;
	GError *error = NULL;
	gchar *str;
	gint cookie;

	/* Don't auto-start the miners here */
	manager = tracker_miner_manager_new_full (FALSE, &error);
	if (!manager) {
		g_printerr (_("Could not pause miner, manager could not be created, %s"),
		            error ? error->message : "unknown error");
		g_printerr ("\n");
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	str = g_strdup_printf (_("Attempting to pause miner '%s' with reason '%s'"),
	                       miner,
	                       reason);
	g_print ("%s\n", str);
	g_free (str);

	if (!tracker_miner_manager_pause (manager, miner, reason, &cookie)) {
		g_printerr (_("Could not pause miner: %s"), miner);
		g_printerr ("\n");
		return EXIT_FAILURE;
	}

	str = g_strdup_printf (_("Cookie is %d"), cookie);
	g_print ("  %s\n", str);
	g_free (str);
	g_object_unref (manager);

	return EXIT_SUCCESS;
}

static gint
miner_resume (const gchar *miner,
              gint         cookie)
{
	TrackerMinerManager *manager;
	GError *error = NULL;
	gchar *str;

	/* Don't auto-start the miners here */
	manager = tracker_miner_manager_new_full (FALSE, &error);
	if (!manager) {
		g_printerr (_("Could not resume miner, manager could not be created, %s"),
		            error ? error->message : "unknown error");
		g_printerr ("\n");
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	str = g_strdup_printf (_("Attempting to resume miner %s with cookie %d"),
	                       miner,
	                       cookie);
	g_print ("%s\n", str);
	g_free (str);

	if (!tracker_miner_manager_resume (manager, miner, cookie)) {
		g_printerr (_("Could not resume miner: %s"), miner);
		return EXIT_FAILURE;
	}

	g_print ("  %s\n", _("Done"));

	g_object_unref (manager);

	return EXIT_SUCCESS;
}

static gint
miner_reindex_mime_types (const gchar **mime_types)
{
	GError *error = NULL;
	TrackerMinerManager *manager;

	/* Auto-start the miners here if we need to */
	manager = tracker_miner_manager_new_full (TRUE, &error);
	if (!manager) {
		g_printerr (_("Could not reindex mimetypes, manager could not be created, %s"),
		            error ? error->message : "unknown error");
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
		            error ? error->message : "unknown error");
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

static gint
miner_list (gboolean available,
            gboolean running)
{
	TrackerMinerManager *manager;
	GError *error = NULL;

	/* Don't auto-start the miners here */
	manager = tracker_miner_manager_new_full (FALSE, &error);
	if (!manager) {
		g_printerr (_("Could not list miners, manager could not be created, %s"),
		            error ? error->message : "unknown error");
		g_printerr ("\n");
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	if (available) {
		GSList *miners_available;
		gchar *str;
		GSList *l;

		miners_available = tracker_miner_manager_get_available (manager);

		str = g_strdup_printf (_("Found %d miners installed"),
		                       g_slist_length (miners_available));
		g_print ("%s%s\n", str, g_slist_length (miners_available) > 0 ? ":" : "");
		g_free (str);

		for (l = miners_available; l; l = l->next) {
			g_print ("  %s\n", (gchar*) l->data);
		}

		g_slist_foreach (miners_available, (GFunc) g_free, NULL);
		g_slist_free (miners_available);
	}

	if (running) {
		GSList *miners_running;
		gchar *str;
		GSList *l;

		miners_running = tracker_miner_manager_get_running (manager);

		str = g_strdup_printf (_("Found %d miners running"),
		                       g_slist_length (miners_running));
		g_print ("%s%s\n", str, g_slist_length (miners_running) > 0 ? ":" : "");
		g_free (str);

		for (l = miners_running; l; l = l->next) {
			g_print ("  %s\n", (gchar*) l->data);
		}

		g_slist_foreach (miners_running, (GFunc) g_free, NULL);
		g_slist_free (miners_running);
	}

	g_object_unref (manager);

	return EXIT_SUCCESS;
}

static gboolean
miner_get_details (TrackerMinerManager  *manager,
                   const gchar          *miner,
                   gchar               **status,
                   gdouble              *progress,
                   GStrv                *pause_applications,
                   GStrv                *pause_reasons)
{
	if ((status || progress) &&
	    !tracker_miner_manager_get_status (manager,
	                                       miner,
	                                       status,
	                                       progress)) {
		g_printerr (_("Could not get status from miner: %s"), miner);
		return FALSE;
	}

	tracker_miner_manager_is_paused (manager,
	                                 miner,
	                                 pause_applications,
	                                 pause_reasons);

	return TRUE;
}

static gboolean
miner_pause_details (void)
{
	TrackerMinerManager *manager;
	GError *error = NULL;
	GSList *miners_running, *l;
	gint paused_miners = 0;

	/* Don't auto-start the miners here */
	manager = tracker_miner_manager_new_full (FALSE, &error);
	if (!manager) {
		g_printerr (_("Could not get pause details, manager could not be created, %s"),
		            error ? error->message : "unknown error");
		g_printerr ("\n");
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	miners_running = tracker_miner_manager_get_running (manager);

	if (!miners_running) {
		g_print ("%s\n", _("No miners are running"));

		g_slist_foreach (miners_running, (GFunc) g_free, NULL);
		g_slist_free (miners_running);

		return EXIT_SUCCESS;
	}

	for (l = miners_running; l; l = l->next) {
		const gchar *name;
		GStrv pause_applications, pause_reasons;
		gint i;

		name = tracker_miner_manager_get_display_name (manager, l->data);

		if (!name) {
			g_critical ("Could not get name for '%s'", (gchar *) l->data);
			continue;
		}

		if (!miner_get_details (manager,
		                        l->data,
		                        NULL,
		                        NULL,
		                        &pause_applications,
		                        &pause_reasons)) {
			continue;
		}

		if (!(*pause_applications) || !(*pause_reasons)) {
			g_strfreev (pause_applications);
			g_strfreev (pause_reasons);
			continue;
		}

		paused_miners++;
		if (paused_miners == 1) {
			g_print ("%s:\n", _("Miners"));
		}

		g_print ("  %s:\n", name);

		for (i = 0; pause_applications[i] != NULL; i++) {
			g_print ("    %s: '%s', %s: '%s'\n",
			         _("Application"),
			         pause_applications[i],
			         _("Reason"),
			         pause_reasons[i]);
		}

		g_strfreev (pause_applications);
		g_strfreev (pause_reasons);
	}

	if (paused_miners < 1) {
		g_print ("%s\n", _("No miners are paused"));
	}

	g_slist_foreach (miners_running, (GFunc) g_free, NULL);
	g_slist_free (miners_running);

	g_object_unref (manager);

	return EXIT_SUCCESS;
}

void
tracker_control_miners_run_default (void)
{
	/* No miners output in the default run */
}

gint
tracker_control_miners_run (void)
{
	/* Constraints */

	if (pause_reason && resume_cookie != -1) {
		g_printerr ("%s\n",
		            _("You can not use miner pause and resume switches together"));
		return EXIT_FAILURE;
	}

	if ((pause_reason || resume_cookie != -1) && !miner_name) {
		g_printerr ("%s\n",
		            _("You must provide the miner for pause or resume commands"));
		return EXIT_FAILURE;
	}

	if ((!pause_reason && resume_cookie == -1) && miner_name) {
		g_printerr ("%s\n",
		            _("You must provide a pause or resume command for the miner"));
		return EXIT_FAILURE;
	}

	/* Known actions */

	if (list_miners_running || list_miners_available) {
		return miner_list (list_miners_available,
		                   list_miners_running);
	}

	if (reindex_mime_types) {
		return miner_reindex_mime_types (reindex_mime_types);
	}

	if (index_file) {
		return miner_index_file (index_file);
	}

	if (pause_reason) {
		return miner_pause (miner_name, pause_reason);
	}

	if (resume_cookie != -1) {
		return miner_resume (miner_name, resume_cookie);
	}

	if (pause_details) {
		return miner_pause_details ();
	}

	/* All known options have their own exit points */
	g_warn_if_reached ();

	return EXIT_FAILURE;
}

GOptionGroup *
tracker_control_miners_get_option_group (void)
{
	GOptionGroup *group;

	/* Status options */
	group = g_option_group_new ("miners",
	                            _("Miner options"),
	                            _("Show miner options"),
	                            NULL,
	                            NULL);
	g_option_group_add_entries (group, entries);

	return group;
}
