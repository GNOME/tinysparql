#include <glib.h>
#include <glib/gtestutils.h>
#include <string.h>

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-language.h>
#include <libtracker-common/tracker-parser.h>

/*
 * len(word) > 3 : 6 words
 * longest word: 10 chars
 * stop words ("here", "a", "of", "various", "to", "after")
 */
#define SAMPLE_TEXT "Here a good collection of various words to parse 12345678 after"

static TrackerConfig *config;
static TrackerLanguage *language;

static void
assert_key_length (gpointer key, gpointer value, gpointer user_data)
{
	gint max_length = GPOINTER_TO_INT (user_data);

	g_assert_cmpint (strlen (key), <=, max_length);
}

/*
 * Test max_words_to_index and min_length of the word
 */
static void
test_parser_text_max_words_to_index (void)
{
	GHashTable *result = NULL;

	result = tracker_parser_text (result,
				      SAMPLE_TEXT,
				      1,
				      language,
				      5, /* max words to index */
				      18, /* max length of the word */
				      3, /* min length of the word */
				      FALSE, FALSE); /* Filter / Delimit */

	g_assert_cmpint (g_hash_table_size (result), ==, 5);

	g_hash_table_unref (result);
}

/*
 * Test max length of the word.
 */
static void
test_parser_text_max_length (void)
{
	GHashTable *result = NULL;
	gint max_length;

	max_length = 6;
	result = tracker_parser_text (result,
				      SAMPLE_TEXT,
				      1,
				      language,
				      10, /* max words to index */
				      max_length, /* max length of the word */
				      3, /* min length of the word */
				      FALSE, FALSE); /* Filter / Delimit */
	g_hash_table_foreach (result, assert_key_length, GINT_TO_POINTER (max_length));
	g_assert_cmpint (g_hash_table_size (result), ==, 8);

	g_hash_table_unref (result);
}

/*
 * Filter numbers
 */
static void
test_parser_text_filter_numbers_stop_words (void)
{
	GHashTable *result = NULL;

	/* Filtering numbers */
	result = tracker_parser_text (result,
				      SAMPLE_TEXT,
				      1,
				      language,
				      100, /* max words to index */
				      100, /* max length of the word */
				      0, /* min length of the word */
				      TRUE, FALSE); /* Filter / Delimit */

	g_assert (!g_hash_table_lookup (result, "12345678"));

	g_assert_cmpint (g_hash_table_size (result), ==, 4);

	g_hash_table_unref (result);
	result = NULL;

	/* No filter */
	result = tracker_parser_text (result,
				      SAMPLE_TEXT,
				      1,
				      language,
				      100, /* max words to index */
				      100, /* max length of the word */
				      0, /* min length of the word */
				      FALSE, FALSE); /* Filter / Delimit */

	g_assert_cmpint (g_hash_table_size (result), ==, 11);

	g_assert (g_hash_table_lookup (result, "12345678"));

	g_hash_table_unref (result);
	result = NULL;
}

static void
test_parser_stop_words (void)
{
	GHashTable *stop_words, *result = NULL;

	/* Check we have the default stop words */
	stop_words = tracker_language_get_stop_words (language);
	g_assert (stop_words);
	g_assert_cmpint (g_hash_table_size (stop_words), >, 1);

	/* Set specific stop words to test */
	tracker_config_set_language (config, "en");
	g_assert (g_hash_table_lookup (stop_words, "after"));

	result = tracker_parser_text (result,
				      SAMPLE_TEXT,
				      1,
				      language,
				      100, /* max words to index */
				      100, /* max length of the word */
				      1, /* min length of the word */
				      TRUE, FALSE); /* Filter / Delimit */
}

static void
test_parser_text_fast (void)
{
	GHashTable  *result = NULL;
	const gchar *contents = "one two three four five six seven eight";

	result = tracker_parser_text_fast (result, NULL, 1);

	g_assert (result);
	g_assert_cmpint (g_hash_table_size (result), ==, 0);

	result = tracker_parser_text_fast (result, contents, 1);
	g_assert_cmpint (g_hash_table_size (result), ==, 8);

	result = tracker_parser_text_fast (result, contents, 1);
	g_assert_cmpint (g_hash_table_size (result), ==, 8);

}

int
main (int argc, char **argv) {

	int result;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	/* Init */
	config = tracker_config_new ();
	language = tracker_language_new (config);

	g_test_add_func ("/libtracker-common/tracker-parser/parser_text/max_words_to_index",
			 test_parser_text_max_words_to_index);

	g_test_add_func ("/libtracker-common/tracker-parser/parser_text/max_length",
			 test_parser_text_max_length);

	g_test_add_func ("/libtracker-common/tracker-parser/parser_text/filter_numbers",
			 test_parser_text_filter_numbers_stop_words);

	g_test_add_func ("/libtracker-common/tracker-parser/stop_words",
			 test_parser_stop_words);

	g_test_add_func ("/libtracker-common/tracker-parser/parser_text_fast",
			 test_parser_text_fast);

	result = g_test_run ();

	/* End */
	g_object_unref (config);
	g_object_unref (language);

	return result;
}
