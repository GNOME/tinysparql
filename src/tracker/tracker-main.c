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

#include <stdlib.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include <libtracker-common/tracker-common.h>

const char usage_string[] =
	"tracker3 [--version] [--help]\n"
	"                <command> [<args>]";

const char about[] =
	"Tracker " PACKAGE_VERSION "\n"
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

static inline void
mput_char (char c, unsigned int num)
{
      while (num--) {
              putchar (c);
      }
}

static int
compare_app_info (GAppInfo *a,
                  GAppInfo *b)
{
	return g_strcmp0 (g_app_info_get_name (a), g_app_info_get_name (b));
}

static void
print_usage_list_cmds (void)
{
	guint longest = 0;
	GList *commands = NULL;
	GList *c;
	GFileEnumerator *enumerator;
	GFileInfo *info;
	GFile *dir;
	GError *error = NULL;
	const gchar *cli_metadata_dir;

	cli_metadata_dir = g_getenv ("TRACKER_CLI_DIR");

	if (!cli_metadata_dir) {
		cli_metadata_dir = CLI_METADATA_DIR;
	}

	dir = g_file_new_for_path (cli_metadata_dir);
	enumerator = g_file_enumerate_children (dir,
	                                        G_FILE_ATTRIBUTE_STANDARD_NAME,
	                                        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                                        NULL, &error);
	g_object_unref (dir);

	if (enumerator) {
		while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {
			const gchar *filename;

			filename = g_file_info_get_name (info);
			if (g_str_has_suffix (filename, ".desktop")) {
				gchar *path = NULL;
				GDesktopAppInfo *desktop_info;

				path = g_build_filename (cli_metadata_dir, filename, NULL);
				desktop_info = g_desktop_app_info_new_from_filename (path);
				if (desktop_info) {
					commands = g_list_prepend (commands, desktop_info);
				} else {
					g_warning ("Unable to load command info: %s", path);
				}

				g_free (path);
			}
			g_object_unref (info);
		}

		g_object_unref (enumerator);
	} else {
		g_warning ("Failed to list commands: %s", error->message);
	}

	puts (_("Available tracker3 commands are:"));

	if (commands) {
		commands = g_list_sort (commands, (GCompareFunc) compare_app_info);

		for (c = commands; c; c = c->next) {
			GDesktopAppInfo *desktop_info = c->data;
			const gchar *name = g_app_info_get_name (G_APP_INFO (desktop_info));

			if (longest < strlen (name))
				longest = strlen (name);
		}

		for (c = commands; c; c = c->next) {
			GDesktopAppInfo *desktop_info = c->data;
			const gchar *name = g_app_info_get_name (G_APP_INFO (desktop_info));
			const gchar *help = g_app_info_get_description (G_APP_INFO (desktop_info));

			g_print ("   %s   ", name);
			mput_char (' ', longest - strlen (name));
			puts (help);
		}

		g_list_free_full (commands, g_object_unref);
	}
}

static void
print_usage (void)
{
	g_print ("usage: %s\n\n", usage_string);
	print_usage_list_cmds ();
	g_print ("\n%s\n", _("See “tracker3 help <command>” to read about a specific subcommand."));
}

int
main (int argc, char *argv[])
{
	const gchar *bin_dir;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	bin_dir = g_getenv ("TRACKER_CLI_DIR");

	if (!bin_dir) {
		bin_dir = BINDIR;
	}

	if (argc == 1) {
		/* The user didn't specify a command; give them help */
		print_usage ();
		exit (EXIT_SUCCESS);
	}

	/* Look up and exec the subcommand. */
	const gchar *subcommand = argv[1];
	gchar *subcommand_binary = NULL;
	gchar *path = NULL;

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

	/* Execute subcommand binary */
	subcommand_binary = g_strdup_printf("tracker3-%s", subcommand);
	path = g_build_filename (bin_dir, subcommand_binary, NULL);

	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		/* Manipulate argv in place, in order to launch subcommand */
		argv[1] = path;
		execv (path, &argv[1]);
	} else {
		g_printerr (_("“%s” is not a tracker3 command. See “tracker3 --help”"), subcommand);
		g_printerr ("\n");
	}

	g_free (path);
	g_free (subcommand_binary);

	return EXIT_FAILURE;
}
