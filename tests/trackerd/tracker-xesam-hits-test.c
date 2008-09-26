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
#include "xesam/xesam-g-utils.h"
#include <xesam-glib/xesam-g-searcher.h>
#include <xesam-glib/xesam-g-dbussearcher.h>
#include <xesam-glib/xesam-g-session.h>
#include <xesam-glib/xesam-g-search.h>
#include "xesam/xesam-g-globals-private.h"
#include "xesam/xesam-g-debug-private.h"
#include "xesam/xesam-g-testsearcher.h"
#include "xesam/gtestextensions.h"

#include "tracker-xesam-test.h"

/* Shortcut to defining a test */
#define TEST(func) g_test_add ("/tracker/xesam/hits/"#func, Fixture, NULL, (void (*) (Fixture*, gconstpointer))setup, test_##func, teardown);


/* Test fixture passed to all tests */
typedef struct {
	GMainLoop		*mainloop;
	GError			*error;
	XesamGSearcher		*searcher;
	XesamGSession		*session;
	XesamGSearch		*search;
	XesamGHits		*hits;
	GList			*hit_list;
} Fixture;

/* Fields set in the session's hit-fields property */
const gchar *HITS_TEST_FIELDS[2] =
{
//	"xesam:url",
//	"xesam:title",
//	"xesam:subject",
	"xesam:album",
	NULL
};


/* HELPER METHODS BEGIN */



/* HELPER METHODS END */

static void
setup (Fixture		 *fix,
	   GType *searcher_type)
{
	fix->mainloop = g_main_loop_new (NULL, FALSE);
	fix->searcher = XESAM_G_SEARCHER(xesam_g_dbus_searcher_new_default ());
	fix->session = xesam_g_session_new (fix->searcher);

	g_object_set (fix->session, "hit-fields", HITS_TEST_FIELDS, NULL);

	fix->search = xesam_g_session_new_search (fix->session, xesam_g_query_new_from_xml(TEST_XML));

	g_assert (XESAM_IS_G_SEARCHER (fix->searcher));
	g_assert (XESAM_IS_G_SESSION (fix->session));
	g_assert (XESAM_IS_G_SEARCH (fix->search));

	xesam_g_search_start (fix->search);
	if (gtx_wait_for_signal(G_OBJECT(fix->search), TIMEOUT, "hits-added",
				&fix->hits))
		g_critical ("Test search '%s' did not spawn any hits",
			    TEST_XML);

	g_assert (XESAM_IS_G_HITS (fix->hits));
	g_assert_cmpint (xesam_g_hits_get_count (fix->hits), >, 0);
}

static void
teardown (Fixture		*fix,
		  gconstpointer test_data)
{
	if (fix->hit_list)
		g_list_foreach (fix->hit_list, (GFunc) g_object_unref, NULL);

	/* Make sure we did not screw up the searcher and session in the test */
	g_assert (XESAM_IS_G_SEARCHER(fix->searcher));
	g_assert (XESAM_IS_G_SESSION(fix->session));
	g_assert (XESAM_IS_G_SEARCH(fix->search));
	g_assert (XESAM_IS_G_HITS(fix->hits));

	/* Allow to process any dangling async calls. This is needed
	 * to make the leak detecction below work. */
	gtx_yield_main_loop (TIMEOUT);

	g_main_loop_unref (fix->mainloop);
	gtx_assert_last_unref (fix->hits);
	gtx_assert_last_unref (fix->search);
	gtx_assert_last_unref (fix->session);

	/* Allow to process any dangling async calls. This is needed
	 * to make the leak detecction below work. Yes, we need it two times */
	gtx_yield_main_loop (TIMEOUT);

	gtx_assert_last_unref (fix->searcher);

	if (fix->error)
		g_error_free (fix->error);
}

static void
test_count (Fixture	    *fix,
	     gconstpointer  data)
{
	guint hit_count;

	hit_count = xesam_g_hits_get_count (fix->hits);

	g_assert_cmpint (hit_count, >, 0);
}

static void
test_get (Fixture	*fix,
	  gconstpointer	data)
{
	guint		hit_count, i;
	XesamGHit	*hit;

	hit_count = xesam_g_hits_get_count (fix->hits);
	g_assert_cmpint (hit_count, >, 0);

	for (i = 0; i < hit_count; i++) {
		hit = xesam_g_hits_get (fix->hits, i);
		g_assert (XESAM_IS_G_HIT (hit));

		/* Build a list of hit objects with refs. We use this
		 * to assert that they are properly freed later */
		fix->hit_list = g_list_prepend (fix->hit_list, hit);
		g_object_ref (hit);
	}
}

void
g_test_add_hits_tests (void)
{
	TEST (count);
	TEST (get);

	return;
}
