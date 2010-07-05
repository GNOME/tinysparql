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
 *
 * Authors:
 * Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-keyfile-object.h>

#include "tracker-db-config.h"

#define TRACKER_DB_CONFIG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_DB_CONFIG, TrackerDBConfigPrivate))

/* GKeyFile defines */
#define GROUP_JOURNAL     "Journal"

/* Default values */
#define DEFAULT_JOURNAL_CHUNK_SIZE           50
#define DEFAULT_JOURNAL_ROTATE_DESTINATION   ""

typedef struct {
	/* Journal */
	gint journal_chunk_size;
	gchar *journal_rotate_destination;
}  TrackerDBConfigPrivate;

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
static void config_create_with_defaults (TrackerDBConfig *config,
                                         GKeyFile      *key_file,
                                         gboolean       overwrite);
static void config_load                 (TrackerDBConfig *config);

enum {
	PROP_0,

	/* Journal */
	PROP_JOURNAL_CHUNK_SIZE,
	PROP_JOURNAL_ROTATE_DESTINATION
};

static ObjectToKeyFile conversions[] = {
	{ G_TYPE_INT,     "journal-chunk-size",         GROUP_JOURNAL,  "JournalChunkSize"         },
	{ G_TYPE_STRING,  "journal-rotate-destination", GROUP_JOURNAL,  "JournalRotateDestination" },
};

G_DEFINE_TYPE (TrackerDBConfig, tracker_db_config, TRACKER_TYPE_CONFIG_FILE);

static void
tracker_db_config_class_init (TrackerDBConfigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = config_set_property;
	object_class->get_property = config_get_property;
	object_class->finalize     = config_finalize;
	object_class->constructed  = config_constructed;

	g_object_class_install_property (object_class,
	                                 PROP_JOURNAL_CHUNK_SIZE,
	                                 g_param_spec_int ("journal-chunk-size",
	                                                   "Journal chunk size",
	                                                   " Size of the journal at rotation in MB. Use -1 to disable rotating",
	                                                   -1,
	                                                   G_MAXINT,
	                                                   DEFAULT_JOURNAL_CHUNK_SIZE,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
	                                 PROP_JOURNAL_ROTATE_DESTINATION,
	                                 g_param_spec_string ("journal-rotate-destination",
	                                                      "Journal rotate destination",
	                                                      " Destination to rotate journal chunks to",
	                                                      DEFAULT_JOURNAL_ROTATE_DESTINATION,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerDBConfigPrivate));
}

static void
tracker_db_config_init (TrackerDBConfig *object)
{
}

static void
config_set_property (GObject      *object,
                     guint         param_id,
                     const GValue *value,
                     GParamSpec           *pspec)
{
	switch (param_id) {
		/* Journal */
	case PROP_JOURNAL_CHUNK_SIZE:
		tracker_db_config_set_journal_chunk_size (TRACKER_DB_CONFIG (object),
		                                          g_value_get_int(value));
		break;
	case PROP_JOURNAL_ROTATE_DESTINATION:
		tracker_db_config_set_journal_rotate_destination (TRACKER_DB_CONFIG (object),
		                                                  g_value_get_string(value));
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
	TrackerDBConfigPrivate *priv;

	priv = TRACKER_DB_CONFIG_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_JOURNAL_CHUNK_SIZE:
		g_value_set_int (value, priv->journal_chunk_size);
		break;
	case PROP_JOURNAL_ROTATE_DESTINATION:
		g_value_set_string (value, priv->journal_rotate_destination);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
config_finalize (GObject *object)
{
	TrackerDBConfigPrivate *priv;

	priv = TRACKER_DB_CONFIG_GET_PRIVATE (object);

	g_free (priv->journal_rotate_destination);

	(G_OBJECT_CLASS (tracker_db_config_parent_class)->finalize) (object);
}

static void
config_constructed (GObject *object)
{
	(G_OBJECT_CLASS (tracker_db_config_parent_class)->constructed) (object);

	config_load (TRACKER_DB_CONFIG (object));
}

static void
config_create_with_defaults (TrackerDBConfig *config,
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

		case G_TYPE_STRING:
			g_key_file_set_string (key_file,
			                       conversions[i].group,
			                       conversions[i].key,
			                       tracker_keyfile_object_default_string (config,
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
config_load (TrackerDBConfig *config)
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

		case G_TYPE_STRING:
			tracker_keyfile_object_load_string (G_OBJECT (file),
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
config_save (TrackerDBConfig *config)
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

		case G_TYPE_STRING:
			tracker_keyfile_object_save_string (file,
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

TrackerDBConfig *
tracker_db_config_new (void)
{
	return g_object_new (TRACKER_TYPE_DB_CONFIG,
	                     "domain", "tracker-db",
	                     NULL);
}

gboolean
tracker_db_config_save (TrackerDBConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_DB_CONFIG (config), FALSE);

	return config_save (config);
}


gint
tracker_db_config_get_journal_chunk_size (TrackerDBConfig *config)
{
	TrackerDBConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_CONFIG (config), DEFAULT_JOURNAL_CHUNK_SIZE);

	priv = TRACKER_DB_CONFIG_GET_PRIVATE (config);

	return priv->journal_chunk_size;
}

const gchar *
tracker_db_config_get_journal_rotate_destination (TrackerDBConfig *config)
{
	TrackerDBConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_CONFIG (config), DEFAULT_JOURNAL_ROTATE_DESTINATION);

	priv = TRACKER_DB_CONFIG_GET_PRIVATE (config);

	return priv->journal_rotate_destination;
}

void
tracker_db_config_set_journal_chunk_size (TrackerDBConfig *config,
                                          gint             value)
{
	TrackerDBConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_CONFIG (config));

	if (!tracker_keyfile_object_validate_int (config, "journal-chunk-size", value)) {
		return;
	}

	priv = TRACKER_DB_CONFIG_GET_PRIVATE (config);

	priv->journal_chunk_size = value;
	g_object_notify (G_OBJECT (config), "journal-chunk-size");
}

void
tracker_db_config_set_journal_rotate_destination (TrackerDBConfig *config,
                                                  const gchar     *value)
{
	TrackerDBConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_CONFIG (config));

	priv = TRACKER_DB_CONFIG_GET_PRIVATE (config);

	g_free (priv->journal_rotate_destination);
	priv->journal_rotate_destination = g_strdup (value);

	g_object_notify (G_OBJECT (config), "journal-rotate-destination");
}
