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

static gchar *process_word_utf8 (TrackerParser         *parser,
                                 const gchar           *word,
                                 gint                  length,
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
	gchar                   *word;
	gint                    word_length;
	guint                   word_position;

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
	gsize i;
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

	/* Find next word break, and in the same loop checking if only ASCII
	 *  characters */
	i = parser->cursor + first_unichar_len;
	while (i < parser->txt_size &&
	       !parser->word_break_flags [i]) {

		if (ascii_only &&
		    !IS_ASCII_UCS4 ((guint32)parser->txt[i])) {
			ascii_only = FALSE;
		}

		i++;
	}

	/* Word end is the first byte after the word, which is either the
	 *  start of next word or the end of the string */
	*p_word_length = i - parser->cursor;

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
parser_next (TrackerParser *parser,
             gint          *byte_offset_start,
             gint          *byte_offset_end)
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

		/* Skip the word if not an allowed word start */
		if (!is_allowed) {
			/* Skip this word and keep on looping */
			parser->cursor += word_length;
			continue;
		}

		/* Skip the word if longer than the maximum allowed */
		if (word_length >= parser->max_word_length) {
			/* Skip this word and keep on looping */
			parser->cursor += word_length;
			continue;
		}

		/* check if word is reserved and skip it if so */
		if (parser->skip_reserved_words &&
		    tracker_parser_is_reserved_word_utf8 (&parser->txt[parser->cursor],
		                                          word_length)) {
			/* Skip this word and keep on looping */
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
		                                    type);
		if (!processed_word) {
			/* Skip this word and keep on looping */
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

	parser->word_break_flags = NULL;

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
                      gboolean       delimit_words,
                      gboolean       enable_stemmer,
                      gboolean       enable_stop_words,
                      gboolean       skip_reserved_words,
                      gboolean       skip_numbers)
{
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
	if (!parser->skip_numbers) {
		parser->allowed_start = uc_general_category_or (parser->allowed_start, UC_NUMBER);
	}
}

gchar *
tracker_parser_process_word (TrackerParser *parser,
                             const gchar    *word,
                             gint           length,
                             gboolean       do_strip)
{
	return process_word_utf8 (parser,
	                          word,
	                          length,
	                          (do_strip ?
	                           TRACKER_PARSER_WORD_TYPE_OTHER_UNAC :
	                           TRACKER_PARSER_WORD_TYPE_OTHER_NO_UNAC));
}

static gchar *
process_word_utf8 (TrackerParser         *parser,
                   const gchar           *word,
                   gint                  length,
                   TrackerParserWordType type)
{
	gchar word_buffer [WORD_BUFFER_LENGTH];
	gchar *normalized = NULL;
	gchar *stripped = NULL;
	gchar *stemmed = NULL;
	size_t new_word_length;

	g_return_val_if_fail (parser != NULL, NULL);
	g_return_val_if_fail (word != NULL, NULL);

	/* If length is set as -1, the input word MUST be NIL-terminated.
	 * Otherwise, this restriction is not needed as the length to process
	 *  is given as input argument */
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

		/* Casefold and NFC normalization in output.
		 *  NOTE: if the output buffer is not big enough, u8_casefold will
		 *  return a newly-allocated buffer. */
		normalized = u8_casefold ((const uint8_t *)word,
		                          length,
		                          uc_locale_language (),
		                          UNINORM_NFC,
		                          word_buffer,
		                          &new_word_length);

		/* Case folding + Normalization failed, skip this word */
		g_return_val_if_fail (normalized != NULL, NULL);

		/* If output buffer is not the same as the one passed to
		 *  u8_casefold, we know it was newly-allocated, so need
		 *  to resize it in 1 byte to add last NIL */
		if (normalized != word_buffer) {
			normalized = g_realloc (normalized, new_word_length + 1);
		}

		/* Log after Normalization */
		tracker_parser_message_hex (" After Casefolding and NFC normalization",
		                            normalized, new_word_length);
	}
	else {
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
	if (type == TRACKER_PARSER_WORD_TYPE_OTHER_UNAC) {
		gsize stripped_word_length;

		stripped = tracker_parser_unaccent_utf8_word (normalized,
		                                              new_word_length,
		                                              &stripped_word_length);

		if (stripped) {
			/* Log after UNAC stripping */
			tracker_parser_message_hex ("  After UNAC stripping",
			                            stripped, stripped_word_length);
			new_word_length = stripped_word_length;
		}
	}

	/* Stemming needed? */
	if (parser->enable_stemmer) {
		stemmed = tracker_language_stem_word (parser->language,
		                                      stripped ? stripped : normalized,
		                                      new_word_length);

		/* Log after stemming */
		tracker_parser_message_hex ("   After stemming",
		                            stemmed, strlen (stemmed));
	}

	/* If stemmed wanted and succeeded, free previous and return it */
	if (stemmed) {
		g_free (stripped);
		if (normalized != word_buffer) {
			g_free (normalized);
		}
		return stemmed;
	}

	/* If stripped wanted and succeeded, free previous and return it */
	if (stripped) {
		if (normalized != word_buffer) {
			g_free (normalized);
		}
		return stripped;
	}

	/* It may be the case that no stripping and no stemming was needed, and
	 * that the output buffer in stack was enough for case-folding and
	 * normalization. In this case, need to strdup() the string to return it */
	return normalized == word_buffer ? g_strdup (word_buffer) : normalized;
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

