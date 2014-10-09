/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2014, Lanedo <martyn@lanedo.com>
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
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include <libtracker-common/tracker-common.h>

#include "tracker-config.h"

#define CONFIG_SCHEMA "org.freedesktop.Tracker.Writeback"
#define CONFIG_PATH   "/org/freedesktop/tracker/writeback/"

static void     config_set_property         (GObject           *object,
                                             guint              param_id,
                                             const GValue      *value,
                                             GParamSpec        *pspec);
static void     config_get_property         (GObject           *object,
                                             guint              param_id,
                                             GValue            *value,
                                             GParamSpec        *pspec);
static void     config_finalize             (GObject           *object);
static void     config_constructed          (GObject           *object);

enum {
	PROP_0,
	PROP_VERBOSITY,
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

	g_object_class_install_property (object_class,
	                                 PROP_VERBOSITY,
	                                 g_param_spec_enum ("verbosity",
	                                                    "Log verbosity",
	                                                    "Log verbosity (0=errors, 1=minimal, 2=detailed, 3=debug)",
	                                                    TRACKER_TYPE_VERBOSITY,
	                                                    TRACKER_VERBOSITY_ERRORS,
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
		tracker_config_set_verbosity (TRACKER_CONFIG (object),
		                              g_value_get_enum (value));
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
		g_value_set_enum (value, tracker_config_get_verbosity (TRACKER_CONFIG (object)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
config_finalize (GObject *object)
{
	(G_OBJECT_CLASS (tracker_config_parent_class)->finalize) (object);
}

static void
config_constructed (GObject *object)
{
	GSettings *settings;

	(G_OBJECT_CLASS (tracker_config_parent_class)->constructed) (object);

	settings = G_SETTINGS (object);

	if (G_LIKELY (!g_getenv ("TRACKER_USE_CONFIG_FILES"))) {
		g_settings_delay (settings);
	}

	g_settings_bind (settings, "verbosity", object, "verbosity", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_GET_NO_CHANGES);
}

TrackerConfig *
tracker_config_new (void)
{
	TrackerConfig *config = NULL;

	/* FIXME: should we unset GSETTINGS_BACKEND env var? */

	if (G_UNLIKELY (g_getenv ("TRACKER_USE_CONFIG_FILES"))) {
		GSettingsBackend *backend;
		gchar *filename, *basename;
		gboolean need_to_save;

		basename = g_strdup_printf ("%s.cfg", g_get_prgname ());
		filename = g_build_filename (g_get_user_config_dir (), "tracker", basename, NULL);
		g_free (basename);

		need_to_save = g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE;

		backend = g_keyfile_settings_backend_new (filename, CONFIG_PATH, "General");
		g_info ("Using config file '%s'", filename);
		g_free (filename);

		config = g_object_new (TRACKER_TYPE_CONFIG,
		                       "backend", backend,
		                       "schema-id", CONFIG_SCHEMA,
		                       "path", CONFIG_PATH,
		                       NULL);
		g_object_unref (backend);

		if (need_to_save) {
			g_info ("  Config file does not exist, using default values...");
		}
	} else {
		config = g_object_new (TRACKER_TYPE_CONFIG,
		                       "schema-id", CONFIG_SCHEMA,
		                       "path", CONFIG_PATH,
		                       NULL);
	}

	return config;
}

gint
tracker_config_get_verbosity (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), 0);

	return g_settings_get_enum (G_SETTINGS (config), "verbosity");
}

void
tracker_config_set_verbosity (TrackerConfig *config,
                              gint           value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_enum (G_SETTINGS (config), "verbosity", value);
	g_object_notify (G_OBJECT (config), "verbosity");
}
