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
#include <libtracker-control/tracker-control.h>

#include "tracker-control.h"

/* Note:
 * Every time a new option is added, make sure it is considered in the
 * 'MINERS_OPTIONS_ENABLED' macro below
 */
static const gchar **reindex_mime_types;
static gchar *index_file;
static gchar *miner_name;
static gchar *pause_reason;
static gchar *pause_for_process_reason;
static gint resume_cookie = -1;
static gboolean list_miners_running;
static gboolean list_miners_available;
static gboolean pause_details;

#define MINERS_OPTIONS_ENABLED() \
	(reindex_mime_types || \
	 index_file || \
	 miner_name || \
	 pause_reason || \
	 pause_for_process_reason || \
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
	{ "pause-for-process", 0 , 0, G_OPTION_ARG_STRING, &pause_for_process_reason,
	  N_("Pause a miner while the calling process is alive or until resumed (you must use this with --miner)"),
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
	{ "list-miners-running", 0, 0, G_OPTION_ARG_NONE, &list_miners_running,
	  N_("List all miners currently running"),
	  NULL
	},
	{ "list-miners-available", 0, 0, G_OPTION_ARG_NONE, &list_miners_available,
	  N_("List all miners installed"),
	  NULL
	},
	{ "pause-details", 0, 0, G_OPTION_ARG_NONE, &pause_details,
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

void
tracker_control_miners_run_default (void)
{
	/* No miners output in the default run */
}

gint
tracker_control_miners_run (void)
{
	/* Constraints */


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
