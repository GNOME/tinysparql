/* Tracker Extract - extracts embedded metadata from files
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef _TRACKER_EXTRACT_H_
#define _TRACKER_EXTRACT_H_

#include <glib.h>


typedef enum {
        TIME = 0,       /* hh:mm:ss (seconds are optionals) */
        TIMEZONE,       /* time added to current time */
        DAY_PART,       /* AM or PM?  */
        DAY_STR,        /* Monday, Tuesday, etc. */
        DAY,            /* day 01, 02, 03, or... 31 in a month */
        MONTH,          /* month? 0 to 11. Or, Jan, Feb, etc. */
        YEAR,           /* 1900 - year */
        LAST_STEP       /* This is the end... The end my friend... */
} steps;


gchar *         tracker_generic_date_extractor (gchar *date, steps steps_to_do[]);

gboolean        tracker_is_empty_string (const gchar *s);

gboolean	tracker_spawn (gchar **argv, int timeout, gchar **tmp_stdout, gint *exit_status);

void 		tracker_read_xmp (const gchar *buffer, size_t len, GHashTable *metadata);

#endif
