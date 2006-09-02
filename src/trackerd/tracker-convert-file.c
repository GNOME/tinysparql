/* Tracker convert file for processing by tracker
 *
 * Copyright (C) 1999-2000 Red Hat Software
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <locale.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "tracker-parser.h"




static char *
load_text_from_file (const char *filename_in_locale)
{

	char *text;
	gsize length;

	if (!g_file_get_contents (filename_in_locale, &text, &length, NULL)) {
		g_warning ("could not open file %s", filename_in_locale);
    		return NULL;
	}


	if (!g_utf8_validate (text, -1, NULL)) {

		char *value;

		/* attempt to convert from locale */
		value = g_locale_to_utf8 (text, length, NULL, NULL, NULL);

		if (!value) {
			g_warning ("Cannot convert file to valid utf-8\n");	
			return NULL;
		}
		
		g_free (text);

		return value;
		
  	}
	
  	return text;

}

static void
write_data (gpointer key,
       	    gpointer value,
	    gpointer user_data)
{
	char *word = (char *)key;
	int score = GPOINTER_TO_INT (value);
	GString *str = user_data;

	g_string_append_printf (str, "%d %s\n", score, word);

}


static void
save_words_to_file (const char *filename_in_locale, GHashTable *table)
{

	GString *str = g_string_new ("");

	if (table) {
		g_hash_table_foreach (table, write_data, str);
		g_hash_table_destroy (table);	
	}

	g_file_set_contents (filename_in_locale, g_string_free (str, FALSE), -1, NULL);
		
}


int
main (int argc, char **argv)
{
	char *text;
	GHashTable *table;


	setlocale (LC_ALL, "");

	if (argc < 3) {
		g_print ("must give two filenames on the command line\n");
  		return 1;
	}


	text = load_text_from_file (argv[1]);
	
	if (!text) {
		return 1;
	}

	table = tracker_parse_text (text, 3, NULL, NULL, TRUE , 1);

	g_free (text);

  	save_words_to_file (argv[2], table);
  
	return 0;
}
