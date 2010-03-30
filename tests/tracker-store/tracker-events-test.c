/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
#include <glib.h>
#include <glib-object.h>
#include <libtracker-common/tracker-dbus.h>
#include <tracker-events.h>


#define EMAIL_CLASS "http://namespace1/nmo/Email"
#define FEED_CLASS "http://namespace1/nmo/FeedMessage"
#define CONTACT_CLASS "http://namespace2/nco/Contact"
#define NON_SIGNAL_CLASS "http://namespace100/myapp/MyClass"

static GPtrArray *types;

static GStrv
initialization_callback (void)
{
	gchar *klasses[] = {
		EMAIL_CLASS,
		FEED_CLASS,
		CONTACT_CLASS,
		NULL};

	return g_strdupv (klasses);
}

static void
test_events_empty (void)
{
	GPtrArray *events = NULL;

	tracker_events_init (initialization_callback);

	events = tracker_events_get_pending ();
	g_assert (events == NULL);

	tracker_events_reset ();

	tracker_events_shutdown ();
}

static void
test_events_reset (void)
{
	GPtrArray *events;

	tracker_events_init (initialization_callback);

	tracker_events_insert ("uri://1", EMAIL_CLASS, types,
	                       TRACKER_DBUS_EVENTS_TYPE_ADD);

	events = tracker_events_get_pending ();
	g_assert_cmpint (events->len, ==, 1);

	/* Try again without reset to ensure the data is
	 * still there
	 */
	events = tracker_events_get_pending ();
	g_assert_cmpint (events->len, ==, 1);

	tracker_events_reset ();
	tracker_events_shutdown ();
}

static void
test_events_insertions (void)
{
	GPtrArray *events;

	tracker_events_init (initialization_callback);

	tracker_events_insert ("uri://1", EMAIL_CLASS, types,
	                       TRACKER_DBUS_EVENTS_TYPE_ADD);

	tracker_events_insert ("uri://2", EMAIL_CLASS, types,
	                       TRACKER_DBUS_EVENTS_TYPE_UPDATE);

	tracker_events_insert ("uri://3", EMAIL_CLASS, types,
	                       TRACKER_DBUS_EVENTS_TYPE_DELETE);

	events = tracker_events_get_pending ();
	g_assert_cmpint (events->len, ==, 2);

	/* Insert class we dont want to signal */
	tracker_events_insert ("uri://x", NON_SIGNAL_CLASS, types,
	                       TRACKER_DBUS_EVENTS_TYPE_DELETE);

	events = tracker_events_get_pending ();
	g_assert_cmpint (events->len, ==, 2);

	tracker_events_reset ();
	tracker_events_shutdown ();
}

static void
test_events_no_allows (void)
{
	gint i;

	tracker_events_init (NULL);
	g_assert (tracker_events_get_pending () == NULL);
	tracker_events_reset ();
	g_assert (tracker_events_get_pending () == NULL);

	for (i = 0; i < 10; i++) {
		tracker_events_insert (g_strdup_printf ("uri://%d", i),
		                       EMAIL_CLASS, types,
		                       TRACKER_DBUS_EVENTS_TYPE_ADD);
	}

	g_assert (tracker_events_get_pending () == NULL);

	tracker_events_shutdown ();
}

static void
test_events_lifecycle (void)
{
	/* Shutdown - no init */
	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_events_shutdown ();
	}
	g_test_trap_assert_stderr ("*tracker_events already shutdown*");

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_events_get_pending ();
	}
	g_test_trap_assert_stderr ("*assertion `private != NULL' failed*");

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR)) {
		tracker_events_reset ();
	}
	g_test_trap_assert_stderr ("*assertion `private != NULL' failed*");
}

int
main (int    argc,
      char **argv)
{
	GStrv types_s;
	guint i;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	types = g_ptr_array_new ();

	types_s = initialization_callback ();

	for (i = 0; types_s[i];  i++) {
		g_ptr_array_add (types, types_s[i]);
	}

	g_test_add_func ("/tracker/tracker-indexer/tracker-events/empty",
	                 test_events_empty);
	g_test_add_func ("/tracker/tracker-indexer/tracker-events/reset",
	                 test_events_reset);
	g_test_add_func ("/tracker/tracker-indexer/tracker-events/insertions",
	                 test_events_insertions);
	g_test_add_func ("/tracker/tracker-indexer/tracker-events/no-allows",
	                 test_events_no_allows);
	g_test_add_func ("/tracker/tracker-indexer/tracker-events/lifecycle",
	                 test_events_lifecycle);

	g_ptr_array_free (types, TRUE);

	return g_test_run ();
}
