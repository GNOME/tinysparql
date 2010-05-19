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

#ifdef HAVE_UNAC
#include <unac.h>
#endif

#ifdef HAVE_LIBICU
#include <unicode/utypes.h>
#include <unicode/ucnv.h>
#endif

#include <libtracker-common/tracker-common.h>
#include "tracker-parser-utils.h"


/* Output is always UTF-8. */
gchar *
tracker_parser_unaccent_utf16be_word (const gchar *string,
                                      gsize        ilength,
                                      gsize        *p_olength)
{
#ifdef HAVE_UNAC
	GError *error = NULL;
	gchar *unaccented_str = NULL;
	gchar *str_utf8 = NULL;
	gsize unaccented_len;
	gsize utf8_len;

	*p_olength = 0;

	if (unac_string_utf16 (string, ilength,
	                       &unaccented_str, &unaccented_len) != 0) {
		g_warning ("UNAC failed to strip accents");
		return NULL;
	}

	/* Convert from UTF-16BE to UTF-8 */
	str_utf8 = g_convert (unaccented_str,
	                      unaccented_len,
	                      "UTF-8",
	                      "UTF-16BE",
	                      NULL,
	                      &utf8_len,
	                      &error);
	g_free (unaccented_str);

	if (error) {
		g_warning ("Could not convert back to UTF-8: %s",
		           error->message);
		g_error_free (error);
		return NULL;
	}

	*p_olength = utf8_len;
	return str_utf8;
#else
	return NULL;
#endif
}


#ifdef HAVE_LIBICU
/* NOTE: Internally, UChars are UTF-16, but conversion needed just in case,
 *  as libunac needs UTF-16BE. Output is always UTF-8.*/
gchar *
tracker_parser_unaccent_UChar_word (const UChar *string,
                                    gsize        ilength,
                                    gsize        *p_olength)
{
#ifdef HAVE_UNAC
	UErrorCode icu_error = U_ZERO_ERROR;
	UConverter *converter;
	gchar *str_utf16;
	gchar *str_utf8 = NULL;
	gsize utf16_len;

	*p_olength = 0;

	/* Open converter UChar to UTF-16BE */
	converter = ucnv_open ("UTF-16BE", &icu_error);
	if (!converter) {
		g_warning ("Cannot open UTF-16BE converter: '%s'",
		           U_FAILURE (icu_error) ? u_errorName (icu_error) : "none");
               return NULL;
	}

	/* Allocate buffer, same size as input string.
	 * Note that ilength specifies number of UChars not
	 *  number of bytes */
	str_utf16 = g_malloc ((ilength + 1) * 2);

	/* Convert from UChar to UTF-16BE */
	utf16_len = ucnv_fromUChars (converter,
	                             str_utf16,
	                             (ilength + 1) * 2,
	                             string,
	                             ilength,
	                             &icu_error);
	if (U_FAILURE (icu_error)) {
		g_warning ("Cannot convert from UChar to UTF-16BE: '%s' "
		           "(ilength: %" G_GSIZE_FORMAT ")",
		           u_errorName (icu_error),
		           ilength);
	} else {
		str_utf8 = tracker_parser_unaccent_utf16be_word (str_utf16,
		                                                 utf16_len,
		                                                 p_olength);
	}
	ucnv_close (converter);
	g_free (str_utf16);
	return str_utf8;
#else
	return NULL;
#endif
}
#endif

gchar *
tracker_parser_unaccent_utf8_word (const gchar *str,
                                   gsize        ilength,
                                   gsize        *p_olength)
{
#ifdef HAVE_UNAC
	GError *error = NULL;
	gchar *str_utf16 = NULL;
	gchar *str_utf8 = NULL;
	gsize utf16_len;

	*p_olength = 0;

	/* unac_string() does roughly the same than below, plus it
	 * corrupts memory in 64bit systems, so avoid it for now.
	 */
	str_utf16 = g_convert (str, ilength, "UTF-16BE", "UTF-8", NULL, &utf16_len, &error);

	if (error) {
		g_warning ("Could not convert to UTF-16: %s", error->message);
		g_error_free (error);
		return NULL;
	} else {

		str_utf8 = tracker_parser_unaccent_utf16be_word (str_utf16,
		                                                 utf16_len,
		                                                 p_olength);
	}

	g_free (str_utf16);
	return str_utf8;
#else
	return NULL;
#endif
}


/*
 * Definition of the possible reserved words.
 *  Length of word is explicitly given to avoid strlen() calls
 */
typedef struct {
	const gchar *word;
	gsize        word_length;
} TrackerParserReservedWord;

static const TrackerParserReservedWord reserved_words[] = {
	{ "or", 2 },
	{ NULL, 0 }
};

gboolean
tracker_parser_is_reserved_word_utf8 (const gchar *word,
                                      gsize word_length)
{
	gint i = 0;

	/* Loop the array of predefined reserved words */
	while (reserved_words[i].word != NULL) {
		if (word_length == reserved_words[i].word_length &&
		    strncmp (word,
		             reserved_words[i].word,
		             word_length) == 0) {
			return TRUE;
		}
		i++;
	}

	return FALSE;
}


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
