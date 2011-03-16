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
	PROP_MAX_BYTES
};

static TrackerConfigMigrationEntry migration[] = {
	{ G_TYPE_ENUM, "General", "Verbosity", "verbosity" },
	{ G_TYPE_INT, "General", "MaxBytes", "max-bytes" },
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
	                                 PROP_MAX_BYTES,
	                                 g_param_spec_int ("max-bytes",
	                                                   "Max Bytes",
	                                                   "Maximum number of UTF-8 bytes to extract per file [0->10485760]",
	                                                   0, 1024 * 1024 * 10,
							   1024 * 1024,
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
	switch (param_id) {
	case PROP_VERBOSITY:
		g_settings_set_enum (G_SETTINGS (object), "verbosity",
		                     g_value_get_enum (value));
		break;

	case PROP_MAX_BYTES:
		g_settings_set_int (G_SETTINGS (object), "max-bytes",
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
	switch (param_id) {
	case PROP_VERBOSITY:
		g_value_set_enum (value,
		                  g_settings_get_enum (G_SETTINGS (object), "verbosity"));
		break;

	case PROP_MAX_BYTES:
		g_value_set_int (value,
		                 g_settings_get_int (G_SETTINGS (object), "max-bytes"));
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

	(G_OBJECT_CLASS (tracker_config_parent_class)->constructed) (object);

	g_settings_delay (G_SETTINGS (object));

	/* Migrate keyfile-based configuration */
	config_file = tracker_config_file_new ();

	if (config_file) {
		tracker_config_file_migrate (config_file, G_SETTINGS (object), migration);
		g_object_unref (config_file);
	}
}

TrackerConfig *
tracker_config_new (void)
{
	return g_object_new (TRACKER_TYPE_CONFIG,
			     "schema", "org.freedesktop.Tracker.Extract",
			     "path", "/org/freedesktop/tracker/extract/",
			     NULL);
}

gint
tracker_config_get_verbosity (TrackerConfig *config)
{
	gint verbosity;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), TRACKER_VERBOSITY_ERRORS);

	g_object_get (config, "verbosity", &verbosity, NULL);
	return verbosity;
}

void
tracker_config_set_verbosity (TrackerConfig *config,
                              gint           value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_object_set (G_OBJECT (config), "verbosity", value, NULL);
}


gint
tracker_config_get_max_bytes (TrackerConfig *config)
{
	gint max_bytes;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), 0);

	g_object_get (config, "max-bytes", &max_bytes, NULL);
	return max_bytes;
}

void
tracker_config_set_max_bytes (TrackerConfig *config,
                              gint           value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_object_set (G_OBJECT (config), "max-bytes", value, NULL);
}
