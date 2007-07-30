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

#include "config.h"

	
#ifdef HAVE_UNAC
#include <unac.h>
#endif

extern Tracker *tracker;

#define NEED_PANGO(c) (((c) >= 0x0780 && (c) <= 0x1B7F)  || ((c) >= 0x2E80 && (c) <= 0xFAFF)  ||  ((c) >= 0xFE30 && (c) <= 0xFE4F))
#define IS_LATIN(c) (((c) <= 0x02AF) || ((c) >= 0x1E00 && (c) <= 0x1EFF))
#define IS_ASCII(c) ((c) <= 0x007F) 
#define IS_ASCII_ALPHA_LOWER(c) ( (c) >= 0x0061 && (c) <= 0x007A )
#define IS_ASCII_ALPHA_HIGHER(c) ( (c) >= 0x0041 && (c) <= 0x005A )
#define IS_ASCII_NUMERIC(c) ((c) >= 0x0030 && (c) <= 0x0039)
#define IS_ASCII_IGNORE(c) ((c) <= 0x002C) 
#define IS_HYPHEN(c) ((c) == 0x002D)
#define IS_UNDERSCORE(c) ((c) == 0x005F)

typedef enum {
	WORD_ASCII_HIGHER,
	WORD_ASCII_LOWER,
	WORD_HYPHEN,
	WORD_UNDERSCORE,
	WORD_NUM,
	WORD_ALPHA_HIGHER,
	WORD_ALPHA_LOWER,
	WORD_ALPHA,
	WORD_ALPHA_NUM,
	WORD_IGNORE
} WordType;



static inline WordType
get_word_type (gunichar c)
{
	/* fast ascii handling */
	if (IS_ASCII (c)) {

		
		if (IS_ASCII_ALPHA_LOWER (c)) {
			return WORD_ASCII_LOWER;
		}

		if (IS_ASCII_ALPHA_HIGHER (c)) {
			return WORD_ASCII_HIGHER;
		}

		if (IS_ASCII_IGNORE (c)) {
			return WORD_IGNORE;	
		}

		if (IS_ASCII_NUMERIC (c)) {
			return WORD_NUM;
		}

		if (IS_HYPHEN (c)) {
			return WORD_HYPHEN;
		}

		if (IS_UNDERSCORE (c)) {
			return WORD_UNDERSCORE;
		}
	

	} else 	{

		if (g_unichar_isalpha (c)) {

			if (!g_unichar_isupper (c)) {
				return  WORD_ALPHA_LOWER;
			} else {
				return  WORD_ALPHA_HIGHER;
			}

		} else if (g_unichar_isdigit (c)) {
			return  WORD_NUM;
		} 
	}

	return WORD_IGNORE;

}


static inline char *
strip_word (const char *str, int length, guint32 *len)
{
// CaféCaféCafé1 

	*len = length;

#ifdef HAVE_UNAC

	if (tracker->strip_accents) {

		char *s = NULL;

		if (unac_string ("UTF-8", str, length, &s, &*len) != 0) {
			tracker_log ("WARNING: unac failed to strip accents");
		}

		return s;

	}

#endif	
	return NULL;	
}


static gboolean
text_needs_pango (const char *text)
{
	/* grab first 50 non-whitespace chars and test */
	const char *p;
	gunichar   c;
	int  i = 0;

	for (p = text; (*p && i < 50); p = g_utf8_next_char (p)) {

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



static const char *
analyze_text (const char *text, char **index_word, gboolean filter_words, gboolean delimit_hyphen)
{
	const char 	*p;
	const char 	*start = NULL;
	
	*index_word = NULL;

	if (text) {
		gunichar	word[64];

		gunichar   	c;
		gboolean 	do_stem = TRUE, do_strip = FALSE, is_valid = TRUE;
		int		length = 0;
		glong		bytes = 0;
		WordType	word_type = WORD_IGNORE;

		for (p = text; *p; p = g_utf8_next_char (p)) {

			c = g_utf8_get_char (p);

			WordType type = get_word_type (c);

			if (type == WORD_IGNORE || (delimit_hyphen && (type == WORD_HYPHEN || type == WORD_UNDERSCORE))) {

				if (!start) {
					continue;
				} else {
					break;
				}
			} 

			if (!is_valid) continue;

			if (!start) {
				start = p;

				/* valid words must start with an alpha or underscore if we are filtering */
				if (filter_words) {

					if (type == WORD_NUM) {
						if (!tracker->index_numbers) {
							is_valid = FALSE;
							continue;
						}

					} else {
	
						if (type == WORD_HYPHEN) {
							is_valid = FALSE;
							continue;
						}
					}	
				}				
				
			}

			if (length >= tracker->max_word_length) {
				continue;
			}

			
			length++;


			switch (type) {

	  			case WORD_ASCII_HIGHER: 
					c += 32;

	  			case WORD_ASCII_LOWER: 
				case WORD_HYPHEN:
				case WORD_UNDERSCORE:

					if (word_type == WORD_NUM || word_type == WORD_ALPHA_NUM) {
						word_type = WORD_ALPHA_NUM;
					} else {
						word_type = WORD_ALPHA;
					}
					
					break;

				case WORD_NUM: 

					if (word_type == WORD_ALPHA || word_type == WORD_ALPHA_NUM) {
						word_type = WORD_ALPHA_NUM;
					} else {
						word_type = WORD_NUM;
					}
					break;

				case WORD_ALPHA_HIGHER: 
					c = g_unichar_tolower (c);

	  			case WORD_ALPHA_LOWER: 

					if (!do_strip) {
						do_strip = TRUE;
					}

					if (word_type == WORD_NUM || word_type == WORD_ALPHA_NUM) {
						word_type = WORD_ALPHA_NUM;
					} else {
						word_type = WORD_ALPHA;
					}
					
					break;

				default: 
					break;

			}

			word[length -1] = c;
			
		}


		if (is_valid) {

			if (word_type == WORD_NUM) {
				if (!filter_words || length >= tracker->index_number_min_length) {
					*index_word = g_ucs4_to_utf8 (word, length, NULL, NULL, NULL);
				} 

			} else {
			
				if (length >= tracker->min_word_length) {
			
					char 	*str = NULL, *tmp;
					guint32 len;

					char *utf8 = g_ucs4_to_utf8 (word, length, NULL, &bytes, NULL);

					if (!utf8) {
						return p;
					}
					
					if (do_strip) {
						str = strip_word (utf8, bytes, &len);
					}

					if (!str) {
						tmp = g_utf8_normalize (utf8, bytes, G_NORMALIZE_NFC);
					} else {
						tmp = g_utf8_normalize (str, len, G_NORMALIZE_NFC);
						g_free (str);
					}

					g_free (utf8);

					/* ignore all stop words */
					if (filter_words && tracker->stop_words) {
						if (g_hash_table_lookup (tracker->stop_words, tmp)) {
							g_free (tmp);
							
							return p;
						}
					}

		
					if (do_stem && (tracker->stemmer && tracker->use_stemmer)) {

						*index_word = tracker_stem (tmp, strlen (tmp));
						g_free (tmp);
					} else {
						*index_word = tmp;			
					}

								
				}
			}

		} 

		return p;	
			
	} 
	return NULL;

}


char *
tracker_parse_text_to_string (const char *txt, gboolean filter_words, gboolean delimit)
{
	const char *p = txt;	

	char *word = NULL;
	guint32 i = 0;

	if (txt) {

		if (text_needs_pango (txt)) {
			/* CJK text does not need stemming or other treatment */

			PangoLogAttr *attrs;
			guint	     nb_bytes, str_len, word_start;
			GString	     *strs;

			nb_bytes = strlen (txt);
                        str_len = g_utf8_strlen (txt, -1);

			strs = g_string_new (" ");

			attrs = g_new0 (PangoLogAttr, str_len + 1);

			pango_get_log_attrs (txt, nb_bytes, 0, pango_language_from_string ("C"), attrs, str_len + 1);

			word_start = 0;

			for (i = 0; i < str_len + 1; i++) {

				if (attrs[i].is_word_end) {
					char *start_word, *end_word;

					start_word = g_utf8_offset_to_pointer (txt, word_start);
					end_word = g_utf8_offset_to_pointer (txt, i);

					if (start_word != end_word) {
						
						/* normalize word */
						char *s = g_utf8_casefold (start_word, end_word - start_word);
					
						char *index_word = g_utf8_normalize (s, -1, G_NORMALIZE_NFC);
						g_free (s);
					
						strs  = g_string_append (strs, index_word);
						strs  = g_string_append_c (strs, ' ');

						g_free (index_word);
					}

					word_start = i;
				}
	
				if (attrs[i].is_word_start) {
					word_start = i;
				}
			}

			g_free (attrs);

			return g_string_free (strs, FALSE);

			
		} else {

			GString *str = g_string_new (" ");

			while (TRUE) {
				i++;
				p = analyze_text (p, &word, filter_words, delimit);

				if (word) {

					g_string_append (str, word);

					g_string_append_c (str, ' ');

					g_free (word);			
				}

				if (!p || !*p) {
					return g_string_free (str, FALSE);
				}
			}
		}
	}

	return NULL;
}


char **
tracker_parse_text_into_array (const char *text)
{
	char *s = tracker_parse_text_to_string (text, TRUE, FALSE);

	char **array =  g_strsplit (s, " ", -1);

	g_free (s);

	return array;
}


static inline void
update_word_count (GHashTable *word_table, const char *word, int weight)
{
	int count;

	g_return_if_fail (word || (weight > 0));

	/* count dupes */
	count = GPOINTER_TO_INT (g_hash_table_lookup (word_table, word));
	g_hash_table_insert (word_table, g_strdup (word), GINT_TO_POINTER (count + weight));

}


/* use this for already processed text only */
GHashTable *
tracker_parse_text_fast (GHashTable *word_table, const char *txt, int weight)
{
	if (!word_table) {
		word_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	} 

	if (!txt || weight == 0) {
		return word_table;
	}

	char **tmp;	
	char **array;
	int  count;

	array =  g_strsplit (txt, " ", -1);

	for (tmp = array; *tmp; tmp++) {
		if (**tmp) {
			count = GPOINTER_TO_INT (g_hash_table_lookup (word_table, *tmp));
			g_hash_table_insert (word_table, g_strdup (*tmp), GINT_TO_POINTER (count + weight));	
		}
	}

	g_strfreev (array);

	return word_table;
}


/* use this for unprocessed raw text */
GHashTable *
tracker_parse_text (GHashTable *word_table, const char *txt, int weight, gboolean filter_words, gboolean delimit_words)
{
	int total_words, count;

	if (!word_table) {
		word_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		total_words = 0;
	} else {
		total_words = g_hash_table_size (word_table);
	}

	if (!txt || weight == 0) {
		return word_table;
	}

	const char *p = txt;	

	char *word = NULL;
	guint32 i = 0;

	if (text_needs_pango (txt)) {
		/* CJK text does not need stemming or other treatment */

		PangoLogAttr *attrs;
		guint	     nb_bytes, str_len, word_start;

                nb_bytes = strlen (txt);
                str_len = g_utf8_strlen (txt, -1);

		attrs = g_new0 (PangoLogAttr, str_len + 1);

		pango_get_log_attrs (txt, nb_bytes, 0, pango_language_from_string ("C"), attrs, str_len + 1);

		word_start = 0;

		for (i = 0; i < str_len + 1; i++) {

			if (attrs[i].is_word_end) {
				char *start_word, *end_word;

				start_word = g_utf8_offset_to_pointer (txt, word_start);
				end_word = g_utf8_offset_to_pointer (txt, i);

				if (start_word != end_word) {
						
					/* normalize word */
					char *s = g_utf8_casefold (start_word, end_word - start_word);
					
					char *index_word = g_utf8_normalize (s, -1, G_NORMALIZE_NFC);
					g_free (s);
					
					total_words++;
					
					if (total_words < tracker->max_words_to_index) { 

						count = GPOINTER_TO_INT (g_hash_table_lookup (word_table, index_word));
						g_hash_table_insert (word_table, index_word, GINT_TO_POINTER (count + weight));	

					} else {
						g_free (index_word);
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

		while (TRUE) {
			i++;
			p = analyze_text (p, &word, filter_words, delimit_words);

			if (word) {

				total_words++;

				if (total_words < tracker->max_words_to_index) { 
			
					count = GPOINTER_TO_INT (g_hash_table_lookup (word_table, word));

					g_hash_table_insert (word_table, word, GINT_TO_POINTER (count + weight));	


				} else {
					g_free (word);
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
