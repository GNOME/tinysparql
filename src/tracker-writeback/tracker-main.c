/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia
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

#include "tracker-writeback-dispatcher.h"
#include "tracker-writeback-consumer.h"

typedef struct {
	gchar *subject;
	GStrv  rdf_types;
} WritebackData;

static TrackerWritebackConsumer *consumer = NULL;
static TrackerWritebackDispatcher *dispatcher = NULL;
static GMainContext *dispatcher_context = NULL;

static WritebackData *
writeback_data_new (const gchar *subject,
		    const GStrv  rdf_types)
{
	WritebackData *data;

	data = g_slice_new (WritebackData);
	data->subject = g_strdup (subject);
	data->rdf_types = g_strdupv (rdf_types);

	return data;
}

static void
writeback_data_free (WritebackData *data)
{
	g_free (data->subject);
	g_strfreev (data->rdf_types);
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
                 const gchar                *subject,
                 const GStrv                 rdf_types)
{
	WritebackData *data;

	g_message ("Got writeback petition on thread '%p' for subject '%s'",
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

int
main (int   argc,
      char *argv[])
{
	GMainLoop *loop;
	GError *error = NULL;

	g_thread_init (NULL);
	dbus_g_thread_init ();

	g_type_init ();

	consumer = tracker_writeback_consumer_new ();

	/* Create dispatcher thread data here, GType
	 * initialization for boxed types don't seem
	 * to be threadsafe, this is troublesome with
	 * signals initialization. */

	dispatcher_context = g_main_context_new ();
	dispatcher = tracker_writeback_dispatcher_new (dispatcher_context);

	g_thread_create (dispatcher_thread_func, dispatcher, FALSE, &error);

	if (error) {
		g_critical ("Error creating dispatcher thread: %s", error->message);
		g_error_free (error);

		return EXIT_FAILURE;
	}

	g_message ("Main thread is: %p", g_thread_self ());

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);

	g_object_unref (consumer);
	g_main_loop_unref (loop);

	return EXIT_SUCCESS;
}
