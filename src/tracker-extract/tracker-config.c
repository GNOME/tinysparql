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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <libtracker-common/tracker-keyfile-object.h>
#include <libtracker-common/tracker-enum-types.h>
#include <libtracker-common/tracker-enums.h>

#include "tracker-config.h"

static void     config_set_property         (GObject       *object,
                                             guint          param_id,
                                             const GValue  *value,
                                             GParamSpec    *pspec);
static void     config_get_property         (GObject       *object,
                                             guint          param_id,
                                             GValue        *value,
                                             GParamSpec    *pspec);
static void     config_finalize             (GObject       *object);
static void     config_constructed          (GObject       *object);

enum {
	PROP_0,
	PROP_VERBOSITY,
	PROP_SCHED_IDLE,
	PROP_MAX_BYTES,
	PROP_MAX_MEDIA_ART_WIDTH,
	PROP_WAIT_FOR_MINER_FS,
};

static TrackerConfigMigrationEntry migration[] = {
	{ G_TYPE_ENUM, "General", "Verbosity", "verbosity" },
	{ G_TYPE_ENUM, "General", "SchedIdle", "sched-idle" },
	{ G_TYPE_INT, "General", "MaxBytes", "max-bytes" },
	{ G_TYPE_INT, "General", "MaxMediaArtWidth", "max-media-art-width" },
	{ 0 }
};

G_DEFINE_TYPE (TrackerConfig, tracker_config, G_TYPE_SETTINGS);

static void
tracker_config_class_init (TrackerConfigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = config_set_property;
	object_class->get_property = config_get_property;
	object_class->finalize     = config_finalize;
	object_class->constructed  = config_constructed;

	/* General */
	g_object_class_install_property (object_class,
	                                 PROP_VERBOSITY,
	                                 g_param_spec_enum ("verbosity",
	                                                    "Log verbosity",
	                                                    "Log verbosity (0=errors, 1=minimal, 2=detailed, 3=debug)",
	                                                    TRACKER_TYPE_VERBOSITY,
	                                                    TRACKER_VERBOSITY_ERRORS,
	                                                    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_SCHED_IDLE,
	                                 g_param_spec_enum ("sched-idle",
	                                                    "Scheduler priority when idle",
	                                                    "Scheduler priority when idle (0=always, 1=first-index, 2=never)",
	                                                    TRACKER_TYPE_SCHED_IDLE,
	                                                    TRACKER_SCHED_IDLE_FIRST_INDEX,
	                                                    G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_MAX_BYTES,
	                                 g_param_spec_int ("max-bytes",
	                                                   "Max Bytes",
	                                                   "Maximum number of UTF-8 bytes to extract per file [0->10485760]",
	                                                   0, 1024 * 1024 * 10,
	                                                   1024 * 1024,
	                                                   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_MAX_MEDIA_ART_WIDTH,
	                                 g_param_spec_int ("max-media-art-width",
	                                                   "Max Media Art Width",
	                                                   " Maximum width of the Media Art to be generated (-1=disable, 0=original width, 1->2048=max pixel width)",
	                                                   -1,
	                                                   2048,
	                                                   0,
	                                                   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_WAIT_FOR_MINER_FS,
	                                 g_param_spec_boolean ("wait-for-miner-fs",
	                                                       "Wait for FS miner to be done before extracting",
	                                                       "%TRUE to wait for tracker-miner-fs is done before extracting. %FAlSE otherwise",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE));
}

static void
tracker_config_init (TrackerConfig *object)
{
}

static void
config_set_property (GObject      *object,
                     guint         param_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
	TrackerConfig *config = TRACKER_CONFIG (object);

	switch (param_id) {
	/* General */
	/* NOTE: We handle these because we have to be able
	 * to save these based on command line overrides.
	 */
	case PROP_VERBOSITY:
		tracker_config_set_verbosity (config, g_value_get_enum (value));
		break;

	/* We don't care about the others... we don't save anyway. */
	case PROP_SCHED_IDLE:
	case PROP_MAX_BYTES:
	case PROP_MAX_MEDIA_ART_WIDTH:
	case PROP_WAIT_FOR_MINER_FS:
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
	TrackerConfig *config = TRACKER_CONFIG (object);

	switch (param_id) {
	case PROP_VERBOSITY:
		g_value_set_enum (value,
		                  tracker_config_get_verbosity (config));
		break;

	case PROP_SCHED_IDLE:
		g_value_set_enum (value,
		                  tracker_config_get_sched_idle (config));
		break;

	case PROP_MAX_BYTES:
		g_value_set_int (value,
		                 tracker_config_get_max_bytes (config));
		break;

	case PROP_MAX_MEDIA_ART_WIDTH:
		g_value_set_int (value,
		                 tracker_config_get_max_media_art_width (config));
		break;

	case PROP_WAIT_FOR_MINER_FS:
		g_value_set_boolean (value,
		                     tracker_config_get_wait_for_miner_fs (config));
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

	(G_OBJECT_CLASS (tracker_config_parent_class)->finalize) (object);
}

static void
config_constructed (GObject *object)
{
	TrackerConfigFile *config_file;
	GSettings *settings;

	(G_OBJECT_CLASS (tracker_config_parent_class)->constructed) (object);

	settings = G_SETTINGS (object);

	g_settings_delay (settings);

	/* Set up bindings */
	g_settings_bind (settings, "verbosity", object, "verbosity", G_SETTINGS_BIND_GET_NO_CHANGES);
	g_settings_bind (settings, "sched-idle", object, "sched-idle", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "max-bytes", object, "max-bytes", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "max-media-art-width", object, "max-media-art-width", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "wait-for-miner-fs", object, "wait-for-miner-fs", G_SETTINGS_BIND_GET);

	/* Migrate keyfile-based configuration */
	config_file = tracker_config_file_new ();

	if (config_file) {
		tracker_config_file_migrate (config_file, settings, migration);
		g_object_unref (config_file);
	}
}

TrackerConfig *
tracker_config_new (void)
{
	return g_object_new (TRACKER_TYPE_CONFIG,
	                     "schema-id", "org.freedesktop.Tracker.Extract",
	                     "path", "/org/freedesktop/tracker/extract/",
	                     NULL);
}

gint
tracker_config_get_verbosity (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), TRACKER_VERBOSITY_ERRORS);

	return g_settings_get_enum (G_SETTINGS (config), "verbosity");
}

void
tracker_config_set_verbosity (TrackerConfig *config,
                              gint           value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_enum (G_SETTINGS (config), "verbosity", value);
}

gint
tracker_config_get_sched_idle (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), TRACKER_SCHED_IDLE_FIRST_INDEX);

	return g_settings_get_enum (G_SETTINGS (config), "sched-idle");
}

gint
tracker_config_get_max_bytes (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), 0);

	return g_settings_get_int (G_SETTINGS (config), "max-bytes");
}

gint
tracker_config_get_max_media_art_width (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), 0);

	return g_settings_get_int (G_SETTINGS (config), "max-media-art-width");
}

gboolean
tracker_config_get_wait_for_miner_fs (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), FALSE);

	return g_settings_get_boolean (G_SETTINGS (config), "wait-for-miner-fs");
}
