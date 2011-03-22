/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
#include <string.h>
#include <time.h>
#include <locale.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-common.h>

#include "tracker-control.h"

#define ABOUT	  \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE	  \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public " \
	"License which can be viewed at:\n" \
	"\n" \
	"  http://www.gnu.org/licenses/gpl.txt\n"

static gboolean print_version;

static GOptionEntry common_entries[] = {
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
	  NULL },
	{ NULL }
};

int
main (int argc, char **argv)
{
	GOptionContext *context;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	/* Translators: this messagge will apper immediately after the  */
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>     */
	context = g_option_context_new (_(" - Manage Tracker processes and data"));

	/* Groups */
	g_option_context_add_group (context,
	                            tracker_control_general_get_option_group ());
	g_option_context_add_group (context,
	                            tracker_control_status_get_option_group ());
	g_option_context_add_group (context,
	                            tracker_control_miners_get_option_group ());
	/* Common options */
	g_option_context_add_main_entries (context, common_entries, NULL);

	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	if (print_version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		return EXIT_SUCCESS;
	}

	/* General options? */
	if (tracker_control_general_options_enabled ()) {
		if (tracker_control_status_options_enabled ()) {
			g_printerr ("%s\n",
			            _("General and Status options cannot be used together"));
			return EXIT_FAILURE;
		}

		if (tracker_control_miners_options_enabled ()) {
			g_printerr ("%s\n",
			            _("General and Miners options cannot be used together"));
			return EXIT_FAILURE;
		}

		return tracker_control_general_run ();
	}

	/* Status options? */
	if (tracker_control_status_options_enabled ()) {
		if (tracker_control_miners_options_enabled ()) {
			g_printerr ("%s\n",
			            _("Status and Miners options cannot be used together"));
			return EXIT_FAILURE;
		}

		return tracker_control_status_run ();
	}

	/* Miners options? */
	if (tracker_control_miners_options_enabled ()) {
		return tracker_control_miners_run ();
	}

	/* Unknown options? */
	if (argc > 1) {
		gint i = 1;

		g_printerr ("%s: ",
		            _("Unrecognized options"));
		for (i = 1; i < argc; i++) {
			g_printerr ("'%s'%s",
			            argv[i],
			            i == (argc - 1) ? "\n" : ", ");
		}
		return EXIT_FAILURE;
	}

	/* No-args output */
	tracker_control_general_run_default ();
	printf ("\n");

	tracker_control_status_run_default ();
	printf ("\n");

	tracker_control_miners_run_default ();

	return EXIT_SUCCESS;
}
