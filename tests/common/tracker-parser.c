/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#include <string.h>
#include <locale.h>

#include <glib.h>
#include <gio/gio.h>

#include <tracker-common.h>

/* Normally this would be in the libtracker-fts config */
#define DEFAULT_MAX_WORD_LENGTH   30
#define DEFAULT_ENABLE_STEMMER    FALSE
#define DEFAULT_ENABLE_UNACCENT   TRUE
#define DEFAULT_IGNORE_NUMBERS    TRUE

static gchar    *text;
static gchar    *filename;
static gboolean  verbose;

/* Command Line options */
static const GOptionEntry options [] = {
	{
		"verbose", 'v', G_OPTION_FLAG_NO_ARG,
		G_OPTION_ARG_NONE, &verbose,
		"Enable verbose output",
		NULL
	},
	{
		"text", 't', 0,
		G_OPTION_ARG_STRING, &text,
		"Specific text to parse",
		NULL
	},
	{
		"file", 'f', 0,
		G_OPTION_ARG_STRING, &filename,
		"Specific file to parse its contents",
		NULL
	},
	{ NULL }
};

static gboolean
setup_context (gint argc,
               gchar **argv)
{
	GOptionContext *context = NULL;
	GError *error = NULL;

	/* Setup command line options */
	context = g_option_context_new ("- Test the Tracker FTS parser");
	g_option_context_add_main_entries (context,
	                                   options,
	                                   argv[0]);

	/* Parse input arguments */
	if (!g_option_context_parse (context,
	                             &argc,
	                             &argv,
	                             &error))
	{
		g_printerr ("%s\nRun '%s --help' to see a full list of available "
		            "command line options.\n",
		            error->message,
		            argv[0]);
		g_error_free (error);
		return FALSE;
	}

	g_option_context_free (context);
	return TRUE;
}

static gboolean
load_file_contents (void)
{
	GError *error = NULL;
	GFile *file;

	file = g_file_new_for_commandline_arg (filename);
	if (!g_file_load_contents (file, NULL, &text, NULL, NULL, &error)) {
		g_printerr ("Error loading file '%s' contents: '%s'\n",
		            filename,
		            error->message);
		g_error_free (error);
		g_object_unref (file);
		return FALSE;
	}
	g_object_unref (file);
	return TRUE;
}

static gboolean
run_parsing (void)
{
	TrackerParser *parser;
	GTimer *timer;

	/* Initialize timing */
	timer = g_timer_new ();

	/* Create the parser */
	parser = tracker_parser_new ();
	if (!parser) {
		g_printerr ("Parser creation failed!\n");
		return FALSE;
	}

	/* Reset the parser with our string, reading the current FTS config */

	tracker_parser_reset (parser,
	                      text,
	                      strlen (text),
	                      DEFAULT_MAX_WORD_LENGTH,
	                      DEFAULT_ENABLE_STEMMER,
	                      DEFAULT_ENABLE_UNACCENT,
	                      DEFAULT_IGNORE_NUMBERS);

	/* Loop through all words! */
	while (1) {
		const gchar *word;
		gint position;
		gint byte_offset_start;
		gint byte_offset_end;
		gint word_length;


		/* Process next word */
		word = tracker_parser_next (parser,
		                            &position,
		                            &byte_offset_start,
		                            &byte_offset_end,
		                            &word_length);

		/* Stop loop if no more words */
		if (!word) {
			break;
		}

		if (verbose) {
			gchar *word_hex;
			gchar *original_word;
			gchar *original_word_hex;
			gint original_word_length;

			/* Get original word */
			original_word_length = byte_offset_end - byte_offset_start;
			original_word = g_malloc (original_word_length + 1);
			memcpy (original_word,
			        &text[byte_offset_start],
			        original_word_length);
			original_word[original_word_length] = '\0';

			/* Get hex strings */
			word_hex = tracker_strhex ((guint8 *)word, word_length, ':');
			original_word_hex = tracker_strhex ((guint8 *)original_word,
			                                    original_word_length,
			                                    ':');

			g_print ("WORD at %d [%d,%d] Original: '%s' (%s), "
			         "Processed: '%s' (%s)\n",
			         position,
			         byte_offset_start,
			         byte_offset_end,
			         original_word,
			         original_word_hex,
			         word,
			         word_hex);

			g_free (word_hex);
			g_free (original_word_hex);
			g_free (original_word);
		}
	}

	g_print ("\n----> Parsing finished after '%lf' seconds\n",
	         g_timer_elapsed (timer, NULL));

	g_timer_destroy (timer);

	tracker_parser_free (parser);
	return TRUE;
}


int
main (int argc, char **argv)
{
	/* Setup locale */
	setlocale (LC_ALL, "");

	/* Setup context */
	if (!setup_context (argc, argv)) {
		g_printerr ("Context setup failed... exiting\n");
		return -1;
	}

	/* Either text or file must be given */
	if (filename == NULL &&
	    text == NULL) {
		g_printerr ("Either 'file' or 'text' options should be used\n"
		            "Run '%s --help' to see a full list of available "
		            "command line options.\n",
		            argv[0]);
		return -2;
	}

	/* If required, load file contents */
	if (filename != NULL &&
	    !load_file_contents ()) {
		g_printerr ("Loading file '%s' contents failed... exiting\n",
		            filename);
		return -3;
	}

	/* Run the parsing! */
	if (!run_parsing ()) {
		g_printerr ("Parsing operation failed... exiting\n");
		return -4;
	}

	/* Clean exit */
	if (filename)
		g_free (text);
	return 0;
}
