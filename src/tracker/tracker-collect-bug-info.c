/*
 * Copyright (C) 2015, Carlos Garnacho <carlosg@gnome.org>
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
 *
 * Author: Carlos garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include "tracker-collect-bug-info.h"
#include "tracker-color.h"

#include <sys/stat.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <libtracker-data/tracker-data.h>
#include <libtracker-sparql/tracker-sparql.h>

/* We most usually appear on logs due to gnome-session */
#define LOG_IDENTIFIER_PATTERN "gnome-session"

#define OPTIONS_ENABLED (inspect_logs || (urns && g_strv_length (urns) > 0))

#define MAX_MEGS 10
#define MAX_SIZE (MAX_MEGS * 1024 * 1024)

typedef struct {
	gchar *uri;
	gchar *urn;
} FailureInfo;

static gchar **urns;
gboolean inspect_logs;
gboolean bypass_limits;

static GOptionEntry entries[] = {
	{ "bypass-limits", 'b', 0,
	  G_OPTION_ARG_NONE, &bypass_limits,
	  N_("Bypass file size restrictions"),
	  NULL,
	},
	{ "inspect-logs", 'l', 0,
	  G_OPTION_ARG_NONE, &inspect_logs,
	  N_("Inspect logs for Tracker failures"),
	  NULL,
	},
	{ G_OPTION_REMAINING, 0, 0,
	  G_OPTION_ARG_FILENAME_ARRAY, &urns, NULL,
	  /* Translators: URN stands for "Uniform resource name", this is a
	   * Tracker term, and should be probably better left untranslated.
	   */
	  N_("[FILE OR URN...]")
	},
	{ NULL }
};

static gchar *
simple_query (TrackerSparqlConnection *connection,
              const gchar            *query)
{
	TrackerSparqlCursor *cursor = NULL;
	GError *error = NULL;

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

	if (!cursor || error) {
		if (error) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
		return NULL;
	}

	if (!tracker_sparql_cursor_next (cursor, NULL, &error)) {
		if (error) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
		return NULL;
	}

	return g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
}

static gchar *
query_urn_uri (TrackerSparqlConnection *connection,
               const gchar             *urn)
{
	gchar *query, *str;

	query = g_strdup_printf ("SELECT nie:url(<%s>) {}", urn);
	str = simple_query (connection, query);
	g_free (query);

	return str;
}

static gchar *
query_uri_urn (TrackerSparqlConnection *connection,
               const gchar             *uri)
{
	gchar *query, *str;

	query = g_strdup_printf ("SELECT ?u { ?u nie:url \"%s\" }", uri);
	str = simple_query (connection, query);
	g_free (query);

	return str;
}

static gboolean
extract_one_subject (TrackerSparqlConnection  *connection,
                     const gchar              *subject,
                     FILE                     *f,
                     GList                   **urns_found)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gboolean first = TRUE;
	gchar *query;

	query = g_strdup_printf ("SELECT ?p ?o { <%s> ?p ?o }", subject);
	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

	if (!cursor || error) {
		if (error) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
		return FALSE;
	}

	g_fprintf (f, "<%s>", subject);

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		const gchar *pred, *object;

		g_fprintf (f, "%s\n", first ? "" : ";");
		first = FALSE;

		pred = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		object = tracker_sparql_cursor_get_string (cursor, 1, NULL);
		g_fprintf (f, "\t%s ", pred);

		if (g_str_has_suffix (pred, "plainTextContent"))
			object = "[EDITED]";

		if (g_str_has_prefix (object, "urn:")) {
			if (urns_found && strcmp (object, subject) != 0) {
				*urns_found = g_list_prepend (*urns_found,
				                              g_strdup (object));
			}
			g_fprintf (f, "<%s>", object);
		} else {
			g_fprintf (f, "'%s'", object);
		}
	}

	g_fprintf (f, ".\n\n");

	return TRUE;
}

static gboolean
extract_urn_info (TrackerSparqlConnection *connection,
                  const gchar             *urn,
                  const gchar             *destdir)
{
	GList *found = NULL;
	gchar *filename;
	FILE *f;

	filename = g_build_filename (destdir, "tracker-database-info.txt", NULL);
	f = fopen (filename, "w");
	g_free (filename);

	if (!extract_one_subject (connection, urn, f, &found)) {
		fclose (f);
		return FALSE;
	}

	while (found) {
		extract_one_subject (connection, found->data, f, NULL);
		g_free (found->data);
		found = g_list_delete_link (found, found);
	}

	fclose (f);
	return TRUE;
}

static GArray *
filter_duplicates (TrackerSparqlConnection *connection,
                   GList                   *file_list,
                   GList                   *urn_list)
{
	GHashTable *urns, *files;
	GHashTableIter iter;
	gchar *uri, *urn;
	GArray *result;
	GList *l;

	urns = g_hash_table_new (g_str_hash, g_str_equal);
	files = g_hash_table_new (g_str_hash, g_str_equal);
	result = g_array_new (FALSE, FALSE, sizeof (FailureInfo));

	/* Start with urns */
	for (l = urn_list; l; l = l->next) {
		gchar *uri;

		if (g_hash_table_lookup (urns, l->data))
			continue;

		uri = query_urn_uri (connection, l->data);

		if (uri) {
			g_hash_table_insert (urns, g_strdup (l->data), uri);
			g_hash_table_insert (files, uri, l->data);
		}
	}

	/* Continue with files */
	for (l = file_list; l; l = l->next) {
		gchar *urn;

		if (g_hash_table_lookup (files, l->data))
			continue;

		urn = query_uri_urn (connection, l->data);

		if (urn) {
			g_hash_table_insert (urns, urn, g_strdup (l->data));
			g_hash_table_insert (files, l->data, urn);
		}
	}

	g_hash_table_iter_init (&iter, urns);

	while (g_hash_table_iter_next (&iter, (gpointer *) &urn, (gpointer *) &uri)) {
		FailureInfo info;

		info.urn = g_strdup (urn);
		info.uri = g_strdup (uri);
		g_array_append_val (result, info);
	}

	g_hash_table_destroy (files);
	g_hash_table_destroy (urns);

	return result;
}

static GArray *
fetch_log_failures (TrackerSparqlConnection *connection)
{
	gchar *argv[] = { "journalctl", "--no-pager", "-t", LOG_IDENTIFIER_PATTERN,
	                  "--since=-30d", "--output=short-iso", NULL };
	GError *error = NULL;
	GDataInputStream *dstream;
	GInputStream *stream;
	gint output;
	gchar *line;
	gssize size;
	GPid pid;
	GRegex *file_regex, *urn_regex;
	GList *files = NULL, *urns = NULL;
	GArray *result;

	if (!g_spawn_async_with_pipes (NULL, argv, NULL,
	                               G_SPAWN_SEARCH_PATH |
	                               G_SPAWN_STDERR_TO_DEV_NULL,
	                               NULL, NULL, &pid,
	                               NULL, &output, NULL, &error)) {
		if (error) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
		return NULL;
	}

	file_regex = g_regex_new ("file:/[^()'\",]+", G_REGEX_OPTIMIZE, 0, NULL);
	urn_regex = g_regex_new ("urn:uuid:[a-zA-Z0-9\\-]+", G_REGEX_OPTIMIZE, 0, NULL);

	stream = g_unix_input_stream_new (output, FALSE);
	dstream = g_data_input_stream_new (stream);

	do {
		line = g_data_input_stream_read_line (dstream, &size, NULL, &error);

		if (!line) {
			if (error) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}
			break;
		}

		if (strstr (line, "Tracker")) {
			GMatchInfo *info;

			if (g_regex_match (file_regex, line, 0, &info)) {
				files = g_list_prepend (files, g_match_info_fetch (info, 0));
				g_match_info_free (info);
			}

			if (g_regex_match (urn_regex, line, 0, &info)) {
				urns = g_list_prepend (urns, g_match_info_fetch (info, 0));
				g_match_info_free (info);
			}
		}

		g_free (line);
	} while (TRUE);

	g_spawn_close_pid (pid);
	g_object_unref (dstream);
	g_object_unref (stream);

	g_regex_unref (file_regex);
	g_regex_unref (urn_regex);

	result = filter_duplicates (connection, files, urns);

	g_list_foreach (files, (GFunc) g_free, NULL);
	g_list_free (files);
	g_list_foreach (urns, (GFunc) g_free, NULL);
	g_list_free (urns);

	return result;
}

static void
extract_related_logs (const gchar *urn,
                      const gchar *uri,
                      const gchar *destdir,
                      const gchar *filename)
{
	gchar *argv[] = { "journalctl", "--no-pager", "-t", LOG_IDENTIFIER_PATTERN,
	                  "--since=-30d", "--output=short-iso", NULL };
	GError *error = NULL;
	GDataInputStream *dstream;
	GInputStream *stream;
	gchar *line, *path;
	gint output;
	gssize size;
	GPid pid;
	FILE *f;

	g_print (_("Extracting related logs... "));

	if (!g_spawn_async_with_pipes (NULL, argv, NULL,
	                               G_SPAWN_SEARCH_PATH |
	                               G_SPAWN_STDERR_TO_DEV_NULL,
	                               NULL, NULL, &pid,
	                               NULL, &output, NULL, &error)) {
		if (error) {
			g_print (WARN_BEGIN "%s" WARN_END "\n", error->message);
			g_error_free (error);
		}
		return;
	}

	path = g_build_filename (destdir, filename, NULL);
	f = fopen (path, "w");
	g_free (path);

	stream = g_unix_input_stream_new (output, FALSE);
	dstream = g_data_input_stream_new (stream);

	do {
		line = g_data_input_stream_read_line (dstream, &size, NULL, &error);

		if (!line) {
			if (error) {
				g_print (WARN_BEGIN "%s" WARN_END "\n", error->message);
				g_error_free (error);
			}
			break;
		}

		if (strstr (line, "Tracker") &&
		    ((urn && strstr (line, urn)) ||
		     (uri && strstr (line, uri)))) {
			g_fprintf (f, line);
		}

		g_free (line);
	} while (TRUE);

	g_spawn_close_pid (pid);
	g_object_unref (dstream);
	g_object_unref (stream);
	fclose (f);

	g_print (TITLE_BEGIN "%s" TITLE_END "\n", _("Done"));
}

static gboolean
answer_yes_no_question (const gchar *str)
{
	/* Translators: these are the replies to yes/no questions, the first
	 * char is used for "[Y/N]" hints on a command line tool, and is also
	 * used to match the response on user input.
	 */
	gchar *yes = N_("Yes"), *no = N_("No");
	gunichar yes_lower, yes_upper, no_lower, no_upper, c;
	gchar y[10], n[10], option[10];
	gssize len;

	c = g_utf8_get_char (_(yes));
	yes_upper = g_unichar_toupper (c);
	yes_lower = g_unichar_tolower (c);
	len = g_unichar_to_utf8 (yes_upper, y);
	y[len] = '\0';

	c = g_utf8_get_char (_(no));
	no_upper = g_unichar_toupper (c);
	no_lower = g_unichar_tolower (c);
	len = g_unichar_to_utf8 (no_upper, n);
	n[len] = '\0';

	while (TRUE) {
		g_print ("%s [%s/%s] ", str, y, n);
		scanf ("%10s", (gchar *) &option);
		c = g_utf8_get_char (option);

		if (c == yes_upper || c == yes_lower)
			return TRUE;
		else if (c == no_upper || c == no_lower)
			return FALSE;
	}

	return FALSE;
}

static TrackerSparqlConnection *
get_connection (void)
{
	TrackerSparqlConnection *connection;
	GError *error = NULL;

	connection = tracker_sparql_connection_get (NULL, &error);

	if (!connection) {
		g_printerr ("%s: %s\n",
		            _("Could not establish a connection to Tracker"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
	}

	return connection;
}

static void
save_sqlite_structure (const gchar *destdir,
		       const gchar *filename)
{
	TrackerDBInterface *iface;
	TrackerDBStatement *stmt;
	TrackerDBCursor *cursor = NULL;
	GError *error = NULL;
	gboolean first_time = FALSE;
	gchar *path;
	FILE *f;

	g_print (_("Saving SQLITE database schema... "));

	if (!tracker_data_manager_init (0, NULL, &first_time, FALSE, FALSE,
	                                100, 100, NULL, NULL, NULL, &error)) {
		g_print (WARN_BEGIN "%s" WARN_END "\n", error->message);
		g_error_free (error);
		return;
	}

	iface = tracker_db_manager_get_db_interface ();

	stmt = tracker_db_interface_create_statement (iface, TRACKER_DB_STATEMENT_CACHE_TYPE_NONE, &error,
	                                              "select sql from sqlite_master");

	if (stmt) {
		cursor = tracker_db_statement_start_cursor (stmt, &error);
	}

	if (error) {
		g_print (WARN_BEGIN "%s" WARN_END "\n", error->message);
		g_error_free (error);
		return;
	}

	path = g_build_filename (destdir, filename, NULL);
	f = fopen (path, "w");
	g_free (path);

	while (tracker_db_cursor_iter_next (cursor, NULL, &error)) {
		const gchar *str;

		str = tracker_db_cursor_get_string (cursor, 0, NULL);
		g_fprintf (f, "%s\n", str);
	}

	fclose (f);

	if (error) {
		g_print (WARN_BEGIN "%s" WARN_END "\n", error->message);
		g_error_free (error);
		return;
	}

	g_print (TITLE_BEGIN "%s" TITLE_END "\n", _("Done"));
}

static void
copy_file (const gchar *uri,
           const gchar *destdir,
           const gchar *filename)
{
	GFileInputStream *istream;
	GFileOutputStream *ostream;
	GFile *ifile, *ofile;
	gchar *dest, buf[1024];
	GError *error = NULL;
	gssize len;

	g_print (_("Copying file... "));

	dest = g_build_filename (destdir, filename, NULL);
	ifile = g_file_new_for_uri (uri);
	ofile = g_file_new_for_path (dest);
	g_free (dest);

	istream = g_file_read (ifile, NULL, &error);
	ostream = g_file_create (ofile, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error);
	g_object_unref (ifile);
	g_object_unref (ofile);

	if (!istream || !ostream) {
		g_print (WARN_BEGIN "%s" WARN_END "\n", error->message);
		g_clear_object (&istream);
		g_clear_object (&ostream);
		g_error_free (error);
		return;
	}

	while ((len = g_input_stream_read (G_INPUT_STREAM (istream), buf, sizeof (buf), NULL, NULL))) {
		buf[len] = '\0';
		g_output_stream_write (G_OUTPUT_STREAM (ostream), buf, len, NULL, NULL);
	}

	g_input_stream_close (G_INPUT_STREAM (istream), NULL, NULL);
	g_output_stream_close (G_OUTPUT_STREAM (ostream), NULL, NULL);

	g_print (TITLE_BEGIN "%s" TITLE_END "\n", _("Done"));
	g_object_unref (istream);
	g_object_unref (ostream);
}

static void
save_command_output (const gchar  *visible_name,
                     gchar       **argv,
                     const gchar  *destdir,
                     const gchar  *filename)
{
	gchar *dest, str[1024];
	GError *error = NULL;
	FILE *o, *d;
	gint output;
	GPid pid;

	/* Translators: This refers to command line tools we gather info from,
	 * meant to be the executable name only (eg. gvfs-info, file...).
	 */
	g_print (_("Saving %s output... "), visible_name);

	if (!g_spawn_async_with_pipes (NULL, argv, NULL,
	                               G_SPAWN_SEARCH_PATH |
	                               G_SPAWN_STDERR_TO_DEV_NULL,
	                               NULL, NULL, &pid,
	                               NULL, &output, NULL, &error)) {
		if (error) {
			g_print (WARN_BEGIN "%s" WARN_END "\n", error->message);
			g_error_free (error);
		}
		return;
	}

	dest = g_build_filename (destdir, filename, NULL);
	d = fopen (dest, "w");
	o = fdopen (output, "r");

	while (fgets (str, sizeof (str), o))
		fputs (str, d);

	g_print (TITLE_BEGIN "%s" TITLE_END "\n", _("Done"));

	fclose (d);
	fclose (o);
	g_free (dest);
}

static gboolean
compress_dir (const gchar  *dir,
              gchar       **dest_file)
{
	gchar *dest = g_strdup_printf ("%s.tar.xz", dir);
	gchar *str = g_strdup_printf ("tar -Jcf %s -C %s .", dest, dir);
	gchar *out, *err;
	gboolean success;
	gint status;

	success = g_spawn_command_line_sync (str, &out, &err, &status, NULL);
	g_free (out);
	g_free (err);
	g_free (str);

	if (!success || status != 0) {
		g_free (dest);
		return FALSE;
	}

	chmod (dest, 0700);

	*dest_file = dest;
	return TRUE;
}

static gboolean
check_file (GFile *file)
{
	GFileInfo *info;
	gssize fsize;

	info = g_file_query_info (file,
	                          G_FILE_ATTRIBUTE_STANDARD_SIZE ","
	                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL, NULL);

	if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR) {
		g_print (WARN_BEGIN "%s" WARN_END "\n", _("Not a regular file"));
		g_object_unref (info);
		return FALSE;
	}

	fsize = g_file_info_get_size (info);

	if (!bypass_limits && fsize > MAX_SIZE) {
		g_print (WARN_BEGIN);
		g_print (_("File size exceeds limit (%d MB)"), MAX_MEGS);
		g_print (WARN_END "\n");
		g_object_unref (info);
		return FALSE;
	}

	g_object_unref (info);
	return TRUE;
}

static void
extract_resource_info (const gchar *resource)
{
	TrackerSparqlConnection *connection;
	gchar *urn = NULL, *uri = NULL;
	gboolean check_uri_info = TRUE;
	gchar *dir, *compressed_file;
	GFile *file = NULL;

	connection = get_connection ();
	dir = g_strdup ("/tmp/tracker-collect-info-XXXXXX");
	g_mkdtemp_full (dir, 0700);

	if (!connection)
		return EXIT_FAILURE;

	if (strstr (resource, ":/")) {
		/* Looks like an uri */
		uri = g_strdup (resource);
	} else if (resource[0] == '/') {
		/* Looks like a path */
		file = g_file_new_for_path (resource);
		uri = g_file_get_uri (file);
	} else {
		/* Fallback to treating these as URNs */
		urn = g_strdup (resource);
	}

	if (!uri && urn)
		uri = query_urn_uri (connection, urn);
	else if (!urn && uri)
		urn = query_uri_urn (connection, uri);

	if (!file)
		file = g_file_new_for_uri (uri);

	g_print ("\n");
	g_print (_("Collecting info for '%s':\n"), uri ? uri : urn);
	g_print (_("Retrieving Tracker database information... "));

	if (!extract_urn_info (connection, urn, dir)) {
		g_print (WARN_BEGIN "%s" WARN_END "\n", _("Element not found"));
		g_object_unref (file);
		g_free (dir);
		return;
	} else {
		g_print (TITLE_BEGIN "%s" TITLE_END "\n", _("Done"));
	}

	save_sqlite_structure (dir, "sqlite-db-schemas.txt");
	extract_related_logs (urn, uri, dir, "journald-logs.txt");

	if (uri) {
		gchar *tracker_extract_argv[] = { "/usr/libexec/tracker-extract", "-v", "3", "-f", uri, NULL };
		gchar *q;

		save_command_output ("tracker-extract", tracker_extract_argv,
				     dir, "tracker-extract-output.txt");

		if (!check_file (file)) {
			check_uri_info = TRUE;
		} else {
			/* Translators: This refers to a file/uri */
			q = g_strdup_printf (_("Save copy of %s?"), uri);

			if (answer_yes_no_question (q)) {
				copy_file (uri, dir, "sample");
				check_uri_info = FALSE;
			}
		}
	} else {
		check_uri_info = FALSE;
	}

	if (check_uri_info) {
		gchar *path = g_file_get_path (file);
		gchar *gvfs_info_argv[] = { "gvfs-info", uri, NULL };
		gchar *file_argv[] = { "file", path, NULL };

		save_command_output ("gvfs-info", gvfs_info_argv,
		                     dir, "gvfs-info-output.txt");
		save_command_output ("file(1)", file_argv,
		                     dir, "file-output.txt");
		g_free (path);
	}

	g_print (_("Compressing collected information... "));

	if (!compress_dir (dir, &compressed_file)) {
		g_print (WARN_BEGIN "%s" WARN_END "\n", _("Error"));
		g_print ("\n");
		/* Translators: this refers to a directory path */
		g_print (_("Collected information left on %s"), dir);
		g_free (dir);
		return;
	} else {
		g_print (TITLE_BEGIN "%s" TITLE_END "\n", _("Done"));
	}

	g_print ("\n");
	/* Translators: placeholders respectively refer to
	 * a file path, and a bugzilla URL.
	 */
	g_print (_("Report file created at: %s\n"
	           "Please consider filing a bug at %s with this file attached."),
	         compressed_file, PACKAGE_BUGREPORT);
	g_print ("\n");

	g_object_unref (file);
	g_free (compressed_file);
	g_free (dir);
}

static void
process_log_failures (GArray *failures)
{
	FailureInfo *info;
	gint i;

	while (failures->len > 0) {
		g_print (ngettext ("Found %d possible failure:",
		                   "Found %d possible failures:",
		                   failures->len), failures->len);
		g_print ("\n");

		for (i = 0; i < failures->len; i++) {
			info = &g_array_index (failures, FailureInfo, i);
			g_print ("[%d]\t%s\n\t(%s)\n", i + 1, info->uri, info->urn);
		}

		g_print ("\n");
		g_print (_("Select number: "));
		scanf ("%d", &i);
		i--;

		if (i >= 0 && i < failures->len) {
			info = &g_array_index (failures, FailureInfo, i);
			extract_resource_info (info->urn);
			g_array_remove_index (failures, i);

			if (failures->len > 0) {
				g_print ("\n");
				if (!answer_yes_no_question (_("Continue with other failures?")))
					break;
			}
		}
	}

	if (failures->len == 0) {
		g_print (_("No Tracker failures found in logs"));
		g_print ("\n");
	}
}

static int
info_run (void)
{
	if (inspect_logs) {
		TrackerSparqlConnection *connection;
		GArray *failures;

		connection = get_connection ();
		failures = fetch_log_failures (connection);
		process_log_failures (failures);

		g_array_unref (failures);
	} else if (urns && *urns) {
		guint i;

		for (i = 0; urns[i]; i++)
			extract_resource_info (urns[i]);
	}

	g_print (_("Thanks for helping improve Tracker."));
	g_print ("\n\n");

	return EXIT_SUCCESS;
}

static int
info_run_default (void)
{
	GOptionContext *context;
	gchar *help;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);
	help = g_option_context_get_help (context, TRUE, NULL);
	g_option_context_free (context);
	g_printerr ("%s\n", help);
	g_free (help);

	return EXIT_FAILURE;
}

int
tracker_collect_bug_info (int argc, const char **argv)
{
	GOptionContext *context;
	GError *error = NULL;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);

	argv[0] = "tracker collect-info";

	if (!g_option_context_parse (context, &argc, (char***) &argv, &error)) {
		g_printerr ("%s, %s\n", _("Unrecognized options"), error->message);
		g_error_free (error);
		g_option_context_free (context);
		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	if (OPTIONS_ENABLED)
		return info_run ();
	else
		return info_run_default ();
}
