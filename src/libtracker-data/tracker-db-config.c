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

/* GKeyFile defines */
#define GROUP_JOURNAL     "Journal"

/* Default values */
#define DEFAULT_JOURNAL_CHUNK_SIZE           50
#define DEFAULT_JOURNAL_ROTATE_DESTINATION   ""

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

	/* Journal */
	PROP_JOURNAL_CHUNK_SIZE,
	PROP_JOURNAL_ROTATE_DESTINATION
};

static TrackerConfigMigrationEntry migration[] = {
        { G_TYPE_INT, GROUP_JOURNAL, "JournalChunkSize", "journal-chunk-size" },
        { G_TYPE_STRING, GROUP_JOURNAL, "JournalRotateDestination", "journal-rotate-destination" },
};

G_DEFINE_TYPE (TrackerDBConfig, tracker_db_config, G_TYPE_SETTINGS);

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
	                                                   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_JOURNAL_ROTATE_DESTINATION,
	                                 g_param_spec_string ("journal-rotate-destination",
	                                                      "Journal rotate destination",
	                                                      " Destination to rotate journal chunks to",
	                                                      DEFAULT_JOURNAL_ROTATE_DESTINATION,
	                                                      G_PARAM_READWRITE));

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
	TrackerDBConfig *config = TRACKER_DB_CONFIG (object);

	switch (param_id) {
	case PROP_JOURNAL_CHUNK_SIZE:
		g_value_set_int (value, tracker_db_config_get_journal_chunk_size (config));
		break;
	case PROP_JOURNAL_ROTATE_DESTINATION:
		g_value_set_string (value, tracker_db_config_get_journal_rotate_destination (config));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
config_finalize (GObject *object)
{
	(G_OBJECT_CLASS (tracker_db_config_parent_class)->finalize) (object);
}


static void
config_constructed (GObject *object)
{
        TrackerConfigFile *config_file;

	(G_OBJECT_CLASS (tracker_db_config_parent_class)->constructed) (object);

        g_settings_delay (G_SETTINGS (object));

        /* Migrate keyfile-based configuration */
        config_file = tracker_config_file_new ();
        if (config_file) {
                tracker_config_file_migrate (config_file,
                                             G_SETTINGS (object), migration);
                g_object_unref (config_file);
        }
}

TrackerDBConfig *
tracker_db_config_new (void)
{
	return g_object_new (TRACKER_TYPE_DB_CONFIG,
                             "schema", "org.freedesktop.Tracker.DB",
	                     "path", "/org/freedesktop/tracker/db/",
	                     NULL);
}

gboolean
tracker_db_config_save (TrackerDBConfig *config)
{
        g_settings_apply (G_SETTINGS (config));
        //FIXME: make this function return void?
        return TRUE;
}


gint
tracker_db_config_get_journal_chunk_size (TrackerDBConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_DB_CONFIG (config), DEFAULT_JOURNAL_CHUNK_SIZE);

	return g_settings_get_int (G_SETTINGS (config), "journal-chunk-size");
}

const gchar *
tracker_db_config_get_journal_rotate_destination (TrackerDBConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_DB_CONFIG (config), DEFAULT_JOURNAL_ROTATE_DESTINATION);

	return g_settings_get_string (G_SETTINGS (config), "journal-rotate-destination");
}

void
tracker_db_config_set_journal_chunk_size (TrackerDBConfig *config,
                                          gint             value)
{
	g_return_if_fail (TRACKER_IS_DB_CONFIG (config));

        g_settings_set_int (G_SETTINGS (config), "journal-chunk-size", value);
	g_object_notify (G_OBJECT (config), "journal-chunk-size");
}

void
tracker_db_config_set_journal_rotate_destination (TrackerDBConfig *config,
                                                  const gchar     *value)
{
	g_return_if_fail (TRACKER_IS_DB_CONFIG (config));

        g_settings_set_string (G_SETTINGS (config), "journal-rotate-destination", g_strdup (value));
	g_object_notify (G_OBJECT (config), "journal-rotate-destination");
}
