/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
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
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-miner/tracker-file-system.h>

/* Fixture struct */
typedef struct {
	/* The filesystem to test */
	TrackerFileSystem *file_system;
} TestCommonContext;

#define test_add(path,fun)	  \
	g_test_add (path, \
	            TestCommonContext, \
	            NULL, \
	            test_common_context_setup, \
	            fun, \
	            test_common_context_teardown)

static void
test_common_context_setup (TestCommonContext *fixture,
                           gconstpointer      data)
{
	fixture->file_system = tracker_file_system_new (NULL);
}

static void
test_common_context_teardown (TestCommonContext *fixture,
                              gconstpointer      data)
{
	if (fixture->file_system)
		g_object_unref (fixture->file_system);
}

static void
test_file_system_insertions (TestCommonContext *fixture,
                             gconstpointer      data)
{
	GFile *file, *canonical, *other;

	file = g_file_new_for_uri ("file:///aaa/");
	canonical = tracker_file_system_peek_file (fixture->file_system, file);
	g_assert (canonical == NULL);

	canonical = tracker_file_system_get_file (fixture->file_system, file,
						  G_FILE_TYPE_DIRECTORY, NULL);
	g_object_unref (file);

	g_assert (canonical != NULL);

	file = g_file_new_for_uri ("file:///aaa/");
	other = tracker_file_system_get_file (fixture->file_system, file,
					      G_FILE_TYPE_DIRECTORY, NULL);
	g_assert (canonical == other);

	other = tracker_file_system_peek_file (fixture->file_system, file);
	g_object_unref (file);
	g_assert (other != NULL);
}

static void
test_file_system_children (TestCommonContext *fixture,
			   gconstpointer      data)
{
	GFile *file, *parent, *child, *other;

	file = g_file_new_for_uri ("file:///aaa/");
	parent = tracker_file_system_get_file (fixture->file_system, file,
						  G_FILE_TYPE_DIRECTORY, NULL);
	g_object_unref (file);

	file = g_file_new_for_uri ("file:///aaa/bbb");
	child = tracker_file_system_get_file (fixture->file_system, file,
					      G_FILE_TYPE_REGULAR, parent);
	g_assert (child != NULL);
	g_object_unref (file);

	file = g_file_new_for_uri ("file:///aaa/bbb");
	other = tracker_file_system_get_file (fixture->file_system, file,
					      G_FILE_TYPE_REGULAR, NULL);
	g_assert (other != NULL);
	g_assert (child == other);

	g_object_unref (file);
}

static void
test_file_system_indirect_children (TestCommonContext *fixture,
				    gconstpointer      data)
{
	GFile *file, *parent, *child, *other;

	file = g_file_new_for_uri ("file:///aaa/");
	parent = tracker_file_system_get_file (fixture->file_system, file,
					       G_FILE_TYPE_DIRECTORY, NULL);
	g_object_unref (file);

	file = g_file_new_for_uri ("file:///aaa/bbb/ccc");
	child = tracker_file_system_get_file (fixture->file_system, file,
					      G_FILE_TYPE_REGULAR, parent);
	g_assert (child != NULL);
	g_object_unref (file);

	file = g_file_new_for_uri ("file:///aaa/bbb/ccc");
	other = tracker_file_system_get_file (fixture->file_system, file,
					      G_FILE_TYPE_REGULAR, NULL);
	g_assert (other != NULL);
	g_assert (child == other);

	/* FIXME: check missing parent in between */

	g_object_unref (file);
}

static void
test_file_system_reparenting (TestCommonContext *fixture,
			      gconstpointer      data)
{
	GFile *file, *parent, *child, *grandchild, *other;

	file = g_file_new_for_uri ("file:///aaa/");
	parent = tracker_file_system_get_file (fixture->file_system, file,
					       G_FILE_TYPE_DIRECTORY, NULL);
	g_object_unref (file);

	file = g_file_new_for_uri ("file:///aaa/bbb/ccc");
	grandchild = tracker_file_system_get_file (fixture->file_system, file,
						   G_FILE_TYPE_REGULAR, parent);
	g_assert (grandchild != NULL);
	g_object_unref (file);

	file = g_file_new_for_uri ("file:///aaa/bbb");
	child = tracker_file_system_get_file (fixture->file_system, file,
					      G_FILE_TYPE_REGULAR, parent);
	g_assert (child != NULL);
	g_object_unref (file);

	file = g_file_new_for_uri ("file:///aaa/bbb/ccc");
	other = tracker_file_system_peek_file (fixture->file_system, file);
	g_assert (other != NULL);
	g_assert (grandchild == other);
	g_object_unref (file);

	/* Delete child in between */
	g_object_unref (child);

	/* Check that child doesn't exist anymore */
	file = g_file_new_for_uri ("file:///aaa/bbb");
	child = tracker_file_system_peek_file (fixture->file_system, file);
	g_assert (child == NULL);
	g_object_unref (file);

	/* Check that grand child still exists */
	file = g_file_new_for_uri ("file:///aaa/bbb/ccc");
	other = tracker_file_system_peek_file (fixture->file_system, file);
	g_assert (other != NULL);
	g_assert (grandchild == other);
	g_object_unref (file);
}

static void
test_file_system_properties (TestCommonContext *fixture,
			     gconstpointer      data)
{
	GQuark property1_quark, property2_quark;
	gchar *value = "value";
	gchar *ret_value;
	GFile *file, *f;

	property1_quark = g_quark_from_string ("file-system-test-property1");
	tracker_file_system_register_property (property1_quark,
					       NULL);
	property2_quark = g_quark_from_string ("file-system-test-property2");
	tracker_file_system_register_property (property2_quark,
					       NULL);

	f = g_file_new_for_uri ("file:///aaa/");
	file = tracker_file_system_get_file (fixture->file_system, f,
					     G_FILE_TYPE_REGULAR, NULL);
	g_object_unref (f);

	/* Set both properties */
	tracker_file_system_set_property (fixture->file_system, file,
					  property1_quark, value);
	tracker_file_system_set_property (fixture->file_system, file,
					  property2_quark, value);

	/* Check second property and remove it */
	ret_value = tracker_file_system_get_property (fixture->file_system,
						      file, property2_quark);
	g_assert (ret_value == value);

	tracker_file_system_unset_property (fixture->file_system,
					    file, property2_quark);

	ret_value = tracker_file_system_get_property (fixture->file_system,
						      file, property2_quark);
	g_assert (ret_value == NULL);

	/* Check first property and remove it */
	ret_value = tracker_file_system_get_property (fixture->file_system,
						      file, property1_quark);
	g_assert (ret_value == value);

	tracker_file_system_unset_property (fixture->file_system,
					    file, property1_quark);

	ret_value = tracker_file_system_get_property (fixture->file_system,
						      file, property1_quark);
	g_assert (ret_value == NULL);
}

gint
main (gint    argc,
      gchar **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing file system abstraction");

	test_add ("/libtracker-miner/file-system/insertions",
	          test_file_system_insertions);
	test_add ("/libtracker-miner/file-system/children",
	          test_file_system_children);
	test_add ("/libtracker-miner/file-system/indirect-children",
	          test_file_system_indirect_children);
	test_add ("/libtracker-miner/file-system/reparenting",
		  test_file_system_reparenting);
	test_add ("/libtracker-miner/file-system/file-properties",
	          test_file_system_properties);

	return g_test_run ();
}
