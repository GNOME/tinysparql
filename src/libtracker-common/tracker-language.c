/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#include "../libstemmer/include/libstemmer.h"

#include "tracker-log.h" 
#include "tracker-language.h"

typedef struct _Languages Languages;

struct _TrackerLanguage {
	GHashTable    *stop_words;

	GMutex        *stemmer_mutex;
	gpointer       stemmer;

	TrackerConfig *config;
};

struct _Languages {
        gchar *code;
        gchar *name;
};

static Languages all_langs[] = {
        { "da", "danish" },
        { "nl", "dutch" },
        { "en", "english" },
        { "fi", "finnish" },
        { "fr", "french" },
        { "de", "german" },
	{ "hu", "hungarian" },
        { "it", "italian" },
        { "nb", "norwegian" },
        { "pt", "portuguese" },
        { "ru", "russian" },
        { "es", "spanish" },
        { "sv", "swedish" },
        { NULL, NULL },
};

static gchar *
language_get_stopword_filename (const gchar *language_code)
{
	gchar *str;
	gchar *filename;

	str = g_strconcat (".", language_code, NULL);
	filename = g_build_filename (SHAREDIR,
				     "tracker",
				     "languages",
				     "stopwords",
				     str,
				     NULL);
	g_free (str);

	return filename;
}

static const gchar *
language_get_name_for_code (const gchar *language_code)
{
	gint i;

	if (!language_code || language_code[0] == '\0') {
		return "english";
	}

	for (i = 0; all_langs[i].code; i++) {
		if (g_str_has_prefix (language_code, all_langs[i].code)) {
			return all_langs[i].name;
		}
	}

	return "";
}

static void
language_add_stopwords (TrackerLanguage *language,
			const gchar     *filename)
{
	GMappedFile  *mapped_file;
	GError       *error = NULL;
	gchar        *content;
	gchar       **words, **p;

	mapped_file = g_mapped_file_new (filename, FALSE, &error);
	if (error) {
		tracker_log ("Tracker couldn't read stopword file:'%s', %s",
			     filename, error->message);
		g_clear_error (&error);
		return;
	}

	content = g_mapped_file_get_contents (mapped_file);
	words = g_strsplit_set (content, "\n" , -1);
	g_free (content);
	g_mapped_file_free (mapped_file);

	/* FIXME: Shouldn't clear the hash table first? */
	for (p = words; *p; p++) {
		g_hash_table_insert (language->stop_words,
				     g_strdup (g_strstrip (*p)),
				     GINT_TO_POINTER (1));
	}

	g_strfreev (words);
}

static void
language_set_stopword_list (TrackerLanguage *language,
			    const gchar     *language_code)
{
	gchar       *stopword_filename;
	const gchar *stem_language;

	g_return_if_fail (language != NULL);

	/* Set up stopwords list */
	tracker_log ("Setting up stopword list for language code:'%s'", language_code);

	stopword_filename = language_get_stopword_filename (language_code);
	language_add_stopwords (language, stopword_filename);
	g_free (stopword_filename);

	if (!language_code || strcmp (language_code, "en") != 0) {
		stopword_filename = language_get_stopword_filename ("en");
		language_add_stopwords (language, stopword_filename);
		g_free (stopword_filename);
	}

	tracker_log ("Setting up stemmer for language code:'%s'", language_code);

	stem_language = language_get_name_for_code (language_code);

	g_mutex_lock (language->stemmer_mutex);

	if (language->stemmer) {
		sb_stemmer_delete (language->stemmer);
	}

	language->stemmer = sb_stemmer_new (stem_language, NULL);
	if (!language->stemmer) {
		tracker_log ("No stemmer could be found for language:'%s'",
			     stem_language);
	}

	g_mutex_unlock (language->stemmer_mutex);
}

static void
language_notify_cb (TrackerConfig *config,
		    GParamSpec    *param,
		    gpointer       user_data)
{
	TrackerLanguage *language;

	language = (TrackerLanguage*) user_data;

	language_set_stopword_list (language,
				    tracker_config_get_language (config));
}

TrackerLanguage *
tracker_language_new (TrackerConfig *config)
{
	TrackerLanguage *language;
	const gchar     *stem_language;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	language = g_new0 (TrackerLanguage, 1);

	language->stop_words = g_hash_table_new_full (g_str_hash,
						      g_str_equal,
						      g_free,
						      NULL);

	language->stemmer_mutex = g_mutex_new ();

	stem_language = language_get_name_for_code (NULL);
	language->stemmer = sb_stemmer_new (stem_language, NULL);

	language->config = g_object_ref (config);

	g_signal_connect (language->config, "notify::language",
			  G_CALLBACK (language_notify_cb),
			  language);

	return language;
}

void
tracker_language_free (TrackerLanguage *language)
{
	if (!language) {
		return;
	}

	g_signal_handlers_disconnect_by_func (language->config,
					      language_notify_cb,
					      language);
	g_object_unref (language->config);

	if (language->stemmer) {
		g_mutex_lock (language->stemmer_mutex);
		sb_stemmer_delete (language->stemmer);
		g_mutex_unlock (language->stemmer_mutex);
	}

	g_mutex_free (language->stemmer_mutex);

	g_hash_table_destroy (language->stop_words);

	g_free (language);
}

gboolean
tracker_language_check_exists (const gchar *language_code)
{
	gint i;

	if (!language_code || language_code[0] == '\0') {
		return FALSE;
	}

	for (i = 0; all_langs[i].code; i++) {
		if (g_str_has_prefix (language_code, all_langs[i].code)) {
			return TRUE;
		}
	}

	return FALSE;
}

gchar *
tracker_language_get_default_code (void)
{
	const gchar **local_languages;
        const gchar **p;

	/* Get langauges for user's locale */
	local_languages = (const gchar**) g_get_language_names ();

	for (p = local_languages; *p; p++) {
                const gchar *code;
                gint         i = 0;

                if (!*p || *p[0] == '\0') {
                        continue;
                }

                code = all_langs[i].code;

                while (code) {
                        if (g_str_has_prefix (*p, code)) {
                                return g_strndup (*p, strlen (code));
                        }

                        code = all_langs[i++].code;
                }
	}

	return g_strdup ("en");
}

gchar *
tracker_language_stem_word (TrackerLanguage *language,
			    const gchar     *word,
			    gint             word_length)
{
	const gchar *stem_word;

	g_return_val_if_fail (language != NULL, NULL);

	if (!tracker_config_get_enable_stemmer (language->config)) {
		return NULL;
	}

	g_mutex_lock (language->stemmer_mutex);
	if (!language->stemmer) {

	}

	stem_word = (const gchar *) sb_stemmer_stem (language->stemmer,
						     (guchar*) word,
						     word_length);
	g_mutex_unlock (language->stemmer_mutex);

	return g_strdup (stem_word);
}
