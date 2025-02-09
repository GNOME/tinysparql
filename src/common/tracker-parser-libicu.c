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
#include <unicode/ucol.h>

#include "tracker-language.h"
#include "tracker-debug.h"
#include "tracker-parser.h"
#include "tracker-parser-utils.h"

/* Type of words detected */
typedef enum {
	TRACKER_PARSER_WORD_TYPE_ASCII,
	TRACKER_PARSER_WORD_TYPE_OTHER_UNAC,
	TRACKER_PARSER_WORD_TYPE_OTHER_NO_UNAC,
} TrackerParserWordType;

typedef UCollator TrackerCollator;

/* Max possible length of a UChar encoded string (just a safety limit) */
#define WORD_BUFFER_LENGTH 512

/* A character encoded in 2 bytes in UTF-16 may get expanded to
 * 3 or 4 bytes in UTF-8.
 */
#define WORD_BUFFER_LENGTH_UTF8 (2 * WORD_BUFFER_LENGTH * sizeof (UChar) + 1)

struct TrackerParser {
	const gchar           *txt;
	gint                   txt_size;

	TrackerLanguage       *language;
	guint                  max_word_length;
	gboolean               enable_stemmer;
	gboolean               enable_unaccent;
	gboolean               ignore_numbers;
	gboolean               enable_forced_wordbreaks;

	/* Private members */
	gchar                  word[WORD_BUFFER_LENGTH_UTF8];
	gint                   word_length;
	guint                  word_position;

	/* Text as UChars */
	UConverter *converter;
	UChar                 *utxt;
	gsize                  utxt_size;
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

/* The input word in this method MUST be normalized in NFKD form,
 * and given in UChars, where str_length is the number of UChars
 * (not the number of bytes) */
static gboolean
tracker_parser_unaccent_nfkd_string (gpointer  str,
                                     gsize    *str_length)
{
	UChar *word;
	gsize word_length;
	gsize i;
	gsize j;

	g_return_val_if_fail (str != NULL, FALSE);
	g_return_val_if_fail (str_length != NULL, FALSE);

	word = (UChar *)str;
	word_length = *str_length;

	i = 0;
	j = 0;
	while (i < word_length) {
		UChar32 unichar;
		gint utf16_len; /* given in UChars */
		gsize aux_i;

		/* Get next character of the word as UCS4 */
		aux_i = i;
		U16_NEXT (word, aux_i, word_length, unichar);
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
	*str_length = j;

	return TRUE;
}

static gboolean
convert_UChar_to_utf8 (TrackerParser *parser,
                       const UChar   *word,
                       gsize          uchar_len)
{
	UErrorCode icu_error = U_ZERO_ERROR;

	g_return_val_if_fail (word, FALSE);

	/* Convert from UChar to UTF-8 (NIL-terminated) */
	parser->word_length = ucnv_fromUChars (parser->converter,
	                                       (gchar *) &parser->word,
	                                       WORD_BUFFER_LENGTH_UTF8,
	                                       word,
	                                       uchar_len,
	                                       &icu_error);
	if (U_FAILURE (icu_error)) {
		g_warning ("Cannot convert from UChar to UTF-8: '%s'",
		           u_errorName (icu_error));
		return FALSE;
	}

	return TRUE;
}

static gboolean
process_word_uchar (TrackerParser         *parser,
                    const UChar           *word,
                    gint                   length,
                    TrackerParserWordType  type)
{
	UErrorCode error = U_ZERO_ERROR;
	UChar normalized_buffer[WORD_BUFFER_LENGTH];
	gsize new_word_length;

	/* Log original word */
	tracker_parser_message_hex ("ORIGINAL word",
	                            (guint8 *)word,
	                            length * sizeof (UChar));


	if (type != TRACKER_PARSER_WORD_TYPE_ASCII) {
		UChar casefolded_buffer [WORD_BUFFER_LENGTH];
		const UNormalizer2 *normalizer;

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
			return FALSE;
		}
		if (new_word_length > WORD_BUFFER_LENGTH)
			new_word_length = WORD_BUFFER_LENGTH;

		/* Log after casefolding */
		tracker_parser_message_hex (" After Casefolding",
		                            (guint8 *)casefolded_buffer,
		                            new_word_length * sizeof (UChar));

		/* NFKD normalization... */
		normalizer = unorm2_getNFKDInstance (&error);

		if (U_SUCCESS (error)) {
			new_word_length = unorm2_normalize (normalizer,
			                                    casefolded_buffer,
			                                    new_word_length,
			                                    normalized_buffer,
			                                    WORD_BUFFER_LENGTH,
			                                    &error);
		}

		if (U_FAILURE (error)) {
			g_warning ("Error normalizing: '%s'",
			           u_errorName (error));
			return FALSE;
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
			return FALSE;
		}

		/* Log after casefolding */
		tracker_parser_message_hex (" After lowercase",
		                            (guint8 *) normalized_buffer,
		                            new_word_length * sizeof (UChar));
	}

	/* UNAC stripping needed? (for non-CJK and non-ASCII) */
	if (parser->enable_unaccent &&
	    type == TRACKER_PARSER_WORD_TYPE_OTHER_UNAC &&
	    tracker_parser_unaccent_nfkd_string (normalized_buffer, &new_word_length)) {
		/* Log after unaccenting */
		tracker_parser_message_hex ("  After UNAC",
		                            (guint8 *) normalized_buffer,
		                            new_word_length * sizeof (UChar));
	}

	/* Finally, convert to UTF-8 */
	if (!convert_UChar_to_utf8 (parser,
	                            normalized_buffer,
	                            new_word_length))
		return FALSE;

	/* Log after unaccenting */
	tracker_parser_message_hex ("   After UTF8 conversion",
	                            &parser->word,
	                            parser->word_length);

	/* Stemming needed? */
	if (parser->enable_stemmer) {
		/* Input for stemmer ALWAYS in UTF-8, as well as output */
		tracker_language_stem_word (parser->language,
		                            (gchar *) &parser->word,
		                            &parser->word_length,
		                            WORD_BUFFER_LENGTH_UTF8);

		/* Log after stemming */
		tracker_parser_message_hex ("    After stemming",
		                            &parser->word,
		                            parser->word_length);
	}

	return TRUE;
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
             gint          *byte_offset_end)
{
	gsize word_length_uchar = 0;
	gsize word_length_utf8 = 0;
	gsize current_word_offset_utf8 = 0;

	*byte_offset_start = 0;
	*byte_offset_end = 0;

	g_return_val_if_fail (parser, FALSE);

	/* Loop to look for next valid word */
	while (parser->cursor < parser->utxt_size) {
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

		/* compute truncated word length (in UChar bytes) if needed (to
		 * avoid extremely long words) */
		truncated_length = (word_length_uchar < 2 * WORD_BUFFER_LENGTH ?
		                    word_length_uchar :
		                    2 * WORD_BUFFER_LENGTH);

		/* Process the word here. If it fails, we can still go
		 *  to the next one.
		 * Enable UNAC stripping only if no ASCII and no CJK
		 * Note we are passing UChar encoded string here!
		 */
		if (process_word_uchar (parser,
		                        &(parser->utxt[parser->cursor]),
		                        truncated_length,
		                        type)) {
			/* Set outputs */
			*byte_offset_start = current_word_offset_utf8;
			*byte_offset_end = current_word_offset_utf8 + word_length_utf8;

			/* Update cursor */
			parser->cursor += word_length_uchar;

			return TRUE;
		}

		/* Ignore this word and keep on looping */
		parser->cursor = next_word_offset_uchar;
	}

	/* No more words... */
	return FALSE;
}

TrackerParser *
tracker_parser_new (void)
{
	TrackerParser *parser;

	parser = g_new0 (TrackerParser, 1);
	parser->language = tracker_language_new (NULL);

	return parser;
}

void
tracker_parser_free (TrackerParser *parser)
{
	g_return_if_fail (parser != NULL);

	g_clear_object (&parser->language);
	g_clear_pointer (&parser->converter, ucnv_close);
	g_clear_pointer (&parser->bi, ubrk_close);

	g_free (parser->utxt);
	g_free (parser->offsets);

	g_free (parser);
}

void
tracker_parser_reset (TrackerParser *parser,
                      const gchar   *txt,
                      gint           txt_size,
                      guint          max_word_length,
                      gboolean       enable_stemmer,
                      gboolean       enable_unaccent,
                      gboolean       ignore_numbers)
{
	UErrorCode error = U_ZERO_ERROR;
	UChar *last_uchar;
	const gchar *last_utf8;

	g_return_if_fail (parser != NULL);
	g_return_if_fail (txt != NULL);

	parser->max_word_length = max_word_length;
	parser->enable_stemmer = enable_stemmer;
	parser->enable_unaccent = enable_unaccent;
	parser->ignore_numbers = ignore_numbers;

	/* Note: We're forcing some unicode characters to behave
	 * as wordbreakers: e.g, the '.' The main reason for this
	 * is to enable FTS searches matching file extension. */
	parser->enable_forced_wordbreaks = TRUE;

	parser->txt_size = txt_size;
	parser->txt = txt;

	parser->word[0] = '\0';
	parser->word_length = 0;
	g_clear_pointer (&parser->bi, ubrk_close);
	g_clear_pointer (&parser->utxt, g_free);
	g_clear_pointer (&parser->offsets, g_free);

	parser->word_position = 0;
	parser->cursor = 0;

	if (parser->txt_size == 0)
		return;

	/* Open converter UTF-8 to UChar */
	if (!parser->converter) {
		parser->converter = ucnv_open ("UTF-8", &error);
		if (!parser->converter) {
			g_warning ("Cannot open UTF-8 converter: '%s'",
			           U_FAILURE (error) ? u_errorName (error) : "none");
			return;
		}
	}

	/* Allocate UChars and offsets buffers */
	parser->utxt_size = txt_size + 1;
	parser->utxt = g_malloc (parser->utxt_size * sizeof (UChar));
	parser->offsets = g_malloc (parser->utxt_size * sizeof (gint32));

	/* last_uchar and last_utf8 will be also an output parameter! */
	last_uchar = parser->utxt;
	last_utf8 = parser->txt;

	/* Convert to UChars storing offsets */
	ucnv_toUnicode (parser->converter,
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
		                       setlocale (LC_CTYPE, NULL),
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
		g_clear_pointer (&parser->utxt, g_free);
		g_clear_pointer (&parser->offsets, g_free);
		g_clear_pointer (&parser->bi, ubrk_close);
		parser->utxt_size = 0;
	}
}

const gchar *
tracker_parser_next (TrackerParser *parser,
                     gint          *position,
                     gint          *byte_offset_start,
                     gint          *byte_offset_end,
                     gint          *word_length)
{
	gint byte_start = 0, byte_end = 0;

	parser->word[0] = '\0';
	parser->word_length = 0;

	if (!parser_next (parser, &byte_start, &byte_end))
		return NULL;

	parser->word_position++;

	*word_length = parser->word_length;
	*position = parser->word_position;
	*byte_offset_start = byte_start;
	*byte_offset_end = byte_end;

	return (const gchar *) &parser->word;
}

gpointer
tracker_collation_init (void)
{
	UCollator *collator = NULL;
	UErrorCode status = U_ZERO_ERROR;
	const gchar *locale;

	/* Get locale! */
	locale = setlocale (LC_COLLATE, NULL);

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
tracker_collation_shutdown (gpointer collator)
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

gunichar2 *
tracker_parser_tolower (const gunichar2 *input,
			gsize            len,
			gsize           *len_out)
{
	UChar *zOutput;
	int nOutput;
	UErrorCode status = U_ZERO_ERROR;

	g_return_val_if_fail (input, NULL);

	nOutput = len * 2 + 2;
	zOutput = malloc (nOutput);

	u_strToLower (zOutput, nOutput / 2,
		      input, len / 2,
		      NULL, &status);

	if (!U_SUCCESS (status)) {
		memcpy (zOutput, input, len);
		zOutput[len] = '\0';
		nOutput = len;
	}

	*len_out = nOutput;

	return zOutput;
}

gunichar2 *
tracker_parser_toupper (const gunichar2 *input,
			gsize            len,
			gsize           *len_out)
{
	UChar *zOutput;
	int nOutput;
	UErrorCode status = U_ZERO_ERROR;

	nOutput = len * 2 + 2;
	zOutput = malloc (nOutput);

	u_strToUpper (zOutput, nOutput / 2,
		      input, len / 2,
		      NULL, &status);

	if (!U_SUCCESS (status)) {
		memcpy (zOutput, input, len);
		zOutput[len] = '\0';
		nOutput = len;
	}

	*len_out = nOutput;

	return zOutput;
}

gunichar2 *
tracker_parser_casefold (const gunichar2 *input,
			 gsize            len,
			 gsize           *len_out)
{
	UChar *zOutput;
	int nOutput;
	UErrorCode status = U_ZERO_ERROR;

	nOutput = len * 2 + 2;
	zOutput = malloc (nOutput);

	u_strFoldCase (zOutput, nOutput / 2,
		       input, len / 2,
		       U_FOLD_CASE_DEFAULT, &status);

	if (!U_SUCCESS (status)){
		memcpy (zOutput, input, len);
		zOutput[len] = '\0';
		nOutput = len;
	}

	*len_out = nOutput;

	return zOutput;
}

static gunichar2 *
normalize_string (const gunichar2    *string,
                  gsize               string_len, /* In gunichar2s */
                  const UNormalizer2 *normalizer,
                  gsize              *len_out,    /* In gunichar2s */
                  UErrorCode         *status)
{
	int nOutput;
	gunichar2 *zOutput;

	nOutput = (string_len * 2) + 1;
	zOutput = g_new0 (gunichar2, nOutput);

	nOutput = unorm2_normalize (normalizer, string, string_len, zOutput, nOutput, status);

	if (*status == U_BUFFER_OVERFLOW_ERROR) {
		/* Try again after allocating enough space for the normalization */
		*status = U_ZERO_ERROR;
		zOutput = g_renew (gunichar2, zOutput, nOutput);
		memset (zOutput, 0, nOutput * sizeof (gunichar2));
		nOutput = unorm2_normalize (normalizer, string, string_len, zOutput, nOutput, status);
	}

	if (!U_SUCCESS (*status)) {
		g_clear_pointer (&zOutput, g_free);
		nOutput = 0;
	}

	if (len_out)
		*len_out = nOutput;

	return zOutput;
}

gunichar2 *
tracker_parser_normalize (const gunichar2 *input,
                          GNormalizeMode   mode,
			  gsize            len,
			  gsize           *len_out)
{
	uint16_t *zOutput = NULL;
	gsize nOutput;
	const UNormalizer2 *normalizer;
	UErrorCode status = U_ZERO_ERROR;

	if (mode == G_NORMALIZE_NFC)
		normalizer = unorm2_getNFCInstance (&status);
	else if (mode == G_NORMALIZE_NFD)
		normalizer = unorm2_getNFDInstance (&status);
	else if (mode == G_NORMALIZE_NFKC)
		normalizer = unorm2_getNFKCInstance (&status);
	else if (mode == G_NORMALIZE_NFKD)
		normalizer = unorm2_getNFKDInstance (&status);
	else
		g_assert_not_reached ();

	if (U_SUCCESS (status)) {
		zOutput = normalize_string (input, len / 2,
					    normalizer,
					    &nOutput, &status);
	}

	if (!U_SUCCESS (status)) {
		zOutput = g_memdup2 (input, len);
		nOutput = len;
	}

	*len_out = nOutput;

	return zOutput;
}

gunichar2 *
tracker_parser_unaccent (const gunichar2 *input,
			 gsize            len,
			 gsize           *len_out)
{
	uint16_t *zOutput = NULL;
	gsize nOutput;
	const UNormalizer2 *normalizer;
	UErrorCode status = U_ZERO_ERROR;

	normalizer = unorm2_getNFKDInstance (&status);

	if (U_SUCCESS (status)) {
		zOutput = normalize_string (input, len / 2,
					    normalizer,
					    &nOutput, &status);
	}

	if (!U_SUCCESS (status)) {
		zOutput = g_memdup2 (input, len);
		nOutput = len;
	}

	/* Unaccenting is done in place */
	tracker_parser_unaccent_nfkd_string (zOutput, &nOutput);

	*len_out = nOutput;

	return zOutput;
}
