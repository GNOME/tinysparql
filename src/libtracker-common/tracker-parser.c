/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include <string.h>

#include "tracker-parser.h"
#include "tracker-log.h"
#include "tracker-utils.h"

#define INDEX_NUMBER_MIN_LENGTH 6

/* Need pango for CJK ranges which are : 0x3400 - 0x4DB5, 0x4E00 -
 * 0x9FA5, 0x20000 - <= 0x2A6D6
 */
#define NEED_PANGO(c)		 (((c) >= 0x3400 && (c) <= 0x4DB5)  ||	\
				  ((c) >= 0x4E00 && (c) <= 0x9FA5)  ||	\
				  ((c) >= 0x20000 && (c) <= 0x2A6D6))
#define IS_LATIN(c)		 (((c) <= 0x02AF) ||			\
				  ((c) >= 0x1E00 && (c) <= 0x1EFF))
#define IS_ASCII(c)		 ((c) <= 0x007F)
#define IS_ASCII_ALPHA_LOWER(c)  ((c) >= 0x0061 && (c) <= 0x007A)
#define IS_ASCII_ALPHA_HIGHER(c) ((c) >= 0x0041 && (c) <= 0x005A)
#define IS_ASCII_NUMERIC(c)	 ((c) >= 0x0030 && (c) <= 0x0039)
#define IS_ASCII_IGNORE(c)	 ((c) <= 0x002C)
#define IS_HYPHEN(c)		 ((c) == 0x002D)
#define IS_UNDERSCORE(c)	 ((c) == 0x005F)
#define IS_NEWLINE(c)		 ((c) == 0x000D)
#define IS_O(c)			 ((c) == 0x006F)
#define IS_R(c)			 ((c) == 0x0072)

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
	TRACKER_PARSER_WORD_IGNORE,
	TRACKER_PARSER_WORD_NEWLINE
} TrackerParserWordType;

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

		if (IS_NEWLINE (c)) {
			return TRACKER_PARSER_WORD_NEWLINE;
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

static inline gchar *
strip_word (const gchar *str,
	    gint	 length,
	    guint32	*len)
{
#ifdef HAVE_UNAC
	gchar *s = NULL;

	if (unac_string ("UTF-8", str, length, &s, &*len) != 0) {
		g_warning ("UNAC failed to strip accents");
	}

	return s;
#else
	*len = length;
	return NULL;
#endif
}

static gboolean
text_needs_pango (const gchar *text)
{
	const gchar *p;
	gunichar     c;
	gint	     i = 0;

	/* Grab first 1024 non-whitespace chars and test */
	for (p = text; *p && i < 1024; p = g_utf8_next_char (p)) {
		c = g_utf8_get_char (p);

		if (!g_unichar_isspace (c)) {
			i++;
		}

		if (NEED_PANGO(c)) {
			return TRUE;
		}
	}

	return FALSE;
}

static TrackerParserEncoding
get_encoding (const gchar *txt)
{
	const gchar *p;
	gunichar     c;
	gint	     i = 0;

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
is_stop_word (TrackerLanguage *language,
	      const gchar     *word)
{
	GHashTable *stop_words;

	if (!word) {
		return FALSE;
	}

	stop_words = tracker_language_get_stop_words (language);

	return g_hash_table_lookup (stop_words, word) != NULL;
}

static const gchar *
analyze_text (const gchar      *text,
	      TrackerLanguage  *language,
	      gint		max_word_length,
	      gint		min_word_length,
	      gboolean		filter_words,
	      gboolean		filter_numbers,
	      gboolean		delimit_hyphen,
	      gchar	      **index_word)
{
	TrackerParserWordType word_type;
	gunichar	      word[64];
	gboolean	      do_strip;
	gboolean	      is_valid;
	gint		      length;
	glong		      bytes;
	const char	     *p;
	const char	     *start;

	*index_word = NULL;

	if (text == NULL || text[0] == '\0') {
		return NULL;
	}

	word_type = TRACKER_PARSER_WORD_IGNORE;
	do_strip = FALSE;
	is_valid = TRUE;
	length = 0;
	bytes = 0;
	start = NULL;

	for (p = text; *p; p = g_utf8_next_char (p)) {
		TrackerParserWordType type;
		gunichar	      c;

		c = g_utf8_get_char (p);
		type = get_word_type (c);

		if (type == TRACKER_PARSER_WORD_IGNORE ||
		    type == TRACKER_PARSER_WORD_NEWLINE ||
		    (delimit_hyphen &&
		     (type == TRACKER_PARSER_WORD_HYPHEN ||
		      type == TRACKER_PARSER_WORD_UNDERSCORE))) {
			if (!start) {
				continue;
			} else {
				break;
			}
		}

		if (!is_valid) {
			continue;
		}

		if (!start) {
			start = p;

			/* Valid words must start with an alpha or
			 * underscore if we are filtering.
			 */
			if (filter_numbers) {
				if (type == TRACKER_PARSER_WORD_NUM) {
					is_valid = FALSE;
					continue;
				} else {
					if (type == TRACKER_PARSER_WORD_HYPHEN) {
						is_valid = FALSE;
						continue;
					}
				}
			}
		}

		if (length >= max_word_length) {
			continue;
		}

		length++;

		switch (type) {
		case TRACKER_PARSER_WORD_ASCII_HIGHER:
			c += 32;

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

		default:
			break;
		}

		word[length -1] = c;
	}

	if (!is_valid) {
		return p;
	}

	if (word_type == TRACKER_PARSER_WORD_NUM) {
		if (!filter_numbers || length >= INDEX_NUMBER_MIN_LENGTH) {
			*index_word = g_ucs4_to_utf8 (word, length, NULL, NULL, NULL);
		}
	} else if (length >= min_word_length) {
		const gchar *stem_word;
		gchar	    *stripped_word;
		gchar	    *str;
		gchar	    *utf8;
		guint32      len;

		utf8 = g_ucs4_to_utf8 (word, length, NULL, &bytes, NULL);

		if (!utf8) {
			return p;
		}

		if (do_strip && get_encoding (utf8) == TRACKER_PARSER_ENCODING_LATIN) {
			stripped_word = strip_word (utf8, bytes, &len);
		} else {
			stripped_word = NULL;
		}

		if (!stripped_word) {
			str = g_utf8_normalize (utf8,
						bytes,
						G_NORMALIZE_NFC);
		} else {
			str = g_utf8_normalize (stripped_word,
						len,
						G_NORMALIZE_NFC);
			g_free (stripped_word);
		}

		g_free (utf8);

		stem_word = tracker_language_stem_word (language,
							str,
							strlen (str));
		g_free (str);

		if (!filter_words || !is_stop_word (language, stem_word)) {
			*index_word = g_strdup (stem_word);
		}
	}

	return p;
}

static gboolean
pango_next (TrackerParser *parser,
	    gint	  *byte_offset_start,
	    gint	  *byte_offset_end,
	    gboolean	  *is_new_paragraph)

{
	/* CJK text does not need stemming or other treatment */
	gint	word_start = -1;
	gint	old_word_start = -1;
	guint	i;

	*is_new_paragraph = FALSE;

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

				if (word_start > 1 && parser->attrs[word_start -1].is_sentence_boundary) {
					*is_new_paragraph = TRUE;
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
	     gint	   *byte_offset_start,
	     gint	   *byte_offset_end,
	     gboolean	   *is_new_paragraph)
{
	TrackerParserWordType word_type;
	gunichar	      word[64];
	gboolean	      is_valid;
	guint		      length;
	gint		      char_count = 0;
	glong		      bytes;
	const gchar	     *p;
	const gchar	     *start;
	const gchar	     *end;
	gboolean	      do_strip = FALSE;

	*byte_offset_start = 0;
	*byte_offset_end = 0;
	*is_new_paragraph = FALSE;

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

	for (p = parser->cursor; *p; p = g_utf8_next_char (p)) {
		TrackerParserWordType type;
		gunichar	      c;

		char_count++;
		c = g_utf8_get_char (p);
		type = get_word_type (c);

		if (type == TRACKER_PARSER_WORD_NEWLINE) {
			*is_new_paragraph = TRUE;
		}

		if (type == TRACKER_PARSER_WORD_IGNORE || type == TRACKER_PARSER_WORD_NEWLINE ||
		    (parser->delimit_words &&
		     (type == TRACKER_PARSER_WORD_HYPHEN ||
		      type == TRACKER_PARSER_WORD_UNDERSCORE))) {
			if (!start) {
				continue;
			} else {
				/* word break */

				/* check if word is reserved */
				if (is_valid && parser->parse_reserved_words) {
					if (length == 2 && word[0] == 'o' && word[1] == 'r') {
						break;
					}
				}

				if (!is_valid ||
				    length < parser->min_word_length ||
				    word_type == TRACKER_PARSER_WORD_NUM) {
					*is_new_paragraph = FALSE;

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

			if (type == TRACKER_PARSER_WORD_NUM) {
				is_valid = FALSE;
				start = NULL;
				continue;
			} else {
				if (type == TRACKER_PARSER_WORD_HYPHEN) {
					is_valid = parser->parse_reserved_words;
					start = NULL;
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
		gchar	    *utf8;
		gchar	    *processed_word;



		utf8 = g_ucs4_to_utf8 (word, length, NULL, &bytes, NULL);

		if (!utf8) {
			return FALSE;
		}

		*byte_offset_start = start-parser->txt;
		*byte_offset_end = *byte_offset_start + bytes;

		parser->cursor = g_utf8_next_char (parser->txt + *byte_offset_end);

		processed_word = tracker_parser_process_word (parser, utf8, bytes, do_strip);

		g_free (utf8);

		parser->word_length = strlen (processed_word);
		parser->word = processed_word;

		return TRUE;

	}

	return FALSE;

}

static gboolean
word_table_increment (GHashTable *word_table,
		      gchar	 *index_word,
		      gint	  weight,
		      gint	  total_words,
		      gint	  max_words_to_index)
{
	gboolean update_count;

	update_count = total_words <= max_words_to_index;

	if (update_count) {
		gpointer p;
		gint	 count;

		p = g_hash_table_lookup (word_table, index_word);
		count = GPOINTER_TO_INT (p);

		g_hash_table_replace (word_table,
				      index_word,
				      GINT_TO_POINTER (count + weight));
	} else {
		g_free (index_word);
	}

	return update_count;
}

TrackerParser *
tracker_parser_new (TrackerLanguage *language,
		    gint	     max_word_length,
		    gint	     min_word_length)
{
	TrackerParser *parser;

	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), NULL);
	g_return_val_if_fail (min_word_length > 0, NULL);
	g_return_val_if_fail (min_word_length < max_word_length, NULL);

	parser = g_new0 (TrackerParser, 1);

	parser->language = g_object_ref (language);

	parser->max_word_length = max_word_length;
	parser->min_word_length = min_word_length;
	parser->word_length = 0;
	parser->attrs = NULL;

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
		      gint	     txt_size,
		      gboolean	     delimit_words,
		      gboolean	     enable_stemmer,
		      gboolean	     enable_stop_words,
		      gboolean	     parse_reserved_words)
{
	g_return_if_fail (parser != NULL);
	g_return_if_fail (txt != NULL);

	g_free (parser->attrs);
	parser->attrs = NULL;

	parser->enable_stemmer = enable_stemmer;
	parser->enable_stop_words = enable_stop_words;
	parser->delimit_words = delimit_words;
	parser->encoding = get_encoding (txt);
	parser->txt_size = txt_size;
	parser->txt = txt;
	parser->parse_reserved_words = parse_reserved_words;

	g_free (parser->word);
	parser->word = NULL;

	parser->word_position = 0;

	parser->cursor = txt;

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

gchar *
tracker_parser_process_word (TrackerParser *parser,
			     const char    *word,
			     gint	    length,
			     gboolean	    do_strip)
{
	const gchar *stem_word;
	gchar	    *str;
	gchar	    *stripped_word;
	guint	     bytes, len;

	g_return_val_if_fail (parser != NULL, NULL);
	g_return_val_if_fail (word != NULL, NULL);

	str = NULL;
	stripped_word = NULL;

	if (word) {
		if (length == -1) {
			bytes = strlen (word);
		} else {
			bytes = length;
		}

		if (do_strip) {
			stripped_word = strip_word (word, bytes, &len);
		} else {
			stripped_word = NULL;
		}

		if (!stripped_word) {
			str = g_utf8_normalize (word,
						bytes,
						G_NORMALIZE_NFC);
		} else {
			str = g_utf8_normalize (stripped_word,
						len,
						G_NORMALIZE_NFC);
			g_free (stripped_word);
		}

		if (!parser->enable_stemmer) {
			return str;
		}

		len = strlen (str);

		stem_word = tracker_language_stem_word (parser->language, str, len);

		if (stem_word) {
			gchar *result;

			result = g_strdup (stem_word);
			g_free (str);

			return result;
		}
	}

	return str;
}

gboolean
tracker_parser_is_stop_word (TrackerParser *parser, const gchar *word)
{
	if (get_encoding (word) == TRACKER_PARSER_ENCODING_CJK) return FALSE;


	char *processed_word = tracker_parser_process_word (parser, word, -1, TRUE);
	gboolean result = is_stop_word (parser->language, processed_word);
	g_free (processed_word);
	return result;
}

const gchar *
tracker_parser_next (TrackerParser *parser,
		     gint	   *position,
		     gint	   *byte_offset_start,
		     gint	   *byte_offset_end,
		     gboolean	   *new_paragraph,
		     gboolean	   *stop_word,
		     gint	   *word_length)
{
	const gchar  *str;
	gint	 byte_start = 0, byte_end = 0;
	gboolean  new_para;

	str = NULL;

	g_free (parser->word);
	parser->word = NULL;

	if (parser->encoding == TRACKER_PARSER_ENCODING_CJK) {
		if (pango_next (parser, &byte_start, &byte_end, &new_para)) {
			str = parser->word;
		}
		parser->word_position++;

		*stop_word = FALSE;
	} else {
		if (parser_next (parser, &byte_start, &byte_end, &new_para)) {
			str = parser->word;
		}

		if (parser->enable_stop_words && is_stop_word (parser->language, str)) {
			*stop_word = TRUE;
		} else {
			parser->word_position++;
			*stop_word = FALSE;
		}
	}

	*word_length = parser->word_length;
	*position = parser->word_position;
	*byte_offset_start = byte_start;
	*byte_offset_end = byte_end;
	*new_paragraph = new_para;

	return str;
}


gchar *
tracker_parser_text_to_string (const gchar     *text,
			       TrackerLanguage *language,
			       gint		max_word_length,
			       gint		min_word_length,
			       gboolean		filter_words,
			       gboolean		filter_numbers,
			       gboolean		delimit)
{
	const gchar *p;
	gchar	    *parsed_text;
	guint32      i = 0;
	gint	     len;

	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), NULL);

	if (text == NULL || text[0] == '\0') {
		return NULL;
	}

	p = text;
	len = strlen (text);
	len = MIN (len, 500);

	if (!g_utf8_validate (text, len, NULL)) {
		return NULL;
	}

	if (text_needs_pango (text)) {
		/* CJK text does not need stemming or other
		 * treatment.
		 */
		PangoLogAttr *attrs;
		guint	      str_len, word_start;
		GString	     *strs;

		str_len = g_utf8_strlen (text, -1);

		strs = g_string_new (" ");

		attrs = g_new0 (PangoLogAttr, str_len + 1);

		pango_get_log_attrs (text,
				     len,
				     0,
				     pango_language_from_string ("C"),
				     attrs,
				     str_len + 1);

		word_start = 0;

		for (i = 0; i < str_len + 1; i++) {
			if (attrs[i].is_word_end) {
				gchar *start_word, *end_word;

				start_word = g_utf8_offset_to_pointer (text, word_start);
				end_word = g_utf8_offset_to_pointer (text, i);

				if (start_word != end_word) {
					/* Normalize word */
					gchar *s;
					gchar *index_word;

					s = g_utf8_casefold (start_word, end_word - start_word);
					index_word  = g_utf8_normalize (s, -1, G_NORMALIZE_NFC);
					g_free (s);

					strs = g_string_append (strs, index_word);
					strs = g_string_append_c (strs, ' ');
					g_free (index_word);
				}

				word_start = i;
			}

			if (attrs[i].is_word_start) {
				word_start = i;
			}
		}

		g_free (attrs);

		parsed_text = g_string_free (strs, FALSE);
		return g_strstrip (parsed_text);
	} else {
		GString *str;
		gchar	*word;

		str = g_string_new (" ");

		while (TRUE) {
			i++;
			p = analyze_text (p,
					  language,
					  max_word_length,
					  min_word_length,
					  filter_words,
					  filter_numbers,
					  delimit,
					  &word);

			if (word) {
				g_string_append (str, word);
				g_string_append_c (str, ' ');
				g_free (word);
			}

			if (!p || !*p) {
				parsed_text = g_string_free (str, FALSE);
				return g_strstrip (parsed_text);
			}
		}

		g_string_free (str, TRUE);
	}

	return NULL;
}

gchar **
tracker_parser_text_into_array (const gchar	*text,
				TrackerLanguage *language,
				gint		 max_word_length,
				gint		 min_word_length)
{
	gchar  *s;
	gchar **strv;

	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), NULL);

	s = tracker_parser_text_to_string (text,
					   language,
					   max_word_length,
					   min_word_length,
					   TRUE,
					   FALSE,
					   FALSE);
	strv = g_strsplit (g_strstrip (s), " ", -1);
	g_free (s);

	return strv;
}

GHashTable *
tracker_parser_text_fast (GHashTable  *word_table,
			  const gchar *txt,
			  gint	       weight)
{
	gchar **array;
	gchar **p;

	/* Use this for already processed text only */
	if (!word_table) {
		word_table = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    NULL);
	}

	if (!txt || weight == 0) {
		return word_table;
	}

	array = g_strsplit (txt, " ", -1);
	if (!array) {
		return word_table;
	}

	for (p = array; *p; p++) {
		word_table_increment (word_table, *p, weight, 0, 0);
	}

	g_free (array);

	return word_table;
}

GHashTable *
tracker_parser_text (GHashTable      *word_table,
		     const gchar     *text,
		     gint	      weight,
		     TrackerLanguage *language,
		     gint	      max_words_to_index,
		     gint	      max_word_length,
		     gint	      min_word_length,
		     gboolean	      filter_words,
		     gboolean	      delimit_words)
{
	const gchar *p;
	guint32      i;

	/* Use this for unprocessed raw text */
	gint	     total_words;

	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), NULL);

	if (!word_table) {
		word_table = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    NULL);
		total_words = 0;
	} else {
		total_words = g_hash_table_size (word_table);
	}

	if (text == NULL || text[0] == '\0' || weight == 0) {
		return word_table;
	}

	p = text;
	i = 0;

	if (text_needs_pango (text)) {
		/* CJK text does not need stemming or other treatment */
		PangoLogAttr *attrs;
		guint	      len, str_len, word_start;

		len = strlen (text);
		str_len = g_utf8_strlen (text, -1);

		attrs = g_new0 (PangoLogAttr, str_len + 1);

		pango_get_log_attrs (text,
				     len,
				     0,
				     pango_language_from_string ("C"),
				     attrs,
				     str_len + 1);

		word_start = 0;

		for (i = 0; i < str_len + 1; i++) {
			if (attrs[i].is_word_end) {
				gchar *start_word, *end_word;

				start_word = g_utf8_offset_to_pointer (text, word_start);
				end_word = g_utf8_offset_to_pointer (text, i);

				if (start_word != end_word) {
					gchar	 *str;
					gchar	 *index_word;
					gboolean  was_updated;

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

					total_words++;
					was_updated = word_table_increment (word_table,
									    index_word,
									    weight,
									    total_words,
									    max_words_to_index);

					if (!was_updated) {
						break;
					}
				}

				word_start = i;
			}

			if (attrs[i].is_word_start) {
				word_start = i;
			}
		}

		g_free (attrs);
	} else {
		gchar *word;

		while (TRUE) {
			i++;
			p = analyze_text (p,
					  language,
					  max_word_length,
					  min_word_length,
					  filter_words,
					  filter_words,
					  delimit_words,
					  &word);

			if (word) {
				total_words++;

				if (!word_table_increment (word_table,
							   word,
							   weight,
							   total_words,
							   max_words_to_index)) {
					break;
				}
			}

			if (!p || !*p) {
				break;
			}
		}
	}

	return word_table;
}
