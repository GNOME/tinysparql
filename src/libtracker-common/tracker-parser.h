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

#ifndef __TRACKERD_PARSER_H__
#define __TRACKERD_PARSER_H__

#include <glib.h>
#include <pango/pango.h>

#include "tracker-language.h"

G_BEGIN_DECLS

typedef enum {
	TRACKER_PARSER_ENCODING_ASCII,
	TRACKER_PARSER_ENCODING_LATIN,
	TRACKER_PARSER_ENCODING_CJK,
	TRACKER_PARSER_ENCODING_OTHER
} TrackerParserEncoding;

typedef struct {
	const gchar	      *txt;
	gint		       txt_size;

	TrackerLanguage       *language;
	gboolean	       enable_stemmer;
	gboolean	       enable_stop_words;
	guint		       max_words_to_index;
	guint		       max_word_length;
	guint		       min_word_length;
	gboolean	       delimit_words;
	gboolean	       parse_reserved_words;

	/* Private members */
	gchar			*word;
	gint			word_length;
	guint			word_position;
	TrackerParserEncoding	encoding;
	const gchar		*cursor;

	/* Pango members for CJK text parsing */
	PangoLogAttr	      *attrs;
	guint		       attr_length;
	guint		       attr_pos;
} TrackerParser;

TrackerParser *tracker_parser_new	      (TrackerLanguage *language,
					       gint		max_word_length,
					       gint		min_word_length);
void	       tracker_parser_reset	      (TrackerParser   *parser,
					       const gchar     *txt,
					       gint		txt_size,
					       gboolean		delimit_words,
					       gboolean		enable_stemmer,
					       gboolean		enable_stop_words,
					       gboolean		parse_reserved_words);
const gchar *  tracker_parser_next	      (TrackerParser   *parser,
					       gint	       *position,
					       gint	       *byte_offset_start,
					       gint	       *byte_offset_end,
					       gboolean        *new_paragraph,
					       gboolean        *stop_word,
					       gint	       *word_length);
void	       tracker_parser_set_posititon   (TrackerParser   *parser,
					       gint		position);
gboolean       tracker_parser_is_stop_word    (TrackerParser   *parser,
					       const gchar     *word);
gchar *        tracker_parser_process_word    (TrackerParser   *parser,
					       const char      *word,
					       gint		length,
					       gboolean		do_strip);
void	       tracker_parser_free	      (TrackerParser   *parser);


/*
 * Functions to parse supplied text and break into individual words and
 * maintain a count of no of occurences of the word multiplied by a
 * "weight" factor.
 *
 * The word_table - can be NULL. It contains the accumulated parsed words
 * with weighted word counts for the text (useful for indexing stuff
 * line by line)
 *
 *   text   - the text to be parsed
 *   weight - used to multiply the count of a word's occurance to create
 *	      a weighted rank score
 *
 * Returns the word_table.
 */
GHashTable *   tracker_parser_text	      (GHashTable      *word_table,
					       const gchar     *txt,
					       gint		weight,
					       TrackerLanguage *language,
					       gint		max_words_to_index,
					       gint		max_word_length,
					       gint		min_word_length,
					       gboolean		filter_words,
					       gboolean		delimit_words);
GHashTable *   tracker_parser_text_fast       (GHashTable      *word_table,
					       const char      *txt,
					       gint		weight);
gchar *        tracker_parser_text_to_string  (const gchar     *txt,
					       TrackerLanguage *language,
					       gint		max_word_length,
					       gint		min_word_length,
					       gboolean		filter_words,
					       gboolean		filter_numbers,
					       gboolean		delimit);
gchar **       tracker_parser_text_into_array (const gchar     *text,
					       TrackerLanguage *language,
					       gint		max_word_length,
					       gint		min_word_length);


G_END_DECLS

#endif /* __TRACKERD_PARSER_H__ */
