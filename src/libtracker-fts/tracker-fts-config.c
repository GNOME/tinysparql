/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2014, Lanedo <martyn@lanedo.com>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include <libtracker-common/tracker-common.h>

#include "tracker-fts-config.h"

#define CONFIG_SCHEMA "org.freedesktop.Tracker.FTS"
#define CONFIG_PATH   "/org/freedesktop/tracker/fts/"

/* Default values */
#define DEFAULT_MAX_WORD_LENGTH      30     /* 0->200 */
#define DEFAULT_MAX_WORDS_TO_INDEX   10000
#define DEFAULT_IGNORE_NUMBERS       TRUE
#define DEFAULT_IGNORE_STOP_WORDS    TRUE
#define DEFAULT_ENABLE_STEMMER       FALSE  /* As per GB#526346, disabled */
#define DEFAULT_ENABLE_UNACCENT      TRUE

static void config_set_property         (GObject       *object,
                                         guint          param_id,
                                         const GValue  *value,
                                         GParamSpec    *pspec);
static void config_get_property         (GObject       *object,
                                         guint          param_id,
                                         GValue        *value,
                                         GParamSpec    *pspec);
static void config_finalize             (GObject       *object);
static void config_constructed          (GObject       *object);

enum {
	PROP_0,

	/* Indexing */
	PROP_MAX_WORD_LENGTH,
	PROP_ENABLE_STEMMER,
	PROP_ENABLE_UNACCENT,
	PROP_IGNORE_NUMBERS,
	PROP_IGNORE_STOP_WORDS,

	/* Performance */
	PROP_MAX_WORDS_TO_INDEX,
};

G_DEFINE_TYPE (TrackerFTSConfig, tracker_fts_config, G_TYPE_SETTINGS);

static void
tracker_fts_config_class_init (TrackerFTSConfigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = config_set_property;
	object_class->get_property = config_get_property;
	object_class->finalize     = config_finalize;
	object_class->constructed  = config_constructed;

	/* Indexing */
	g_object_class_install_property (object_class,
	                                 PROP_MAX_WORD_LENGTH,
	                                 g_param_spec_int ("max-word-length",
	                                                   "Maximum word length",
	                                                   " Set the maximum length of words to index (0->200, default=30)",
	                                                   0,
	                                                   200, /* Is this a reasonable limit? */
	                                                   DEFAULT_MAX_WORD_LENGTH,
	                                                   G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_ENABLE_STEMMER,
	                                 g_param_spec_boolean ("enable-stemmer",
	                                                       "Enable Stemmer",
	                                                       " Flag to enable word stemming utility (default=FALSE)",
	                                                       DEFAULT_ENABLE_STEMMER,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_ENABLE_UNACCENT,
	                                 g_param_spec_boolean ("enable-unaccent",
	                                                       "Enable Unaccent",
	                                                       " Flag to enable word unaccenting (default=TRUE)",
	                                                       DEFAULT_ENABLE_UNACCENT,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_IGNORE_NUMBERS,
	                                 g_param_spec_boolean ("ignore-numbers",
	                                                       "Ignore numbers",
	                                                       " Flag to ignore numbers in FTS (default=TRUE)",
	                                                       DEFAULT_IGNORE_NUMBERS,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_IGNORE_STOP_WORDS,
	                                 g_param_spec_boolean ("ignore-stop-words",
	                                                       "Ignore stop words",
	                                                       " Flag to ignore stop words in FTS (default=TRUE)",
	                                                       DEFAULT_IGNORE_STOP_WORDS,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_MAX_WORDS_TO_INDEX,
	                                 g_param_spec_int ("max-words-to-index",
	                                                   "Maximum words to index",
	                                                   " Maximum unique words to index from a file's content (default=10000)",
	                                                   0,
	                                                   G_MAXINT,
	                                                   DEFAULT_MAX_WORDS_TO_INDEX,
	                                                   G_PARAM_READWRITE));

}

static void
tracker_fts_config_init (TrackerFTSConfig *object)
{
}

static void
config_set_property (GObject      *object,
                     guint         param_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
	switch (param_id) {
		/* Indexing */
	case PROP_MAX_WORD_LENGTH:
		tracker_fts_config_set_max_word_length (TRACKER_FTS_CONFIG (object),
		                                        g_value_get_int (value));
		break;
	case PROP_ENABLE_STEMMER:
		tracker_fts_config_set_enable_stemmer (TRACKER_FTS_CONFIG (object),
		                                       g_value_get_boolean (value));
		break;
	case PROP_ENABLE_UNACCENT:
		tracker_fts_config_set_enable_unaccent (TRACKER_FTS_CONFIG (object),
		                                        g_value_get_boolean (value));
		break;
	case PROP_IGNORE_NUMBERS:
		tracker_fts_config_set_ignore_numbers (TRACKER_FTS_CONFIG (object),
		                                       g_value_get_boolean (value));
		break;
	case PROP_IGNORE_STOP_WORDS:
		tracker_fts_config_set_ignore_stop_words (TRACKER_FTS_CONFIG (object),
		                                          g_value_get_boolean (value));
		break;
	case PROP_MAX_WORDS_TO_INDEX:
		tracker_fts_config_set_max_words_to_index (TRACKER_FTS_CONFIG (object),
		                                           g_value_get_int (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
config_get_property (GObject    *object,
                     guint       param_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
        TrackerFTSConfig *config = TRACKER_FTS_CONFIG (object);

	switch (param_id) {
		/* Indexing */
	case PROP_MAX_WORD_LENGTH:
		g_value_set_int (value, tracker_fts_config_get_max_word_length (config));
		break;
	case PROP_ENABLE_STEMMER:
		g_value_set_boolean (value, tracker_fts_config_get_enable_stemmer (config));
		break;
	case PROP_ENABLE_UNACCENT:
		g_value_set_boolean (value, tracker_fts_config_get_enable_unaccent (config));
		break;
	case PROP_IGNORE_NUMBERS:
		g_value_set_boolean (value, tracker_fts_config_get_ignore_numbers (config));
		break;
	case PROP_IGNORE_STOP_WORDS:
		g_value_set_boolean (value, tracker_fts_config_get_ignore_stop_words (config));
		break;
	case PROP_MAX_WORDS_TO_INDEX:
		g_value_set_int (value, tracker_fts_config_get_max_words_to_index (config));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
config_finalize (GObject *object)
{
	/* For now we do nothing here, we left this override in for
	 * future expansion.
	 */

	(G_OBJECT_CLASS (tracker_fts_config_parent_class)->finalize) (object);
}

static void
config_constructed (GObject *object)
{
	GSettings *settings;

	(G_OBJECT_CLASS (tracker_fts_config_parent_class)->constructed) (object);

	settings = G_SETTINGS (object);

	if (G_LIKELY (!g_getenv ("TRACKER_USE_CONFIG_FILES"))) {
		g_settings_delay (settings);
	}

	g_settings_bind (settings, "max-word-length", object, "max-word-length", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_GET_NO_CHANGES);
	g_settings_bind (settings, "enable-stemmer", object, "enable-stemmer", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_GET_NO_CHANGES);
	g_settings_bind (settings, "enable-unaccent", object, "enable-unaccent", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_GET_NO_CHANGES);
	g_settings_bind (settings, "ignore-numbers", object, "ignore-numbers", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_GET_NO_CHANGES);
	g_settings_bind (settings, "ignore-stop-words", object, "ignore-stop-words", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_GET_NO_CHANGES);
	g_settings_bind (settings, "max-words-to-index", object, "max-words-to-index", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_GET_NO_CHANGES);
}

TrackerFTSConfig *
tracker_fts_config_new (void)
{
	TrackerFTSConfig *config = NULL;

	/* FIXME: should we unset GSETTINGS_BACKEND env var? */

	if (G_UNLIKELY (g_getenv ("TRACKER_USE_CONFIG_FILES"))) {
		GSettingsBackend *backend;
		gchar *filename;
		gboolean need_to_save;

		filename = g_build_filename (g_get_user_config_dir (), "tracker", "tracker-fts.cfg", NULL);

		need_to_save = g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE;

		backend = g_keyfile_settings_backend_new (filename, CONFIG_PATH, "General");
		g_info ("Using config file '%s'", filename);
		g_free (filename);

		config = g_object_new (TRACKER_TYPE_FTS_CONFIG,
		                       "backend", backend,
		                       "schema-id", CONFIG_SCHEMA,
		                       "path", CONFIG_PATH,
		                       NULL);
		g_object_unref (backend);

		if (need_to_save) {
			g_info ("  Config file does not exist, using default values...");
		}
	} else {
		config = g_object_new (TRACKER_TYPE_FTS_CONFIG,
		                       "schema-id", CONFIG_SCHEMA,
		                       "path", CONFIG_PATH,
		                       NULL);
	}

	return config;
}

gboolean
tracker_fts_config_save (TrackerFTSConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), FALSE);

        g_settings_apply (G_SETTINGS (config));

	return TRUE;
}

gint
tracker_fts_config_get_max_word_length (TrackerFTSConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_MAX_WORD_LENGTH);

	return g_settings_get_int (G_SETTINGS (config), "max-word-length");
}

gboolean
tracker_fts_config_get_enable_stemmer (TrackerFTSConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_ENABLE_STEMMER);

	return g_settings_get_boolean (G_SETTINGS (config), "enable-stemmer");
}

gboolean
tracker_fts_config_get_enable_unaccent (TrackerFTSConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_ENABLE_UNACCENT);

	return g_settings_get_boolean (G_SETTINGS (config), "enable-unaccent");
}

gboolean
tracker_fts_config_get_ignore_numbers (TrackerFTSConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_IGNORE_NUMBERS);

	return g_settings_get_boolean (G_SETTINGS (config), "ignore-numbers");
}

gboolean
tracker_fts_config_get_ignore_stop_words (TrackerFTSConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_IGNORE_STOP_WORDS);

	return g_settings_get_boolean (G_SETTINGS (config),  "ignore-stop-words");
}

gint
tracker_fts_config_get_max_words_to_index (TrackerFTSConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_MAX_WORDS_TO_INDEX);

	return g_settings_get_int (G_SETTINGS (config), "max-words-to-index");
}

void
tracker_fts_config_set_max_word_length (TrackerFTSConfig *config,
                                        gint              value)
{
	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

        g_settings_set_int (G_SETTINGS (config), "max-word-length", value);
	g_object_notify (G_OBJECT (config), "max-word-length");
}

void
tracker_fts_config_set_enable_stemmer (TrackerFTSConfig *config,
                                       gboolean          value)
{
	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

        g_settings_set_boolean (G_SETTINGS (config), "enable-stemmer", value);
	g_object_notify (G_OBJECT (config), "enable-stemmer");
}

void
tracker_fts_config_set_enable_unaccent (TrackerFTSConfig *config,
					gboolean          value)
{
	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

        g_settings_set_boolean (G_SETTINGS (config), "enable-unaccent", value);
	g_object_notify (G_OBJECT (config), "enable-unaccent");
}

void
tracker_fts_config_set_ignore_numbers (TrackerFTSConfig *config,
                                       gboolean          value)
{
	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

        g_settings_set_boolean (G_SETTINGS (config), "ignore-numbers", value);
	g_object_notify (G_OBJECT (config), "ignore-numbers");
}

void
tracker_fts_config_set_ignore_stop_words (TrackerFTSConfig *config,
                                          gboolean          value)
{
	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

        g_settings_set_boolean (G_SETTINGS (config), "ignore-stop-words", value);
	g_object_notify (G_OBJECT (config), "ignore-stop-words");
}

void
tracker_fts_config_set_max_words_to_index (TrackerFTSConfig *config,
                                           gint              value)
{
	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

        g_settings_set_int (G_SETTINGS (config), "max-words-to-index", value);
	g_object_notify (G_OBJECT (config), "max-words-to-index");
}
