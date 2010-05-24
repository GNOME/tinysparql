/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <libtracker-common/tracker-keyfile-object.h>

#include "tracker-fts-config.h"

#define TRACKER_FTS_CONFIG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_FTS_CONFIG, TrackerFTSConfigPrivate))

/* GKeyFile defines */
#define GROUP_INDEXING             "Indexing"

/* Default values */
#define DEFAULT_MIN_WORD_LENGTH      3      /* 0->30 */
#define DEFAULT_MAX_WORD_LENGTH      30     /* 0->200 */
#define DEFAULT_MAX_WORDS_TO_INDEX   10000
#define DEFAULT_IGNORE_NUMBERS       TRUE
#define DEFAULT_IGNORE_STOP_WORDS    TRUE
#define DEFAULT_ENABLE_STEMMER       FALSE  /* As per GB#526346, disabled */
#define DEFAULT_ENABLE_UNACCENT      TRUE

typedef struct {
	/* Indexing */
	gint min_word_length;
	gint max_word_length;
	gboolean enable_stemmer;
	gboolean enable_unaccent;
	gboolean ignore_numbers;
	gboolean ignore_stop_words;
	gint max_words_to_index;
}  TrackerFTSConfigPrivate;

typedef struct {
	GType  type;
	const gchar *property;
	const gchar *group;
	const gchar *key;
} ObjectToKeyFile;

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
static void config_create_with_defaults (TrackerFTSConfig *config,
                                         GKeyFile      *key_file,
                                         gboolean       overwrite);
static void config_load                 (TrackerFTSConfig *config);

enum {
	PROP_0,

	/* Indexing */
	PROP_MIN_WORD_LENGTH,
	PROP_MAX_WORD_LENGTH,
	PROP_ENABLE_STEMMER,
	PROP_ENABLE_UNACCENT,
	PROP_IGNORE_NUMBERS,
	PROP_IGNORE_STOP_WORDS,

	/* Performance */
	PROP_MAX_WORDS_TO_INDEX,
};

static ObjectToKeyFile conversions[] = {
	{ G_TYPE_INT,     "min-word-length",    GROUP_INDEXING, "MinWordLength"   },
	{ G_TYPE_INT,     "max-word-length",    GROUP_INDEXING, "MaxWordLength"   },
	{ G_TYPE_BOOLEAN, "enable-stemmer",     GROUP_INDEXING, "EnableStemmer"   },
	{ G_TYPE_BOOLEAN, "enable-unaccent",    GROUP_INDEXING, "EnableUnaccent"  },
	{ G_TYPE_BOOLEAN, "ignore-numbers",     GROUP_INDEXING, "IgnoreNumbers"   },
	{ G_TYPE_BOOLEAN, "ignore-stop-words",  GROUP_INDEXING, "IgnoreStopWords" },
	{ G_TYPE_INT,     "max-words-to-index", GROUP_INDEXING, "MaxWordsToIndex" },
};

G_DEFINE_TYPE (TrackerFTSConfig, tracker_fts_config, TRACKER_TYPE_CONFIG_FILE);

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
	                                 PROP_MIN_WORD_LENGTH,
	                                 g_param_spec_int ("min-word-length",
	                                                   "Minimum word length",
	                                                   " Set the minimum length of words to index (0->30, default=3)",
	                                                   0,
	                                                   30,
	                                                   DEFAULT_MIN_WORD_LENGTH,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_MAX_WORD_LENGTH,
	                                 g_param_spec_int ("max-word-length",
	                                                   "Maximum word length",
	                                                   " Set the maximum length of words to index (0->200, default=30)",
	                                                   0,
	                                                   200, /* Is this a reasonable limit? */
	                                                   DEFAULT_MAX_WORD_LENGTH,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_ENABLE_STEMMER,
	                                 g_param_spec_boolean ("enable-stemmer",
	                                                       "Enable Stemmer",
	                                                       " Flag to enable word stemming utility (default=FALSE)",
	                                                       DEFAULT_ENABLE_STEMMER,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_ENABLE_UNACCENT,
	                                 g_param_spec_boolean ("enable-unaccent",
	                                                       "Enable Unaccent",
	                                                       " Flag to enable word unaccenting (default=TRUE)",
	                                                       DEFAULT_ENABLE_UNACCENT,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_IGNORE_NUMBERS,
	                                 g_param_spec_boolean ("ignore-numbers",
	                                                       "Ignore numbers",
	                                                       " Flag to ignore numbers in FTS (default=TRUE)",
	                                                       DEFAULT_IGNORE_NUMBERS,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_IGNORE_STOP_WORDS,
	                                 g_param_spec_boolean ("ignore-stop-words",
	                                                       "Ignore stop words",
	                                                       " Flag to ignore stop words in FTS (default=TRUE)",
	                                                       DEFAULT_IGNORE_STOP_WORDS,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_MAX_WORDS_TO_INDEX,
	                                 g_param_spec_int ("max-words-to-index",
	                                                   "Maximum words to index",
	                                                   " Maximum unique words to index from a file's content (default=10000)",
	                                                   0,
	                                                   G_MAXINT,
	                                                   DEFAULT_MAX_WORDS_TO_INDEX,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerFTSConfigPrivate));
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
	case PROP_MIN_WORD_LENGTH:
		tracker_fts_config_set_min_word_length (TRACKER_FTS_CONFIG (object),
		                                        g_value_get_int (value));
		break;
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
	TrackerFTSConfigPrivate *priv;

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (object);

	switch (param_id) {
		/* Indexing */
	case PROP_MIN_WORD_LENGTH:
		g_value_set_int (value, priv->min_word_length);
		break;
	case PROP_MAX_WORD_LENGTH:
		g_value_set_int (value, priv->max_word_length);
		break;
	case PROP_ENABLE_STEMMER:
		g_value_set_boolean (value, priv->enable_stemmer);
		break;
	case PROP_ENABLE_UNACCENT:
		g_value_set_boolean (value, priv->enable_unaccent);
		break;
	case PROP_IGNORE_NUMBERS:
		g_value_set_boolean (value, priv->ignore_numbers);
		break;
	case PROP_IGNORE_STOP_WORDS:
		g_value_set_boolean (value, priv->ignore_stop_words);
		break;
	case PROP_MAX_WORDS_TO_INDEX:
		g_value_set_int (value, priv->max_words_to_index);
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
	(G_OBJECT_CLASS (tracker_fts_config_parent_class)->constructed) (object);

	config_load (TRACKER_FTS_CONFIG (object));
}

static void
config_create_with_defaults (TrackerFTSConfig *config,
                             GKeyFile      *key_file,
                             gboolean       overwrite)
{
	gint i;

	g_message ("Loading defaults into GKeyFile...");

	for (i = 0; i < G_N_ELEMENTS (conversions); i++) {
		gboolean has_key;

		has_key = g_key_file_has_key (key_file,
		                              conversions[i].group,
		                              conversions[i].key,
		                              NULL);
		if (!overwrite && has_key) {
			continue;
		}

		switch (conversions[i].type) {
		case G_TYPE_INT:
			g_key_file_set_integer (key_file,
			                        conversions[i].group,
			                        conversions[i].key,
			                        tracker_keyfile_object_default_int (config,
			                                                            conversions[i].property));
			break;
		case G_TYPE_BOOLEAN:
			g_key_file_set_boolean (key_file,
			                        conversions[i].group,
			                        conversions[i].key,
			                        tracker_keyfile_object_default_boolean (config,
			                                                                conversions[i].property));
			break;
		default:
			g_assert_not_reached ();
			break;
		}

		g_key_file_set_comment (key_file,
		                        conversions[i].group,
		                        conversions[i].key,
		                        tracker_keyfile_object_blurb (config,
		                                                      conversions[i].property),
		                        NULL);
	}
}

static void
config_load (TrackerFTSConfig *config)
{
	TrackerConfigFile *file;
	gint i;

	file = TRACKER_CONFIG_FILE (config);
	config_create_with_defaults (config, file->key_file, FALSE);

	if (!file->file_exists) {
		tracker_config_file_save (file);
	}

	for (i = 0; i < G_N_ELEMENTS (conversions); i++) {
		gboolean has_key;

		has_key = g_key_file_has_key (file->key_file,
		                              conversions[i].group,
		                              conversions[i].key,
		                              NULL);

		switch (conversions[i].type) {
		case G_TYPE_INT:
			tracker_keyfile_object_load_int (G_OBJECT (file),
			                                 conversions[i].property,
			                                 file->key_file,
			                                 conversions[i].group,
			                                 conversions[i].key);
			break;
		case G_TYPE_BOOLEAN:
			tracker_keyfile_object_load_boolean (G_OBJECT (file),
			                                     conversions[i].property,
			                                     file->key_file,
			                                     conversions[i].group,
			                                     conversions[i].key);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}
}

static gboolean
config_save (TrackerFTSConfig *config)
{
	TrackerConfigFile *file;
	gint i;

	file = TRACKER_CONFIG_FILE (config);

	if (!file->key_file) {
		g_critical ("Could not save config, GKeyFile was NULL, has the config been loaded?");

		return FALSE;
	}

	g_message ("Setting details to GKeyFile object...");

	for (i = 0; i < G_N_ELEMENTS (conversions); i++) {
		switch (conversions[i].type) {
		case G_TYPE_INT:
			tracker_keyfile_object_save_int (file,
			                                 conversions[i].property,
			                                 file->key_file,
			                                 conversions[i].group,
			                                 conversions[i].key);
			break;

		case G_TYPE_BOOLEAN:
			tracker_keyfile_object_save_boolean (file,
			                                     conversions[i].property,
			                                     file->key_file,
			                                     conversions[i].group,
			                                     conversions[i].key);
			break;

		default:
			g_assert_not_reached ();
			break;
		}
	}

	return tracker_config_file_save (TRACKER_CONFIG_FILE (config));
}

TrackerFTSConfig *
tracker_fts_config_new (void)
{
	return g_object_new (TRACKER_TYPE_FTS_CONFIG,
	                     "domain", "tracker-fts",
	                     NULL);
}

gboolean
tracker_fts_config_save (TrackerFTSConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), FALSE);

	return config_save (config);
}

gint
tracker_fts_config_get_min_word_length (TrackerFTSConfig *config)
{
	TrackerFTSConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_MIN_WORD_LENGTH);

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	return priv->min_word_length;
}

gint
tracker_fts_config_get_max_word_length (TrackerFTSConfig *config)
{
	TrackerFTSConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_MAX_WORD_LENGTH);

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	return priv->max_word_length;
}

gboolean
tracker_fts_config_get_enable_stemmer (TrackerFTSConfig *config)
{
	TrackerFTSConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_ENABLE_STEMMER);

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	return priv->enable_stemmer;
}

gboolean
tracker_fts_config_get_enable_unaccent (TrackerFTSConfig *config)
{
	TrackerFTSConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_ENABLE_UNACCENT);

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	return priv->enable_unaccent;
}

gboolean
tracker_fts_config_get_ignore_numbers (TrackerFTSConfig *config)
{
	TrackerFTSConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_IGNORE_NUMBERS);

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	return priv->ignore_numbers;
}

gboolean
tracker_fts_config_get_ignore_stop_words (TrackerFTSConfig *config)
{
	TrackerFTSConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_IGNORE_STOP_WORDS);

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	return priv->ignore_stop_words;
}

gint
tracker_fts_config_get_max_words_to_index (TrackerFTSConfig *config)
{
	TrackerFTSConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_FTS_CONFIG (config), DEFAULT_MAX_WORDS_TO_INDEX);

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	return priv->max_words_to_index;
}

void
tracker_fts_config_set_min_word_length (TrackerFTSConfig *config,
                                        gint              value)
{
	TrackerFTSConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

	if (!tracker_keyfile_object_validate_int (config, "min-word-length", value)) {
		return;
	}

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	priv->min_word_length = value;
	g_object_notify (G_OBJECT (config), "min-word-length");
}

void
tracker_fts_config_set_max_word_length (TrackerFTSConfig *config,
                                        gint              value)
{
	TrackerFTSConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

	if (!tracker_keyfile_object_validate_int (config, "max-word-length", value)) {
		return;
	}

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	priv->max_word_length = value;
	g_object_notify (G_OBJECT (config), "max-word-length");
}

void
tracker_fts_config_set_enable_stemmer (TrackerFTSConfig *config,
                                       gboolean          value)
{
	TrackerFTSConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	priv->enable_stemmer = value;
	g_object_notify (G_OBJECT (config), "enable-stemmer");
}

void
tracker_fts_config_set_enable_unaccent (TrackerFTSConfig *config,
					gboolean          value)
{
	TrackerFTSConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	priv->enable_unaccent = value;
	g_object_notify (G_OBJECT (config), "enable-unaccent");
}

void
tracker_fts_config_set_ignore_numbers (TrackerFTSConfig *config,
                                       gboolean          value)
{
	TrackerFTSConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	priv->ignore_numbers = value;
	g_object_notify (G_OBJECT (config), "ignore-numbers");
}

void
tracker_fts_config_set_ignore_stop_words (TrackerFTSConfig *config,
                                          gboolean          value)
{
	TrackerFTSConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	priv->ignore_stop_words = value;
	g_object_notify (G_OBJECT (config), "ignore-stop-words");
}

void
tracker_fts_config_set_max_words_to_index (TrackerFTSConfig *config,
                                           gint              value)
{
	TrackerFTSConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_FTS_CONFIG (config));

	if (!tracker_keyfile_object_validate_int (config, "max-words-to-index", value)) {
		return;
	}

	priv = TRACKER_FTS_CONFIG_GET_PRIVATE (config);

	priv->max_words_to_index = value;
	g_object_notify (G_OBJECT (config), "max-words-to-index");
}
