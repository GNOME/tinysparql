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

#include <string.h>

#include <pango/pango.h>

#include "tracker-parser.h"
#include "tracker-parser-utils.h"

/* Need pango for CJK ranges which are : 0x3400 - 0x4DB5, 0x4E00 -
 * 0x9FA5, 0x20000 - <= 0x2A6D6
 */
#define NEED_PANGO(c)            (((c) >= 0x3400 && (c) <= 0x4DB5)  ||  \
                                  ((c) >= 0x4E00 && (c) <= 0x9FA5)  ||  \
                                  ((c) >= 0x20000 && (c) <= 0x2A6D6))
#define IS_LATIN(c)              (((c) <= 0x02AF) ||	\
                                  ((c) >= 0x1E00 && (c) <= 0x1EFF))
#define IS_ASCII(c)              ((c) <= 0x007F)
#define IS_ASCII_ALPHA_LOWER(c)  ((c) >= 0x0061 && (c) <= 0x007A)
#define IS_ASCII_ALPHA_HIGHER(c) ((c) >= 0x0041 && (c) <= 0x005A)
#define IS_ASCII_NUMERIC(c)      ((c) >= 0x0030 && (c) <= 0x0039)
#define IS_ASCII_IGNORE(c)       ((c) <= 0x002C)
#define IS_HYPHEN(c)             ((c) == 0x002D)
#define IS_UNDERSCORE(c)         ((c) == 0x005F)

typedef enum {
	TRACKER_PARSER_WORD_ASCII_HIGHER,
	TRACKER_PARSER_WORD_ASCII_LOWER,
	TRACKER_PARSER_WORD_HYPHEN,
	TRACKER_PARSER_WORD_UNDERSCORE,
	TRACKER_PARSER_WORD_NUM,
	TRACKER_PARSER_WORD_ALPHA_HIGHER,
	TRACKER_PARSER_WORD_ALPHA_LOWER,
	TRACKER_PARSER_WORD_ALPHA,
	TRACKER_PARSER_WORD_ALPHA_NUM,
	TRACKER_PARSER_WORD_IGNORE
} TrackerParserWordType;

typedef enum {
	TRACKER_PARSER_ENCODING_ASCII,
	TRACKER_PARSER_ENCODING_LATIN,
	TRACKER_PARSER_ENCODING_CJK,
	TRACKER_PARSER_ENCODING_OTHER
} TrackerParserEncoding;

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

	/* Private members */
	gchar                 *word;
	gint                   word_length;
	guint                  word_position;
	TrackerParserEncoding  encoding;
	const gchar           *cursor;

	/* Pango members for CJK text parsing */
	PangoLogAttr          *attrs;
	guint                  attr_length;
	guint                  attr_pos;
};

static inline TrackerParserWordType
get_word_type (gunichar c)
{
	/* Fast ascii handling */
	if (IS_ASCII (c)) {
		if (IS_ASCII_ALPHA_LOWER (c)) {
			return TRACKER_PARSER_WORD_ASCII_LOWER;
		}

		if (IS_ASCII_ALPHA_HIGHER (c)) {
			return TRACKER_PARSER_WORD_ASCII_HIGHER;
		}

		if (IS_ASCII_IGNORE (c)) {
			return TRACKER_PARSER_WORD_IGNORE;
		}

		if (IS_ASCII_NUMERIC (c)) {
			return TRACKER_PARSER_WORD_NUM;
		}

		if (IS_HYPHEN (c)) {
			return TRACKER_PARSER_WORD_HYPHEN;
		}

		if (IS_UNDERSCORE (c)) {
			return TRACKER_PARSER_WORD_UNDERSCORE;
		}
	} else {
		if (g_unichar_isalpha (c)) {
			if (!g_unichar_isupper (c)) {
				return TRACKER_PARSER_WORD_ALPHA_LOWER;
			} else {
				return TRACKER_PARSER_WORD_ALPHA_HIGHER;
			}
		} else if (g_unichar_isdigit (c)) {
			return TRACKER_PARSER_WORD_NUM;
		}
	}

	return TRACKER_PARSER_WORD_IGNORE;
}

static TrackerParserEncoding
get_encoding (const gchar *txt)
{
	const gchar *p;
	gunichar     c;
	gint         i = 0;

	/* Grab first 255 non-whitespace chars and test */
	for (p = txt; *p && i < 255; p = g_utf8_next_char (p)) {
		c = g_utf8_get_char (p);

		if (!g_unichar_isspace (c)) {
			i++;
		}

		if (IS_ASCII(c)) continue;

		if (IS_LATIN(c)) return TRACKER_PARSER_ENCODING_LATIN;

		if (NEED_PANGO(c)) return TRACKER_PARSER_ENCODING_CJK;

		return TRACKER_PARSER_ENCODING_OTHER;
	}

	return TRACKER_PARSER_ENCODING_ASCII;

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
		gunichar unichar;
		gchar *next_utf8;
		gint utf8_len;

		/* Get next character of the word as UCS4 */
		unichar = g_utf8_get_char_validated (&word[i], -1);

		/* Invalid UTF-8 character or end of original string. */
		if (unichar == (gunichar) -1 ||
		    unichar == (gunichar) -2) {
			break;
		}

		/* Find next UTF-8 character */
		next_utf8 = g_utf8_next_char (&word[i]);
		utf8_len = next_utf8 - &word[i];

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
process_word_utf8 (TrackerParser *parser,
                   const gchar   *word,
                   gint           length,
                   gboolean       do_strip,
                   gboolean      *stop_word)
{
	gchar *stem_word;
	gchar *str;
	gsize  bytes;

	g_return_val_if_fail (parser != NULL, NULL);
	g_return_val_if_fail (word != NULL, NULL);

	str = NULL;

	if (word) {
		bytes = length == -1 ? strlen (word) : length;

		/* Log original word */
		tracker_parser_message_hex ("ORIGINAL word",
		                            word, bytes);

		str = g_utf8_normalize (word, bytes, G_NORMALIZE_NFKD);
		if (!str) {
			return NULL;
		}

		/* Update string length */
		bytes = strlen (str);

		/* Log after normalization */
		tracker_parser_message_hex (" After NFKD normalization",
		                            str, bytes);

		if (parser->enable_unaccent &&
		    do_strip &&
		    parser_unaccent_nfkd_word (str, &bytes)) {
			/* Log after UNAC stripping */
			tracker_parser_message_hex ("  After UNAC stripping",
			                            str, bytes);
		}

		/* Check if stop word */
		if (parser->ignore_stop_words) {
			*stop_word = tracker_language_is_stop_word (parser->language,
			                                            str);
		}

		if (!parser->enable_stemmer) {
			return str;
		}

		stem_word = tracker_language_stem_word (parser->language,
		                                        str,
		                                        bytes);

		if (stem_word) {
			g_free (str);

			return stem_word;
		}
	}

	return str;
}

static gboolean
pango_next (TrackerParser *parser,
            gint          *byte_offset_start,
            gint          *byte_offset_end)

{
	/* CJK text does not need stemming or other treatment */
	gint    word_start = -1;
	gint    old_word_start = -1;
	guint   i;

	for (i = parser->attr_pos; i < parser->attr_length; i++) {
		if (parser->attrs[i].is_word_start) {
			word_start = i;
			continue;
		}

		if (parser->attrs[i].is_word_end && word_start != old_word_start) {
			gchar *start_word, *end_word;

			old_word_start = word_start;

			start_word = g_utf8_offset_to_pointer (parser->txt, word_start);
			end_word = g_utf8_offset_to_pointer (parser->txt, i);

			if (start_word != end_word) {
				gchar *str;
				gchar *index_word;

				/* Normalize word */
				str = g_utf8_casefold (start_word, end_word - start_word);
				if (!str) {
					continue;
				}

				index_word = g_utf8_normalize (str, -1, G_NORMALIZE_NFC);
				g_free (str);

				if (!index_word) {
					continue;
				}

				parser->word_length = strlen (index_word);
				parser->word = index_word;

				*byte_offset_start = (start_word - parser->txt);
				*byte_offset_end = *byte_offset_start + (end_word - start_word);
				parser->attr_pos = i;


				return TRUE;

			}

			word_start = i;
		}
	}

	parser->attr_pos = i;

	return FALSE;
}


static gboolean
parser_next (TrackerParser *parser,
             gint          *byte_offset_start,
             gint          *byte_offset_end,
             gboolean      *stop_word)
{
	TrackerParserWordType word_type;
	gunichar              word[64];
	gboolean              is_valid;
	guint                 length;
	gint                  char_count = 0;
	glong                 bytes;
	const gchar          *p;
	const gchar          *start;
	const gchar          *end;
	gboolean              do_strip = FALSE;

	*byte_offset_start = 0;
	*byte_offset_end = 0;

	g_return_val_if_fail (parser, FALSE);

	if (!parser->cursor) {
		return FALSE;
	}

	word_type = TRACKER_PARSER_WORD_IGNORE;
	is_valid = TRUE;
	length = 0;
	bytes = 0;

	start = NULL;
	end = NULL;

	for (p = parser->cursor; *p && *p != '\0'; p = g_utf8_next_char (p)) {
		TrackerParserWordType type;
		gunichar              c;

		char_count++;
		c = g_utf8_get_char (p);
		type = get_word_type (c);

		if (type == TRACKER_PARSER_WORD_IGNORE) {
			if (!start) {
				continue;
			} else {
				/* word break */

				/* check if word is reserved */
				if (is_valid && parser->ignore_reserved_words) {
					if (length == 2 && word[0] == 'o' && word[1] == 'r') {
						is_valid = FALSE;
					}
				}

				if (!is_valid ||
				    (parser->ignore_numbers && word_type == TRACKER_PARSER_WORD_NUM)) {
					word_type = TRACKER_PARSER_WORD_IGNORE;
					is_valid = TRUE;
					length = 0;
					bytes = 0;
					start = NULL;
					end = NULL;
					do_strip = FALSE;

					continue;
				}

				break;
			}
		}

		if (!is_valid) {
			continue;
		}

		if (!start) {
			start = g_utf8_offset_to_pointer (parser->cursor, char_count-1);

			/* Valid words must start with an alpha or
			 * underscore if we are filtering.
			 */

			if (parser->ignore_numbers && type == TRACKER_PARSER_WORD_NUM) {
				is_valid = FALSE;
				continue;
			} else {
				if (type == TRACKER_PARSER_WORD_HYPHEN) {
					is_valid = !parser->ignore_reserved_words;
					continue;
				}
			}
		}

		if (length >= parser->max_word_length) {
			continue;
		}

		length++;

		switch (type) {
		case TRACKER_PARSER_WORD_ASCII_HIGHER:
			c += 32;

			/* Fall through */
		case TRACKER_PARSER_WORD_ASCII_LOWER:
		case TRACKER_PARSER_WORD_HYPHEN:
		case TRACKER_PARSER_WORD_UNDERSCORE:
			if (word_type == TRACKER_PARSER_WORD_NUM ||
			    word_type == TRACKER_PARSER_WORD_ALPHA_NUM) {
				word_type = TRACKER_PARSER_WORD_ALPHA_NUM;
			} else {
				word_type = TRACKER_PARSER_WORD_ALPHA;
			}

			break;

		case TRACKER_PARSER_WORD_NUM:
			if (word_type == TRACKER_PARSER_WORD_ALPHA ||
			    word_type == TRACKER_PARSER_WORD_ALPHA_NUM) {
				word_type = TRACKER_PARSER_WORD_ALPHA_NUM;
			} else {
				word_type = TRACKER_PARSER_WORD_NUM;
			}
			break;

		case TRACKER_PARSER_WORD_ALPHA_HIGHER:
			c = g_unichar_tolower (c);

			/* Fall through */
		case TRACKER_PARSER_WORD_ALPHA_LOWER:
			if (!do_strip) {
				do_strip = TRUE;
			}

			if (word_type == TRACKER_PARSER_WORD_NUM ||
			    word_type == TRACKER_PARSER_WORD_ALPHA_NUM) {
				word_type = TRACKER_PARSER_WORD_ALPHA_NUM;
			} else {
				word_type = TRACKER_PARSER_WORD_ALPHA;
			}

			break;

		case TRACKER_PARSER_WORD_ALPHA:
		case TRACKER_PARSER_WORD_ALPHA_NUM:
		case TRACKER_PARSER_WORD_IGNORE:
		default:
			break;
		}

		word[length -1] = c;
	}

	parser->cursor = NULL;

	if (!is_valid) {
		return FALSE;
	}

	if (word_type == TRACKER_PARSER_WORD_ALPHA_NUM || word_type == TRACKER_PARSER_WORD_ALPHA) {
		gchar       *utf8;
		gchar       *processed_word;

		utf8 = g_ucs4_to_utf8 (word, length, NULL, &bytes, NULL);

		if (!utf8) {
			return FALSE;
		}

		*byte_offset_start = start-parser->txt;
		*byte_offset_end = *byte_offset_start + bytes;

		parser->cursor = parser->txt + *byte_offset_end;

		processed_word = process_word_utf8 (parser, utf8, bytes, do_strip, stop_word);
		g_free (utf8);

		if (processed_word) {
			parser->word_length = strlen (processed_word);
			parser->word = processed_word;

			return TRUE;
		}

	}

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

	g_free (parser->attrs);

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

	g_free (parser->attrs);
	parser->attrs = NULL;

	parser->cursor = txt;
	parser->encoding = get_encoding (txt);

	parser->max_word_length = max_word_length;
	parser->enable_stemmer = enable_stemmer;
	parser->enable_unaccent = enable_unaccent;
	parser->ignore_stop_words = ignore_stop_words;
	parser->ignore_reserved_words = ignore_reserved_words;
	parser->ignore_numbers = ignore_numbers;

	parser->txt_size = txt_size;
	parser->txt = txt;

	g_free (parser->word);
	parser->word = NULL;

	parser->word_position = 0;

	if (parser->encoding == TRACKER_PARSER_ENCODING_CJK) {
		PangoLogAttr *attrs;

		if (parser->txt_size == -1) {
			parser->txt_size = strlen (parser->txt);
		}

		parser->attr_length = g_utf8_strlen (parser->txt, parser->txt_size) + 1;

		attrs = g_new0 (PangoLogAttr, parser->attr_length);

		pango_get_log_attrs (parser->txt,
		                     txt_size,
		                     0,
		                     pango_language_from_string ("C"),
		                     attrs,
		                     parser->attr_length);

		parser->attrs = attrs;
		parser->attr_pos = 0;
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

	if (parser->encoding == TRACKER_PARSER_ENCODING_CJK) {
		if (pango_next (parser, &byte_start, &byte_end)) {
			str = parser->word;
		}
	} else {
		if (parser_next (parser, &byte_start, &byte_end, stop_word)) {
			str = parser->word;
		}
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

