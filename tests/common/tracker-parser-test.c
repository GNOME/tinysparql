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

#include <tracker-common.h>

/* -------------- COMMON FOR ALL TESTS ----------------- */

/* Fixture object type */
typedef struct {
	/* The parser object */
	TrackerParser *parser;

	/* Default parser configuration to use */
	gint max_word_length;
	gboolean enable_stemmer;
	gboolean enable_unaccent;
	gboolean ignore_numbers;
} TrackerParserTestFixture;

/* Common setup for all tests */
static void
test_common_setup (TrackerParserTestFixture *fixture,
                   gconstpointer             data)
{
	/* Default conf parameters */
	fixture->max_word_length = 50;
	fixture->enable_stemmer = TRUE;
	fixture->enable_unaccent = TRUE;
	fixture->ignore_numbers = TRUE;

	/* Create the parser */
	fixture->parser = tracker_parser_new ();
	if (!fixture->parser) {
		g_critical ("Parser creation failed!");
		return;
	}
}

/* Common teardown for all tests */
static void
test_common_teardown (TrackerParserTestFixture *fixture,
                      gconstpointer             data)
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
	gboolean ignore_numbers;
	guint expected_nwords;
	gint alternate_expected_nwords;
};

/* Common expected_word test method */
static void
expected_nwords_check (TrackerParserTestFixture *fixture,
                       gconstpointer             data)
{
	const TestDataExpectedNWords *testdata = data;
	gint position;
	gint byte_offset_start;
	gint byte_offset_end;
	gint word_length;
	guint nwords = 0;

	/* Reset the parser with the test string */
	tracker_parser_reset (fixture->parser,
	                      testdata->str,
	                      strlen (testdata->str),
	                      fixture->max_word_length,
	                      fixture->enable_stemmer,
	                      fixture->enable_unaccent,
	                      testdata->ignore_numbers);

	/* Count number of output words */
	while (tracker_parser_next (fixture->parser,
				    &position,
				    &byte_offset_start,
				    &byte_offset_end,
				    &word_length)) {
		nwords++;
	}

	/* Some tests will yield different results when using different versions of
	 * libicu (e.g. chinese ones). Handle this by allowing an alternate number
	 * of words expected in the test. Note that our whole purpose is to test
	 * that we can split different words, not much about the number of words
	 * itself (althogh we should check that as well) */

	if (testdata->alternate_expected_nwords < 0) {
		/* Check if input is same as expected */
		g_assert_cmpuint (nwords, == , testdata->expected_nwords);
	} else {
		/* Treat the possible differences as a range */
		g_assert_cmpuint (nwords, >=, testdata->expected_nwords);
		g_assert_cmpuint (nwords, <=, testdata->alternate_expected_nwords);
	}
}

/* -------------- EXPECTED WORD TESTS ----------------- */

/* Test struct for the expected-word tests */
typedef struct TestDataExpectedWord TestDataExpectedWord;
struct TestDataExpectedWord {
	const gchar *str;
	const gchar *expected;
	gboolean enable_stemmer;
	gboolean enable_unaccent;
};

/* Common expected_word test method */
static void
expected_word_check (TrackerParserTestFixture *fixture,
                     gconstpointer             data)
{
	const TestDataExpectedWord *testdata = data;
	const gchar *word;
	gchar *expected_nfkd;
	gint position;
	gint byte_offset_start;
	gint byte_offset_end;
	gint word_length;

	/* Reset the parser with our string */
	tracker_parser_reset (fixture->parser,
	                      testdata->str,
	                      strlen (testdata->str),
	                      fixture->max_word_length,
	                      testdata->enable_stemmer,
	                      testdata->enable_unaccent,
	                      fixture->ignore_numbers);

	/* Process next word */
	word = tracker_parser_next (fixture->parser,
	                            &position,
	                            &byte_offset_start,
	                            &byte_offset_end,
	                            &word_length);

	/* Expected word MUST always be in NFKD normalization */
	expected_nfkd = g_utf8_normalize (testdata->expected,
	                                  -1,
	                                  G_NORMALIZE_NFKD);

	/* Check if input is same as expected */
	g_assert_cmpstr (word, == , expected_nfkd);

	g_free (expected_nfkd);
}

static void
test_stemmer (TrackerParserTestFixture *fixture,
              gconstpointer             data)
{
#ifdef HAVE_LIBSTEMMER
       expected_word_check (fixture, data);
#else
       g_test_skip ("Built without libstemmer");
#endif
}

static void
test_unac (TrackerParserTestFixture *fixture,
           gconstpointer             data)
{
#ifdef HAVE_UNAC
       expected_word_check (fixture, data);
#else
       g_test_skip ("Built without UNAC");
#endif
}

/* -------------- LIST OF TESTS ----------------- */

/* Normalization-related tests (unaccenting) */
static const TestDataExpectedWord test_data_normalization[] = {
	{ "école",                "ecole", FALSE, TRUE  },
	{ "ÉCOLE",                "ecole", FALSE, TRUE  },
	{ "École",                "ecole", FALSE, TRUE  },
	{ "e" "\xCC\x81" "cole",  "ecole", FALSE, TRUE  },
	{ "E" "\xCC\x81" "COLE",  "ecole", FALSE, TRUE  },
	{ "E" "\xCC\x81" "cole",  "ecole", FALSE, TRUE  },
	{ NULL,                   NULL,    FALSE, FALSE }
};

/* Unaccenting-related tests */
static const TestDataExpectedWord test_data_unaccent[] = {
	{ "Murciélago",   "murcielago", FALSE, TRUE  },
	{ "camión",       "camion",     FALSE, TRUE  },
	{ "desagüe",      "desague",    FALSE, TRUE  },
	{ "Ὰ",            "α",          FALSE, TRUE  }, /* greek capital alpha with U+0300, composed */
	{ "ὰ",            "α",          FALSE, TRUE  }, /* greek small alpha with U+0300, composed */
	{ "Ὶ",            "ι",          FALSE, TRUE  }, /* greek capital iotta with U+0300, composed */
	{ "ὶ",            "ι",          FALSE, TRUE  }, /* greek small iotta with U+0300, composed */
	{ "Ὼ",            "ω",          FALSE, TRUE  }, /* greek capital omega with U+0300, composed */
	{ "ὼ",            "ω",          FALSE, TRUE  }, /* greek small omega with U+0300, composed */
	{ "Ὰ",          "α",          FALSE, TRUE  }, /* capital alpha with U+0300, decomposed */
	{ "ὰ",          "α",          FALSE, TRUE  }, /* small alpha with U+0300, decomposed */
	{ "Ὶ",          "ι",          FALSE, TRUE  }, /* capital iotta with U+0300, decomposed */
	{ "ὶ",          "ι",          FALSE, TRUE  }, /* small iotta with U+0300, decomposed */
	{ "Ὼ",          "ω",          FALSE, TRUE  }, /* capital omega with U+0300, decomposed */
	{ "ὼ",          "ω",          FALSE, TRUE  }, /* small omega with U+0300, decomposed */
	{ "aN͡Ga",       "anga",       FALSE, TRUE  }, /* 0x0361 affects to two characters */
	{ "aNG͡a",       "anga",       FALSE, TRUE  }, /* 0x0361 affects to two characters */
	{ "Murciélago", "murciélago", FALSE, FALSE },
	{ "camión",     "camión",     FALSE, FALSE },
	{ "desagüe",    "desagüe",    FALSE, FALSE },
	{ NULL,         NULL,         FALSE, FALSE }
};

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
	{ "groß",  "gross", FALSE, TRUE  },
	{ NULL,    NULL,    FALSE, FALSE }
};

/* Number of expected words tests */
static const TestDataExpectedNWords test_data_nwords[] = {
	{ "The quick (\"brown\") fox can’t jump 32.3 feet, right?", TRUE,   8, -1 },
	{ "The quick (\"brown\") fox can’t jump 32.3 feet, right?", FALSE, 10, -1 },
	/* Note: as of 0.9.15, the dot is always a word breaker, even between
	 *  numbers. */
	{ "filename.txt",                                           TRUE,   2, -1 },
	{ ".hidden.txt",                                            TRUE,   2, -1 },
	{ "noextension.",                                           TRUE,   1, -1 },
	{ "ホモ・サピエンス",                                          TRUE,   2, -1 }, /* katakana */
	{ "喂人类",                                                   TRUE,   1, 3 }, /* chinese */
	{ "Американские суда находятся в международных водах.",     TRUE,   6, -1 }, /* russian */
	{ "Bần chỉ là một anh nghèo xác",                            TRUE,   7, -1 }, /* vietnamese */
	{ "ホモ・サピエンス 喂人类 katakana, chinese, english",          TRUE,   6, 8 }, /* mixed */
	{ NULL,                                                     FALSE,  0, 0 }
};

int
main (int argc, char **argv)
{
	gint i;

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

	/* Add unaccent checks */
	for (i = 0; test_data_unaccent[i].str != NULL; i++) {
		gchar *testpath;

		testpath = g_strdup_printf ("/libtracker-fts/parser/unaccent_%d", i);
		g_test_add (testpath,
		            TrackerParserTestFixture,
		            &test_data_unaccent[i],
		            test_common_setup,
		            test_unac,
		            test_common_teardown);
		g_free (testpath);
	}

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
		            test_stemmer,
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
