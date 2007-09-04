/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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



#ifndef _TRACKER_PARSER_H_
#define _TRACKER_PARSER_H_

#include <glib.h>



/* tracker_parse_text 
 *
 * function to parse supplied text and break into individual words and maintain a count of no of occurances of the word multiplied by a "weight" factor.
 * 
 * word_table - can be NULL. Contains the accumulated parsed words with weighted word counts for the text (useful for indexing stuff line by line)
 * text - the text to be parsed
 * weight - used to multiply the count of a word's occurance to create a weighted rank score
 * 
 * returns the word_table.
 */
GHashTable *	tracker_parse_text (GHashTable *word_table, const char *txt, int weight, gboolean filter_words, gboolean delimit_words);

GHashTable *	tracker_parse_text_fast (GHashTable *word_table, const char *txt, int weight);

char *		tracker_parse_text_to_string (const char *txt, gboolean filter_words, gboolean delimit);	

char **		tracker_parse_text_into_array (const char *text);

void		tracker_word_table_free (GHashTable *table);

#endif
