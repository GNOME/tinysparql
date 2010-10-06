/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <errno.h>
#include <glib.h>
#include <tracker-sparql.h>

#define COPY_TIMEOUT_MS 100

static gint copy_rate = 1024; /* 1MByte/second */
static gint n_copies = 1;
static gchar **remaining;
static GFile *file;
static gchar *file_uri;
static GFile *destdir;
static gchar *destdir_uri;
static gboolean use_hidden;

/* copy_rate*1024*COPY_TIMEOUT_MS/1000 */
static gint timeout_copy_rate;

static gchar *buffer;
static gsize  buffer_size;

static GMainLoop *loop;
static GList *task_list;
static GList *task_list_li;

/* Note: don't use a GOutputStream, as that actually
 * creates a hidden temporary file */

typedef struct {
	GFile *destfile;
	GFile *destfile_hidden;
	FILE *fp;
	gsize bytes_copied;
	gsize bytes_remaining;
} CopyTask;

static GOptionEntry entries[] = {
	{ "rate", 'r', 0, G_OPTION_ARG_INT, &copy_rate,
	  "Rate of copy, in KBytes per second",
	  "1024"
	},
	{ "copies", 'c', 0, G_OPTION_ARG_INT, &n_copies,
	  "Number of copies to be done",
	  "1"
	},
	{ "hidden", 'h', 0, G_OPTION_ARG_NONE, &use_hidden,
	  "Use a hidden temp file while copying",
	  NULL,
	},
	{ G_OPTION_REMAINING, 0, 0,
	  G_OPTION_ARG_STRING_ARRAY, &remaining,
	  "file destdir",
	  "FILE DESTDIR",
	},
	{ NULL }
};

static void
update_store (GFile *destfile)
{
	gchar *sparql;
	GError *error = NULL;
	TrackerSparqlConnection *connection;
	gchar *uri;

	uri = g_file_get_uri (destfile);
	sparql = g_strdup_printf ("DELETE {?file a rdfs:Resource} "
	                          "WHERE {?file nie:url <%s> } "
	                          "INSERT { _:x a nfo:FileDataObject;"
	                          "             nie:url <%s> }",
	                          uri, uri);

	g_print ("  Updating store with new resource '%s'\n", uri);

	/* Get connection */
	connection = tracker_sparql_connection_get (NULL, &error);

	/* Run a synchronous update query */
	tracker_sparql_connection_update (connection,
	                                  sparql,
	                                  G_PRIORITY_DEFAULT,
	                                  NULL,
	                                  &error);

	if (error) {
		/* Some error happened performing the query, not good */
		g_error ("Couldn't update store for '%s': %s",
		         uri,
		         error ? error->message : "unknown error");
	}

	g_free (uri);
	g_free (sparql);
	g_object_unref (connection);
}

static gboolean
context_init (gint    argc,
              gchar **argv)
{
	GOptionContext *context;
	gint n_remaining;
	gchar *file_path;
	GError *error = NULL;

	/* Setup context */
	context = g_option_context_new ("- Simulate MTP daemon");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* Check input arguments */
	n_remaining = remaining ? g_strv_length (remaining) : 0;
	if (n_remaining != 2) {
		g_printerr ("You must provide FILE and DESTDIR\n");
		return FALSE;
	}

	/* Get and check input file */
	file = g_file_new_for_commandline_arg (remaining[0]);
	file_uri = g_file_get_uri (file);
	file_path = g_file_get_path (file);
	if (g_file_query_file_type (file,
	                            G_FILE_QUERY_INFO_NONE,
	                            NULL) !=  G_FILE_TYPE_REGULAR) {
		g_printerr ("File '%s' is not a valid regular file\n",
		            file_uri);
		return FALSE;
	}

	/* Get destination directory */
	destdir = g_file_new_for_commandline_arg (remaining[1]);
	destdir_uri = g_file_get_uri (destdir);
	if (g_file_query_file_type (destdir,
	                            G_FILE_QUERY_INFO_NONE,
	                            NULL) !=  G_FILE_TYPE_DIRECTORY) {
		g_printerr ("Destination path '%s' is not a valid directory\n",
		            destdir_uri);
		return FALSE;
	}

	/* Check n_copies */
	if (n_copies == 0) {
		g_printerr ("Number of copies must be greater than 0\n");
		return FALSE;
	}

	/* Check rate */
	if (copy_rate == 0) {
		g_printerr ("Copy rate must be greater than 0\n");
		return FALSE;
	}

	/* copy_rate*1024*COPY_TIMEOUT_MS/1000 */
	timeout_copy_rate = copy_rate * 1024.0 * COPY_TIMEOUT_MS / 1000.0;

	/* Read all input file contents */
	if (!g_file_get_contents (file_path,
	                          &buffer,
	                          &buffer_size,
	                          &error)) {
		g_error ("Couldn't load file '%s' contents: %s",
		         file_uri,
		         error ? error->message : "unknown error");
	}

	g_print ("\
Simulating MTP daemon with:\n\
  * File:    %s (%" G_GSIZE_FORMAT " bytes)\n\
  * Destdir: %s\n\
  * Copies:  %d\n\
  * Rate:    %d KBytes/s (%d bytes every %d ms)\n",
	         file_uri,
	         buffer_size,
	         destdir_uri,
	         n_copies,
	         copy_rate,
	         timeout_copy_rate,
	         COPY_TIMEOUT_MS);

	return TRUE;
}

static void
context_deinit (void)
{
	g_object_unref (file);
	g_free (file_uri);
	g_object_unref (destdir);
	g_free (destdir_uri);
	g_free (buffer);
}

static gboolean
task_run_cb (gpointer data)
{
	CopyTask *current;
	gsize n_write;

	/* Get current task */
	current = task_list_li ? task_list_li->data : NULL;

	/* Stop looping? */
	if (!current) {
		g_print ("\n\nNo more tasks to run, finishing...\n");
		g_main_loop_quit (loop);
		return FALSE;
	}

	/* If we just started copying it... */
	if (!current->fp) {
		gchar *destfile_path;

		g_print ("Running new copy task...\n");

		destfile_path = (use_hidden ?
		                 g_file_get_path (current->destfile_hidden) :
		                 g_file_get_path (current->destfile));

		/* Get file pointer */
		current->fp = fopen (destfile_path, "w");
		if (!current->fp)
		{
			g_error ("Couldn't get file pointer: %s",
			         g_strerror (errno));
		}

		/* Create new item in the store right away */
		update_store (use_hidden ?
		              current->destfile_hidden :
		              current->destfile);
		g_free (destfile_path);
	}

	/* Copy bytes */
	n_write = MIN (current->bytes_remaining, timeout_copy_rate);
	if (fwrite (&buffer[current->bytes_copied],
	            1,
	            n_write,
	            current->fp) != n_write) {
		g_error ("Couldn't write in output file: %s",
		         g_strerror (errno));
	}

	current->bytes_remaining -= n_write;
	current->bytes_copied += n_write;

	/* Finished with this task? */
	if (current->bytes_remaining == 0) {
		fclose (current->fp);
		current->fp = NULL;

		if (use_hidden) {
			GError *error = NULL;

			/* Copying finished, now MOVE to the final path */
			if (!g_file_move (current->destfile_hidden,
			                  current->destfile,
			                  G_FILE_COPY_OVERWRITE,
			                  NULL,
			                  NULL,
			                  NULL,
			                  &error)) {
				g_error ("Couldn't copy file to the final destination: %s",
				         error ? error->message : "unknown error");
			}
		}

		/* Setup next task */
		task_list_li = g_list_next (task_list_li);
	}

	return TRUE;
}

static void
setup_tasks (void)
{
	gint i;
	gchar *input_file_basename;

	input_file_basename = g_file_get_basename (file);

	for (i=n_copies-1; i>=0; i--) {
		CopyTask *task;
		gchar *basename;

		basename = g_strdup_printf ("file-copy-%d-%s",
		                            i,
		                            input_file_basename);

		task = g_new0 (CopyTask, 1);
		task->destfile = g_file_get_child (destdir, basename);
		task->bytes_remaining = buffer_size;

		if (use_hidden) {
			gchar *basename_hidden;

			basename_hidden = g_strdup_printf (".mtp-dummy.file-copy-%d-%s",
			                                   i,
			                                   input_file_basename);
			task->destfile_hidden = g_file_get_child (destdir, basename_hidden);
			g_free (basename_hidden);
		}

		task_list = g_list_prepend (task_list, task);

		g_free (basename);
	}

	/* Setup first task */
	task_list_li = task_list;

	/* Timeout every N milliseconds */
	g_timeout_add (COPY_TIMEOUT_MS,
	               task_run_cb,
	               NULL);
}

static void
check_duplicates_for_uri (const gchar *uri)
{
	GError *error = NULL;
	TrackerSparqlConnection *connection;
	TrackerSparqlCursor *cursor;
	gchar *sparql;

	sparql = g_strdup_printf ("SELECT ?u WHERE { ?u nie:url <%s> }",
	                          uri);

	/* As we know only read-only queries will be done, it's enough
	 * to use a connection with only direct-access setup.
	 */
	connection = tracker_sparql_connection_get_direct (NULL, &error);
	if (!connection) {
		g_error ("Couldn't obtain a direct connection to the Tracker store: %s",
		         error ? error->message : "unknown error");
	}

	/* Make a synchronous query to the store */
	cursor = tracker_sparql_connection_query (connection,
	                                          sparql,
	                                          NULL,
	                                          &error);
	if (error) {
		/* Some error happened performing the query, not good */
		g_error ("Couldn't query the Tracker Store: '%s'",
		         error ? error->message : "unknown error");
	}

	/* Check results... */
	if (!cursor) {
		g_print ("  For '%s' found 0 results!\n", uri);
	} else {
		gint i = 0;

		g_print ("  For '%s' found:\n", uri);
		/* Iterate, synchronously, the results... */
		while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
			g_print ("    [%d]: %s\n",
			         i++,
			         tracker_sparql_cursor_get_string (cursor, 0, NULL));
		}
		g_print ("    A total of '%d' results were found\n", i);

		g_object_unref (cursor);
	}

	g_object_unref (connection);
}

static void
check_duplicates (void)
{

	g_print ("\nChecking duplicates...\n");

	task_list_li = task_list;

	while (task_list_li) {
		CopyTask *current;
		gchar *uri;

		current = task_list_li->data;
		uri = g_file_get_uri (current->destfile);

		check_duplicates_for_uri (uri);

		g_free (uri);
		g_object_unref (current->destfile);
		g_free (current);
		task_list_li = g_list_next (task_list_li);;
	}

	g_list_free (task_list);
}

int main (int argc, char **argv)
{
	/* Initialize stuff */
	g_thread_init (NULL);
	g_type_init ();

	/* Initialize context */
	if (!context_init (argc, argv)) {
		g_printerr ("Couldn't setup context, exiting.");
		return -1;
	}

	/* Setup tasks */
	setup_tasks ();

	/* Run */
	g_print ("\nStarting...\n\n");
	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	/* Check for duplicates and cleanup copied files */
	check_duplicates ();

	context_deinit ();
	return 0;
}

