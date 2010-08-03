/*
 * Copyright (C) 2008-2010, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2010, Codeminded BVBA <abustany@gnome.org>
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

#include <string.h>

#include <glib.h>

#include "tracker-sparql.h"

static const char *
find_conversion (const char  *format,
                 const char **after)
{
	const char *start = format;
	const char *cp;

	while (*start != '\0' && *start != '%')
		start++;

	if (*start == '\0') {
		*after = start;
		return NULL;
	}

	cp = start + 1;

	if (*cp == '\0') {
		*after = cp;
		return NULL;
	}

	/* Test for positional argument.  */
	if (*cp >= '0' && *cp <= '9') {
		const char *np;

		for (np = cp; *np >= '0' && *np <= '9'; np++)
			;
		if (*np == '$')
			cp = np + 1;
	}

	/* Skip the flags.  */
	for (;;) {
		if (*cp == '\'' ||
		    *cp == '-' ||
		    *cp == '+' ||
		    *cp == ' ' ||
		    *cp == '#' ||
		    *cp == '0')
			cp++;
		else
			break;
	}

	/* Skip the field width.  */
	if (*cp == '*') {
		cp++;

		/* Test for positional argument.  */
		if (*cp >= '0' && *cp <= '9') {
			const char *np;

			for (np = cp; *np >= '0' && *np <= '9'; np++)
				;
			if (*np == '$')
				cp = np + 1;
		}
	} else {
		for (; *cp >= '0' && *cp <= '9'; cp++)
			;
	}

	/* Skip the precision.  */
	if (*cp == '.') {
		cp++;
		if (*cp == '*') {
			/* Test for positional argument.  */
			if (*cp >= '0' && *cp <= '9') {
				const char *np;

				for (np = cp; *np >= '0' && *np <= '9'; np++)
					;
				if (*np == '$')
					cp = np + 1;
			}
		} else {
			for (; *cp >= '0' && *cp <= '9'; cp++)
				;
		}
	}

	/* Skip argument type/size specifiers.  */
	while (*cp == 'h' ||
	       *cp == 'L' ||
	       *cp == 'l' ||
	       *cp == 'j' ||
	       *cp == 'z' ||
	       *cp == 'Z' ||
	       *cp == 't')
		cp++;

	/* Skip the conversion character.  */
	cp++;

	*after = cp;
	return start;
}

/**
 * tracker_sparql_escape_uri_vprintf:
 * @format: a standard printf() format string, but notice
 *     <link linkend="string-precision">string precision pitfalls</link>
 * @args: the list of parameters to insert into the format string
 *
 * Similar to the standard C vsprintf() function but safer, since it
 * calculates the maximum space required and allocates memory to hold
 * the result.
 *
 * The result is escaped using g_uri_escape_string().
 *
 * Returns: a newly-allocated string holding the result.
 *  The returned string should be freed with g_free() when no longer needed.
 *
 * Since: 0.9
 */
gchar *
tracker_sparql_escape_uri_vprintf (const gchar *format,
                                   va_list      args)
{
	GString *format1;
	GString *format2;
	GString *result = NULL;
	gchar *output1 = NULL;
	gchar *output2 = NULL;
	const char *p;
	gchar *op1, *op2;
	va_list args2;

	format1 = g_string_new (NULL);
	format2 = g_string_new (NULL);
	p = format;
	while (TRUE) {
		const char *after;
		const char *conv = find_conversion (p, &after);
		if (!conv)
			break;

		g_string_append_len (format1, conv, after - conv);
		g_string_append_c (format1, 'X');
		g_string_append_len (format2, conv, after - conv);
		g_string_append_c (format2, 'Y');

		p = after;
	}

	/* Use them to format the arguments
	 */
	G_VA_COPY (args2, args);

	output1 = g_strdup_vprintf (format1->str, args);
	va_end (args);
	if (!output1) {
		va_end (args2);
		goto cleanup;
	}

	output2 = g_strdup_vprintf (format2->str, args2);
	va_end (args2);
	if (!output2)
		goto cleanup;

	result = g_string_new (NULL);

	op1 = output1;
	op2 = output2;
	p = format;
	while (TRUE) {
		const char *after;
		const char *output_start;
		const char *conv = find_conversion (p, &after);
		char *escaped;

		if (!conv) {
			g_string_append_len (result, p, after - p);
			break;
		}

		g_string_append_len (result, p, conv - p);
		output_start = op1;
		while (*op1 == *op2) {
			op1++;
			op2++;
		}

		*op1 = '\0';
		escaped = g_uri_escape_string (output_start, NULL, FALSE);
		g_string_append (result, escaped);
		g_free (escaped);

		p = after;
		op1++;
		op2++;
	}

cleanup:
	g_string_free (format1, TRUE);
	g_string_free (format2, TRUE);
	g_free (output1);
	g_free (output2);

	if (result)
		return g_string_free (result, FALSE);
	else
		return NULL;
}

/**
 * tracker_sparql_escape_uri_printf:
 * @format: a standard printf() format string, but notice
 *     <link linkend="string-precision">string precision pitfalls</link>
 * @Varargs: the parameters to insert into the format string
 *
 * Calls tracker_sparql_escape_uri_vprintf() with the @Varargs supplied.
 *
 * Returns: a newly-allocated string holding the result.
 *  The returned string should be freed with g_free() when no longer needed.
 *
 * Since: 0.9
 */
gchar *
tracker_sparql_escape_uri_printf (const gchar *format, ...)
{
	gchar *result;
	va_list args;

	va_start (args, format);
	result = tracker_sparql_escape_uri_vprintf (format, args);
	va_end (args);

	return result;
}

