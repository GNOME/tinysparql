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

/* libunistring versions prior to 9.1.2 need this hack */
#define _UNUSED_PARAMETER_
#include <unistr.h>
#include <uniwbrk.h>
#include <unictype.h>
#include <unicase.h>

#include "tracker-parser.h"
#include "tracker-parser-utils.h"

/* Type of words detected */
typedef enum {
	TRACKER_PARSER_WORD_TYPE_ASCII,
	TRACKER_PARSER_WORD_TYPE_OTHER_UNAC,
	TRACKER_PARSER_WORD_TYPE_OTHER_NO_UNAC,
} TrackerParserWordType;

/* Max possible length of a UTF-8 encoded string (just a safety limit) */
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

	/* Cursor, as index of the input array of bytes */
	gsize                  cursor;
	/* libunistring flags array */
	gchar                 *word_break_flags;
	/* general category of the  start character in words */
	uc_general_category_t  allowed_start;
};

static gboolean
get_word_info (TrackerParser         *parser,
               gsize                 *p_word_length,
               gboolean              *p_is_allowed_word_start,
               TrackerParserWordType *p_word_type)
{
	ucs4_t first_unichar;
	gint first_unichar_len;
	gboolean ascii_only;

	/* Defaults */
	*p_is_allowed_word_start = TRUE;

	/* Get first character of the word as UCS4 */
	first_unichar_len = u8_strmbtouc (&first_unichar,
	                                  &(parser->txt[parser->cursor]));
	if (first_unichar_len <= 0) {
		/* This should only happen if NIL was passed to u8_strmbtouc,
		 *  so better just force stop here */
		return FALSE;
	} else  {
		/* If first character has length 1, it's ASCII-7 */
		ascii_only = first_unichar_len == 1 ? TRUE : FALSE;
	}

	/* Consider word starts with a forced wordbreak */
	if (parser->enable_forced_wordbreaks &&
	    IS_FORCED_WORDBREAK_UCS4 ((guint32)first_unichar)) {
		*p_word_length = first_unichar_len;
	} else {
		gsize i;

		/* Find next word break, and in the same loop checking if only ASCII
		 *  characters */
		i = parser->cursor + first_unichar_len;
		while (1) {
			/* Text bounds reached? */
			if (i >= parser->txt_size)
				break;
			/* Proper unicode word break detected? */
			if (parser->word_break_flags[i])
				break;
			/* Forced word break detected? */
			if (parser->enable_forced_wordbreaks &&
			    IS_FORCED_WORDBREAK_UCS4 ((guint32)parser->txt[i]))
				break;

			if (ascii_only &&
			    !IS_ASCII_UCS4 ((guint32)parser->txt[i])) {
				ascii_only = FALSE;
			}

			i++;
		}

		/* Word end is the first byte after the word, which is either the
		 *  start of next word or the end of the string */
		*p_word_length = i - parser->cursor;
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
	if (!IS_UNDERSCORE_UCS4 ((guint32)first_unichar) &&
	    !uc_is_general_category (first_unichar,
	                             parser->allowed_start)) {
		*p_is_allowed_word_start = FALSE;
		return TRUE;
	}

	/* Decide word type */
	if (ascii_only) {
		*p_word_type = TRACKER_PARSER_WORD_TYPE_ASCII;
	} else if (IS_CJK_UCS4 (first_unichar)) {
		*p_word_type = TRACKER_PARSER_WORD_TYPE_OTHER_NO_UNAC;
	} else {
		*p_word_type = TRACKER_PARSER_WORD_TYPE_OTHER_UNAC;
	}
	return TRUE;
}

static gboolean
parser_unaccent_nfkd_word (gchar *word,
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
		ucs4_t unichar;
		gint utf8_len;

		/* Get next character of the word as UCS4 */
		utf8_len = u8_strmbtouc (&unichar, &word[i]);

		/* Invalid UTF-8 character or end of original string. */
		if (utf8_len <= 0) {
			break;
		}

		/* If the given unichar is a combining diacritical mark,
		 * just update the original index, not the output one */
		if (IS_CDM_UCS4 ((guint32) unichar)) {
			i += utf8_len;
			continue;
		}

		/* If already found a previous combining
		 * diacritical mark, indexes are different so
		 * need to copy characters. As output and input
		 * buffers may overlap, need to use memmove
		 * instead of memcpy */
		if (i != j) {
			memmove (&word[j], &word[i], utf8_len);
		}

		/* Update both indexes */
		i += utf8_len;
		j += utf8_len;
	}

	/* Force proper string end */
	word[j] = '\0';

	/* Set new output length */
	*word_length = j;

	return TRUE;
}

static gchar *
process_word_utf8 (TrackerParser         *parser,
                   const gchar           *word,
                   gint                   length,
                   TrackerParserWordType  type,
                   gboolean              *stop_word)
{
	gchar word_buffer [WORD_BUFFER_LENGTH];
	gchar *normalized = NULL;
	gchar *stemmed = NULL;
	size_t new_word_length;

	g_return_val_if_fail (parser != NULL, NULL);
	g_return_val_if_fail (word != NULL, NULL);

	/* If length is set as -1, the input word MUST be NIL-terminated.
	 * Otherwise, this restriction is not needed as the length to process
	 * is given as input argument */
	if (length < 0) {
		length = strlen (word);
	}

	/* Log original word */
	tracker_parser_message_hex ("ORIGINAL word",
	                            word, length);

	/* Normalization and case-folding ONLY for non-ASCII */
	if (type != TRACKER_PARSER_WORD_TYPE_ASCII) {
		/* Leave space for last NIL */
		new_word_length = WORD_BUFFER_LENGTH - 1;

		/* Casefold and NFKD normalization in output.
		 * NOTE: if the output buffer is not big enough, u8_casefold will
		 * return a newly-allocated buffer. */
		normalized = u8_casefold ((const uint8_t *)word,
		                          length,
		                          uc_locale_language (),
		                          UNINORM_NFKD,
		                          word_buffer,
		                          &new_word_length);

		/* Case folding + Normalization failed, ignore this word */
		g_return_val_if_fail (normalized != NULL, NULL);

		/* If output buffer is not the same as the one passed to
		 * u8_casefold, we know it was newly-allocated, so need
		 * to resize it in 1 byte to add last NIL */
		if (normalized != word_buffer) {
			normalized = g_realloc (normalized, new_word_length + 1);
		}

		/* Log after Normalization */
		tracker_parser_message_hex (" After Casefolding and NFKD normalization",
		                            normalized, new_word_length);
	} else {
		/* For ASCII-only, just tolower() each character */
		gsize i;

		normalized = length > WORD_BUFFER_LENGTH ? g_malloc (length + 1) : word_buffer;

		for (i = 0; i < length; i++) {
			normalized[i] = g_ascii_tolower (word[i]);
		}

		new_word_length = length;

		/* Log after tolower */
		tracker_parser_message_hex (" After Lowercasing",
		                            normalized, new_word_length);
	}

	/* Set output NIL */
	normalized[new_word_length] = '\0';

	/* UNAC stripping needed? (for non-CJK and non-ASCII) */
	if (parser->enable_unaccent &&
	    type == TRACKER_PARSER_WORD_TYPE_OTHER_UNAC &&
	    parser_unaccent_nfkd_word (normalized, &new_word_length)) {
		/* Log after UNAC stripping */
		tracker_parser_message_hex ("  After UNAC stripping",
		                            normalized, new_word_length);
	}

	/* Check if stop word */
	if (parser->ignore_stop_words) {
		*stop_word = tracker_language_is_stop_word (parser->language,
		                                            normalized);
	}

	/* Stemming needed? */
	if (parser->enable_stemmer) {
		stemmed = tracker_language_stem_word (parser->language,
		                                      normalized,
		                                      new_word_length);

		/* Log after stemming */
		tracker_parser_message_hex ("   After stemming",
		                            stemmed, strlen (stemmed));
	}

	/* If stemmed wanted and succeeded, free previous and return it */
	if (stemmed) {
		if (normalized != word_buffer) {
			g_free (normalized);
		}
		return stemmed;
	}

	/* It may be the case that no stripping and no stemming was needed, and
	 * that the output buffer in stack was enough for case-folding and
	 * normalization. In this case, need to strdup() the string to return it */
	return normalized == word_buffer ? g_strdup (word_buffer) : normalized;
}

static gboolean
parser_next (TrackerParser *parser,
             gint          *byte_offset_start,
             gint          *byte_offset_end,
             gboolean      *stop_word)
{
	gsize word_length = 0;
	gchar *processed_word = NULL;

	*byte_offset_start = 0;
	*byte_offset_end = 0;

	g_return_val_if_fail (parser, FALSE);

	/* Loop to look for next valid word */
	while (!processed_word &&
	       parser->cursor < parser->txt_size) {
		TrackerParserWordType type;
		gsize truncated_length;
		gboolean is_allowed;

		/* Get word info */
		if (!get_word_info (parser,
		                    &word_length,
		                    &is_allowed,
		                    &type)) {
			/* Quit loop just in case */
			parser->cursor = parser->txt_size;
			break;
		}

		/* Ignore the word if not an allowed word start */
		if (!is_allowed) {
			/* Ignore this word and keep on looping */
			parser->cursor += word_length;
			continue;
		}

		/* Ignore the word if longer than the maximum allowed */
		if (word_length >= parser->max_word_length) {
			/* Ignore this word and keep on looping */
			parser->cursor += word_length;
			continue;
		}

		/* check if word is reserved and ignore it if so */
		if (parser->ignore_reserved_words &&
		    tracker_parser_is_reserved_word_utf8 (&parser->txt[parser->cursor],
		                                          word_length)) {
			/* Ignore this word and keep on looping */
			parser->cursor += word_length;
			continue;
		}

		/* compute truncated word length if needed (to avoid extremely
		 *  long words)*/
		truncated_length = (word_length < WORD_BUFFER_LENGTH ?
		                    word_length :
		                    WORD_BUFFER_LENGTH - 1);

		/* Process the word here. If it fails, we can still go
		 *  to the next one. Returns newly allocated string
		 *  always */
		processed_word = process_word_utf8 (parser,
		                                    &(parser->txt[parser->cursor]),
		                                    truncated_length,
		                                    type,
		                                    stop_word);
		if (!processed_word) {
			/* Ignore this word and keep on looping */
			parser->cursor += word_length;
			continue;
		}
	}

	/* If we got a word here, set output */
	if (processed_word) {
		/* Set outputs */
		*byte_offset_start = parser->cursor;
		*byte_offset_end = parser->cursor + word_length;

		/* Update cursor */
		parser->cursor += word_length;

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

	g_free (parser->word_break_flags);

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

	parser->word_position = 0;

	parser->cursor = 0;

	g_free (parser->word_break_flags);

	/* Create array of flags, same size as original text. */
	parser->word_break_flags = g_malloc (txt_size);

	/* Get wordbreak flags in the whole string */
	u8_wordbreaks ((const uint8_t *)txt,
	               (size_t) txt_size,
	               (char *)parser->word_break_flags);

	/* Prepare a custom category which is a combination of the
	 * desired ones */
	parser->allowed_start = UC_LETTER;
	if (!parser->ignore_numbers) {
		parser->allowed_start = uc_general_category_or (parser->allowed_start, UC_NUMBER);
	}
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

