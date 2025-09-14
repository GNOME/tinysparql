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

#include <libstemmer.h>

#include "tracker-language.h"

typedef struct _TrackerLanguagePrivate TrackerLanguagePrivate;

struct _TrackerLanguagePrivate {
	gchar         *language_code;
	gboolean       lang_has_english;

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
	object_class->set_property = language_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_LANGUAGE_CODE,
	                                 g_param_spec_string ("language-code",
	                                                      "Language code",
	                                                      "Language code",
	                                                      NULL,
	                                                      G_PARAM_WRITABLE |
	                                                      G_PARAM_CONSTRUCT_ONLY));
}

static void
tracker_language_init (TrackerLanguage *language)
{
}

static void
language_finalize (GObject *object)
{
	TrackerLanguagePrivate *priv;

	priv = tracker_language_get_instance_private (TRACKER_LANGUAGE (object));

	if (priv->stemmer) {
		g_mutex_lock (&priv->stemmer_mutex);
		sb_stemmer_delete (priv->stemmer);
		g_mutex_unlock (&priv->stemmer_mutex);
	}
	g_mutex_clear (&priv->stemmer_mutex);

	g_free (priv->language_code);

	(G_OBJECT_CLASS (tracker_language_parent_class)->finalize) (object);
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

static void
ensure_language (TrackerLanguage *language)
{
	TrackerLanguagePrivate *priv =
		tracker_language_get_instance_private (language);
	const gchar * const *langs;
	gint i;

	if (priv->language_code)
		return;

	langs = g_get_language_names ();

	for (i = 0; langs[i]; i++) {
		const gchar *sep;
		gchar *code;
		int len;

		if (strcmp (langs[i], "C") == 0 ||
		    strncmp (langs[i], "C.", 2) == 0 ||
		    strcmp (langs[i], "POSIX") == 0)
			continue;

		sep = strchr (langs[i], '_');
		len = sep ? (int) (sep - langs[i]) : (int) strlen(langs[i]);
		code = g_strndup (langs[i], len);

		if (!priv->language_code)
			priv->language_code = g_strdup (code);

		if (strcmp (code, "en") == 0)
			priv->lang_has_english = TRUE;

		g_free (code);
	}

	if (!priv->language_code)
		priv->language_code = g_strdup ("en");
}

static void
language_constructed (GObject *object)
{
	TrackerLanguage *language = TRACKER_LANGUAGE (object);
	TrackerLanguagePrivate *priv =
		tracker_language_get_instance_private (language);

	G_OBJECT_CLASS (tracker_language_parent_class)->constructed (object);

	ensure_language (language);

	priv->stemmer = sb_stemmer_new (priv->language_code, NULL);
	if (!priv->stemmer) {
		g_debug ("No stemmer could be found for language:'%s'",
		         priv->language_code);
	}
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

void
tracker_language_stem_word (TrackerLanguage *language,
                            gchar           *buffer,
                            gint            *buffer_len,
                            gint             buffer_size)
{
	TrackerLanguagePrivate *priv;

	g_return_if_fail (TRACKER_IS_LANGUAGE (language));
	g_return_if_fail (buffer != NULL);
	g_return_if_fail (buffer_len != NULL);
	g_return_if_fail (*buffer_len >= 0);

	priv = tracker_language_get_instance_private (language);

	g_mutex_lock (&priv->stemmer_mutex);

	if (priv->stemmer) {
		const sb_symbol *symbol;
		int len;

		symbol = sb_stemmer_stem (priv->stemmer,
		                          (const sb_symbol *) buffer,
		                          *buffer_len);
		len = sb_stemmer_length (priv->stemmer);

		if (len < buffer_size) {
			memcpy (buffer, symbol, len + 1);
			*buffer_len = len;
		}
	}

	g_mutex_unlock (&priv->stemmer_mutex);
}
