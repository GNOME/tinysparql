/*
 * Copyright (C) 2014, Lanedo <martyn@lanedo.com>
 * Copyright (C) 2024, Sam Thursfield <sam@afuera.me.uk>
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

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "tracker-help.h"
#include "tracker-endpoint.h"
#include "tracker-export.h"
#include "tracker-import.h"
#include "tracker-introspect.h"
#include "tracker-query.h"
#include "tracker-webide.h"

const char usage_string[] =
	"tinysparql [--version] [--help]\n"
	"                <command> [<args>]";

const char about[] =
	"TinySPARQL " PACKAGE_VERSION "\n"
	"\n"
	"This program is free software and comes without any warranty.\n"
	"It is licensed under version 2 or later of the General Public "
	"License which can be viewed at:\n"
	"\n"
	"  http://www.gnu.org/licenses/gpl.txt"
	"\n";

static void print_usage (void);

static int
print_version (void)
{
	puts (about);
	return 0;
}

struct cmd_struct {
       const char *cmd;
       int (*fn)(int, const char **);
       const char *help;
};

static struct cmd_struct commands[] = {
       { "help", tracker_help, N_("Get help on how to use TinySPARQL and any of these commands") },
       { "endpoint", tracker_endpoint, N_("Create a SPARQL endpoint") },
       { "export", tracker_export, N_("Export data from a TinySPARQL database") },
       { "import", tracker_import, N_("Import data into a TinySPARQL database") },
       { "introspect", tracker_introspect, N_("Introspect a SPARQL endpoint") },
       { "query", tracker_query, N_("Query and update the index using SPARQL") },
       { "webide", tracker_webide, N_("Create a Web IDE to query local databases") },
};

static inline void
mput_char (char c, unsigned int num)
{
      while (num--) {
              putchar (c);
      }
}

static void
print_usage_list_cmds (void)
{
	guint longest = 0;
	guint i;

	puts (_("Available tinysparql commands are:"));

	for (i = 0; i < G_N_ELEMENTS (commands); i++) {
		if (longest < strlen (commands[i].cmd))
			longest = strlen (commands[i].cmd);
	}

	for (i = 0; i < G_N_ELEMENTS (commands); i++) {
		g_print ("   %s   ", commands[i].cmd);
		mput_char (' ', longest - strlen (commands[i].cmd));
		puts (_(commands[i].help));
	}
}

static void
print_usage (void)
{
	g_print ("usage: %s\n\n", usage_string);
	print_usage_list_cmds ();
	g_print ("\n%s\n", _("See “tinysparql help <command>” to read about a specific subcommand."));
}

int
main (int argc, char *argv[])
{
	int (* func) (int, const char *[]) = NULL;
	const gchar *subcommand = argv[1];
	guint i;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	if (argc == 1) {
		/* The user didn't specify a command; give them help */
		print_usage ();
		exit (EXIT_SUCCESS);
	}

	if (g_strcmp0 (subcommand, "--version") == 0) {
		print_version ();
		exit (EXIT_SUCCESS);
	} else if (g_strcmp0 (subcommand, "--help") == 0) {
		subcommand = "help";
	}

	if (g_strcmp0 (subcommand, "help") == 0 && argc == 2) {
		/* Print usage here to avoid duplicating it in tracker-help.c */
		print_usage ();
		exit (EXIT_SUCCESS);
	}

	for (i = 0; i < G_N_ELEMENTS (commands); i++) {
		if (g_strcmp0 (commands[i].cmd, subcommand) == 0)
			func = commands[i].fn;
	}

	if (func) {
		return func (argc - 1, (const char **) &argv[1]);
	} else {
		g_printerr (_("“%s” is not a tinysparql command. See “tinysparql --help”"), subcommand);
		g_printerr ("\n");
		return EXIT_FAILURE;
	}
}
