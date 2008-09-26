/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with main.c; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <glib.h>
#include <glib/gtestutils.h>
#include <glib-object.h>

#include <xesam-glib/xesam-glib.h>
#include <xesam-glib/xesam-g-searcher.h>
#include <xesam-glib/xesam-g-dbussearcher.h>
#include <xesam-glib/xesam-g-session.h>

#include "xesam/xesam-g-utils.h"
#include "xesam/xesam-g-testsearcher.h"
#include "xesam/gtestextensions.h"

#include "tracker-xesam-test.h"

/* Shortcut to defining a test */
#define TEST(func) g_test_add ("/tracker/xesam/session/"#func, Fixture, NULL, (void (*) (Fixture*, gconstpointer))setup, test_##func, teardown);


/* Test fixture passed to all tests */
typedef struct {
	GMainLoop			*mainloop;
	GError				*error;
	XesamGSearcher		*searcher;
	XesamGSession		*session;
} Fixture;

/* HELPER METHODS BEGIN */

/**
 * check_property
 * @session:
 * @property:
 *
 * Returns: %TRUE iff the property is available after atmost %TIMEOUT.
 */
static gboolean
check_property (XesamGSession *session, gchar *property)
{
	gchar *notify_signal;
	GParamSpec	  *tmp_pspec;

	notify_signal = g_strconcat ("notify::", property, NULL);

	if (xesam_g_session_test_property (session, property)) {
		g_free (notify_signal);
		return TRUE;
	}

	if (gtx_wait_for_signal (G_OBJECT(session), TIMEOUT,
							 notify_signal, &tmp_pspec)) {
		g_param_spec_unref (tmp_pspec);
		if (xesam_g_session_test_property (session, property)) {
			g_free (notify_signal);
			return TRUE;
		}
		g_free (notify_signal);
		return FALSE;

	}

	if (xesam_g_session_test_property (session, property)) {
		g_free (notify_signal);
		return TRUE;
	}

	g_critical ("'%s' emitted, but test_property() returns FALSE", notify_signal);
	g_free (notify_signal);
	return FALSE;
}

/* HELPER METHODS END */

static void
setup (Fixture		 *fix,
	   GType *searcher_type)
{
	fix->mainloop = g_main_loop_new (NULL, FALSE);
	fix->searcher = XESAM_G_SEARCHER(xesam_g_dbus_searcher_new_default ());
	fix->session = xesam_g_session_new (fix->searcher);

	g_assert (XESAM_IS_G_SEARCHER(fix->searcher));
	g_assert (XESAM_IS_G_SESSION(fix->session));
}

static void
teardown (Fixture		*fix,
		  gconstpointer test_data)
{
	/* Make sure we did not screw up the searcher and session in the test */
	g_assert (XESAM_IS_G_SEARCHER(fix->searcher));
	g_assert (XESAM_IS_G_SESSION(fix->session));

	g_main_loop_unref (fix->mainloop);
	g_object_unref (fix->searcher);
	g_object_unref (fix->session);

	if (fix->error)
		g_error_free (fix->error);
}

/* Test that a session becomes ready within a reasonable timeframe */
static void
test_ready (Fixture *fix,
			  gconstpointer	test_data)
{
	if (gtx_wait_for_signal (G_OBJECT(fix->session), TIMEOUT, "ready"))
		g_critical ("'ready' signal never emitted");

	if (!gtx_wait_for_signal (G_OBJECT(fix->session), TIMEOUT, "ready"))
		g_critical ("'ready' signal emitted twice");

	g_assert (xesam_g_session_is_ready (fix->session));
}

/* Test that we can call close immediately on a session */
static void
test_immediate_close (Fixture		*fix,
					  gconstpointer	test_data)
{
	xesam_g_session_close (fix->session);

	if (gtx_wait_for_signal (G_OBJECT(fix->session), TIMEOUT, "closed"))
		g_critical ("'closed' signal never emitted");

	g_assert (xesam_g_session_is_closed (fix->session));
}

/* Test that we can call close multiple times without problems */
static void
test_multiple_close (Fixture		*fix,
					 gconstpointer	test_data)
{
	/* The first close() should go well */
	xesam_g_session_close (fix->session);
	if (gtx_wait_for_signal (G_OBJECT(fix->session), TIMEOUT, "closed"))
		g_critical ("'closed' signal never emitted");
	g_assert (xesam_g_session_is_closed (fix->session));

	/* Next close should not emit 'closed' */
	xesam_g_session_close (fix->session);
	if (!gtx_wait_for_signal (G_OBJECT(fix->session), TIMEOUT, "closed"))
		g_critical ("'closed' signal emitted, but session already closed");
	g_assert (xesam_g_session_is_closed (fix->session));

	/* Even more closing should not emit 'closed' */
	xesam_g_session_close (fix->session);
	if (!gtx_wait_for_signal (G_OBJECT(fix->session), TIMEOUT, "closed"))
		g_critical ("'closed' signal emitted, but session already closed");
	g_assert (xesam_g_session_is_closed (fix->session));
}

/* Test that we can call sync on all props */
static void
test_sync_properties (Fixture *fix,
					 gconstpointer test_data)
{
	XesamGSession *session;

	session = fix->session;

	xesam_g_session_sync_property (session, "search-live");
	if (!check_property (session, "search-live"))
		g_critical ("search-live not synced");

	xesam_g_session_sync_property (session, "hit-fields");
	if (!check_property(session, "hit-fields"))
		g_critical ("hit-fields not synced");

	xesam_g_session_sync_property (session, "hit-fields-extended");
	if (!check_property(session, "hit-fields-extended"))
		g_critical ("hit-fields-extended not synced");

	xesam_g_session_sync_property (session, "hit-snippet-length");
	if (!check_property(session, "hit-snippet-length"))
		g_critical ("hit-snippet-length not synced");

	xesam_g_session_sync_property (session, "sort-primary");
	if (!check_property(session, "sort-primary"))
		g_critical ("sort-primary not synced");

	xesam_g_session_sync_property (session, "sort-secondary");
	if (!check_property(session, "sort-secondary"))
		g_critical ("sort-secondary not synced");

	xesam_g_session_sync_property (session, "sort-order");
	if (!check_property(session, "sort-order"))
		g_critical ("sort-order not synced");

	xesam_g_session_sync_property (session, "vendor-id");
	if (!check_property(session, "vendor-id"))
		g_critical ("vendor-id not synced");

	xesam_g_session_sync_property (session, "vendor-version");
	if (!check_property(session, "vendor-version"))
		g_critical ("vendor-version not synced");

	xesam_g_session_sync_property (session, "vendor-display");
	if (!check_property(session, "vendor-display"))
		g_critical ("vendor-display not synced");

	xesam_g_session_sync_property (session, "vendor-xesam");
	if (!check_property(session, "vendor-xesam"))
		g_critical ("vendor-xesam not synced");

	xesam_g_session_sync_property (session, "vendor-ontology-fields");
	if (!check_property(session, "vendor-ontology-fields"))
		g_critical ("vendor-ontology-fields not synced");

	xesam_g_session_sync_property (session, "vendor-ontology-contents");
	if (!check_property(session, "vendor-ontology-contents"))
		g_critical ("vendor-ontology-ceontents not synced");

	xesam_g_session_sync_property (session, "vendor-ontology-sources");
	if (!check_property(session, "vendor-ontology-sources"))
		g_critical ("vendor-ontology-sources not synced");

	xesam_g_session_sync_property (session, "vendor-extensions");
	if (!check_property(session, "vendor-extensions"))
		g_critical ("vendor-extensions not synced");

	xesam_g_session_sync_property (session, "vendor-ontologies");
	if (!check_property(session, "vendor-ontologies"))
		g_critical ("vendor-ontologies not synced");

	xesam_g_session_sync_property (session, "vendor-maxhits");
	if (!check_property(session, "vendor-maxhits"))
		g_critical ("vendor-maxhits not synced");

}

/* Test that we can create a new Search object. Testing the functionality
 * of the spawned search is done in the test-search.c test suite. */
static void
test_immediate_new_search (Fixture *fix,
						   gconstpointer test_data)
{
	XesamGSearch	*search;

	/* Don't bother to wait for 'ready'. This should work regardless. */
	search = xesam_g_session_new_search_from_text (fix->session,
												   "hello world");

	g_assert (XESAM_IS_G_SEARCH (search));
	g_assert (xesam_g_search_get_session (search) == fix->session);

	g_object_unref (search);

	/* search will not be finalized before it has received a handle */
	gtx_yield_main_loop (TIMEOUT);
}


static void
test_field_map (Fixture	*fix,
		gconstpointer	test_data)
{
	GHashTable  *map;
	GParamSpec  *pspec;

	if (gtx_wait_for_signal (G_OBJECT(fix->session),TIMEOUT, "ready"))
		g_critical ("'ready' never emitted on session");

	map = xesam_g_session_get_field_map (fix->session);
	g_assert (map != NULL);
	g_assert_cmpint (g_hash_table_size (map), == , 1);
	g_assert_cmpint (*((int*)g_hash_table_lookup (map, "xesam:url")), ==, 0);

	/* Update hit-fields */
	gchar *hit_fields[4] = {"xesam:url", "xesam:title", "xesam:subject", NULL};
	g_object_set (fix->session, "hit-fields", hit_fields, NULL);

	/* Wait for propoerty change to be registered remotely */
	if (gtx_wait_for_signal (G_OBJECT(fix->session),TIMEOUT,
				 "notify::hit-fields", &pspec))
		g_critical ("'notify::hit-fields' never emitted on session");

	g_param_spec_unref (pspec);

	map = xesam_g_session_get_field_map (fix->session);
	g_assert_cmpint (g_hash_table_size (map), == , 3);
	g_assert_cmpint (*((int*)g_hash_table_lookup (map, "xesam:url")), ==, 0);
	g_assert_cmpint (*((int*)g_hash_table_lookup (map, "xesam:title")), ==, 1);
	g_assert_cmpint (*((int*)g_hash_table_lookup (map, "xesam:subject")), ==, 2);
}


void
g_test_add_session_tests (void)
{
	TEST (ready);
	TEST (immediate_close);
	TEST (multiple_close);
	TEST (sync_properties);
	TEST (immediate_new_search);
	TEST (field_map);

	return;
}
