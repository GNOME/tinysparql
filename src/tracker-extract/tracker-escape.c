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

gchar *
tracker_escape_metadata (const gchar *str)
{
        gchar *dest, *d;

	if (!str) {
		return NULL;
	}

        d = dest = g_malloc (strlen (str) * 4 + 1);

        while (*str) {
                switch (*str) {
                case '\n':
                        *d++ = '\\';
                        *d++ = 'n';
                        break;
                case '\t':
                        *d++ = '\\';
                        *d++ = 't';
                        break;
                case '\\':
                        *d++ = '\\';
                        *d++ = '\\';
                        break;
                case '|':
                case ';':
                case '=':
                {
                        /* special case fields separators */
                        gchar *octal_str, *o;

                        o = octal_str = g_strdup_printf ("\\%o", *str);

                        while (*o) {
                                *d++ = *o++;
                        }

                        g_free (octal_str);
                        break;
                }
                default:
                        *d++ = *str;
                        break;
                }

                str++;
        }

        *d = '\0';

        return dest;
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
