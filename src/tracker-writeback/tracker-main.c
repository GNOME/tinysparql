/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <locale.h>

#include <dbus/dbus-glib.h>

#include <glib/gi18n.h>

#include <libtracker-common/tracker-log.h>

#include "tracker-writeback-dispatcher.h"
#include "tracker-writeback-consumer.h"
#include "tracker-config.h"

#define ABOUT	  \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE	  \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public " \
	"License which can be viewed at:\n" \
	"\n" \
	"  http://www.gnu.org/licenses/gpl.txt\n"

static gboolean      version;
static gint          verbosity = -1;

static GOptionEntry  entries[] = {
	{ "version", 'V', 0,
	  G_OPTION_ARG_NONE, &version,
	  N_("Displays version information"),
	  NULL },
	{ "verbosity", 'v', 0,
	  G_OPTION_ARG_INT, &verbosity,
	  N_("Logging, 0 = errors only, "
	     "1 = minimal, 2 = detailed and 3 = debug (default=0)"),
	  NULL },
	{ NULL }
};

typedef struct {
	gint    subject;
	GArray *rdf_types;
} WritebackData;

static TrackerWritebackConsumer *consumer = NULL;
static TrackerWritebackDispatcher *dispatcher = NULL;
static GMainContext *dispatcher_context = NULL;

static WritebackData *
writeback_data_new (gint    subject,
                    GArray *rdf_types)
{
	WritebackData *data;
	guint i;

	data = g_slice_new (WritebackData);
	data->subject = subject;

	data->rdf_types = g_array_sized_new (FALSE, FALSE, sizeof (gint), rdf_types->len);

	for (i = 0; i < rdf_types->len; i++) {
		gint id = g_array_index (rdf_types, gint, i);
		g_array_append_val (data->rdf_types, id);
	}

	return data;
}

static void
writeback_data_free (WritebackData *data)
{
	g_array_free (data->rdf_types, TRUE);
	g_slice_free (WritebackData, data);
}

/* This function will be executed in the main thread */
static gboolean
on_writeback_idle_cb (gpointer user_data)
{
	WritebackData *data = user_data;

	g_message ("Main thread (%p) got signaled of writeback petition", g_thread_self ());

	tracker_writeback_consumer_add_subject (consumer, data->subject, data->rdf_types);

	writeback_data_free (data);

	return FALSE;
}

/* This callback run in the dispatcher thread */
static void
on_writeback_cb (TrackerWritebackDispatcher *dispatcher,
                 gint                        subject,
                 GArray                     *rdf_types)
{
	WritebackData *data;

	g_message ("Got writeback petition on thread '%p' for subject '%d'",
	           g_thread_self (), subject);

	data = writeback_data_new (subject, rdf_types);
	g_idle_add_full (G_PRIORITY_HIGH_IDLE,
	                 on_writeback_idle_cb,
	                 data, NULL);
}

static gpointer
dispatcher_thread_func (gpointer data)
{
	GMainLoop *loop;

	g_message ("DBus Dispatcher thread created: %p", g_thread_self ());

	g_signal_connect (dispatcher, "writeback",
	                  G_CALLBACK (on_writeback_cb), NULL);

	loop = g_main_loop_new (dispatcher_context, FALSE);
	g_main_loop_run (loop);

	g_object_unref (dispatcher);
	g_main_loop_unref (loop);
	g_main_context_unref (dispatcher_context);

	return NULL;
}

static void
sanity_check_option_values (TrackerConfig *config)
{
	g_message ("General options:");
	g_message ("  Verbosity  ............................  %d",
	           tracker_config_get_verbosity (config));
}

int
main (int   argc,
      char *argv[])
{
	TrackerConfig *config;
	GOptionContext *context;
	GMainLoop *loop;
	GError *error = NULL;
	gchar *log_filename;

	g_thread_init (NULL);
	dbus_g_thread_init ();

	g_type_init ();

	/* Set up locale */
	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the
	 * usage string - Usage: COMMAND <THIS_MESSAGE>
	 */
	context = g_option_context_new (_("- start the tracker writeback service"));

	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);

	if (version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		return EXIT_SUCCESS;
	}

	/* Initialize logging */
	config = tracker_config_new ();

	if (verbosity > -1) {
		tracker_config_set_verbosity (config, verbosity);
	}

	tracker_log_init (tracker_config_get_verbosity (config),
	                  &log_filename);
	g_print ("Starting log:\n  File:'%s'\n", log_filename);
	g_free (log_filename);

	sanity_check_option_values (config);


	consumer = tracker_writeback_consumer_new ();

	/* Create dispatcher thread data here, GType
	 * initialization for boxed types don't seem
	 * to be threadsafe, this is troublesome with
	 * signals initialization. */

	dispatcher_context = g_main_context_new ();
	dispatcher = tracker_writeback_dispatcher_new (dispatcher_context,
	                                               &error);

	if (error) {
		g_critical ("Error creating dispatcher: %s", error->message);
		g_error_free (error);

		return EXIT_FAILURE;
	}

	g_thread_create (dispatcher_thread_func, dispatcher, FALSE, &error);

	if (error) {
		g_critical ("Error creating dispatcher thread: %s", error->message);
		g_error_free (error);

		return EXIT_FAILURE;
	}

	g_message ("Main thread is: %p", g_thread_self ());

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);

	tracker_log_shutdown ();

	g_object_unref (consumer);
	g_main_loop_unref (loop);

	g_object_unref (config);

	return EXIT_SUCCESS;
}
