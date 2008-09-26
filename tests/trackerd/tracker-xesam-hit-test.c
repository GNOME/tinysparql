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
#define TEST(func) g_test_add ("/tracker/xesam/hit/"#func, Fixture, NULL, (void (*) (Fixture*, gconstpointer))setup, test_##func, teardown);

#define NUM_FIELDS _get_num_fields()

/* Fields set in the session's hit-fields property */
const gchar *TEST_FIELDS[2] =
{
//	"xesam:url",
//	"xesam:title",
//	"xesam:subject",
	"xesam:album",
	NULL
};

/* Test fixture passed to all tests */
typedef struct {
	GMainLoop		*mainloop;
	GError			*error;
	XesamGSearcher		*searcher;
	XesamGSession		*session;
	XesamGSearch		*search;
	XesamGHits		*hits;
} Fixture;

/* HELPER METHODS BEGIN */

static guint
_get_num_fields (void)
{
	int	i;
	static	guint num_fields = 0;

	if (num_fields != 0)
		return num_fields;

	for (i = 0; ; i++) {
		if (TEST_FIELDS[i] == NULL) {
			num_fields = i;
			return i;
		}
	}
}

/* HELPER METHODS END */

static void
setup (Fixture		 *fix,
	   GType *searcher_type)
{
	fix->mainloop = g_main_loop_new (NULL, FALSE);
	fix->searcher = XESAM_G_SEARCHER(xesam_g_dbus_searcher_new_default ());
	fix->session = xesam_g_session_new (fix->searcher);

	g_object_set (fix->session, "hit-fields", TEST_FIELDS, NULL);

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
test_field_data (Fixture	*fix,
	  gconstpointer	data)
{
	guint		hit_count, i, j;
	XesamGHit	*hit;
	const GPtrArray	*raw;

	hit_count = xesam_g_hits_get_count (fix->hits);
	g_assert_cmpint (hit_count, >, 0);

	for (i = 0; i < hit_count; i++) {
		hit = xesam_g_hits_get (fix->hits, i);
		g_assert (XESAM_IS_G_HIT (hit));

		raw = xesam_g_hit_get_data (hit);
		g_assert (raw != NULL);
		g_assert_cmpint (raw->len, ==, NUM_FIELDS);

		/* Assert that all field data members are valid GValues */
		for (j = 0; j < raw->len; j++) {
			g_assert (G_IS_VALUE(g_ptr_array_index(raw, j)));
		}
	}
}

static void
test_get_field (Fixture		*fix,
		gconstpointer	data)
{
	XesamGHit	*hit;
	const GValue	*field_value;
	guint		i;

	/* We just check the first Hit in the Hits object */
	hit = xesam_g_hits_get (fix->hits, 0);

	for (i = 0; i < NUM_FIELDS; i++) {
		field_value = xesam_g_hit_get_field (hit, TEST_FIELDS[i]);
		g_assert (G_IS_VALUE (field_value));
	}
}

static void
test_get_field_by_id (Fixture		*fix,
		      gconstpointer	data)
{
	XesamGHit	*hit;
	const GValue	*field_value_by_key, *field_value_by_id;
	guint		i;

	/* We just check the first Hit in the Hits object */
	hit = xesam_g_hits_get (fix->hits, 0);

	for (i = 0; i < NUM_FIELDS; i++) {
		field_value_by_key = xesam_g_hit_get_field (hit, TEST_FIELDS[i]);
		field_value_by_id = xesam_g_hit_get_field_by_id (hit, i);

		g_assert (field_value_by_key == field_value_by_id);
		g_assert (G_IS_VALUE (field_value_by_key));
		g_assert (G_IS_VALUE (field_value_by_id));
	}
}

static void
test_get_id (Fixture		*fix,
	     gconstpointer	data)
{
	XesamGHit	*hit;
	guint		i;
	guint count = xesam_g_hits_get_count (fix->hits);

	for (i = 0; i < count; i++) {
		hit = xesam_g_hits_get (fix->hits, i);

		/* This assumes that hit is the first XesamGHit to be delievered */
		g_assert_cmpint (i, ==, xesam_g_hit_get_id (hit));
	}
}

static void
test_get_field_names (Fixture		*fix,
		      gconstpointer	data)
{
	XesamGHit	*hit;
	GStrv		field_names;
	guint		i;

	hit = xesam_g_hits_get (fix->hits, 0);
	field_names = xesam_g_hit_get_field_names (hit);

	/* This also tests that both arrays ends with NULL */
	for (i = 0; i <= NUM_FIELDS; i++) {
		g_assert_cmpstr (TEST_FIELDS[i], ==, field_names[i]);
	}

	g_strfreev (field_names);
}


void
g_test_add_hit_tests (void)
{

	TEST (field_data);
	TEST (get_field);
	TEST (get_field_by_id);
	TEST (get_id);
	TEST (get_field_names);

	return;
}
