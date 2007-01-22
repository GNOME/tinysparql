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

/* check word either contains at least one alpha char or is a number of at least 5 consecutive digits (we want to index meaningful numbers only like telephone no.s, bank accounts, ISBN etc) */
static gboolean 
numbered_word_is_valid (const char *word)
{
	const char *p;
	gunichar   c;
	int i = 0;

	for (p = word; *p; p = g_utf8_next_char (p)) {

		i++;

		c = g_utf8_get_char (p);

		if (g_unichar_isalpha (c) ) {
			return TRUE;
		}

		if (!g_unichar_isdigit (c) ) {
			return FALSE;
		}	
	}

	return (i > 4);

}

/* check word starts with alphanumeric char or underscore and only numbers of 5 or more digits are indexed */
static gboolean
word_is_valid (const char *word)
{
	gunichar c;

	c = g_utf8_get_char (word);

	if (tracker->index_numbers && g_unichar_isalnum (c)) {
		
		return numbered_word_is_valid (word);
	} 

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

	table = tracker_parse_text (NULL, text, 1, TRUE);

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


static inline gboolean
valid_char (const unsigned char c)
{
	return ((c > 96 && c < 123) || (c > 64 && c < 91) || (c > 47 && c < 58) || (c == '_') || (c == '-') || (c > 127));
}



static char *
process_word (const char *index_word) 
{
	char *word, *s;
	int word_len;

	/* ignore all words less than min word length unless pango delimited as it may be CJK language */
	word_len = g_utf8_strlen (index_word, -1);
	if (!tracker->use_pango_word_break && (word_len < tracker->min_word_length)) {
		return NULL;
	}

	/* remove words that dont contain at least one alpha char */
	if (!word_is_valid (index_word)) {
		return NULL;
	}


	/* normalize word */
	s = g_utf8_casefold (index_word, -1);
	word = g_utf8_normalize (s, -1, G_NORMALIZE_NFD);
	g_free (s);
	
	if (!word || word[0] == '\0') {
		return NULL;
	}

	/* ignore all stop words */
	if (tracker->stop_words) {
		if (g_hash_table_lookup (tracker->stop_words, word)) {
			g_free (word);
			return NULL;
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
			return NULL;
		}
	}

	/* stem words if word only contains alpha chars */
	if (tracker->stemmer && tracker->use_stemmer && word_is_alpha (word)) {
		char *aword;

		aword = tracker_stem (word);

		g_free (word);
		word = aword;
	}

	return word;
	
}


GHashTable *
tracker_parse_text (GHashTable *word_table, const char *text, int weight, gboolean filter_words)
{
	char *delimit_text;
	int  i, j, total_words;
	char 	*start;

	total_words = 0;

	if (!text) {
		return g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	}

	if (!word_table) {
		word_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	} else {
		total_words = g_hash_table_size (word_table);
	}

	if (tracker->use_pango_word_break) {
		delimit_text = delimit_utf8_string (text);
	} else {
		delimit_text = (char *) text;
	}

	/* break text into words */
	start = NULL;
	i=0;
	j=0;
	
	while (TRUE) {
	
		if (text[i] && valid_char (text[i])) {	
			if (!start) {
				start = (char *)(text + i);
				j = i;
			} 
			i++;
			continue;
		} else { 

			if (!start) {
				if (!text[i]) {
					break;
				}
				i++;
				continue;
			} else {

				/* we have a word */
				char *word, *index_word;

				word = g_strndup (start, i-j);

				if (filter_words) {
					index_word = process_word (word);
					g_free (word);
				} else {
					index_word = word;
				}

				if (!index_word) {
					start = NULL;
					continue;
				}
				total_words++;

				if (total_words < 10000) {
					update_word_count (word_table, index_word, weight);
				} else {
					//tracker_log ("Number of unique words in this indexable content exceeds 10,000 - cropping index..."); 
					g_free (index_word);
					break;
				}

				g_free (index_word);

				if (!text[i]) {
					break;
				} else {
					start = NULL;
				}

				
			}

		}
	}

	if (tracker->use_pango_word_break) {
		g_free (delimit_text);
	}

	return word_table;
}
