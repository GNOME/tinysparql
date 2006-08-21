
/* Tracker 
 * utility routines
 * Copyright (C) 2005, Mr Jamie McCracken
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */ 

#include <sys/types.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pango/pango.h>
#include <magic.h>

#include "tracker-parser.h"
#include "tracker-stemmer.h"



static gboolean
is_ascii (const char* text)
{
	magic_t m;
	int len = strlen (text);

	m = magic_open (MAGIC_NONE);

	magic_load (m, NULL);

	const char *str = magic_buffer (m, text, len);

	if (strstr (str, "ASCII")) {
		//g_print ("%s\n", str);
		magic_close (m);		
		return TRUE;
	} 


	magic_close (m);


	return FALSE;

}


static char *
delimit_utf8_string (const gchar *str)
{

	PangoLogAttr *attrs;
	guint str_len = strlen (str), word_start = 0, i;
	GString *strs = g_string_new (" ");
	char *start_word, *end_word;

	attrs = g_new0 (PangoLogAttr, str_len + 1);  

	pango_get_log_attrs (str, -1, 0, pango_language_from_string ("C"), attrs, str_len + 1);

	for (i = 0; i < str_len + 1; i++) {
		
		if (attrs[i].is_word_end) {
			start_word = g_utf8_offset_to_pointer (str, word_start);
			end_word = g_utf8_offset_to_pointer (str, i);

			strs  = g_string_append_len  (strs, start_word, end_word - start_word);
			strs  = g_string_append  (strs, " ");
		}
		if (attrs[i].is_word_start) {
			word_start = i;
		}
	}
	g_free (attrs);

	return g_string_free (strs, FALSE);
}


/* check word starts with alpha char or underscore */
static gboolean
word_is_valid (const char *word)
{
	gunichar c = g_utf8_get_char (word);

	return (g_unichar_isalpha (c) || word[0] == '_');
}


/* check word contains only alpha chars */
static gboolean
word_is_alpha (const char *word)
{
	const char *p;
	gunichar c;
	gboolean result = TRUE;

	for (p = word; *p; p = g_utf8_next_char (p)) {
		c = g_utf8_get_char (p);

		if (!g_unichar_isalpha (c) ) {
			return FALSE;
		}
	}

	return result;

}






GHashTable *
tracker_parse_text (const char *text, int min_word_length, GHashTable *stop_words, GHashTable *aux_stop_words, gboolean use_stemmer, int weight)
{
	
	int count =0;
	GHashTable *table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	gboolean ascii = FALSE;
	char *delimit_text;
	gboolean pango_delimited = FALSE;

	g_return_val_if_fail (text, NULL);

	delimit_text =  (char *) text;

	if (is_ascii (text) ) {
		ascii = TRUE;
		
	} else {

		/* crude hack to determine if we need to break words up with unbelivably slow pango */
		if (!strchr (text, ' ')) { 	
		  	delimit_text = delimit_utf8_string (text);
			pango_delimited = TRUE;
		}
	}
	
	/* break text into words */
	char  **p, **words = g_strsplit_set (delimit_text, "\t\n\v\f\r !\"#$%&'()*/<=>?[\\]^`{|}~+,.:;@\"[]" , -1);
	
	if (words) {

		for (p = words; *p; p++) {

			

			
			char *word;

			/* rermove words that dont contain at least one alpha char */
			if (!word_is_valid (*p)) {
				continue;
			}


			/* ignore all words less than min word length */

			int word_len = g_utf8_strlen (*p, -1);

			if ( word_len < min_word_length) {
				continue;
			}

			
			/* normalize word */
			if (ascii) {
				word = g_ascii_strdown (*p, -1);
				
			} else {
				char *s2 = g_utf8_strdown (*p, -1);
				word = g_utf8_normalize (s2, -1, G_NORMALIZE_ALL);
				g_free (s2);
			}


			if (!word) {
				continue;
			}

			/* ignore all stop words */
			if (stop_words) {
				if (g_hash_table_lookup (stop_words, word)) {	
					g_free (word);
					continue;
				}
			}

			if (aux_stop_words) {
				if (g_hash_table_lookup (aux_stop_words, word)) {	
					g_free (word);
					continue;
				}
			}


			word_len = strlen (word);

			/* truncate words more than 30 bytes */
			if (word_len > 30) {
			
				word[29] = '\0';
				word_len = 29;				

				while (word_len != 0) {
					
					if (g_utf8_validate (word, -1, NULL)) {
						break;
					}
					word[word_len-1] = '\0';
					word_len--;
				}
			}


			/* stem words if ascii */
			if (use_stemmer && word_is_alpha (word)) {

				char *aword  = tracker_stem ( word , strlen(word)-1);

				g_free (word);
				word = aword;				        

			}

			/* count dupes */
			count = GPOINTER_TO_INT (g_hash_table_lookup (table, word));
						
			if (!g_hash_table_lookup (table, word)) {
				g_hash_table_insert (table, g_strdup (word), GINT_TO_POINTER (weight));
			} else {
				int r = count + weight;
				g_hash_table_replace (table, g_strdup (word), GINT_TO_POINTER (r));
			}

			g_free (word);


		}

		g_strfreev  (words);

	}
	
	if (pango_delimited ) {
		g_free (delimit_text);
	}


	return table;
}

