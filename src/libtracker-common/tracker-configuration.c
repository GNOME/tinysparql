/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Saleem Abdulrasool (compnerd@compnerd.org)
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

#include "tracker-configuration.h"

#include <string.h>

static gboolean dirty = FALSE;
static gchar *filename = NULL;
static GKeyFile *configuration = NULL;

const LanguageMapEntry LanguageMap[] = {
	{ "da", "Danish" },
	{ "nl", "Duch" },
	{ "en", "English" },
	{ "fi", "Finnish" },
	{ "fr", "French" },
	{ "de", "German" },
	{ "it", "Italian" },
	{ "nb", "Norwegian" },
	{ "pt", "Portugese" },
	{ "ru", "Russian" },
	{ "es", "Spanish" },
	{ "sv", "Swedish" },
	{ NULL, NULL },
};


static gchar *
get_default_language_code (void)
{
	gchar **langs, **lang;

	/* get languages for user's locale */
	langs = (char **)g_get_language_names ();

	for (lang = langs; *lang; lang++) {
		if (strlen (*lang) > 1) {
			gint i;
			for (i = 0; LanguageMap[i].language; i++) {
				if (g_str_has_prefix (*lang, LanguageMap[i].language)) {
					return g_strndup (*lang, 2);
				}
			}
		}
	}

	return g_strdup ("en");
}

static void
write_default_config (void)
{
	gchar * contents = NULL, * language = NULL;

	language = get_default_language_code ();
	contents = g_strconcat ("[General]\n",
				"# Log verbosity"
				"# Valid values are:\n"
				"#    0 (displays/logs only errors)\n"
				"#    1 (minimal)\n"
				"#    2 (detailed)\n"
				"#    3 (debug)\n",
				"Verbosity = 0\n",
				"# Set the initial sleeping time, in seconds\n",
				"InitialSleep = 45\n\n",
				"# Minimizes the use of memory but may slow indexing down\n",
				"LowMemoryMode = false\n",
				"[Watches]\n",
				"# List of directory roots to index and watch separated by semicolons\n",
				"WatchDirectoryRoots =", g_get_home_dir (), ";\n",
				"# List of directory roots to index but not watch (no live updates but are\n"
				"# refreshed when trackerd is next restarted) separated by semicolons\n",
				"CrawlDirectory =\n",
				"# List of directory roots to not index and not watch separated by semicolons\n",
				"NoWatchDirectory =\n",
				"# Set to false to prevent watching of any kind\n",
				"EnableWatching = true\n\n",
				"[Indexing]\n",
				"# Throttles the indexing process.  Allowable values are 0 - 20.  Higher values\n"
				"# decrease indexing speed\n",
				"Throttle = 0\n",
				"# Disables the indexing process\n",
				"EnableIndexing = true\n",
				"# Enables indexing of a file's text contents\n",
				"EnableFileContentIndexing = true\n",
				"# Enables generation of thumbnails\n",
				"EnableThumbnails = false\n",
				"# Enables fast index merges but may hog the disk for extended periods.\n",
				"EnableFastMerges = false\n",
				"# List of partial file patterns (glob) separated by semicolons that specify files\n"
				"# to not index (basic stat info is only indexed for files that match these patterns)\n",
				"NoIndexFileTypes =;\n",
				"# Sets minimum length of words to index\n",
				"MinWordLength = 3\n",
				"# Sets the maximum length of words to index (words are cropped if bigger than this)\n",
				"MaxWordLength = 30\n",
				"# Sets the language specific stemmer and stopword list to use\n"
				"# Valid values are:\n"
				"#   'en'  (english)\n"
				"#   'da'  (danish)\n"
				"#   'nl'  (dutch)\n"
				"#   'fi'  (finnish)\n"
				"#   'fr'  (french)\n"
				"#   'de'  (german)\n"
				"#   'it'  (italian)\n"
				"#   'nb'  (norwegian)\n"
				"#   'pt'  (portugese)\n"
				"#   'ru'  (russian)\n"
				"#   'es'  (spanish)\n"
				"#   'sv'  (swedish)\n",
				"Language = ", language, "\n",
				"# Enables use of language-specific stemmer\n",
				"EnableStemmer = true\n",
				"# Set to true to prevent tracker from descending into mounted directory trees\n",
				"SkipMountPoints = false\n",
				"# Disable all indexing when on battery\n",
				"BatteryIndex = true\n",
				"# Disable initial index sweep when on battery\n",
				"BatteryIndexInitial = false\n",
				"# Pause indexer when disk space equals or goes below this value in %% of the $HOME filesystem\n"
				"# Set it to a value smaller than zero to disable pausing at all.\n",
				"LowDiskSpaceLimit = 1\n\n",
				"[Emails]\n",
				"# Index email messages from Evolution\n",
				"IndexEvolutionEmails = true\n",
				"# Index email messages from Modest\n",
				"IndexModestEmails = false\n",
				"# Index email messages from Thunderbird\n",
				"IndexThunderbirdEmails = true\n\n",
				"[Performance]\n",
				"# Maximum size of text in bytes to index from a file's text contents\n",
				"MaxTextToIndex = 1048576\n",
				"# Maximum number of unique words to index from a file's text contents\n",
				"MaxWordsToIndex = 10000\n",
				"# Specifies the number of entities to index before determining whether to perform\n"
				"# index optimization\n",
				"OptimizationSweepCount = 10000\n",
				"# Sets the maximum bucket count for the indexer\n",
				"MaxBucketCount = 524288\n",
				"# Sets the minimum bucket count for the indexer\n",
				"MinBucketCount = 65536\n",
				"# Sets number of divisions of the index file\n",
				"Divisions = 4\n",
				"# Selects the desired ratio of used records to buckets to be used when optimizing\n"
				"# the index (should be a value between 0 and 4)\n",
				"BucketRatio = 1\n",
				"# Alters how much padding is used to prevent index relocations.  Higher values improve\n"
				"# indexing speed but waste more disk space.  Values should be in the range 1-8.\n",
				"Padding = 2\n",
				"# sets stack size of trackerd threads in bytes.  The default on Linux is 8Mb (0 will use\n"
				"# the system default).\n",
				"ThreadStackSize = 0\n",
				NULL);

	g_file_set_contents (filename, contents, strlen (contents), NULL);
	g_free (contents);
}

static gchar *
string_replace (const gchar * haystack, gchar * needle, gchar * replacement)
{
	GString *str;
	char *start_pos = NULL, *end_pos = NULL;
	gint needle_len = 0;

	g_return_val_if_fail (haystack, NULL);
	g_return_val_if_fail (needle, NULL);
	g_return_val_if_fail (g_utf8_validate (haystack, -1, NULL), NULL);

	str = g_string_new ("");
	needle_len = g_utf8_strlen(needle, -1);

	while ((end_pos = strstr (start_pos, needle)) != NULL) {
		str = g_string_append_len (str, start_pos, start_pos - end_pos);
		if (replacement) {
			str = g_string_append (str, replacement);
		}

		start_pos = end_pos + needle_len;
	}

	return g_string_free (str, FALSE);
}


void
tracker_configuration_load (void)
{
	GError *error = NULL;

	filename = g_build_filename (g_get_user_config_dir (), "tracker", "tracker.cfg", NULL);

	if (! g_file_test (filename, G_FILE_TEST_EXISTS)) {
		gchar *tracker_dir = g_build_filename (g_get_user_config_dir (), "tracker", NULL);

		if (! g_file_test (tracker_dir, G_FILE_TEST_EXISTS)) {
			g_mkdir_with_parents (tracker_dir, 0700);
		}

		g_free (tracker_dir);

		write_default_config();
	}

	g_key_file_load_from_file (configuration, filename, G_KEY_FILE_KEEP_COMMENTS, &error);
	if (error)
		g_error ("failed: g_key_file_load_from_file(): %s\n", error->message);
}

void
tracker_configuration_save (void)
{
	gsize length = 0;
	GError *error = NULL;
	gchar *data = NULL, *contents = NULL;

	if (! dirty)
		return;

	data = g_key_file_to_data (configuration, &length, &error);
	if (error)
		g_error ("failed: g_key_file_to_data(): %s\n", error->message);

	contents = string_replace (data, "\n\n\n", "\n\n");
	g_free (data);

	g_file_set_contents (filename, contents, -1, NULL);
	g_free (contents);

	dirty = FALSE;
}

void
tracker_configuration_free (void)
{
	if (dirty)
		tracker_configuration_save ();

	g_free (filename);
	g_key_file_free (configuration);
}

#define TRACKER_CONFIGURATION_GET_(TypeName, GTypeName, Null)					\
GTypeName											\
tracker_configuration_get_##TypeName (const gchar * const key, GError ** error)			\
{												\
	gchar **data = g_strsplit (key, "/", 3);						\
	GTypeName value = Null;									\
												\
	if (g_key_file_has_key (configuration, data[1], data[2], error)) {			\
		value = g_key_file_get_##TypeName (configuration, data[1], data[2], error);	\
	}											\
												\
	g_strfreev (data);									\
	return value;										\
}

#define TRACKER_CONFIGURATION_SET_(TypeName, GTypeName)						\
void												\
tracker_configuration_set_##TypeName (const gchar * const key, const GTypeName value)		\
{												\
	gchar **data = g_strsplit (key, "/", 3);						\
												\
	g_key_file_set_##TypeName (configuration, data[1], data[2], value);			\
												\
	g_strfreev (data);									\
	dirty = TRUE;										\
}

TRACKER_CONFIGURATION_GET_(boolean, gboolean, FALSE)
TRACKER_CONFIGURATION_SET_(boolean, gboolean)

TRACKER_CONFIGURATION_GET_(integer, gint, 0)
TRACKER_CONFIGURATION_SET_(integer, gint)

TRACKER_CONFIGURATION_GET_(string, gchar *, NULL)
TRACKER_CONFIGURATION_SET_(string, gchar *)

#undef TRACKER_CONFIGURATION_GET_
#undef TRACKER_CONFIGURATION_SET_

#define TRACKER_CONFIGURATION_LIST_GET_(TypeName, GTypeName)							\
static GSList *													\
tracker_configuration_get_##TypeName##_list (const gchar * const key, GError ** error)				\
{														\
	gchar **data = NULL;											\
	GSList *retval = NULL;											\
														\
	g_return_val_if_fail (key, NULL);									\
														\
	data = g_strsplit (key, "/", 3);									\
														\
	if (g_key_file_has_key (configuration, data[1], data[2], error)) {					\
		gsize length = 0;										\
		GTypeName *values = NULL;									\
														\
		values = g_key_file_get_##TypeName##_list (configuration, data[1], data[2], &length, error);	\
														\
		if (values) {											\
			gsize i = 0;										\
														\
			for (i = 0; i < length; i++) {								\
				GTypeName *value = g_new0 (GTypeName, 1);					\
				*value = values[i];								\
														\
				retval = g_slist_prepend (retval, value);					\
			}											\
														\
			g_free (values);									\
		}												\
	}													\
														\
	g_strfreev (data);											\
	return g_slist_reverse (retval);									\
}

#define TRACKER_CONFIGURATION_LIST_SET_(TypeName, GTypeName)					\
static void											\
tracker_configuration_set_##TypeName##_list (const gchar * const key, GSList * value)		\
{												\
	gchar **data = NULL;									\
	GTypeName *list = NULL;									\
	guint length = 0;									\
												\
	g_return_if_fail (key);									\
	g_return_if_fail (value);								\
												\
	data = g_strsplit (key, "/", 3);							\
	length = g_slist_length (value);							\
	list = g_new0 (GTypeName, length);							\
												\
	guint i;										\
	const GSList *tmp;									\
	for (i = 0, tmp = value; tmp; tmp = tmp->next, i++) {					\
		if (tmp->data) {								\
			GTypeName *n = tmp->data;						\
			list[i] = *n;								\
		}										\
	}											\
												\
	g_key_file_set_##TypeName##_list (configuration, data[1], data[2], list, length);	\
												\
	g_free (list);										\
	g_strfreev (data);									\
}

TRACKER_CONFIGURATION_LIST_GET_(boolean, gboolean)
TRACKER_CONFIGURATION_LIST_SET_(boolean, gboolean)

TRACKER_CONFIGURATION_LIST_GET_(double, gdouble)
TRACKER_CONFIGURATION_LIST_SET_(double, gdouble)

TRACKER_CONFIGURATION_LIST_GET_(integer, gint)
TRACKER_CONFIGURATION_LIST_SET_(integer, gint)

#undef TRACKER_CONFIGURATION_LIST_GET_
#undef TRACKER_CONFIGURATION_LIST_SET_

/* Implement string lists manually for strings are special you see */

static GSList *
tracker_configuration_get_string_list (const gchar * const key, GError ** error)
{
	gchar **data = NULL;
	GSList *retval = NULL;

	g_return_val_if_fail (key, NULL);

	data = g_strsplit (key, "/", 3);

	if (g_key_file_has_key (configuration, data[1], data[2], error)) {
		gsize length = 0;
		gchar **values = NULL;

		values = g_key_file_get_string_list (configuration, data[1], data[2], &length, error);

		if (values) {
			gsize i = 0;

			for (i = 0; i < length; i++) {
				gchar *value = g_strdup (values[i]);
				retval = g_slist_prepend (retval, value);
			}

			g_strfreev (values);
		}
	}

	g_strfreev (data);
	return g_slist_reverse (retval);
}

static void
tracker_configuration_set_string_list (const gchar * const key, GSList * value)
{
	gchar **data = NULL;
	gchar **list = NULL;
	guint length = 0;

	g_return_if_fail (key);
	g_return_if_fail (value);

	data = g_strsplit (key, "/", 3);
	length = g_slist_length (value);
	list = g_new0 (gchar *, length + 1);

	guint i;
	const GSList *tmp;
	for (i = 0, tmp = value; tmp; tmp = tmp->next, i++) {
		if (tmp->data) {
			gchar *value = g_strdup (tmp->data);
			list[i] = value;
		}
	}

	g_key_file_set_string_list (configuration, data[1], data[2], (const gchar * const *)list, length);

	g_strfreev (list);
	g_strfreev (data);
}

GSList *
tracker_configuration_get_list (const gchar * const key, GType g_type, GError ** error)
{
	switch (g_type) {
		case G_TYPE_BOOLEAN:
			return tracker_configuration_get_boolean_list (key, error);
			break;
		case G_TYPE_DOUBLE:
			return tracker_configuration_get_double_list (key, error);
			break;
		case G_TYPE_INT:
			return tracker_configuration_get_integer_list (key, error);
			break;
		case G_TYPE_STRING:
			return tracker_configuration_get_string_list (key, error);
			break;
		default:
			g_assert_not_reached ();
	}
}

void
tracker_configuration_set_list (const gchar * const key, GType g_type, GSList * value)
{
	switch (g_type) {
		case G_TYPE_BOOLEAN:
			tracker_configuration_set_boolean_list (key, value);
			break;
		case G_TYPE_DOUBLE:
			tracker_configuration_set_double_list (key, value);
			break;
		case G_TYPE_INT:
			tracker_configuration_set_integer_list (key, value);
			break;
		case G_TYPE_STRING:
			tracker_configuration_set_string_list (key, value);
			break;
		default:
			g_assert_not_reached ();
	}
}
