/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>

#ifdef __sun
#include <procfs.h>
#endif

#include <libtracker-common/tracker-common.h>
#include <libtracker-data/tracker-data.h>
#include <libtracker-miner/tracker-miner.h>
#include <libtracker-control/tracker-control.h>

#include "tracker-control.h"
#include "tracker-dbus.h"

#define OPTION_TERM_ALL "all"
#define OPTION_TERM_STORE "store"
#define OPTION_TERM_MINERS "miners"

typedef enum {
	TERM_NONE,
	TERM_ALL,
	TERM_STORE,
	TERM_MINERS
} TermOption;

/* Note:
 * Every time a new option is added, make sure it is considered in the
 * 'GENERAL_OPTIONS_ENABLED' macro below
 */
static gboolean list_processes;
static TermOption kill_option = TERM_NONE;
static TermOption terminate_option = TERM_NONE;
static gboolean hard_reset;
static gboolean soft_reset;
static gboolean remove_config;
static gchar *set_log_verbosity;
static gboolean get_log_verbosity;
static gboolean start;
static gchar *backup;
static gchar *restore;
static gboolean collect_debug_info;

#define GENERAL_OPTIONS_ENABLED() \
	(list_processes || \
	 kill_option != TERM_NONE || \
	 terminate_option != TERM_NONE || \
	 hard_reset || \
	 soft_reset || \
	 remove_config || \
	 get_log_verbosity || \
	 set_log_verbosity || \
	 start || \
	 backup || \
	 restore || \
	 collect_debug_info)

static gboolean term_option_arg_func (const gchar  *option_value,
                                      const gchar  *value,
                                      gpointer      data,
                                      GError      **error);

static GOptionEntry entries[] = {
	{ "list-processes", 'p', 0, G_OPTION_ARG_NONE, &list_processes,
	  N_("List all Tracker processes") },
	{ "kill", 'k', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, term_option_arg_func,
	  N_("Use SIGKILL to stop all matching processes, either \"store\", \"miners\" or \"all\" may be used, no parameter equals \"all\""),
	  N_("APPS") },
	{ "terminate", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, term_option_arg_func,
	  N_("Use SIGTERM to stop all matching processes, either \"store\", \"miners\" or \"all\" may be used, no parameter equals \"all\""),
	  N_("APPS") },
	{ "hard-reset", 'r', 0, G_OPTION_ARG_NONE, &hard_reset,
	  N_("Kill all Tracker processes and remove all databases"),
	  NULL },
	{ "soft-reset", 'e', 0, G_OPTION_ARG_NONE, &soft_reset,
	  N_("Same as --hard-reset but the backup & journal are restored after restart"),
	  NULL },
	{ "remove-config", 'c', 0, G_OPTION_ARG_NONE, &remove_config,
	  N_("Remove all configuration files so they are re-generated on next start"),
	  NULL },
	{ "start", 's', 0, G_OPTION_ARG_NONE, &start,
	  N_("Starts miners (which indirectly starts tracker-store too)"),
	  NULL },
	{ "backup", 'b', 0, G_OPTION_ARG_FILENAME, &backup,
	  N_("Backup databases to the file provided"),
	  N_("FILE") },
	{ "restore", 'o', 0, G_OPTION_ARG_FILENAME, &restore,
	  N_("Restore databases from the file provided"),
	  N_("FILE") },
	{ "set-log-verbosity", 0, 0, G_OPTION_ARG_STRING, &set_log_verbosity,
	  N_("Sets the logging verbosity to LEVEL ('debug', 'detailed', 'minimal', 'errors') for all processes"),
	  N_("LEVEL") },
	{ "get-log-verbosity", 0, 0, G_OPTION_ARG_NONE, &get_log_verbosity,
	  N_("Show logging values in terms of log verbosity for each process"),
	  NULL },
	{ "collect-debug-info", 0, 0, G_OPTION_ARG_NONE, &collect_debug_info,
	  N_("Collect debug information useful for problem reporting and investigation, results are output to terminal"),
	  NULL },
	{ NULL }
};

gboolean
tracker_control_general_options_enabled (void)
{
	return GENERAL_OPTIONS_ENABLED ();
}

void
tracker_control_general_run_default (void)
{
	/* Enable list processes in the default run */
	list_processes = TRUE;

	tracker_control_general_run ();
}

gint
tracker_control_general_run (void)
{

	return EXIT_SUCCESS;
}

GOptionGroup *
tracker_control_general_get_option_group (void)
{
	GOptionGroup *group;

	/* Status options */
	group = g_option_group_new ("general",
	                            _("General options"),
	                            _("Show general options"),
	                            NULL,
	                            NULL);
	g_option_group_add_entries (group, entries);

	return group;
}
