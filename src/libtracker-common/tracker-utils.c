/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <stdio.h>
#include <string.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "tracker-utils.h"

/**
 * tracker_strhex:
 * @data: The input array of bytes
 * @size: Number of bytes in the input array
 * @delimiter: Character to use as separator between each printed byte
 *
 * Returns the contents of @data as a printable string in hexadecimal
 *  representation.
 *
 * Based on GNU PDF's pdf_text_test_get_hex()
 *
 * Returns: A newly allocated string which should be disposed with g_free()
 **/
gchar *
tracker_strhex (const guint8 *data,
                gsize         size,
                gchar         delimiter)
{
	gsize i;
	gsize j;
	gsize new_str_length;
	gchar *new_str;

	/* Get new string length. If input string has N bytes, we need:
	 * - 1 byte for last NUL char
	 * - 2N bytes for hexadecimal char representation of each byte...
	 * - N-1 bytes for the separator ':'
	 * So... a total of (1+2N+N-1) = 3N bytes are needed... */
	new_str_length =  3 * size;

	/* Allocate memory for new array and initialize contents to NUL */
	new_str = g_malloc0 (new_str_length);

	/* Print hexadecimal representation of each byte... */
	for(i=0, j=0; i<size; i++, j+=3) {
		/* Print character in output string... */
		snprintf (&new_str[j], 3, "%02X", data[i]);

		/* And if needed, add separator */
		if(i != (size-1) ) {
			new_str[j+2] = delimiter;
		}
	}

	/* Set output string */
	return new_str;
}

/**
 * tracker_utf8_truncate:
 * @str: Nul-terminated input string
 * @max_size: Maximum length of the output string
 *
 * Returns up to @max_size characters long substring of @str, followed
 * with "[…]" when actually truncated.
 *
 * Returns: A newly allocated string which should be disposed with g_free()
 */
gchar *
tracker_utf8_truncate (const gchar  *str,
                       gsize         max_size)
{
	gchar *retv = NULL;

	if ((gsize) g_utf8_strlen (str, -1) > max_size) {
		gchar *substring = g_utf8_substring (str, 0, max_size - 3);
		retv = g_strdup_printf ("%s[…]", substring);
		g_free (substring);
	} else {
		retv = g_strdup (str);
	}

	return retv;
}

static gboolean
range_is_xdigit (const gchar *str,
                 gssize       start,
                 gssize       end)
{
	gssize i;

	g_assert (end > start);

	for (i = start; i < end; i++) {
		if (!g_ascii_isxdigit (str[i]))
			return FALSE;
	}

	return TRUE;
}

static gunichar
xdigit_to_unichar (const gchar *str,
		   gssize       start,
		   gssize       end)
{
	gunichar ch = 0;
	gssize i;

	g_assert (end > start);

	for (i = start; i < end; i++) {
		ch |= g_ascii_xdigit_value (str[i]);
		if (i < end - 1)
			ch <<= 4;
	}

	return ch;
}

/*
 * tracker_unescape_unichars:
 * @str: Input string
 * @len: Length
 *
 * Unescapes \u and \U sequences into their respective unichars.
 *
 * Returns: a string with no \u nor \U sequences
 */
gchar *
tracker_unescape_unichars (const gchar  *str,
                           gssize        len)
{
	GString *copy;
	gunichar ch;
	gssize i = 0;

	if (len < 0)
		len = strlen (str);

	copy = g_string_new (NULL);

	while (i < len) {
		if (len - i >= 2 &&
		    str[i] == '\\' &&
		    g_ascii_tolower (str[i + 1]) != 'u') {
			/* Not an unicode escape sequence */
			g_string_append_c (copy, str[i]);
			g_string_append_c (copy, str[i + 1]);
			i += 2;
		} else if (len - i >= 6 &&
		    strncmp (&str[i], "\\u", 2) == 0 &&
		    range_is_xdigit (&str[i], 2, 6)) {
			ch = xdigit_to_unichar (&str[i], 2, 6);
			g_string_append_unichar (copy, ch);
			i += 6;
		} else if (len - i >= 10 &&
		           strncmp (&str[i], "\\U", 2) == 0 &&
		           range_is_xdigit (&str[i], 2, 10)) {
			ch = xdigit_to_unichar (&str[i], 2, 10);
			g_string_append_unichar (copy, ch);
			i += 10;
		} else {
			g_string_append_c (copy, str[i]);
			i++;
		}
	}

	return g_string_free (copy, FALSE);
}

gboolean
parse_abs_uri (const gchar  *uri,
               gchar       **base,
               const gchar **rel_path)
{
	const gchar *loc, *end;

	end = &uri[strlen (uri)];
	loc = uri;

	if (!g_ascii_isalpha (loc[0]))
		return FALSE;

	while (loc != end) {
		if (loc[0] == ':')
			break;
		if (!g_ascii_isalpha (loc[0]) &&
		    loc[0] != '+' && loc[0] != '-' && loc[0] != '.')
			return FALSE;
		loc++;
	}

	if (loc == uri)
		return FALSE;

	if (strncmp (loc, "://", 3) == 0) {
		/* Include authority in base */
		loc += 3;
		loc = strchr (loc, '/');
		if (!loc)
			loc = end;
	}

	*base = g_strndup (uri, loc - uri);
	*rel_path = loc + 1;

	return TRUE;
}

GPtrArray *
remove_dot_segments (gchar **uri_elems)
{
	GPtrArray *array;
	gint i;

	array = g_ptr_array_new ();

	for (i = 0; uri_elems[i] != NULL; i++) {
		if (g_strcmp0 (uri_elems[i], ".") == 0) {
			continue;
		} else if (g_strcmp0 (uri_elems[i], "..") == 0) {
			if (array->len > 0)
				g_ptr_array_remove_index (array, array->len - 1);
			continue;
		} else if (*uri_elems[i] != '\0') {
			/* NB: Not a copy */
			g_ptr_array_add (array, uri_elems[i]);
		}
	}

	return array;
}

gchar *
tracker_resolve_relative_uri (const gchar  *base,
                              const gchar  *rel_uri)
{
	gchar **base_split, **rel_split, *host;
	GPtrArray *base_norm, *rel_norm;
	GString *str;
	guint i;

	/* Relative IRIs are combined with base IRIs with a simplified version
	 * of the algorithm described at RFC3986, Section 5.2. We don't care
	 * about query and fragment parts of an URI, and some simplifications
	 * are taken on base uri parsing and relative uri validation.
	 */
	rel_split = g_strsplit (rel_uri, "/", -1);

	/* Rel uri is a full uri? */
	if (strchr (rel_split[0], ':')) {
		g_strfreev (rel_split);
		return g_strdup (rel_uri);
	}

	if (!parse_abs_uri (base, &host, &base)) {
		g_strfreev (rel_split);
		return g_strdup (rel_uri);
	}

	base_split = g_strsplit (base, "/", -1);

	base_norm = remove_dot_segments (base_split);
	rel_norm = remove_dot_segments (rel_split);

	for (i = 0; i < rel_norm->len; i++) {
		g_ptr_array_add (base_norm,
		                 g_ptr_array_index (rel_norm, i));
	}

	str = g_string_new (host);
	for (i = 0; i < base_norm->len; i++) {
		g_string_append_c (str, '/');
		g_string_append (str,
		                 g_ptr_array_index (base_norm, i));
	}

	g_ptr_array_unref (base_norm);
	g_ptr_array_unref (rel_norm);
	g_strfreev (base_split);
	g_strfreev (rel_split);
	g_free (host);

	return g_string_free (str, FALSE);
}

gboolean
tracker_util_parse_dbus_uri (const gchar  *uri,
                             GBusType     *bus_type,
                             gchar       **service,
                             gchar       **path)
{
	const gchar *separator;

	if (!g_str_has_prefix (uri, "dbus:"))
		return FALSE;

	uri += strlen ("dbus:");

	if (g_str_has_prefix (uri, "system:")) {
		*bus_type = G_BUS_TYPE_SYSTEM;
		uri += strlen ("system:");
	} else if (g_str_has_prefix (uri, "session:")) {
		*bus_type = G_BUS_TYPE_SESSION;
		uri += strlen ("session:");
	} else {
		/* Fall back to session bus by default */
		*bus_type = G_BUS_TYPE_SESSION;
	}

	separator = strstr (uri, ":/");

	if (separator) {
		*service = g_strndup (uri, separator - uri);
		separator += 1;
		*path = g_strdup (separator);
	} else {
		*service = g_strdup (uri);
		*path = NULL;
	}

	return TRUE;
}

gchar *
tracker_util_build_dbus_uri (GBusType     bus_type,
                             const gchar *service,
                             const gchar *path)
{
	GString *str;

	if (!g_dbus_is_name (service))
		return NULL;
	if (path && path[0] != '/')
		return NULL;

	if (bus_type == G_BUS_TYPE_SESSION)
		str = g_string_new ("dbus:");
	else if (bus_type == G_BUS_TYPE_SYSTEM)
		str = g_string_new ("dbus:system:");
	else
		return NULL;

	g_string_append (str, service);

	if (path) {
		g_string_append_c (str, ':');
		g_string_append (str, path);
	}

	return g_string_free (str, FALSE);
}
