/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <string.h>

#include <glib.h>

#ifdef HAVE_LIBSTEMMER
#include <libstemmer.h>
#endif /* HAVE_LIBSTEMMER */

#include "tracker-language.h"

typedef struct _TrackerLanguagePrivate TrackerLanguagePrivate;

struct _TrackerLanguagePrivate {
	GHashTable    *stop_words;
	gchar         *language_code;

	GMutex         stemmer_mutex;
	gpointer       stemmer;
};

/* GObject properties */
enum {
	PROP_0,

	PROP_LANGUAGE_CODE,
};

static void         language_constructed       (GObject       *object);
static void         language_finalize          (GObject       *object);
static void         language_get_property      (GObject       *object,
                                                guint          param_id,
                                                GValue        *value,
                                                GParamSpec    *pspec);
static void         language_set_property      (GObject       *object,
                                                guint          param_id,
                                                const GValue  *value,
                                                GParamSpec    *pspec);

G_DEFINE_TYPE_WITH_PRIVATE (TrackerLanguage, tracker_language, G_TYPE_OBJECT)

static void
tracker_language_class_init (TrackerLanguageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructed = language_constructed;
	object_class->finalize = language_finalize;
	object_class->get_property = language_get_property;
	object_class->set_property = language_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_LANGUAGE_CODE,
	                                 g_param_spec_string ("language-code",
	                                                      "Language code",
	                                                      "Language code",
	                                                      "en",
	                                                      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));
}

static void
tracker_language_init (TrackerLanguage *language)
{
	TrackerLanguagePrivate *priv;

	priv = tracker_language_get_instance_private (language);

	priv->stop_words = g_hash_table_new_full (g_str_hash,
	                                          g_str_equal,
	                                          g_free,
	                                          NULL);
}

static void
language_finalize (GObject *object)
{
	TrackerLanguagePrivate *priv;

	priv = tracker_language_get_instance_private (TRACKER_LANGUAGE (object));

#ifdef HAVE_LIBSTEMMER
	if (priv->stemmer) {
		g_mutex_lock (&priv->stemmer_mutex);
		sb_stemmer_delete (priv->stemmer);
		g_mutex_unlock (&priv->stemmer_mutex);
	}
	g_mutex_clear (&priv->stemmer_mutex);
#endif /* HAVE_LIBSTEMMER */

	if (priv->stop_words) {
		g_hash_table_unref (priv->stop_words);
	}

	g_free (priv->language_code);

	(G_OBJECT_CLASS (tracker_language_parent_class)->finalize) (object);
}

static void
language_get_property (GObject    *object,
                       guint       param_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
	TrackerLanguagePrivate *priv;

	priv = tracker_language_get_instance_private (TRACKER_LANGUAGE (object));

	switch (param_id) {
	case PROP_LANGUAGE_CODE:
		g_value_set_string (value, priv->language_code);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
language_set_property (GObject      *object,
                       guint         param_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
	TrackerLanguage *language = TRACKER_LANGUAGE (object);
	TrackerLanguagePrivate *priv =
		tracker_language_get_instance_private (language);

	switch (param_id) {
	case PROP_LANGUAGE_CODE:
		priv->language_code = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static gchar *
language_get_stopword_filename (const gchar *language_code)
{
	gchar *str;
	gchar *filename;
	const gchar *testpath;

	str = g_strconcat ("stopwords.", language_code, NULL);

	/* Look if the testpath for stopwords dictionary was set
	 *  (used during unit tests) */
	testpath = g_getenv ("TRACKER_LANGUAGE_STOP_WORDS_DIR");
	if (!testpath) {
		filename = g_build_filename (SHAREDIR,
		                             "tracker3",
		                             "stop-words",
		                             str,
		                             NULL);
	} else {
		filename = g_build_filename (testpath,
		                             str,
		                             NULL);
	}

	g_free (str);
	return filename;
}

static void
language_add_stopwords (TrackerLanguage *language,
                        const gchar     *filename)
{
	TrackerLanguagePrivate *priv;
	GMappedFile          *mapped_file;
	GError               *error = NULL;
	gchar                *content;
	gchar               **words, **p;

	priv = tracker_language_get_instance_private (language);

	mapped_file = g_mapped_file_new (filename, FALSE, &error);
	if (error) {
		g_message ("Tracker couldn't read stopword file:'%s', %s",
		           filename, error->message);
		g_clear_error (&error);
		return;
	}

	content = g_mapped_file_get_contents (mapped_file);
	words = g_strsplit_set (content, "\n" , -1);

	g_mapped_file_unref (mapped_file);

	/* FIXME: Shouldn't clear the hash table first? */
	for (p = words; *p; p++) {
		g_hash_table_insert (priv->stop_words,
		                     g_strdup (g_strstrip (*p)),
		                     GINT_TO_POINTER (1));
	}

	g_strfreev (words);
}

static void
language_set_stopword_list (TrackerLanguage *language,
                            const gchar     *language_code)
{
	gchar *stopword_filename;

	g_return_if_fail (TRACKER_IS_LANGUAGE (language));

	/* Set up stopwords list */
	/* g_message ("Setting up stopword list for language code:'%s'", language_code); */

	stopword_filename = language_get_stopword_filename (language_code);
	language_add_stopwords (language, stopword_filename);
	g_free (stopword_filename);

	if (g_strcmp0 (language_code, "en") != 0) {
		stopword_filename = language_get_stopword_filename ("en");
		language_add_stopwords (language, stopword_filename);
		g_free (stopword_filename);
	}
}

static void
language_constructed (GObject *object)
{
	TrackerLanguage *language = TRACKER_LANGUAGE (object);
	TrackerLanguagePrivate *priv =
		tracker_language_get_instance_private (language);

	G_OBJECT_CLASS (tracker_language_parent_class)->constructed (object);

	if (!priv->language_code) {
		priv->language_code = g_strdup ("en");
	}

	language_set_stopword_list (language, priv->language_code);

#ifdef HAVE_LIBSTEMMER
	priv->stemmer = sb_stemmer_new (priv->language_code, NULL);
	if (!priv->stemmer) {
		g_debug ("No stemmer could be found for language:'%s'",
		           priv->language_code);
	}
#endif /* HAVE_LIBSTEMMER */
}

/**
 * tracker_language_new:
 * @language_code: language code in ISO 639-1 format
 *
 * Creates a new #TrackerLanguage instance for the passed language code.
 *
 * Returns: a newly created #TrackerLanguage
 **/
TrackerLanguage *
tracker_language_new (const gchar *language_code)
{
	TrackerLanguage *language;

	language = g_object_new (TRACKER_TYPE_LANGUAGE,
	                         "language-code", language_code,
	                         NULL);

	return language;
}

/**
 * tracker_language_is_stop_word:
 * @language: a #TrackerLanguage
 * @word: a string containing a word
 *
 * Returns %TRUE if the given @word is in the list of stop words of the
 *  given @language.
 *
 * Returns: %TRUE if @word is a stop word. %FALSE otherwise.
 */
gboolean
tracker_language_is_stop_word (TrackerLanguage *language,
                               const gchar     *word)
{
	TrackerLanguagePrivate *priv;

	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), FALSE);
	g_return_val_if_fail (word, FALSE);

	priv = tracker_language_get_instance_private (language);

	return g_hash_table_lookup (priv->stop_words, word) != NULL;
}

/**
 * tracker_language_stem_word:
 * @language: a #TrackerLanguage
 * @word: string pointing to a word
 * @word_length: word ascii length
 *
 * If the stemmer is enabled, it will return the stem word for @word.
 * If it's disabled, it will return the passed word.
 *
 * Returns: a string with the processed word. This string must be
 *          freed with g_free()
 **/
gchar *
tracker_language_stem_word (TrackerLanguage *language,
                            const gchar     *word,
                            gint             word_length)
{
#ifdef HAVE_LIBSTEMMER
	TrackerLanguagePrivate *priv;
	const gchar *stem_word = NULL;
#endif /* HAVE_LIBSTEMMER */

	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), NULL);

	if (word_length < 0) {
		word_length = strlen (word);
	}

#ifdef HAVE_LIBSTEMMER
	priv = tracker_language_get_instance_private (language);

	g_mutex_lock (&priv->stemmer_mutex);

	if (priv->stemmer) {
		stem_word = (const gchar*) sb_stemmer_stem (priv->stemmer,
							    (guchar*) word,
							    word_length);
	}

	g_mutex_unlock (&priv->stemmer_mutex);

	if (stem_word)
		return g_strdup (stem_word);
#endif /* HAVE_LIBSTEMMER */

	return g_strndup (word, word_length);
}
