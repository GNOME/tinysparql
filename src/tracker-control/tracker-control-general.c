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

static GSList *
get_pids (void)
{
	GError *error = NULL;
	GDir *dir;
	GSList *pids = NULL;
	const gchar *name;

	dir = g_dir_open ("/proc", 0, &error);
	if (error) {
		g_printerr ("%s: %s\n",
		            _("Could not open /proc"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
		return NULL;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		gchar c;
		gboolean is_pid = TRUE;

		for (c = *name; c && c != ':' && is_pid; c++) {
			is_pid &= g_ascii_isdigit (c);
		}

		if (!is_pid) {
			continue;
		}

		pids = g_slist_prepend (pids, g_strdup (name));
	}

	g_dir_close (dir);

	return g_slist_reverse (pids);
}

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

typedef struct {
	gchar *name;
	GSettings *settings;
	gboolean is_miner;
} ComponentGSettings;

inline static const gchar *
verbosity_to_string (TrackerVerbosity verbosity)
{
        GType type;
        GEnumClass *enum_class;
        GEnumValue *enum_value;

        type = tracker_verbosity_get_type ();
        enum_class = G_ENUM_CLASS (g_type_class_peek (type));
        enum_value = g_enum_get_value (enum_class, verbosity);

        if (!enum_value) {
                return "unknown";
        }

        return enum_value->value_nick;
}

inline static void
tracker_gsettings_print_verbosity (GSList   *all,
                                   gint      longest,
                                   gboolean  miners)
{
	GSList *l;

	for (l = all; l; l = l->next) {
		ComponentGSettings *c;
		TrackerVerbosity v;

		c = l->data;

		if (c->is_miner == miners) {
			continue;
		}

		v = g_settings_get_enum (c->settings, "verbosity");

		g_print ("  %-*.*s: %s\n",
		         longest,
		         longest,
		         c->name,
		         verbosity_to_string (v));
	}
}

static gboolean
tracker_gsettings_set_all (GSList           *all,
                           TrackerVerbosity  verbosity)
{
	GSList *l;
	gboolean success = TRUE;

	for (l = all; l && success; l = l->next) {
		ComponentGSettings *c = l->data;

		if (!c) {
			continue;
		}

		success &= g_settings_set_enum (c->settings, "verbosity", verbosity);
		g_settings_apply (c->settings);
	}

	g_settings_sync ();

	return success;
}

static GSList *
tracker_gsettings_get_all (gint *longest_name_length)
{
	typedef struct {
		const gchar *schema;
		const gchar *path;
	} SchemaWithPath;

	TrackerMinerManager *manager;
	GError *error = NULL;
	GSettings *settings;
	GSList *all = NULL;
	GSList *l;
	GSList *miners_available;
	GSList *valid_schemas = NULL;
	const gchar * const *schema;
	gint len = 0;
	SchemaWithPath components[] = {
		{ "Store", "store" },
		{ "Extract", "extract" },
		{ "Writeback", "writeback" },
		{ 0 }
	};
	SchemaWithPath *swp;

	/* Don't auto-start the miners here */
	manager = tracker_miner_manager_new_full (FALSE, &error);
	if (!manager) {
		g_printerr (_("Could not get GSettings for miners, manager could not be created, %s"),
		            error ? error->message : _("No error given"));
		g_printerr ("\n");
		g_clear_error (&error);
		return NULL;
	}

	miners_available = tracker_miner_manager_get_available (manager);

	/* Get valid schemas so we don't try to load invalid ones */
	for (schema = g_settings_list_schemas (); schema && *schema; schema++) {
		if (!g_str_has_prefix (*schema, "org.freedesktop.Tracker.")) {
			continue;
		}

		valid_schemas = g_slist_prepend (valid_schemas, g_strdup (*schema));
	}

	/* Store / General */
	for (swp = components; swp && swp->schema; swp++) {
		gchar *schema;
		gchar *path;

		schema = g_strdup_printf ("org.freedesktop.Tracker.%s", swp->schema);
		path = g_strdup_printf ("/org/freedesktop/tracker/%s/", swp->path);

		/* If miner doesn't have a schema, no point in getting config */
		if (!tracker_string_in_gslist (schema, valid_schemas)) {
			g_free (path);
			g_free (schema);
			continue;
		}

		len = MAX (len, strlen (swp->schema));

		settings = g_settings_new_with_path (schema, path);
		if (settings) {
			ComponentGSettings *c = g_slice_new (ComponentGSettings);

			c->name = g_strdup (swp->schema);
			c->settings = settings;
			c->is_miner = FALSE;

			all = g_slist_prepend (all, c);
		}
	}

	/* Miners */
	for (l = miners_available; l; l = l->next) {
		const gchar *name;
		gchar *schema;
		gchar *name_lowercase;
		gchar *path;
		gchar *miner;

		miner = l->data;
		if (!miner) {
			continue;
		}

		name = g_utf8_strrchr (miner, -1, '.');
		if (!name) {
			continue;
		}

		name++;
		name_lowercase = g_utf8_strdown (name, -1);

		schema = g_strdup_printf ("org.freedesktop.Tracker.Miner.%s", name);
		path = g_strdup_printf ("/org/freedesktop/tracker/miner/%s/", name_lowercase);
		g_free (name_lowercase);

		/* If miner doesn't have a schema, no point in getting config */
		if (!tracker_string_in_gslist (schema, valid_schemas)) {
			g_free (path);
			g_free (schema);
			continue;
		}

		settings = g_settings_new_with_path (schema, path);
		g_free (path);
		g_free (schema);

		if (settings) {
			ComponentGSettings *c = g_slice_new (ComponentGSettings);

			c->name = g_strdup (name);
			c->settings = settings;
			c->is_miner = TRUE;

			all = g_slist_prepend (all, c);
			len = MAX (len, strlen (name));
		}
	}

	g_slist_foreach (valid_schemas, (GFunc) g_free, NULL);
	g_slist_free (valid_schemas);
	g_slist_foreach (miners_available, (GFunc) g_free, NULL);
	g_slist_free (miners_available);
	g_object_unref (manager);

	if (longest_name_length) {
		*longest_name_length = len;
	}

	return g_slist_reverse (all);
}

static void
tracker_gsettings_free (GSList *all)
{
	GSList *l;

	/* Clean up */
	for (l = all; l; l = l->next) {
		ComponentGSettings *c = l->data;

		g_free (c->name);
		g_object_unref (c->settings);
		g_slice_free (ComponentGSettings, c);
	}
}

static gboolean
term_option_arg_func (const gchar  *option_value,
                      const gchar  *value,
                      gpointer      data,
                      GError      **error)
{
	TermOption option;

	if (!value) {
		value = OPTION_TERM_ALL;
	}

	if (strcmp (value, OPTION_TERM_ALL) == 0) {
		option = TERM_ALL;
	} else if (strcmp (value, OPTION_TERM_STORE) == 0) {
		option = TERM_STORE;
	} else if (strcmp (value, OPTION_TERM_MINERS) == 0) {
		option = TERM_MINERS;
	} else {
		g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
		                     _("Only one of 'all', 'store' and 'miners' options are allowed"));
		return FALSE;
	}

	if (strcmp (option_value, "-k") == 0 ||
	    strcmp (option_value, "--kill") == 0) {
		kill_option = option;
	} else if (strcmp (option_value, "-t") == 0 ||
	           strcmp (option_value, "--terminate") == 0) {
		terminate_option = option;
	}

	return TRUE;
}

static gboolean
has_valid_uri_scheme (const gchar *uri)
{
	const gchar *s;

	s = uri;

	if (!g_ascii_isalpha (*s)) {
		return FALSE;
	}

	do {
		s++;
	} while (g_ascii_isalnum (*s) || *s == '+' || *s == '.' || *s == '-');

	return (*s == ':');
}

static gchar *
get_uri_from_arg (const gchar *arg)
{
	gchar *uri;

	/* support both, URIs and local file paths */
	if (has_valid_uri_scheme (arg)) {
		uri = g_strdup (arg);
	} else {
		GFile *file;

		file = g_file_new_for_commandline_arg (arg);
		uri = g_file_get_uri (file);
		g_object_unref (file);
	}

	return uri;
}

static inline guint32
get_uid_for_pid (const gchar  *pid_as_string,
                 gchar       **filename)
{
	GFile *f;
	GFileInfo *info;
	GError *error = NULL;
	gchar *fn;
	guint uid;

#ifdef __sun /* Solaris */
	fn = g_build_filename ("/proc", pid_as_string, "psinfo", NULL);
#else
	fn = g_build_filename ("/proc", pid_as_string, "cmdline", NULL);
#endif

	f = g_file_new_for_path (fn);
	info = g_file_query_info (f,
	                          G_FILE_ATTRIBUTE_UNIX_UID,
	                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                          NULL,
	                          &error);

	if (error) {
		g_printerr ("%s '%s', %s", _("Could not stat() file"), fn, error->message);
		g_error_free (error);
		uid = 0;
	} else {
		uid = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID);
		g_object_unref (info);
	}

	if (filename) {
		*filename = fn;
	} else {
		g_free (fn);
	}

	g_object_unref (f);

	return uid;
}

static void
collect_debug (void)
{
	/* What to collect?
	 * This is based on information usually requested from maintainers to users.
	 *
	 * 1. Package details, e.g. version.
	 * 2. Disk size, space left, type (SSD/etc)
	 * 3. Size of dataset (tracker-stats), size of databases
	 * 4. Current configuration (libtracker-fts, tracker-miner-fs, tracker-extract)
	 *    All txt files in ~/.cache/
	 * 5. Statistics about data (tracker-stats)
	 */

	GDir *d;
	gchar *data_dir;
	gchar *str;

	data_dir = g_build_filename (g_get_user_cache_dir (), "tracker", NULL);

	/* 1. Package details, e.g. version. */
	g_print ("[Package Details]\n");
	g_print ("%s: " PACKAGE_VERSION "\n", _("Version"));
	g_print ("\n\n");

	/* 2. Disk size, space left, type (SSD/etc) */
	guint64 remaining_bytes;
	gdouble remaining;

	g_print ("[%s]\n", _("Disk Information"));

	remaining_bytes = tracker_file_system_get_remaining_space (data_dir);
	str = g_format_size (remaining_bytes);

	remaining = tracker_file_system_get_remaining_space_percentage (data_dir);
	g_print ("%s: %s (%3.2lf%%)\n",
	         _("Remaining space on database partition"),
	         str,
	         remaining);
	g_free (str);
	g_print ("\n\n");

	/* 3. Size of dataset (tracker-stats), size of databases */
	g_print ("[%s]\n", _("Data Set"));

	for (d = g_dir_open (data_dir, 0, NULL); d != NULL;) {
		const gchar *f;
		gchar *path;
		goffset size;

		f = g_dir_read_name (d);
		if (!f) {
			break;
		}

		if (g_str_has_suffix (f, ".txt")) {
			continue;
		}

		path = g_build_filename (data_dir, f, NULL);
		size = tracker_file_get_size (path);
		str = g_format_size (size);

		g_print ("%s\n%s\n\n", path, str);
		g_free (str);
		g_free (path);
	}
	g_dir_close (d);
	g_print ("\n");

	/* 4. Current configuration (libtracker-fts, tracker-miner-fs, tracker-extract)
	 *    All txt files in ~/.cache/
	 */
	GSList *all, *l;

	g_print ("[%s]\n", _("Configuration"));

	all = tracker_gsettings_get_all (NULL);

	if (all) {
		for (l = all; l; l = l->next) {
			ComponentGSettings *c = l->data;
			gchar **keys, **p;

			if (!c) {
				continue;
			}

			keys = g_settings_list_keys (c->settings);
			for (p = keys; p && *p; p++) {
				GVariant *v;
				gchar *printed;

				v = g_settings_get_value (c->settings, *p);
				printed = g_variant_print (v, FALSE);
				g_print ("%s.%s: %s\n", c->name, *p, printed);
				g_free (printed);
				g_variant_unref (v);
			}
		}

		tracker_gsettings_free (all);
	} else {
		g_print ("** %s **\n", _("No configuration was found"));
	}
	g_print ("\n\n");

	g_print ("[%s]\n", _("States"));

	for (d = g_dir_open (data_dir, 0, NULL); d != NULL;) {
		const gchar *f;
		gchar *path;
		gchar *content = NULL;

		f = g_dir_read_name (d);
		if (!f) {
			break;
		}

		if (!g_str_has_suffix (f, ".txt")) {
			continue;
		}

		path = g_build_filename (data_dir, f, NULL);
		if (g_file_get_contents (path, &content, NULL, NULL)) {
			/* Special case last-index.txt which is time() dump to file */
			if (g_str_has_suffix (path, "last-crawl.txt")) {
				guint64 then, now;

				now = (guint64) time (NULL);
				then = g_ascii_strtoull (content, NULL, 10);
				str = tracker_seconds_to_string (now - then, FALSE);

				g_print ("%s\n%s (%s)\n\n", path, content, str);
			} else {
				g_print ("%s\n%s\n\n", path, content);
			}
			g_free (content);
		}
		g_free (path);
	}
	g_dir_close (d);
	g_print ("\n");

	/* 5. Statistics about data (tracker-stats) */
	TrackerSparqlConnection *connection;
	GError *error = NULL;

	g_print ("[%s]\n", _("Data Statistics"));

	connection = tracker_sparql_connection_get (NULL, &error);

	if (!connection) {
		g_print ("** %s, %s **\n",
		         _("No connection available"),
		         error ? error->message : _("No error given"));
		g_clear_error (&error);
	} else {
		TrackerSparqlCursor *cursor;

		cursor = tracker_sparql_connection_statistics (connection, NULL, &error);

		if (error) {
			g_print ("** %s, %s **\n",
			         _("Could not get statistics"),
			         error ? error->message : _("No error given"));
			g_error_free (error);
		} else {
			if (!cursor) {
				g_print ("** %s **\n",
				         _("No statistics were available"));
			} else {
				gint count = 0;

				while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
					g_print ("%s: %s\n",
					         tracker_sparql_cursor_get_string (cursor, 0, NULL),
					         tracker_sparql_cursor_get_string (cursor, 1, NULL));
					count++;
				}

				if (count == 0) {
					g_print ("%s\n",
					         _("Database is currently empty"));
				}

				g_object_unref (cursor);
			}
		}
	}

	g_object_unref (connection);
	g_print ("\n\n");

	g_print ("\n");

	g_free (data_dir);
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
	GError *error = NULL;
	GSList *pids;
	GSList *l;
	gchar *str;
	gpointer verbosity_type_enum_class_pointer = NULL;
	TrackerVerbosity set_log_verbosity_value = TRACKER_VERBOSITY_ERRORS;

	/* Constraints */

	if (kill_option != TERM_NONE && terminate_option != TERM_NONE) {
		g_printerr ("%s\n",
		            _("You can not use the --kill and --terminate arguments together"));
		return EXIT_FAILURE;
	}

	if ((hard_reset || soft_reset) && terminate_option != TERM_NONE) {
		g_printerr ("%s\n",
		            _("You can not use the --terminate with --hard-reset or --soft-reset, --kill is implied"));
		return EXIT_FAILURE;
	}

	if (hard_reset && soft_reset) {
		g_printerr ("%s\n",
		            _("You can not use the --hard-reset and --soft-reset arguments together"));
		return EXIT_FAILURE;
	}

	if (get_log_verbosity && set_log_verbosity) {
		g_printerr ("%s\n",
		            _("You can not use the --get-logging and --set-logging arguments together"));
		return EXIT_FAILURE;
	}

	if (set_log_verbosity) {
		if (g_ascii_strcasecmp (set_log_verbosity, "debug") == 0) {
			set_log_verbosity_value = TRACKER_VERBOSITY_DEBUG;
		} else if (g_ascii_strcasecmp (set_log_verbosity, "detailed") == 0) {
			set_log_verbosity_value = TRACKER_VERBOSITY_DETAILED;
		} else if (g_ascii_strcasecmp (set_log_verbosity, "minimal") == 0) {
			set_log_verbosity_value = TRACKER_VERBOSITY_MINIMAL;
		} else if (g_ascii_strcasecmp (set_log_verbosity, "errors") == 0) {
			set_log_verbosity_value = TRACKER_VERBOSITY_ERRORS;
		} else {
			g_printerr ("%s\n",
			            _("Invalid log verbosity, try 'debug', 'detailed', 'minimal' or 'errors'"));
			return EXIT_FAILURE;
		}
	}

	if (collect_debug_info) {
		collect_debug ();
		return EXIT_SUCCESS;
	}

	if (hard_reset || soft_reset) {
		/* Imply --kill */
		kill_option = TERM_ALL;
	}

	if (get_log_verbosity || set_log_verbosity) {
		GType etype;

		/* Since we don't reference this enum anywhere, we do
		 * it here to make sure it exists when we call
		 * g_type_class_peek(). This wouldn't be necessary if
		 * it was a param in a GObject for example.
		 *
		 * This does mean that we are leaking by 1 reference
		 * here and should clean it up, but it doesn't grow so
		 * this is acceptable.
		 */
		etype = tracker_verbosity_get_type ();
		verbosity_type_enum_class_pointer = g_type_class_ref (etype);
	}

	/* Unless we are stopping processes or listing processes,
	 * don't iterate them.
	 */
	if (kill_option != TERM_NONE ||
	    terminate_option != TERM_NONE ||
	    list_processes) {
		guint32 own_pid;
		guint32 own_uid;
		gchar *own_pid_str;

		pids = get_pids ();
		str = g_strdup_printf (g_dngettext (NULL,
		                                    "Found %d PID…",
		                                    "Found %d PIDs…",
		                                    g_slist_length (pids)),
		                       g_slist_length (pids));
		g_print ("%s\n", str);
		g_free (str);

		/* Establish own uid/pid */
		own_pid = (guint32) getpid ();
		own_pid_str = g_strdup_printf ("%d", own_pid);
		own_uid = get_uid_for_pid (own_pid_str, NULL);
		g_free (own_pid_str);

		for (l = pids; l; l = l->next) {
			GError *error = NULL;
			gchar *filename;
#ifdef __sun /* Solaris */
			psinfo_t psinfo = { 0 };
#endif
			gchar *contents = NULL;

			gchar **strv;
			guint uid;

			uid = get_uid_for_pid (l->data, &filename);

			/* Stat the file and make sure current user == file owner */
			if (uid != own_uid) {
				continue;
			}

			/* Get contents to determine basename */
			if (!g_file_get_contents (filename, &contents, NULL, &error)) {
				str = g_strdup_printf (_("Could not open '%s'"), filename);
				g_printerr ("%s: %s\n",
				            str,
				            error ? error->message : _("No error given"));
				g_free (str);
				g_clear_error (&error);
				g_free (contents);
				g_free (filename);

				continue;
			}
#ifdef __sun /* Solaris */
			memcpy (&psinfo, contents, sizeof (psinfo));

			/* won't work with paths containing spaces :( */
			strv = g_strsplit (psinfo.pr_psargs, " ", 2);
#else
			strv = g_strsplit (contents, "^@", 2);
#endif
			if (strv && strv[0]) {
				gchar *basename;

				basename = g_path_get_basename (strv[0]);

				if ((g_str_has_prefix (basename, "tracker") == TRUE ||
				     g_str_has_prefix (basename, "lt-tracker") == TRUE) &&
				    g_str_has_suffix (basename, "-control") == FALSE &&
				    g_str_has_suffix (basename, "-status-icon") == FALSE) {
					pid_t pid;

					pid = atoi (l->data);
					str = g_strdup_printf (_("Found process ID %d for '%s'"), pid, basename);
					g_print ("%s\n", str);
					g_free (str);

					if (terminate_option != TERM_NONE) {
						if ((terminate_option == TERM_STORE &&
						     !g_str_has_suffix (basename, "tracker-store")) ||
						    (terminate_option == TERM_MINERS &&
						     !strstr (basename, "tracker-miner"))) {
							continue;
						}

						if (kill (pid, SIGTERM) == -1) {
							const gchar *errstr = g_strerror (errno);

							str = g_strdup_printf (_("Could not terminate process %d"), pid);
							g_printerr ("  %s: %s\n",
							            str,
							            errstr ? errstr : _("No error given"));
							g_free (str);
						} else {
							str = g_strdup_printf (_("Terminated process %d"), pid);
							g_print ("  %s\n", str);
							g_free (str);
						}
					} else if (kill_option != TERM_NONE) {
						if ((kill_option == TERM_STORE &&
						     !g_str_has_suffix (basename, "tracker-store")) ||
						    (kill_option == TERM_MINERS &&
						     !strstr (basename, "tracker-miner"))) {
							continue;
						}

						if (kill (pid, SIGKILL) == -1) {
							const gchar *errstr = g_strerror (errno);

							str = g_strdup_printf (_("Could not kill process %d"), pid);
							g_printerr ("  %s: %s\n",
							            str,
							            errstr ? errstr : _("No error given"));
							g_free (str);
						} else {
							str = g_strdup_printf (_("Killed process %d"), pid);
							g_print ("  %s\n", str);
							g_free (str);
						}
					}
				}

				g_free (basename);
			}

			g_strfreev (strv);
			g_free (contents);
			g_free (filename);
		}

		g_slist_foreach (pids, (GFunc) g_free, NULL);
		g_slist_free (pids);

		/* If we just wanted to list processes, all done */
		if (list_processes &&
		    terminate_option == TERM_NONE &&
		    kill_option == TERM_NONE) {
			return EXIT_SUCCESS;
		}
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

	/* Deal with logging changes AFTER the config may have been
	 * reset, this way users can actually use --remove-config with
	 * the --set-logging switch.
	 */
	if (get_log_verbosity) {
		GSList *all;
		gint longest = 0;

		all = tracker_gsettings_get_all (&longest);

		if (!all) {
			return EXIT_FAILURE;
		}

		g_print ("%s:\n", _("Components"));
		tracker_gsettings_print_verbosity (all, longest, TRUE);
		g_print ("\n");

		/* Miners */
		g_print ("%s (%s):\n",
		         _("Miners"),
		         _("Only those with config listed"));
		tracker_gsettings_print_verbosity (all, longest, FALSE);
		g_print ("\n");

		tracker_gsettings_free (all);
	}

	if (set_log_verbosity) {
		GSList *all;
		gchar *str;
		gint longest = 0;

		all = tracker_gsettings_get_all (&longest);

		if (!all) {
			return EXIT_FAILURE;
		}

		str = g_strdup_printf (_("Setting log verbosity for all components to '%s'…"), set_log_verbosity);
		g_print ("%s\n", str);
		g_print ("\n");
		g_free (str);

		tracker_gsettings_set_all (all, set_log_verbosity_value);
		tracker_gsettings_free (all);

		/* We free to make sure we get new settings and that
		 * they're saved properly.
		 */
		all = tracker_gsettings_get_all (&longest);

		if (!all) {
			return EXIT_FAILURE;
		}

		g_print ("%s:\n", _("Components"));
		tracker_gsettings_print_verbosity (all, longest, TRUE);
		g_print ("\n");

		/* Miners */
		g_print ("%s (%s):\n",
		         _("Miners"),
		         _("Only those with config listed"));
		tracker_gsettings_print_verbosity (all, longest, FALSE);
		g_print ("\n");

		tracker_gsettings_free (all);
	}

	if (verbosity_type_enum_class_pointer) {
		g_type_class_unref (verbosity_type_enum_class_pointer);
	}

	if (start) {
		TrackerMinerManager *manager;
		GSList *miners, *l;

		if (hard_reset || soft_reset) {
			g_print ("%s\n", _("Waiting one second before starting miners…"));

			/* Give a second's grace to avoid race conditions */
			g_usleep (G_USEC_PER_SEC);
		}

		g_print ("%s\n", _("Starting miners…"));


		/* Auto-start the miners here */
		manager = tracker_miner_manager_new_full (TRUE, &error);
		if (!manager) {
			g_printerr (_("Could not start miners, manager could not be created, %s"),
			            error ? error->message : _("No error given"));
			g_printerr ("\n");
			g_clear_error (&error);
			return EXIT_FAILURE;
		}

		miners = tracker_miner_manager_get_available (manager);

		/* Get the status of all miners, this will start all
		 * miners not already running.
		 */
		for (l = miners; l; l = l->next) {
			const gchar *display_name;
			gdouble progress = 0.0;

			display_name = tracker_miner_manager_get_display_name (manager, l->data);

			if (!tracker_miner_manager_get_status (manager,
			                                       l->data,
			                                       NULL,
			                                       &progress,
			                                       NULL)) {
				g_printerr ("  ✗ %s (%s)\n",
				            display_name,
				            _("perhaps a disabled plugin?"));
			} else {
				g_print ("  ✓ %s\n",
				         display_name);
			}

			g_free (l->data);
		}

		g_slist_free (miners);
		g_object_unref (manager);
	}

	if (backup) {
		GDBusConnection *connection;
		GDBusProxy *proxy;
		GError *error = NULL;
		GVariant *v;
		gchar *uri;

		if (!tracker_control_dbus_get_connection ("org.freedesktop.Tracker1",
		                                          "/org/freedesktop/Tracker1/Backup",
		                                          "org.freedesktop.Tracker1.Backup",
		                                          G_DBUS_PROXY_FLAGS_NONE,
		                                          &connection,
		                                          &proxy)) {
			return EXIT_FAILURE;
		}

		uri = get_uri_from_arg (backup);

		g_print ("%s\n", _("Backing up database"));
		g_print ("  %s\n", uri);

		/* Backup/Restore can take some time */
		g_dbus_proxy_set_default_timeout (proxy, G_MAXINT);

		v = g_dbus_proxy_call_sync (proxy,
		                            "Save",
		                            g_variant_new ("(s)", uri),
		                            G_DBUS_CALL_FLAGS_NONE,
		                            -1,
		                            NULL,
		                            &error);

		if (proxy) {
			g_object_unref (proxy);
		}

		if (error) {
			g_critical ("%s, %s",
			            _("Could not backup database"),
			            error ? error->message : _("No error given"));
			g_clear_error (&error);
			g_free (uri);

			return EXIT_FAILURE;
		}

		if (v) {
			g_variant_unref (v);
		}

		g_free (uri);
	}

	if (restore) {
		GDBusConnection *connection;
		GDBusProxy *proxy;
		GError *error = NULL;
		GVariant *v;
		gchar *uri;

		if (!tracker_control_dbus_get_connection ("org.freedesktop.Tracker1",
		                                          "/org/freedesktop/Tracker1/Backup",
		                                          "org.freedesktop.Tracker1.Backup",
		                                          G_DBUS_PROXY_FLAGS_NONE,
		                                          &connection,
		                                          &proxy)) {
			return EXIT_FAILURE;
		}

		uri = get_uri_from_arg (restore);

		g_print ("%s\n", _("Restoring database from backup"));
		g_print ("  %s\n", uri);

		/* Backup/Restore can take some time */
		g_dbus_proxy_set_default_timeout (proxy, G_MAXINT);

		v = g_dbus_proxy_call_sync (proxy,
		                            "Restore",
		                            g_variant_new ("(s)", uri),
		                            G_DBUS_CALL_FLAGS_NONE,
		                            -1,
		                            NULL,
		                            &error);

		if (proxy) {
			g_object_unref (proxy);
		}

		if (error) {
			g_critical ("%s, %s",
			            _("Could not backup database"),
			            error ? error->message : _("No error given"));
			g_clear_error (&error);
			g_free (uri);

			return EXIT_FAILURE;
		}

		if (v) {
			g_variant_unref (v);
		}

		g_free (uri);
	}

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
