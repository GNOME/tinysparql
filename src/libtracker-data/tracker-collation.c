/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include "config.h"
#include <glib.h>
#include <string.h>

#include "tracker-collation.h"

/* If defined, will dump some additional logs as prints,
 *  instead of debugs*/
#ifdef PRINT_INSTEADOF_LOG
#undef g_debug
#define g_debug(message, ...) \
	g_print ("(debug) %s:%d: " message "\n", \
	         __FILE__, __LINE__, ##__VA_ARGS__)
#undef g_warning
#define g_warning(message, ...) \
	g_print ("(warning) %s:%d: " message "\n", \
	         __FILE__, __LINE__, ##__VA_ARGS__)
#undef g_critical
#define g_critical(message, ...) \
	g_print ("(critical) %s:%d: " message "\n", \
	         __FILE__, __LINE__, ##__VA_ARGS__)
#endif /* PRINT_INSTEADOF_LOG */

#ifdef HAVE_LIBUNISTRING
/* libunistring versions prior to 9.1.2 need this hack */
#define _UNUSED_PARAMETER_
#include <unistr.h>
#elif HAVE_LIBICU
#include <locale.h>
#include <unicode/ucol.h>
#endif

/* If string lenth less than this value, allocating from the stack */
#define MAX_STACK_STR_SIZE 8192

#ifdef HAVE_LIBUNISTRING /* ---- GNU libunistring based collation ---- */

gpointer
tracker_collation_init (void)
{
	g_debug ("[libunistring collation] Initializing collator");
	/* Nothing to do */
	return NULL;
}

void
tracker_collation_deinit (gpointer collator)
{
	/* Nothing to do */
}

gint
tracker_collation_utf8 (gpointer      collator,
                        gint          len1,
                        gconstpointer str1,
                        gint          len2,
                        gconstpointer str2)
{
	gint result;
	gchar *aux1;
	gchar *aux2;

	/* Note: str1 and str2 are NOT NUL-terminated */
	aux1 = (len1 < MAX_STACK_STR_SIZE) ? g_alloca (len1+1) : g_malloc (len1+1);
	aux2 = (len2 < MAX_STACK_STR_SIZE) ? g_alloca (len2+1) : g_malloc (len2+1);

	memcpy (aux1, str1, len1); aux1[len1] = '\0';
	memcpy (aux2, str2, len2); aux2[len2] = '\0';

	result = u8_strcoll (aux1, aux2);

	if (len1 >= MAX_STACK_STR_SIZE)
		g_free (aux1);
	if (len2 >= MAX_STACK_STR_SIZE)
		g_free (aux2);
	return result;
}

#elif HAVE_LIBICU /* ---- ICU based collation (UTF-16) ----*/

gpointer
tracker_collation_init (void)
{
	UCollator *collator = NULL;
	UErrorCode status = U_ZERO_ERROR;
	const gchar *locale = setlocale (LC_ALL, NULL);

	g_debug ("[ICU collation] Initializing collator for locale '%s'", locale);
	collator = ucol_open (locale, &status);
	if (!collator) {
		g_warning ("[ICU collation] Collator for locale '%s' cannot be created: %s",
		               locale, u_errorName (status));
		/* Try to get UCA collator then... */
		status = U_ZERO_ERROR;
		collator = ucol_open ("root", &status);
		if (!collator) {
			g_critical ("[ICU collation] UCA Collator cannot be created: %s",
			                u_errorName (status));
		}
	}
	return collator;
}

void
tracker_collation_deinit (gpointer collator)
{
	if (collator)
		ucol_close ((UCollator *)collator);
}

gint
tracker_collation_utf8 (gpointer      collator,
                        gint          len1,
                        gconstpointer str1,
                        gint          len2,
                        gconstpointer str2)
{
	UErrorCode status = U_ZERO_ERROR;
	UCharIterator iter1;
	UCharIterator iter2;
	UCollationResult result;

	/* Collator must be created before trying to collate */
	g_return_val_if_fail (collator, -1);

	/* Setup iterators */
	uiter_setUTF8 (&iter1, str1, len1);
	uiter_setUTF8 (&iter2, str2, len2);

	result = ucol_strcollIter ((UCollator *)collator,
	                           &iter1,
	                           &iter2,
	                           &status);
	if (status != U_ZERO_ERROR)
		g_critical ("Error collating: %s", u_errorName (status));

	if (result == UCOL_GREATER)
		return 1;
	if (result == UCOL_LESS)
		return -1;
	return 0;
}

#else /* ---- GLib based collation ---- */

gpointer
tracker_collation_init (void)
{
	g_debug ("[GLib collation] Initializing collator");
	/* Nothing to do */
	return NULL;
}

void
tracker_collation_deinit (gpointer collator)
{
	/* Nothing to do */
}

gint
tracker_collation_utf8 (gpointer      collator,
                        gint          len1,
                        gconstpointer str1,
                        gint          len2,
                        gconstpointer str2)
{
	gint result;
	gchar *aux1;
	gchar *aux2;

	/* Note: str1 and str2 are NOT NUL-terminated */
	aux1 = (len1 < MAX_STACK_STR_SIZE) ? g_alloca (len1+1) : g_malloc (len1+1);
	aux2 = (len2 < MAX_STACK_STR_SIZE) ? g_alloca (len2+1) : g_malloc (len2+1);

	memcpy (aux1, str1, len1); aux1[len1] = '\0';
	memcpy (aux2, str2, len2); aux2[len2] = '\0';

	result = g_utf8_collate (aux1, aux2);

	if (len1 >= MAX_STACK_STR_SIZE)
		g_free (aux1);
	if (len2 >= MAX_STACK_STR_SIZE)
		g_free (aux2);
	return result;
}

#endif
