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

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-fts/tracker-parser.h>

/* Note
 *  Currently, three different types of parsers are defined in libtracker-fts:
 *    - GNU libunistring-based parser, up to 20% faster than the others, and full
 *       unicode compliant.
 *    - libicu-based parser, full unicode compliant but slower as it needs
 *       conversions to/from UChars (UTF-16 encoded strings)
 *    - glib/pango parser, not fully unicode compliant as it doesn't work properly
 *       with decomposed strings (NFD normalized), doesn't make a unicode-based
 *       word-breaking and doesn't make full-word casefolding.
 *
 * Some of the tests, thus, will be DISABLED for the GLIB/PANGO parser.
 */
#undef FULL_UNICODE_TESTS
#if defined HAVE_LIBUNISTRING || defined HAVE_LIBICU
#define FULL_UNICODE_TESTS
#endif

/* -------------- COMMON FOR ALL TESTS ----------------- */

/* Fixture object type */
typedef struct {
	/* The parser object */
	TrackerParser    *parser;

	/* Default parser configuration to use */
	gint              max_word_length;
	gboolean          enable_stemmer;
	gboolean          enable_unaccent;
	gboolean          ignore_stop_words;
	gboolean          ignore_reserved_words;
	gboolean          ignore_numbers;
} TrackerParserTestFixture;

/* Common setup for all tests */
static void
test_common_setup (TrackerParserTestFixture *fixture,
                   gconstpointer data)
{
	TrackerLanguage  *language;

	/* Setup language for parser. We make sure that always English is used
	 *  in the unit tests, because we want the English stemming method to
	 *  be used. */
	language = tracker_language_new ("en");
	if (!language) {
		g_critical ("Language setup failed!");
		return;
	}

	/* Default conf parameters */
	fixture->max_word_length = 50;
	fixture->enable_stemmer = TRUE;
	fixture->enable_unaccent = TRUE;
	fixture->ignore_stop_words = TRUE;
	fixture->ignore_reserved_words = TRUE;
	fixture->ignore_numbers = TRUE;

	/* Create the parser */
	fixture->parser = tracker_parser_new (language,
	                                      fixture->max_word_length);
	if (!fixture->parser) {
		g_critical ("Parser creation failed!");
		return;
	}

	g_object_unref (language);
}

/* Common teardown for all tests */
static void
test_common_teardown (TrackerParserTestFixture *fixture,
                      gconstpointer data)
{
	if (fixture->parser) {
		tracker_parser_free (fixture->parser);
	}
}

/* -------------- EXPECTED NUMBER OF WORDS TESTS ----------------- */

/* Test struct for the expected-nwords tests */
typedef struct TestDataExpectedNWords TestDataExpectedNWords;
struct TestDataExpectedNWords {
	const gchar *str;
	gboolean     ignore_numbers;
	guint        expected_nwords;
};

/* Common expected_word test method */
static void
expected_nwords_check (TrackerParserTestFixture *fixture,
                       gconstpointer data)
{
	const TestDataExpectedNWords *testdata = data;
	const gchar *word;
	gint position;
	gint byte_offset_start;
	gint byte_offset_end;
	gboolean stop_word;
	gint word_length;
	guint nwords = 0;

	/* Reset the parser with the test string */
	tracker_parser_reset (fixture->parser,
	                      testdata->str,
	                      strlen (testdata->str),
	                      fixture->enable_stemmer,
	                      fixture->enable_unaccent,
	                      fixture->ignore_stop_words,
	                      fixture->ignore_reserved_words,
	                      testdata->ignore_numbers);

	/* Count number of output words */
	while ((word = tracker_parser_next (fixture->parser,
	                                    &position,
	                                    &byte_offset_start,
	                                    &byte_offset_end,
	                                    &stop_word,
	                                    &word_length))) {
		nwords++;
	}

	/* Check if input is same as expected */
	g_assert_cmpuint (nwords, == , testdata->expected_nwords);
}

/* -------------- EXPECTED WORD TESTS ----------------- */

/* Test struct for the expected-word tests */
typedef struct TestDataExpectedWord TestDataExpectedWord;
struct TestDataExpectedWord {
	const gchar  *str;
	const gchar  *expected;
	gboolean      enable_stemmer;
	gboolean      enable_unaccent;
};

/* Common expected_word test method */
static void
expected_word_check (TrackerParserTestFixture *fixture,
                     gconstpointer data)
{
	const TestDataExpectedWord *testdata = data;
	const gchar *word;
	gint position;
	gint byte_offset_start;
	gint byte_offset_end;
	gboolean stop_word;
	gint word_length;

	/* Reset the parser with our string */
	tracker_parser_reset (fixture->parser,
	                      testdata->str,
	                      strlen (testdata->str),
	                      testdata->enable_stemmer,
	                      testdata->enable_unaccent,
	                      fixture->ignore_stop_words,
	                      fixture->ignore_reserved_words,
	                      fixture->ignore_numbers);

	/* Process next word */
	word = tracker_parser_next (fixture->parser,
	                            &position,
	                            &byte_offset_start,
	                            &byte_offset_end,
	                            &stop_word,
	                            &word_length);

	/* Check if input is same as expected */
	g_assert_cmpstr (word, == , testdata->expected);
}

/* -------------- LIST OF TESTS ----------------- */

#ifdef HAVE_UNAC
/* Normalization-related tests (unaccenting) */
static const TestDataExpectedWord test_data_normalization[] = {
	{ "école",                "ecole", FALSE, TRUE  },
	{ "ÉCOLE",                "ecole", FALSE, TRUE  },
	{ "École",                "ecole", FALSE, TRUE  },
#ifdef FULL_UNICODE_TESTS /* glib/pango doesn't like NFD strings */
	{ "e" "\xCC\x81" "cole",  "ecole", FALSE, TRUE  },
	{ "E" "\xCC\x81" "COLE",  "ecole", FALSE, TRUE  },
	{ "E" "\xCC\x81" "cole",  "ecole", FALSE, TRUE  },
#endif
	{ NULL,                   NULL,    FALSE, FALSE }
};

/* Unaccenting-related tests */
static const TestDataExpectedWord test_data_unaccent[] = {
	{ "Murciélago", "murcielago", FALSE, TRUE  },
	{ "camión",     "camion",     FALSE, TRUE  },
	{ "desagüe",    "desague",    FALSE, TRUE  },
	{ "Murciélago", "murciélago", FALSE, FALSE },
	{ "camión",     "camión",     FALSE, FALSE },
	{ "desagüe",    "desagüe",    FALSE, FALSE },
	{ NULL,         NULL,         FALSE, FALSE }
};
#else
/* Normalization-related tests (not unaccenting) */
static const TestDataExpectedWord test_data_normalization[] = {
	{ "école",                "école", FALSE, FALSE },
	{ "ÉCOLE",                "école", FALSE, FALSE },
	{ "École",                "école", FALSE, FALSE },
#ifdef FULL_UNICODE_TESTS /* glib/pango doesn't like NFD strings */
	{ "e" "\xCC\x81" "cole",  "école", FALSE, FALSE },
	{ "E" "\xCC\x81" "COLE",  "école", FALSE, FALSE },
	{ "E" "\xCC\x81" "cole",  "école", FALSE, FALSE },
#endif
	{ "école",                "école", FALSE, TRUE  },
	{ "ÉCOLE",                "école", FALSE, TRUE  },
	{ "École",                "école", FALSE, TRUE  },
#ifdef FULL_UNICODE_TESTS /* glib/pango doesn't like NFD strings */
	{ "e" "\xCC\x81" "cole",  "école", FALSE, TRUE  },
	{ "E" "\xCC\x81" "COLE",  "école", FALSE, TRUE  },
	{ "E" "\xCC\x81" "cole",  "école", FALSE, TRUE  },
#endif
	{ NULL,                   NULL,    FALSE, FALSE }
};
#endif /* !HAVE_UNAC */

/* Stemming-related tests */
static const TestDataExpectedWord test_data_stemming[] = {
	{ "ecole", "ecol",  TRUE,  TRUE  },
	{ "ecole", "ecole", FALSE, TRUE  },
	{ NULL,    NULL,    FALSE, FALSE }
};

/* Casefolding-related tests */
static const TestDataExpectedWord test_data_casefolding[] = {
	{ "gross", "gross", FALSE, TRUE  },
	{ "GROSS", "gross", FALSE, TRUE  },
	{ "GrOsS", "gross", FALSE, TRUE  },
#ifdef FULL_UNICODE_TESTS /* glib/pango doesn't do full-word casefolding */
	{ "groß",  "gross", FALSE, TRUE  },
#endif
	{ NULL,    NULL,    FALSE, FALSE }
};

/* Number of expected words tests */
static const TestDataExpectedNWords test_data_nwords[] = {
#ifdef FULL_UNICODE_TESTS /* glib/pango thinks 32.3 are 2 words */
	{ "The quick (\"brown\") fox can’t jump 32.3 feet, right?", TRUE,   8 },
	{ "The quick (\"brown\") fox can’t jump 32.3 feet, right?", FALSE,  9 },
#endif
	{ "ホモ・サピエンス",                                          TRUE,   2 }, /* katakana */
#ifdef FULL_UNICODE_TESTS /* glib/pango doesn't work properly with chinese */
	{ "本州最主流的风味",                                          TRUE,   8 }, /* chinese */
#endif
	{ "Американские суда находятся в международных водах.",     TRUE,   6 }, /* russian */
	{ "Bần chỉ là một anh nghèo xác",                            TRUE,   7 }, /* vietnamese */
#ifdef FULL_UNICODE_TESTS /* glib/pango doesn't work properly with chinese */
	{ "ホモ・サピエンス 本州最主流的风味 katakana, chinese, english", TRUE,  13 }, /* mixed */
#endif
	{ NULL,                                                     FALSE,  0 }
};

int
main (int argc, char **argv)
{
	gint i;

	g_type_init ();
	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}
	g_test_init (&argc, &argv, NULL);

	/* Add normalization checks */
	for (i = 0; test_data_normalization[i].str != NULL; i++) {
		gchar *testpath;

		testpath = g_strdup_printf ("/libtracker-fts/parser/normalization_%d", i);
		g_test_add (testpath,
		            TrackerParserTestFixture,
		            &test_data_normalization[i],
		            test_common_setup,
		            expected_word_check,
		            test_common_teardown);
		g_free (testpath);
	}

#ifdef HAVE_UNAC
	/* Add unaccent checks */
	for (i = 0; test_data_unaccent[i].str != NULL; i++) {
		gchar *testpath;

		testpath = g_strdup_printf ("/libtracker-fts/parser/unaccent_%d", i);
		g_test_add (testpath,
		            TrackerParserTestFixture,
		            &test_data_unaccent[i],
		            test_common_setup,
		            expected_word_check,
		            test_common_teardown);
		g_free (testpath);
	}
#endif

	/* Add casefolding checks */
	for (i = 0; test_data_casefolding[i].str != NULL; i++) {
		gchar *testpath;

		testpath = g_strdup_printf ("/libtracker-fts/parser/casefolding_%d", i);
		g_test_add (testpath,
		            TrackerParserTestFixture,
		            &test_data_casefolding[i],
		            test_common_setup,
		            expected_word_check,
		            test_common_teardown);
		g_free (testpath);
	}

	/* Add stemming checks */
	for (i = 0; test_data_stemming[i].str != NULL; i++) {
		gchar *testpath;

		testpath = g_strdup_printf ("/libtracker-fts/parser/stemming_%d", i);
		g_test_add (testpath,
		            TrackerParserTestFixture,
		            &test_data_stemming[i],
		            test_common_setup,
		            expected_word_check,
		            test_common_teardown);
		g_free (testpath);
	}

	/* Add expected number of words checks */
	for (i = 0; test_data_nwords[i].str != NULL; i++) {
		gchar *testpath;

		testpath = g_strdup_printf ("/libtracker-fts/parser/nwords_%d", i);
		g_test_add (testpath,
		            TrackerParserTestFixture,
		            &test_data_nwords[i],
		            test_common_setup,
		            expected_nwords_check,
		            test_common_teardown);
		g_free (testpath);
	}

	return g_test_run ();
}
