/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
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

#include "config.h"

#include <string.h>

#include <glib.h>

#include "tracker-escape.h"

#ifdef SHOULD_VALIDATE_UTF8

gchar *
tracker_escape_metadata (const gchar *str)
{
	const gchar *end;

	if (!str) {
		return NULL;
	}

	if (g_utf8_validate (str, -1, &end)) {
		return g_strstrip (g_strdup (str));
	}

	return g_strstrip (g_strndup (str, end - str));
}

gchar *
tracker_escape_metadata_printf (const gchar *format,
				...)
{
 	va_list args;
 	gchar *str, *escaped;
 	
 	va_start (args, format);
 	str = g_strdup_vprintf (format, args);
 	va_end (args);
 	
 	escaped = tracker_escape_metadata (str);
 	g_free (str);
 	
 	return escaped;
}

#endif /* SHOULD_VALIDATE_UTF8 */
