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
#include "tracker-utils.h"
#include "tracker-stemmer.h"

extern Tracker *tracker;

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


static void
get_value (gpointer key,
   	   gpointer value,
	   gpointer user_data)
{
	GString *str = user_data;

	g_string_append_printf (str, "%s,", (char *)key);
}



char **
tracker_parse_text_into_array (const char *text)
{
	GHashTable *table;
	char **array, *s;
	int count;
	GString *str;

	table = tracker_parse_text (NULL, text, 1);

	if (!table || g_hash_table_size (table) == 0) {
		return NULL;
	}

	count = g_hash_table_size (table);

	array = g_new (char *, count + 1);

	array[count] = NULL;
	
	str = g_string_new ("");

	g_hash_table_foreach (table, get_value, str);
	
	s = g_string_free (str, FALSE);

	count = strlen (s);

	if (count > 0) {
		s[strlen(s)-1] = '\0';
	}

	array =  g_strsplit (s, ",", -1);

	g_free (s);

	if (table) {
		g_hash_table_destroy (table);
	}

	return array;

}


static void
update_word_count (GHashTable *word_table, const char *word, int weight)
{
	int count;

	g_return_if_fail (word || (weight > 0));

	/* count dupes */
	count = GPOINTER_TO_INT (g_hash_table_lookup (word_table, word));
	g_hash_table_insert (word_table, g_strdup (word), GINT_TO_POINTER (count + weight));

}



GHashTable *
tracker_parse_text (GHashTable *word_table, const char *text, int weight)
{

	char	   *delimit_text;
	char	   **words;

	g_return_val_if_fail (text, NULL);


	if (!word_table) {	
		word_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	}

	delimit_text = (char *) text;


	if (tracker->use_pango_word_break) {
	  	delimit_text = delimit_utf8_string (text);
	}

	/* break text into words */

	words = g_strsplit_set (delimit_text, "\t\n\v\f\r !\"#$%&'()*/<=>?[\\]^`{|}~+,.:;@\"[]" , -1);

	if (words) {
		char **p;

		for (p = words; *p; p++) {
			char *word;
			int  word_len;

			/* remove words that dont contain at least one alpha char */
			if (!word_is_valid (*p)) {
				continue;
			}

			/* ignore all words less than min word length unless pango delimited as it may be CJK language */
			word_len = g_utf8_strlen (*p, -1);
			if (!tracker->use_pango_word_break && (word_len < tracker->min_word_length)) {
				continue;
			}


			/* normalize word */			
			char *s2;
			s2 = g_utf8_casefold (*p, -1);
			word = g_utf8_normalize (s2, -1, G_NORMALIZE_NFD);
			g_free (s2);
			
			word_len = strlen (word);

			if (!word || (word_len == 0)) {
				continue;
			}

			/* ignore all stop words */
			if (tracker->stop_words) {
				if (g_hash_table_lookup (tracker->stop_words, word)) {
					g_free (word);
					continue;
				}
			}



			/* truncate words more than max word length in bytes */
			if (word_len > tracker->max_word_length) {

				word_len = tracker->max_word_length -1;

				word[word_len] = '\0';

				while (word_len != 0) {

					word_len--;

					if ((word_len > 0) && (word_len  <= tracker->max_word_length) && (g_utf8_validate (word, word_len, NULL))) {
						word[word_len-1] = '\0';
						break;
					}
				}

				if (!word) {
					continue;
				}
			}

			/* stem words if word only contains alpha chars */
			if (tracker->stemmer && tracker->use_stemmer && word_is_alpha (word)) {
				char *aword;

				aword = tracker_stem (word);

				//g_print ("stemmed %s to %s\n", word, aword);
				g_free (word);
				word = aword;

			
			}

			update_word_count (word_table, word, weight);
			g_free (word);

		}

		g_strfreev  (words);
	}

	if (tracker->use_pango_word_break) {
		g_free (delimit_text);
	}

	return word_table;
}
