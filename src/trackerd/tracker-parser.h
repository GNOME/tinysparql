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
GHashTable *	tracker_parse_text (GHashTable *word_table, const char *text,  int weight);

char **		tracker_parse_text_into_array (const char *text);

#endif
