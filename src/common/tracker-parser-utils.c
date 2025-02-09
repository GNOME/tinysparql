/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"

#include <string.h>

#include "tracker-utils.h"

#include "tracker-parser-utils.h"

#if TRACKER_PARSER_DEBUG_HEX
void
tracker_parser_message_hex (const gchar  *message,
                            const gchar  *str,
                            gsize         str_length)
{
	gchar *hex_aux;
	gchar *str_aux;

	g_return_if_fail (message);
	g_return_if_fail (str);
	g_return_if_fail (str_length != 0);

	/* String may not come NIL-terminated */
	str_aux = g_malloc (str_length + 1);
	memcpy (str_aux, str, str_length);
	str_aux[str_length] = '\0';

	/* Get hexadecimal representation of the input string */
	hex_aux = tracker_strhex (str, str_length, ':');

	/* Log it */
	g_message ("%s: '%s' (%s)",
	           message, str_aux, hex_aux);

	g_free (str_aux);
	g_free (hex_aux);
}
#endif
