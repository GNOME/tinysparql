/*
 * Copyright (C) 2014, Lanedo GmbH <martyn@lanedo.com>
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

#include <libtracker-common/tracker-common.h>

#include "tracker-control.h"
#include "tracker-daemon.h"
#include "tracker-index.h"
#include "tracker-reset.h"
#include "tracker-help.h"

const char usage_string[] =
	"tracker [--version] [--help]\n"
	"               <command> [<args>]";
 
const char usage_more_info_string[] =
	N_("See 'tracker help <command>' to read about a specific subcommand.");

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
cmd_backup (int argc, const char **argv, const char *prefix)
{
	

	return 0;
}

static int
cmd_daemon (int argc, const char **argv, const char *prefix)
{
	GOptionContext *context;
	GError *error = NULL;

	context = g_option_context_new (NULL);
	g_option_context_add_group (context, tracker_daemon_get_option_group ());

	/* Common options */
	/* g_option_context_add_main_entries (context, common_entries, NULL); */

	if (!g_option_context_parse (context, &argc, (char***) &argv, &error)) {
		g_printerr ("%s, %s\n", _("Unrecognized options"), error->message);
		g_error_free (error);
		g_option_context_free (context);
		return 1;
	}

	g_option_context_free (context);

	if (tracker_daemon_options_enabled ()) {
		return tracker_daemon_run ();
	}

	tracker_daemon_run_default ();

	return 0;
}

static int
cmd_import (int argc, const char **argv, const char *prefix)
{
	return 0;
}

static int
cmd_help (int argc, const char **argv, const char *prefix)
{
	if (!argv[0] || !argv[1]) {
		print_usage ();
		return 0;
	}

	tracker_help_show_man_page (argv[1]);

	return 0;
}

static int
cmd_info (int argc, const char **argv, const char *prefix)
{
	return 0;
}

static int
cmd_index (int argc, const char **argv, const char *prefix)
{
	GOptionContext *context;
	GError *error = NULL;

	context = g_option_context_new (NULL);
	g_option_context_add_group (context, tracker_index_get_option_group ());

	/* Common options */
	/* g_option_context_add_main_entries (context, common_entries, NULL); */

	if (!g_option_context_parse (context, &argc, (char***) &argv, &error)) {
		g_printerr ("%s, %s\n", _("Unrecognized options"), error->message);
		g_error_free (error);
		g_option_context_free (context);
		return 1;
	}

	g_option_context_free (context);

	if (tracker_index_options_enabled ()) {
		return tracker_index_run ();
	}

	tracker_index_run_default ();

	return 0;
}

static int
cmd_reset (int argc, const char **argv, const char *prefix)
{
	GOptionContext *context;
	GError *error = NULL;

	context = g_option_context_new (NULL);
	g_option_context_add_group (context, tracker_reset_get_option_group ());

	/* Common options */
	/* g_option_context_add_main_entries (context, common_entries, NULL); */

	if (!g_option_context_parse (context, &argc, (char***) &argv, &error)) {
		g_printerr ("%s, %s\n", _("Unrecognized options"), error->message);
		g_error_free (error);
		g_option_context_free (context);
		return 1;
	}

	g_option_context_free (context);

	if (tracker_reset_options_enabled ()) {
		return tracker_reset_run ();
	}

	tracker_reset_run_default ();

	return 0;
}

static int
cmd_restore (int argc, const char **argv, const char *prefix)
{
	return 0;
}

static int
cmd_search (int argc, const char **argv, const char *prefix)
{
	return 0;
}

static int
cmd_sparql (int argc, const char **argv, const char *prefix)
{
	return 0;
}

static int
cmd_stats (int argc, const char **argv, const char *prefix)
{
	return 0;
}

static int
cmd_status (int argc, const char **argv, const char *prefix)
{
	return 0;
}

static int
cmd_tag (int argc, const char **argv, const char *prefix)
{
	return 0;
}

static int
cmd_version (int argc, const char **argv, const char *prefix)
{
	puts (about);
	return 0;
}


/*
 * require working tree to be present -- anything uses this needs
 * RUN_SETUP for reading from the configuration file.
 */
#define NEED_NOTHING		(1<<0)
#define NEED_WORK_TREE		(1<<1)

struct cmd_struct {
	const char *cmd;
	int (*fn)(int, const char **, const char *);
	int option;
	const char *help;
};

static struct cmd_struct commands[] = {
	{ "backup", cmd_backup, NEED_WORK_TREE, N_("Backup indexed content") },
	{ "daemon", cmd_daemon, NEED_WORK_TREE, N_("Start, stop, restart and list daemons responsible for indexing content") },
	{ "import", cmd_import, NEED_WORK_TREE, N_("Import a data set into the index") },
	{ "help", cmd_help, NEED_NOTHING, N_("Get help on how to use Tracker and any of these commands") },
	{ "info", cmd_info, NEED_WORK_TREE, N_("Show information known about local files or items indexed") }, 
	{ "index", cmd_index, NEED_NOTHING, N_("List, pause, resume and command data miners indexing content") },
	{ "reset", cmd_reset, NEED_NOTHING,  N_("Reset the index, configuration or replay journal") },
	{ "restore", cmd_restore, NEED_NOTHING, N_("Restore the index from a previous backup") },
	{ "search", cmd_search, NEED_WORK_TREE, N_("Search the index by RDF class") },
	{ "sparql", cmd_sparql, NEED_WORK_TREE, N_("Query and update the index using SPARQL or search and list ontology in use") },
	{ "stats", cmd_stats, NEED_WORK_TREE, N_("Show statistical information about indexed content") },
	{ "status", cmd_status, NEED_NOTHING, N_("Show the index status for the working tree") },
	{ "tag", cmd_tag, NEED_WORK_TREE, N_("Create, list or delete tags and related content") },
	{ "version", cmd_version, NEED_NOTHING, N_("Show the license and version in use") },
};

static int
run_builtin (struct cmd_struct *p, int argc, const char **argv)
{
	int status, help;
	const char *prefix;

	prefix = NULL;
	help = argc == 2 && !strcmp (argv[1], "-h");

	if (!help && p->option & NEED_WORK_TREE) {
		/* Check we have working tree */
		/* FIXME: Finish */
	}

	status = p->fn (argc, argv, prefix);
	if (status) {
		return status;
	}

	return 0;
}

static void
handle_command (int argc, const char **argv)
{
	const char *cmd = argv[0];
	int i;

	/* Turn "tracker cmd --help" into "tracker help cmd" */
	if (argc > 1 && !strcmp (argv[1], "--help")) {
		argv[1] = argv[0];
		argv[0] = cmd = "help";
	}

	for (i = 0; i < G_N_ELEMENTS (commands); i++) {
		struct cmd_struct *p = commands + i;

		if (strcmp (p->cmd, cmd)) {
			continue;
		}

		exit (run_builtin (p, argc, argv));
	}
}

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
	int i, longest = 0;

	for (i = 0; i < G_N_ELEMENTS(commands); i++) {
		if (longest < strlen (commands[i].cmd))
			longest = strlen(commands[i].cmd);
	}

	puts (_("Available tracker commands are:"));
	for (i = 0; i < G_N_ELEMENTS(commands); i++) {
		/* Don't list version in commands */
		if (!strcmp (commands[i].cmd, "version") ||
		    !strcmp (commands[i].cmd, "help")) {
			continue;
		}

		g_print("   %s   ", commands[i].cmd);
		mput_char (' ', longest - strlen (commands[i].cmd));
		puts(_(commands[i].help));
	}
}

static void
print_usage (void)
{
	g_print("usage: %s\n\n", usage_string);
	print_usage_list_cmds ();
	g_print("\n%s\n", _(usage_more_info_string));
}

int
main (int argc, char **av)
{
	const char **argv = (const char **) av;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	argv++;
	argc--;

	if (argc > 0) {
		/* For cases like --version */
		if (g_str_has_prefix (argv[0], "--")) {
			argv[0] += 2;
		}
	} else {
		/* The user didn't specify a command; give them help */
		print_usage ();
		exit (1);
	}

	handle_command (argc, argv);

	return 1;
}
