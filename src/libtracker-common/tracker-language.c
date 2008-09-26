/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include <libstemmer/libstemmer.h>

#include "tracker-log.h"
#include "tracker-language.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_LANGUAGE, TrackerLanguagePriv))

typedef struct _TrackerLanguagePriv TrackerLanguagePriv;
typedef struct _Languages	    Languages;

struct _TrackerLanguagePriv {
	TrackerConfig *config;

	GHashTable    *stop_words;

	GMutex	      *stemmer_mutex;
	gpointer       stemmer;
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

/* GObject properties */
enum {
	PROP_0,

	PROP_CONFIG,
	PROP_STOP_WORDS
};

static void	    language_finalize	       (GObject       *object);
static void	    language_get_property      (GObject       *object,
						guint	       param_id,
						GValue	      *value,
						GParamSpec    *pspec);
static void	    language_set_property      (GObject       *object,
						guint	       param_id,
						const GValue  *value,
						GParamSpec    *pspec);
static const gchar *language_get_name_for_code (const gchar   *language_code);
static void	    language_notify_cb	       (TrackerConfig *config,
						GParamSpec    *param,
						gpointer       user_data);

G_DEFINE_TYPE (TrackerLanguage, tracker_language, G_TYPE_OBJECT);

static void
tracker_language_class_init (TrackerLanguageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = language_finalize;
	object_class->get_property = language_get_property;
	object_class->set_property = language_set_property;

	g_object_class_install_property (object_class,
					 PROP_CONFIG,
					 g_param_spec_object ("config",
							      "Config",
							      "Config",
							      tracker_config_get_type (),
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_STOP_WORDS,
					 g_param_spec_boxed ("stop-words",
							     "Stop words",
							     "Stop words",
							     g_hash_table_get_type (),
							     G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof (TrackerLanguagePriv));
}

static void
tracker_language_init (TrackerLanguage *language)
{
	TrackerLanguagePriv *priv;
	const gchar	    *stem_language;

	priv = GET_PRIV (language);

	priv->stop_words = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  NULL);

	priv->stemmer_mutex = g_mutex_new ();

	stem_language = language_get_name_for_code (NULL);
	priv->stemmer = sb_stemmer_new (stem_language, NULL);
}

static void
language_finalize (GObject *object)
{
	TrackerLanguagePriv *priv;

	priv = GET_PRIV (object);

	if (priv->config) {
		g_signal_handlers_disconnect_by_func (priv->config,
						      language_notify_cb,
						      TRACKER_LANGUAGE (object));
		g_object_unref (priv->config);
	}

	if (priv->stemmer) {
		g_mutex_lock (priv->stemmer_mutex);
		sb_stemmer_delete (priv->stemmer);
		g_mutex_unlock (priv->stemmer_mutex);
	}

	g_mutex_free (priv->stemmer_mutex);

	if (priv->stop_words) {
		g_hash_table_unref (priv->stop_words);
	}

	(G_OBJECT_CLASS (tracker_language_parent_class)->finalize) (object);
}

static void
language_get_property (GObject	  *object,
		       guint	   param_id,
		       GValue	  *value,
		       GParamSpec *pspec)
{
	TrackerLanguagePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CONFIG:
		g_value_set_object (value, priv->config);
		break;
	case PROP_STOP_WORDS:
		g_value_set_boxed (value, priv->stop_words);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
language_set_property (GObject	    *object,
		       guint	     param_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
	TrackerLanguagePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CONFIG:
		tracker_language_set_config (TRACKER_LANGUAGE (object),
					     g_value_get_object (value));
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

	str = g_strconcat ("stopwords.", language_code, NULL);
	filename = g_build_filename (SHAREDIR,
				     "tracker",
				     "languages",
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
			const gchar	*filename)
{
	TrackerLanguagePriv  *priv;
	GMappedFile	     *mapped_file;
	GError		     *error = NULL;
	gchar		     *content;
	gchar		    **words, **p;

	priv = GET_PRIV (language);

	mapped_file = g_mapped_file_new (filename, FALSE, &error);
	if (error) {
		g_message ("Tracker couldn't read stopword file:'%s', %s",
			     filename, error->message);
		g_clear_error (&error);
		return;
	}

	content = g_mapped_file_get_contents (mapped_file);
	words = g_strsplit_set (content, "\n" , -1);

	g_mapped_file_free (mapped_file);

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
	TrackerLanguagePriv *priv;
	gchar		    *stopword_filename;
	const gchar	    *stem_language;

	g_return_if_fail (TRACKER_IS_LANGUAGE (language));

	priv = GET_PRIV (language);

	/* Set up stopwords list */
	g_message ("Setting up stopword list for language code:'%s'", language_code);

	stopword_filename = language_get_stopword_filename (language_code);
	language_add_stopwords (language, stopword_filename);
	g_free (stopword_filename);

	if (!language_code || strcmp (language_code, "en") != 0) {
		stopword_filename = language_get_stopword_filename ("en");
		language_add_stopwords (language, stopword_filename);
		g_free (stopword_filename);
	}

	g_message ("Setting up stemmer for language code:'%s'", language_code);

	stem_language = language_get_name_for_code (language_code);

	g_mutex_lock (priv->stemmer_mutex);

	if (priv->stemmer) {
		sb_stemmer_delete (priv->stemmer);
	}

	priv->stemmer = sb_stemmer_new (stem_language, NULL);
	if (!priv->stemmer) {
		g_message ("No stemmer could be found for language:'%s'",
			   stem_language);
	}

	g_mutex_unlock (priv->stemmer_mutex);
}

static void
language_notify_cb (TrackerConfig *config,
		    GParamSpec	  *param,
		    gpointer	   user_data)
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
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	language = g_object_new (TRACKER_TYPE_LANGUAGE,
				 "config", config,
				 NULL);

	language_set_stopword_list (language,
				    tracker_config_get_language (config));

	return language;

}

TrackerConfig *
tracker_language_get_config (TrackerLanguage *language)
{
	TrackerLanguagePriv *priv;

	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), NULL);

	priv = GET_PRIV (language);

	return priv->config;
}

GHashTable *
tracker_language_get_stop_words (TrackerLanguage *language)
{
	TrackerLanguagePriv *priv;

	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), NULL);

	priv = GET_PRIV (language);

	return priv->stop_words;
}

void
tracker_language_set_config (TrackerLanguage *language,
			     TrackerConfig   *config)
{
	TrackerLanguagePriv *priv;

	g_return_if_fail (TRACKER_IS_LANGUAGE (language));
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (language);

	if (config) {
		g_object_ref (config);
	}

	if (priv->config) {
		g_signal_handlers_disconnect_by_func (priv->config,
						      G_CALLBACK (language_notify_cb),
						      language);
		g_object_unref (priv->config);
	}

	priv->config = config;

	if (priv->config) {
		g_signal_connect (priv->config, "notify::language",
				  G_CALLBACK (language_notify_cb),
				  language);
	}

	g_object_notify (G_OBJECT (language), "config");
}

const gchar *
tracker_language_stem_word (TrackerLanguage *language,
			    const gchar     *word,
			    gint	     word_length)
{
	TrackerLanguagePriv *priv;
	const gchar	    *stem_word;

	g_return_val_if_fail (TRACKER_IS_LANGUAGE (language), NULL);

	priv = GET_PRIV (language);

	if (!tracker_config_get_enable_stemmer (priv->config)) {
		return g_strdup (word);
	}

	g_mutex_lock (priv->stemmer_mutex);

	stem_word = (const gchar*) sb_stemmer_stem (priv->stemmer,
						    (guchar*) word,
						    word_length);

	g_mutex_unlock (priv->stemmer_mutex);

	return stem_word;
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
		gint	     i = 0;

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
