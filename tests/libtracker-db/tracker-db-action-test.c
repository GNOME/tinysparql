/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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
#include <libtracker-db/tracker-db-action.h>

static void
test_actiontype ()
{
        TrackerDBAction iter;
        
        for (iter = TRACKER_DB_ACTION_IGNORE; 
             iter != TRACKER_DB_ACTION_FORCE_REFRESH;
             iter++) {
                g_assert(tracker_db_action_to_string (iter));
        }
}

static void
test_is_delete ()
{
        TrackerDBAction iter;
        gint            counter = 0;

        /* Only for delete actions in the enum */
        for (iter = 0; iter < 23; iter++) {
                if (tracker_db_action_is_delete (iter)) {
                        counter++;
                }
        }
        g_assert_cmpint (counter, ==, 4);
}


int
main (int argc, char **argv)
{
	int result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/libtracker-db/tracker-db-action/to_string",
                         test_actiontype);
        g_test_add_func ("/libtracker-db/tracker-db-action/is_delete",
                         test_is_delete);

	result = g_test_run ();

	return result;
}
