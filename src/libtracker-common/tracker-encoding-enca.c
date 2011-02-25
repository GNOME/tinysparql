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

#include <glib.h>
#include "tracker-encoding-enca.h"

#include <enca.h>

gchar *
tracker_encoding_guess_enca (const gchar *buffer,
                             gsize        size)
{
	gchar *encoding = NULL;
	const gchar **langs;
	gsize s;
	gsize i;

	langs = enca_get_languages (&s);

	for (i = 0; i < s && !encoding; i++) {
		EncaAnalyser analyser;
		EncaEncoding eencoding;

		analyser = enca_analyser_alloc (langs[i]);
		eencoding = enca_analyse_const (analyser, (guchar *)buffer, size);

		if (enca_charset_is_known (eencoding.charset)) {
			encoding = g_strdup (enca_charset_name (eencoding.charset,
			                                        ENCA_NAME_STYLE_ICONV));
		}

		enca_analyser_free (analyser);
	}

	free (langs);

	if (encoding)
		g_debug ("Guessing charset as '%s'", encoding);

	return encoding;
}
