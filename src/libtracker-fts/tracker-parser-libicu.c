/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008,2009,2010 Nokia <ivan.frade@nokia.com>
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

#include <stdio.h>
#include <string.h>
#include <locale.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>
#include <unicode/ubrk.h>
#include <unicode/ustring.h>
#include <unicode/uchar.h>
#include <unicode/unorm.h>

#include "tracker-parser.h"
#include "tracker-parser-utils.h"

/* Type of words detected */
typedef enum {
	TRACKER_PARSER_WORD_TYPE_ASCII,
	TRACKER_PARSER_WORD_TYPE_OTHER_UNAC,
	TRACKER_PARSER_WORD_TYPE_OTHER_NO_UNAC,
} TrackerParserWordType;

/* Max possible length of a UChar encoded string (just a safety limit) */
#define WORD_BUFFER_LENGTH 512

struct TrackerParser {
	const gchar           *txt;
	gint                   txt_size;

	TrackerLanguage       *language;
	guint                  max_word_length;
	gboolean               enable_stemmer;
	gboolean               enable_unaccent;
	gboolean               ignore_stop_words;
	gboolean               ignore_reserved_words;
	gboolean               ignore_numbers;
	gboolean               enable_forced_wordbreaks;

	/* Private members */
	gchar                 *word;
	gint                   word_length;
	guint                  word_position;

	/* Text as UChars */
	UChar                 *utxt;
	gint                   utxt_size;
	/* Original offset of each UChar in the input txt string */
	gint32                *offsets;

	/* The word-break iterator */
	UBreakIterator        *bi;

	/* Cursor, as index of the utxt array of bytes */
	gsize                  cursor;
};


static gboolean
get_word_info (const UChar           *word,
               gsize                  word_length,
               gboolean               ignore_numbers,
               gboolean              *p_is_allowed_word_start,
               TrackerParserWordType *p_word_type)
{
	UCharIterator iter;
	UChar32 unichar;
	guint8 unichar_gc;

	/* Get first character of the word as UCS4 */
	uiter_setString (&iter, word, word_length);
	unichar = uiter_current32 (&iter);
	if (unichar == U_SENTINEL) {
		return FALSE;
	}

	/* We only want the words where the first character
	 * in the word is either a letter, a number or a symbol.
	 *
	 * This is needed because the word break algorithm also
	 * considers word breaks after for example commas or other
	 * punctuation marks.
	 *
	 * Note that looking at the first character in the string
	 * should be compatible with all Unicode normalization
	 * methods.
	 */
	unichar_gc = u_charType (unichar);
	if (unichar_gc == U_UPPERCASE_LETTER ||
	    unichar_gc == U_LOWERCASE_LETTER ||
	    unichar_gc == U_TITLECASE_LETTER ||
	    unichar_gc == U_MODIFIER_LETTER ||
	    unichar_gc == U_OTHER_LETTER ||
	    IS_UNDERSCORE_UCS4 ((guint32)unichar) ||
	    (!ignore_numbers &&
	     (unichar_gc == U_DECIMAL_DIGIT_NUMBER ||
	      unichar_gc == U_LETTER_NUMBER ||
	      unichar_gc == U_OTHER_NUMBER))) {
		*p_is_allowed_word_start = TRUE;
	} else {
		*p_is_allowed_word_start = FALSE;
		return TRUE;
	}

	/* Word starts with a CJK character? */
	if (IS_CJK_UCS4 ((guint32)unichar)) {
		*p_word_type = TRACKER_PARSER_WORD_TYPE_OTHER_NO_UNAC;
		return TRUE;
	}

	/* Is ASCII-only string? */
	while (unichar != U_SENTINEL) {
		if (!IS_ASCII_UCS4 ((guint32)unichar)) {
			*p_word_type = TRACKER_PARSER_WORD_TYPE_OTHER_UNAC;
			return TRUE;
		}
		unichar = uiter_next32 (&iter);
	}

	*p_word_type = TRACKER_PARSER_WORD_TYPE_ASCII;
	return TRUE;
}

static gboolean
parser_unaccent_nfkd_word (UChar *word,
			   gsize *word_length)
{
	/* The input word in this method MUST be normalized in NFKD form */
	gsize i;
	gsize j;

	g_return_val_if_fail (word, FALSE);
	g_return_val_if_fail (word_length, FALSE);
	g_return_val_if_fail (*word_length > 0, FALSE);

	i = 0;
	j = 0;
	while (i < *word_length) {
		UChar32 unichar;
		gint utf16_len; /* given in UChars */
		gsize aux_i;

		/* Get next character of the word as UCS4 */
		aux_i = i;
		U16_NEXT (word, aux_i, *word_length, unichar);
		utf16_len = aux_i - i;

		/* Invalid UTF-16 character or end of original string. */
		if (utf16_len <= 0) {
			break;
		}

		/* If the given unichar is a combining diacritical mark,
		 * just update the original index, not the output one */
		if (IS_CDM_UCS4 ((guint32) unichar)) {
			i += utf16_len;
			continue;
		}

		/* If already found a previous combining
		 * diacritical mark, indexes are different so
		 * need to copy characters. As output and input
		 * buffers may overlap, need to use memmove
		 * instead of memcpy */
		if (i != j) {
			memmove (&word[j], &word[i], sizeof (UChar) * utf16_len);
		}

		/* Update both indexes */
		i += utf16_len;
		j += utf16_len;
	}

	/* Force proper string end */
	word[j] = (UChar) 0;

	/* Set new output length */
	*word_length = j;

	return TRUE;
}

static gchar *
convert_UChar_to_utf8 (const UChar *word,
                       gsize        uchar_len,
                       gsize       *utf8_len)
{
	gchar *utf8_str;
	UErrorCode icu_error = U_ZERO_ERROR;
	UConverter *converter;
	gsize new_utf8_len;

	g_return_val_if_fail (word, NULL);
	g_return_val_if_fail (utf8_len, NULL);

	/* Open converter UChar to UTF-16BE */
	converter = ucnv_open ("UTF-8", &icu_error);
	if (!converter) {
		g_warning ("Cannot open UTF-8 converter: '%s'",
		           U_FAILURE (icu_error) ? u_errorName (icu_error) : "none");
		return NULL;
	}

	/* A character encoded in 2 bytes in UTF-16 may get expanded to 3 or 4 bytes
	 *  in UTF-8. */
	utf8_str = g_malloc (2 * uchar_len * sizeof (UChar) + 1);

	/* Convert from UChar to UTF-8 (NIL-terminated) */
	new_utf8_len = ucnv_fromUChars (converter,
	                                utf8_str,
	                                2 * uchar_len * sizeof (UChar) + 1,
	                                word,
	                                uchar_len,
	                                &icu_error);
	if (U_FAILURE (icu_error)) {
		g_warning ("Cannot convert from UChar to UTF-8: '%s'",
		           u_errorName (icu_error));
		g_free (utf8_str);
		ucnv_close (converter);
		return NULL;
	}

	*utf8_len = new_utf8_len;
	ucnv_close (converter);

	return utf8_str;
}

static gchar *
process_word_uchar (TrackerParser         *parser,
                    const UChar           *word,
                    gint                   length,
                    TrackerParserWordType  type,
                    gboolean              *stop_word)
{
	UErrorCode error = U_ZERO_ERROR;
	UChar normalized_buffer[WORD_BUFFER_LENGTH];
	gchar *utf8_str = NULL;
	gsize new_word_length;

	/* Log original word */
	tracker_parser_message_hex ("ORIGINAL word",
	                            (guint8 *)word,
	                            length * sizeof (UChar));


	if (type != TRACKER_PARSER_WORD_TYPE_ASCII) {
		UChar casefolded_buffer [WORD_BUFFER_LENGTH];

		/* Casefold... */
		new_word_length = u_strFoldCase (casefolded_buffer,
		                                 WORD_BUFFER_LENGTH,
		                                 word,
		                                 length,
		                                 U_FOLD_CASE_DEFAULT,
		                                 &error);
		if (U_FAILURE (error)) {
			g_warning ("Error casefolding: '%s'",
			           u_errorName (error));
			return NULL;
		}
		if (new_word_length > WORD_BUFFER_LENGTH)
			new_word_length = WORD_BUFFER_LENGTH;

		/* Log after casefolding */
		tracker_parser_message_hex (" After Casefolding",
		                            (guint8 *)casefolded_buffer,
		                            new_word_length * sizeof (UChar));

		/* NFKD normalization... */
		new_word_length = unorm_normalize (casefolded_buffer,
		                                   new_word_length,
		                                   UNORM_NFKD,
		                                   0,
		                                   normalized_buffer,
		                                   WORD_BUFFER_LENGTH,
		                                   &error);
		if (U_FAILURE (error)) {
			g_warning ("Error normalizing: '%s'",
			           u_errorName (error));
			return NULL;
		}

		if (new_word_length > WORD_BUFFER_LENGTH)
			new_word_length = WORD_BUFFER_LENGTH;

		/* Log after casefolding */
		tracker_parser_message_hex (" After Normalization",
		                            (guint8 *) normalized_buffer,
		                            new_word_length * sizeof (UChar));
	} else {
		/* For ASCII-only, just tolower() each character */
		new_word_length = u_strToLower (normalized_buffer,
		                                WORD_BUFFER_LENGTH,
		                                word,
		                                length,
		                                NULL,
		                                &error);
		if (U_FAILURE (error)) {
			g_warning ("Error lowercasing: '%s'",
			           u_errorName (error));
			return NULL;
		}

		/* Log after casefolding */
		tracker_parser_message_hex (" After lowercase",
		                            (guint8 *) normalized_buffer,
		                            new_word_length * sizeof (UChar));
	}

	/* UNAC stripping needed? (for non-CJK and non-ASCII) */
	if (parser->enable_unaccent &&
	    type == TRACKER_PARSER_WORD_TYPE_OTHER_UNAC &&
	    parser_unaccent_nfkd_word (normalized_buffer, &new_word_length)) {
		/* Log after unaccenting */
		tracker_parser_message_hex ("  After UNAC",
		                            (guint8 *) normalized_buffer,
		                            new_word_length * sizeof (UChar));
	}

	/* Finally, convert to UTF-8 */
	utf8_str = convert_UChar_to_utf8 (normalized_buffer,
	                                  new_word_length,
	                                  &new_word_length);

	/* Log after unaccenting */
	tracker_parser_message_hex ("   After UTF8 conversion",
	                            utf8_str,
	                            new_word_length);

	/* Check if stop word */
	if (parser->ignore_stop_words) {
		*stop_word = tracker_language_is_stop_word (parser->language,
		                                            utf8_str);
	}

	/* Stemming needed? */
	if (utf8_str &&
	    parser->enable_stemmer) {
		gchar *stemmed;

		/* Input for stemmer ALWAYS in UTF-8, as well as output */
		stemmed = tracker_language_stem_word (parser->language,
		                                      utf8_str,
		                                      new_word_length);

		/* Log after stemming */
		tracker_parser_message_hex ("    After stemming",
		                            stemmed, strlen (stemmed));

		/* If stemmed wanted and succeeded, free previous and return it */
		if (stemmed) {
			g_free (utf8_str);
			return stemmed;
		}
	}

	return utf8_str;
}

static gboolean
parser_check_forced_wordbreaks (const UChar *buffer,
                                gsize        current,
                                gsize       *next)
{
	gsize unicode_word_length = *next - current;
	gsize word_length = 0;
	UCharIterator iter;
	UChar32 unichar;

	uiter_setString (&iter, &buffer[current], unicode_word_length);

	/* Iterate over the string looking for forced word breaks */
	while ((unichar = uiter_next32 (&iter)) != U_SENTINEL &&
	       word_length < unicode_word_length) {

		if (IS_FORCED_WORDBREAK_UCS4 ((guint32) unichar)) {
			/* Support word starting with a forced wordbreak */
			if (word_length == 0) {
				word_length = 1;
			}
			break;
		}

		word_length ++;
	}

	/* g_debug ("current: %" G_GSIZE_FORMAT ", " */
	/*          "next: %" G_GSIZE_FORMAT ", " */
	/*          "now: %" G_GSIZE_FORMAT, */
	/*          current, */
	/*          *next, */
	/*          current + word_length); */

	if (word_length != unicode_word_length) {
		*next = current + word_length;
		return TRUE;
	}
	return FALSE;
}

static gboolean
parser_next (TrackerParser *parser,
             gint          *byte_offset_start,
             gint          *byte_offset_end,
             gboolean      *stop_word)
{
	gsize word_length_uchar = 0;
	gsize word_length_utf8 = 0;
	gchar *processed_word = NULL;
	gsize current_word_offset_utf8;

	*byte_offset_start = 0;
	*byte_offset_end = 0;

	g_return_val_if_fail (parser, FALSE);

	/* Loop to look for next valid word */
	while (!processed_word &&
	       parser->cursor < parser->utxt_size) {
		TrackerParserWordType type;
		gboolean is_allowed;
		gsize next_word_offset_uchar;
		gsize next_word_offset_utf8;
		gsize truncated_length;

		/* Set current word offset in the original UTF-8 string */
		current_word_offset_utf8 = parser->offsets[parser->cursor];

		/* Find next word break. */
		next_word_offset_uchar = ubrk_next (parser->bi);

		/* Check if any forced wordbreaks here... */
		if (parser->enable_forced_wordbreaks) {
			/* Returns TRUE if next word offset changed */
			if (parser_check_forced_wordbreaks (parser->utxt,
			                                    parser->cursor,
			                                    &next_word_offset_uchar)) {
				/* We need to reset the iterator so that next word
				 * actually returns the same result */
				ubrk_previous (parser->bi);
			}
		}

		if (next_word_offset_uchar >= parser->utxt_size) {
			/* Last word support... */
			next_word_offset_uchar = parser->utxt_size;
			next_word_offset_utf8 = parser->txt_size;
		} else {
			next_word_offset_utf8 = parser->offsets[next_word_offset_uchar];
		}

		/* Word end is the first byte after the word, which is either the
		 *  start of next word or the end of the string */
		word_length_uchar = next_word_offset_uchar - parser->cursor;
		word_length_utf8 = next_word_offset_utf8 - current_word_offset_utf8;

		/* g_debug ("word_length_uchar: %" G_GSIZE_FORMAT, word_length_uchar); */
		/* g_debug ("next_word_offset_uchar: %" G_GSIZE_FORMAT, next_word_offset_uchar); */
		/* g_debug ("current_word_offset_uchar: %" G_GSIZE_FORMAT, parser->cursor); */
		/* g_debug ("word_length_utf8: %" G_GSIZE_FORMAT, word_length_utf8); */
		/* g_debug ("next_word_offset_utf8: %" G_GSIZE_FORMAT, next_word_offset_utf8); */
		/* g_debug ("current_word_offset_utf8: %" G_GSIZE_FORMAT, current_word_offset_utf8); */

		/* Ignore the word if longer than the maximum allowed */
		if (word_length_utf8 >= parser->max_word_length) {
			/* Ignore this word and keep on looping */
			parser->cursor = next_word_offset_uchar;
			continue;
		}

		/* Get word info... */
		if (!get_word_info (&parser->utxt[parser->cursor],
		                    word_length_uchar,
		                    parser->ignore_numbers,
		                    &is_allowed,
		                    &type)) {
			/* Quit loop just in case */
			parser->cursor = parser->utxt_size;
			break;
		}

		/* Ignore the word if not an allowed word start */
		if (!is_allowed) {
			/* Ignore this word and keep on looping */
			parser->cursor = next_word_offset_uchar;
			continue;
		}

		/* check if word is reserved (looking at ORIGINAL UTF-8 buffer here! */
		if (parser->ignore_reserved_words &&
		    tracker_parser_is_reserved_word_utf8 (&parser->txt[current_word_offset_utf8],
		                                          word_length_utf8)) {
			/* Ignore this word and keep on looping */
			parser->cursor = next_word_offset_uchar;
			continue;
		}

		/* compute truncated word length (in UChar bytes) if needed (to
		 * avoid extremely long words) */
		truncated_length = (word_length_uchar < 2 * WORD_BUFFER_LENGTH ?
		                    word_length_uchar :
		                    2 * WORD_BUFFER_LENGTH);

		/* Process the word here. If it fails, we can still go
		 *  to the next one. Returns newly allocated UTF-8
		 *  string always.
		 * Enable UNAC stripping only if no ASCII and no CJK
		 * Note we are passing UChar encoded string here!
		 */
		processed_word = process_word_uchar (parser,
		                                     &(parser->utxt[parser->cursor]),
		                                     truncated_length,
		                                     type,
		                                     stop_word);
		if (!processed_word) {
			/* Ignore this word and keep on looping */
			parser->cursor = next_word_offset_uchar;
			continue;
		}
	}

	/* If we got a word here, set output */
	if (processed_word) {
		/* Set outputs */
		*byte_offset_start = current_word_offset_utf8;
		*byte_offset_end = current_word_offset_utf8 + word_length_utf8;

		/* Update cursor */
		parser->cursor += word_length_uchar;

		parser->word_length = strlen (processed_word);
		parser->word = processed_word;

		return TRUE;
	}

	/* No more words... */
	return FALSE;
}

TrackerParser *
tracker_parser_new (TrackerLanguage *language)
{
	TrackerParser *parser;

	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), NULL);

	parser = g_new0 (TrackerParser, 1);

	parser->language = g_object_ref (language);

	return parser;
}

void
tracker_parser_free (TrackerParser *parser)
{
	g_return_if_fail (parser != NULL);

	if (parser->language) {
		g_object_unref (parser->language);
	}

	if (parser->bi) {
		ubrk_close (parser->bi);
	}

	g_free (parser->utxt);
	g_free (parser->offsets);

	g_free (parser->word);

	g_free (parser);
}

void
tracker_parser_reset (TrackerParser *parser,
                      const gchar   *txt,
                      gint           txt_size,
                      guint          max_word_length,
                      gboolean       enable_stemmer,
                      gboolean       enable_unaccent,
                      gboolean       ignore_stop_words,
                      gboolean       ignore_reserved_words,
                      gboolean       ignore_numbers)
{
	UErrorCode error = U_ZERO_ERROR;
	UConverter *converter;
	UChar *last_uchar;
	const gchar *last_utf8;

	g_return_if_fail (parser != NULL);
	g_return_if_fail (txt != NULL);

	parser->max_word_length = max_word_length;
	parser->enable_stemmer = enable_stemmer;
	parser->enable_unaccent = enable_unaccent;
	parser->ignore_stop_words = ignore_stop_words;
	parser->ignore_reserved_words = ignore_reserved_words;
	parser->ignore_numbers = ignore_numbers;

	/* Note: We're forcing some unicode characters to behave
	 * as wordbreakers: e.g, the '.' The main reason for this
	 * is to enable FTS searches matching file extension. */
	parser->enable_forced_wordbreaks = TRUE;

	parser->txt_size = txt_size;
	parser->txt = txt;

	g_free (parser->word);
	parser->word = NULL;

	if (parser->bi) {
		ubrk_close (parser->bi);
		parser->bi = NULL;
	}
	g_free (parser->utxt);
	parser->utxt = NULL;
	g_free (parser->offsets);
	parser->offsets = NULL;

	parser->word_position = 0;

	parser->cursor = 0;

	/* Open converter UTF-8 to UChar */
	converter = ucnv_open ("UTF-8", &error);
	if (!converter) {
		g_warning ("Cannot open UTF-8 converter: '%s'",
		           U_FAILURE (error) ? u_errorName (error) : "none");
		return;
	}

	/* Allocate UChars and offsets buffers */
	parser->utxt_size = txt_size + 1;
	parser->utxt = g_malloc (parser->utxt_size * sizeof (UChar));
	parser->offsets = g_malloc (parser->utxt_size * sizeof (gint32));

	/* last_uchar and last_utf8 will be also an output parameter! */
	last_uchar = parser->utxt;
	last_utf8 = parser->txt;

	/* Convert to UChars storing offsets */
	ucnv_toUnicode (converter,
	                &last_uchar,
	                &parser->utxt[txt_size],
	                &last_utf8,
	                &parser->txt[txt_size],
	                parser->offsets,
	                FALSE,
	                &error);
	if (U_SUCCESS (error)) {
		/* Proper UChar array size is now given by 'last_uchar' */
		parser->utxt_size = last_uchar - parser->utxt;

		/* Open word-break iterator */
		parser->bi = ubrk_open(UBRK_WORD,
		                       setlocale (LC_ALL, NULL),
		                       parser->utxt,
		                       parser->utxt_size,
		                       &error);
		if (U_SUCCESS (error)) {
			/* Find FIRST word in the UChar array */
			parser->cursor = ubrk_first (parser->bi);
		}
	}

	/* If any error happened, reset buffers */
	if (U_FAILURE (error)) {
		g_warning ("Error initializing libicu support: '%s'",
		           u_errorName (error));
		/* Reset buffers */
		g_free (parser->utxt);
		parser->utxt = NULL;
		g_free (parser->offsets);
		parser->offsets = NULL;
		parser->utxt_size = 0;
		if (parser->bi) {
			ubrk_close (parser->bi);
			parser->bi = NULL;
		}
	}

	/* Close converter */
	ucnv_close (converter);
}

const gchar *
tracker_parser_next (TrackerParser *parser,
                     gint          *position,
                     gint          *byte_offset_start,
                     gint          *byte_offset_end,
                     gboolean      *stop_word,
                     gint          *word_length)
{
	const gchar  *str;
	gint byte_start = 0, byte_end = 0;

	str = NULL;

	g_free (parser->word);
	parser->word = NULL;

	*stop_word = FALSE;

	if (parser_next (parser, &byte_start, &byte_end, stop_word)) {
		str = parser->word;
	}

	if (!*stop_word) {
		parser->word_position++;
	}

	*word_length = parser->word_length;
	*position = parser->word_position;
	*byte_offset_start = byte_start;
	*byte_offset_end = byte_end;

	return str;
}

