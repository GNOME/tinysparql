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
#include <tracker-sparql.h>

typedef struct {
        const gchar *input ;
        const gchar *output;
} ESCAPE_TEST_DATA;

ESCAPE_TEST_DATA  test_data []  = {
        {"SELECT \"a\"", "SELECT \\\"a\\\""},
        {"SELECT ?u \t \n \r \b \f", "SELECT ?u \\t \\n \\r \\b \\f"},
        {NULL, NULL }
};

static void
test_tracker_sparql_escape_string ()
{
        gint i;
        gchar *result;

        for (i = 0; test_data[i].input != NULL; i++) {
                result = tracker_sparql_escape_string (test_data[i].input);
                g_assert_cmpstr (result, ==, test_data[i].output);
                g_free (result);
        }
}

static void
test_tracker_sparql_escape_uri_vprintf ()
{
        gchar *result;

        result = tracker_sparql_escape_uri_printf ("test:uri:contact-%d", 14, NULL);
        g_assert_cmpstr (result, ==, "test:uri:contact-14");
        g_free (result);
}

gint
main (gint argc, gchar **argv)
{
        g_type_init ();
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/libtracker-sparql/tracker/tracker_sparql_escape_string", test_tracker_sparql_escape_string);
        g_test_add_func ("/libtracker-sparql/tracker/tracker_sparql_escape_uri_vprintf",
                         test_tracker_sparql_escape_uri_vprintf);

        return g_test_run ();
}
