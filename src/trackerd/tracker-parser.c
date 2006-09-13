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

#include <string.h>
#include <pango/pango.h>

#include "tracker-parser.h"
#include "tracker-stemmer.h"



static char *
delimit_utf8_string (const gchar *str)
{
	PangoLogAttr *attrs;
	guint	     str_len, word_start, i;
	GString	     *strs;

	str_len = strlen (str);

	strs = g_string_new (" ");

	attrs = g_new0 (PangoLogAttr, str_len + 1);

	pango_get_log_attrs (str, -1, 0, pango_language_from_string ("C"), attrs, str_len + 1);

	word_start = 0;

	for (i = 0; i < str_len + 1; i++) {

		if (attrs[i].is_word_end) {
			char *start_word, *end_word;

			start_word = g_utf8_offset_to_pointer (str, word_start);
			end_word = g_utf8_offset_to_pointer (str, i);

			strs  = g_string_append_len (strs, start_word, end_word - start_word);
			strs  = g_string_append (strs, " ");
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
	gunichar c;

	c = g_utf8_get_char (word);

	return (g_unichar_isalpha (c) || word[0] == '_');
}


/* check word contains only alpha chars */
static gboolean
word_is_alpha (const char *word)
{
	const char *p;
	gunichar   c;

	for (p = word; *p; p = g_utf8_next_char (p)) {
		c = g_utf8_get_char (p);

		if (!g_unichar_isalpha (c) ) {
			return FALSE;
		}
	}

	return TRUE;
}


GHashTable *
tracker_parse_text (TextParser *parser, GHashTable *word_table, const char *text, int weight)
{

	char	   *delimit_text;
	char	   **words;
	gboolean   pango_delimited = FALSE;

	g_return_val_if_fail (text, NULL);


	if (!word_table) {	
		word_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	}

	delimit_text = (char *) text;

	/* crude hack to determine if we need to break words up with unbelivably slow pango word breaking */
	if (!strchr (text, ' ') && !strchr (text, ',') && !strchr (text, '.') && !strchr (text, '/') ) {
		g_print ("word breaks not detected\n");
	  	delimit_text = delimit_utf8_string (text);
		pango_delimited = TRUE;
	}
	

	/* break text into words */

	words = g_strsplit_set (delimit_text, "\t\n\v\f\r !\"#$%&'()*/<=>?[\\]^`{|}~+,.:;@\"[]" , -1);

	if (words) {
		char **p;

		for (p = words; *p; p++) {
			char *word;
			int  word_len, count;

			/* remove words that dont contain at least one alpha char */
			if (!word_is_valid (*p)) {
				continue;
			}

			/* ignore all words less than min word length unless pango delimited as it may be CJK language */
			word_len = g_utf8_strlen (*p, -1);
			if ( !pango_delimited && (word_len < parser->min_word_length)) {
				continue;
			}


			/* normalize word */			
			char *s2;
			s2 = g_utf8_strdown (*p, -1);
			word = g_utf8_normalize (s2, -1, G_NORMALIZE_ALL);
			g_free (s2);
			

			if (!word) {
				continue;
			}

			/* ignore all stop words */
			if (parser->stop_words) {
				if (g_hash_table_lookup (parser->stop_words, word)) {
					g_free (word);
					continue;
				}
			}

			word_len = strlen (word);

			/* truncate words more than max word length in bytes */
			if (word_len > parser->max_word_length) {

				word_len = parser->max_word_length -1;

				word[word_len] = '\0';

				while (word_len != 0) {

					if (g_utf8_validate (word, -1, NULL)) {
						break;
					}

					word[word_len-1] = '\0';
					word_len--;
				}
			}

			/* stem words if word only contains alpha chars */
			if (parser->use_stemmer && word_is_alpha (word)) {
				char *aword;

				aword = tracker_stem_eng (word , strlen(word)-1);

				//g_print ("stemmed %s to %s\n", word, aword);
				g_free (word);
				word = aword;
			}

			/* count dupes */
			count = GPOINTER_TO_INT (g_hash_table_lookup (word_table, word));

			if (!g_hash_table_lookup (word_table, word)) {
				g_hash_table_insert (word_table, word, GINT_TO_POINTER (weight));
			} else {
				int r;

				r = count + weight;
				g_hash_table_replace (word_table, word, GINT_TO_POINTER (r));
			}


		}

		g_strfreev  (words);
	}

	if (pango_delimited ) {
		g_free (delimit_text);
	}

	return word_table;
}
