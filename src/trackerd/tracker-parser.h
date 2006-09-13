/* Tracker
 * Copyright (C) 2005, Mr Jamie McCracken
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _TRACKER_PARSER_H_
#define _TRACKER_PARSER_H_

#include <glib.h>

typedef enum {
	STEM_NONE,
	STEM_ENG,
	STEM_FRENCH,
	STEM_SPANISH,
	STEM_PORTUGESE,
	STEM_ITALIAN,
	STEM_GERMAN,
	STEM_DUTCH,
	STEM_SWEDISH,
	STEM_NORWEGIAN,
	STEM_DANISH,
	STEM_RUSSIAN,
	STEM_FINNISH,
	STEM_HUNGARIAN	
} Stemmer;

typedef struct {
	int		min_word_length;  	/* words shorter than this are not parsed */
	int		max_word_length;  	/* words longer than this are cropped */
	gboolean	use_stemmer;	 	/* enables stemming support */
	Stemmer		stem_language;		/* the language specific stemmer to use */	
	GHashTable	*stop_words;	  	/* table of stop words that are to be ignored by the parser */
} TextParser;

/* tracker_parse_text 
 *
 * function to parse supplied text and break into individual words and maintain a count of no of occurances of the word multiplied by a "weight" factor.
 * 
 * parser  - TextParser containing parsing optons
 * word_table - can be NULL. Contains the accumulated parsed words with weighted word counts for the text (useful for indexing stuff line by line)
 * text - the text to be parsed
 * weight - used to multiply the count of a word's occurance to create a weighted rank score
 * 
 * returns the word_table.
 */
GHashTable *	tracker_parse_text (TextParser *parser, GHashTable *word_table, const char *text,  int weight);

#endif
