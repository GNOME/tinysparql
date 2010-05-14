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


static gchar *process_word_uchar (TrackerParser *parser,
                                  const UChar   *word,
                                  gint           length,
                                  TrackerParserWordType type);


struct TrackerParser {
	const gchar           *txt;
	gint                   txt_size;

	TrackerLanguage       *language;
	gboolean               enable_stemmer;
	gboolean               enable_stop_words;
	guint                  max_words_to_index;
	guint                  max_word_length;
	gboolean               delimit_words;
	gboolean               skip_reserved_words;
	gboolean               skip_numbers;

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
               gboolean               skip_numbers,
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
	 *  in the word is either a letter, a number or a symbol.
	 * This is needed because the word break algorithm also
	 *  considers word breaks after for example commas or other
	 *  punctuation marks.
	 * Note that looking at the first character in the string
	 *  should be compatible with all Unicode normalization
	 *  methods.
	 */
	unichar_gc = u_charType (unichar);
	if (unichar_gc == U_UPPERCASE_LETTER ||
	    unichar_gc == U_LOWERCASE_LETTER ||
	    unichar_gc == U_TITLECASE_LETTER ||
	    unichar_gc == U_MODIFIER_LETTER ||
	    unichar_gc == U_OTHER_LETTER ||
	    IS_UNDERSCORE_UCS4 ((guint32)unichar) ||
	    (!skip_numbers &&
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
	while (unichar != U_SENTINEL)
	{
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
parser_next (TrackerParser *parser,
             gint          *byte_offset_start,
             gint          *byte_offset_end)
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
		if (next_word_offset_uchar >= parser->utxt_size) {
			/* Last word support... */
			next_word_offset_uchar = parser->utxt_size;
			next_word_offset_utf8 = parser->txt_size;
		}
		else {
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

		/* Skip the word if longer than the maximum allowed */
		if (word_length_utf8 >= parser->max_word_length) {
			/* Skip this word and keep on looping */
			parser->cursor = next_word_offset_uchar;
			continue;
		}

		/* Get word info... */
		if (!get_word_info (&parser->utxt[parser->cursor],
		                    word_length_uchar,
		                    parser->skip_numbers,
		                    &is_allowed,
		                    &type)) {
			/* Quit loop just in case */
			parser->cursor = parser->utxt_size;
			break;
		}

		/* Skip the word if not an allowed word start */
		if (!is_allowed) {
			/* Skip this word and keep on looping */
			parser->cursor = next_word_offset_uchar;
			continue;
		}

		/* check if word is reserved (looking at ORIGINAL UTF-8 buffer here! */
		if (parser->skip_reserved_words &&
		    tracker_parser_is_reserved_word_utf8 (&parser->txt[current_word_offset_utf8],
		                                          word_length_utf8)) {
			/* Skip this word and keep on looping */
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
		                                     type);
		if (!processed_word) {
			/* Skip this word and keep on looping */
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
tracker_parser_new (TrackerLanguage *language,
                    gint             max_word_length)
{
	TrackerParser *parser;

	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), NULL);
	g_return_val_if_fail (max_word_length > 0, NULL);

	parser = g_new0 (TrackerParser, 1);

	parser->language = g_object_ref (language);

	parser->max_word_length = max_word_length;
	parser->word_length = 0;

	parser->utxt = NULL;
	parser->offsets = NULL;
	parser->utxt_size = 0;
	parser->bi = NULL;
	parser->cursor = 0;

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
                      gboolean       delimit_words,
                      gboolean       enable_stemmer,
                      gboolean       enable_stop_words,
                      gboolean       skip_reserved_words,
                      gboolean       skip_numbers)
{
	UErrorCode error = U_ZERO_ERROR;
	UConverter *converter;
	UChar *last_uchar;
	const gchar *last_utf8;

	g_return_if_fail (parser != NULL);
	g_return_if_fail (txt != NULL);

	parser->enable_stemmer = enable_stemmer;
	parser->enable_stop_words = enable_stop_words;
	parser->delimit_words = delimit_words;

	parser->txt_size = txt_size;
	parser->txt = txt;
	parser->skip_reserved_words = skip_reserved_words;
	parser->skip_numbers = skip_numbers;

	g_free (parser->word);
	parser->word = NULL;

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
		g_free (parser->offsets);
		parser->utxt = NULL;
		parser->offsets = NULL;
		parser->utxt_size = 0;
	}

	/* Close converter */
	ucnv_close (converter);
}

static gchar *
process_word_uchar (TrackerParser         *parser,
                    const UChar           *word,
                    gint                   length,
                    TrackerParserWordType  type)
{
	UErrorCode error = U_ZERO_ERROR;
	UChar normalized_buffer [WORD_BUFFER_LENGTH];
	gchar *utf8_str = NULL;
	gchar *stemmed = NULL;
	size_t new_word_length;


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

		/* NFC normalization... */
		new_word_length = unorm_normalize (casefolded_buffer,
		                                   new_word_length,
		                                   UNORM_NFC,
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
	}

	/* UNAC stripping needed? (for non-CJK and non-ASCII) */
	if (type == TRACKER_PARSER_WORD_TYPE_OTHER_UNAC) {
		gsize stripped_word_length;

		/* Get unaccented string in UTF-8 */
		utf8_str = tracker_parser_unaccent_UChar_word (normalized_buffer,
		                                               new_word_length,
		                                               &stripped_word_length);
		if (utf8_str) {
			new_word_length = stripped_word_length;
		}
	}

	/* If stripping failed or not needed, convert to UTF-8 */
	if (!utf8_str) {
		UErrorCode icu_error = U_ZERO_ERROR;
		UConverter *converter;
		gsize utf8_len;

		/* Open converter UChar to UTF-16BE */
		converter = ucnv_open ("UTF-8", &icu_error);
		if (!converter) {
			g_warning ("Cannot open UTF-8 converter: '%s'",
			           U_FAILURE (icu_error) ? u_errorName (icu_error) : "none");
			return NULL;
		}
		/* Using same buffer size as for UTF-16 should always work. */
		utf8_str = g_malloc (new_word_length * sizeof (UChar) + 1);

		/* Convert from UChar to UTF-8 (NIL-terminated) */
		utf8_len = ucnv_fromUChars (converter,
		                            utf8_str,
		                            new_word_length * sizeof (UChar) + 1,
		                            normalized_buffer,
		                            new_word_length,
		                            &icu_error);
		if (U_FAILURE (icu_error)) {
			g_warning ("Cannot convert from UChar to UTF-8: '%s'",
			           u_errorName (icu_error));
			g_free (utf8_str);
			ucnv_close (converter);
			return NULL;
		}

		new_word_length = utf8_len;
		ucnv_close (converter);
	}

	/* Stemming needed? */
	if (parser->enable_stemmer) {
		/* Input for stemmer ALWAYS in UTF-8, as well as output */
		stemmed = tracker_language_stem_word (parser->language,
		                                      utf8_str,
		                                      new_word_length);

		/* Log after stemming */
		tracker_parser_message_hex ("   After stemming",
		                            stemmed, strlen (stemmed));
	}

	/* If stemmed wanted and succeeded, free previous and return it */
	if (stemmed) {
		g_free (utf8_str);
		return stemmed;
	}

	return utf8_str;
}


/* Both Input and Output are always UTF-8 */
gchar *
tracker_parser_process_word (TrackerParser *parser,
                             const gchar   *word,
                             gint           length,
                             gboolean       do_strip)
{
	UErrorCode icu_error = U_ZERO_ERROR;
	UConverter *converter;
	UChar *uchar_word;
	gsize uchar_len;
	gchar *processed;

	/* Open converter UTF-8 to UChar */
	converter = ucnv_open ("UTF-8", &icu_error);
	if (!converter) {
		g_warning ("Cannot open UTF-8 converter: '%s'",
		           U_FAILURE (icu_error) ? u_errorName (icu_error) : "none");
		return NULL;
	}

	/* Compute length if not already as input */
	if (length < 0) {
		length = strlen (word);
	}

	/* Twice the size of the UTF-8 string for UChars */
	uchar_word = g_malloc (2 * length);

	/* Convert from UTF-8 to UChars*/
	uchar_len = ucnv_toUChars (converter,
	                           uchar_word,
	                           2 * length,
	                           word,
	                           length,
	                           &icu_error);
	if (U_FAILURE (icu_error)) {
		g_warning ("Cannot convert from UTF-8 to UChar: '%s'",
		           u_errorName (icu_error));
		g_free (uchar_word);
		ucnv_close (converter);
		return NULL;
	}

	ucnv_close (converter);

	/* Process UChar based word */
	processed = process_word_uchar (parser,
	                                uchar_word,
	                                uchar_len,
	                                do_strip);
	g_free (uchar_word);
	return processed;
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
	gint     byte_start = 0, byte_end = 0;

	str = NULL;

	g_free (parser->word);
	parser->word = NULL;

	if (parser_next (parser, &byte_start, &byte_end)) {
		str = parser->word;
	}

	if (str &&
	    parser->enable_stop_words &&
	    tracker_language_is_stop_word (parser->language, str)) {
		*stop_word = TRUE;
	} else {
		parser->word_position++;
		*stop_word = FALSE;
	}

	*word_length = parser->word_length;
	*position = parser->word_position;
	*byte_offset_start = byte_start;
	*byte_offset_end = byte_end;

	return str;
}

