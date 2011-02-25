/*
 * Copyright (C) 2011 Nokia <ivan.frade@nokia.com>
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

#include <MCharsetDetector>
#include <MCharsetMatch>

#include <glib.h>
#include "tracker-encoding-meegotouch.h"

/*
 * See http://apidocs.meego.com/git-tip/mtf/class_m_charset_detector.html
 */

gchar *
tracker_encoding_guess_meegotouch (const gchar *buffer,
                                   gsize        size)
{
	/* Initialize detector */
	MCharsetDetector detector ((const char *)buffer, (int)size);

	if (detector.hasError ()) {
		g_warning ("Charset detector error when creating: %s",
		           detector.errorString ().toUtf8 (). data ());
		return NULL;
	}

	MCharsetMatch bestMatch = detector.detect ();

	if (detector.hasError ()) {
		g_warning ("Charset detector error when detecting: %s",
		           detector.errorString ().toUtf8 (). data ());
		return NULL;
	}

	gchar *encoding = g_strdup (bestMatch.name ().toUtf8 ().data ());

	g_debug ("Guessing charset as '%s' with %d confidence",
	         encoding, bestMatch.confidence ());

	return encoding;
}
