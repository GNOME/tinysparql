/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "tracker-version.h"

const guint tracker_major_version = TRACKER_MAJOR_VERSION;
const guint tracker_minor_version = TRACKER_MINOR_VERSION;
const guint tracker_micro_version = TRACKER_MICRO_VERSION;
const guint tracker_interface_age = TRACKER_INTERFACE_AGE;
const guint tracker_binary_age = TRACKER_BINARY_AGE;

/**
 * tracker_check_version:
 * @required_major: the required major version.
 * @required_minor: the required minor version.
 * @required_micro: the required micro version.
 *
 * Checks that the Tracker library in use is compatible with the given version.
 *
 * Generally you would pass in the constants
 * [const@MAJOR_VERSION], [const@MINOR_VERSION], [const@MICRO_VERSION]
 * as the three arguments to this function; that produces
 * a check that the library in use is compatible with
 * the version of Tracker the application or module was compiled
 * against.
 *
 * Compatibility is defined by two things: first the version
 * of the running library is newer than the version
 * @required_major.@required_minor.@required_micro. Second
 * the running library must be binary compatible with the
 * version @required_major.@required_minor.@required_micro
 * (same major version.)
 *
 * Return value: %NULL if the Tracker library is compatible with the
 *   given version, or a string describing the version mismatch.
 **/
const gchar *
tracker_check_version (guint required_major,
                       guint required_minor,
                       guint required_micro)
{
	gint tracker_effective_micro = 100 * TRACKER_MINOR_VERSION + TRACKER_MICRO_VERSION;
	gint required_effective_micro = 100 * required_minor + required_micro;

	if (required_major > TRACKER_MAJOR_VERSION)
		return "Tracker version too old (major mismatch)";
	if (required_major < TRACKER_MAJOR_VERSION)
		return "Tracker version too new (major mismatch)";
	if (required_effective_micro < tracker_effective_micro - TRACKER_BINARY_AGE)
		return "Tracker version too new (micro mismatch)";
	if (required_effective_micro > tracker_effective_micro)
		return "Tracker version too old (micro mismatch)";

	return NULL;
}
