/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
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

#include <libtracker-common/tracker-crc32.h>

// Using http://crc32-checksum.waraxe.us/ to check the result
static void
test_crc32_calculate ()
{
        guint32 result;
        guint32 expected = 0x81F8B2A3;

        result = tracker_crc32 ("Who is Meego? Meego is dead, baby. Meego is dead", 48);
        
        g_assert_cmpint (expected, ==, result);
}

gint
main (gint argc, gchar **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/libtracker-common/crc32/calculate",
                         test_crc32_calculate);

        return g_test_run ();
}
