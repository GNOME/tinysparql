/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "tracker-config-file.h"

#define TRACKER_CONFIG_FILE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_CONFIG_FILE, TrackerConfigFilePrivate))

typedef struct _TrackerConfigFilePrivate TrackerConfigFilePrivate;

struct _TrackerConfigFilePrivate {
	gchar *domain;
};

static void     config_finalize     (GObject              *object);
static void     config_load         (TrackerConfigFile *config);
static gboolean config_save         (TrackerConfigFile *config);
static void     config_get_property (GObject              *object,
				     guint                 param_id,
				     GValue               *value,
				     GParamSpec           *pspec);
static void     config_set_property (GObject              *object,
				     guint                 param_id,
				     const GValue         *value,
				     GParamSpec           *pspec);
static void     config_constructed  (GObject              *object);

enum {
	PROP_0,
	PROP_DOMAIN
};

G_DEFINE_TYPE (TrackerConfigFile, tracker_config_file, G_TYPE_OBJECT);

static void
tracker_config_file_class_init (TrackerConfigFileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = config_get_property;
	object_class->set_property = config_set_property;
	object_class->finalize	   = config_finalize;
	object_class->constructed  = config_constructed;

	g_object_class_install_property (object_class,
					 PROP_DOMAIN,
					 g_param_spec_string ("domain",
							      "Config domain",
							      "The prefix before .cfg for the filename",
							      g_get_application_name (),
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerConfigFilePrivate));
}

static void
tracker_config_file_init (TrackerConfigFile *file)
{
	file->key_file = g_key_file_new ();
}

static void     
config_get_property (GObject       *object,
		     guint          param_id,
		     GValue        *value,
		     GParamSpec    *pspec)
{
	TrackerConfigFilePrivate *priv;

	priv = TRACKER_CONFIG_FILE_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_DOMAIN:
		g_value_set_string (value, priv->domain);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
config_set_property (GObject	  *object,
		     guint	   param_id,
		     const GValue *value,
		     GParamSpec	  *pspec)
{
	TrackerConfigFilePrivate *priv;

	priv = TRACKER_CONFIG_FILE_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_DOMAIN:
		g_free (priv->domain);
		priv->domain = g_strdup (g_value_get_string (value));
		g_object_notify (object, "domain");
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
config_finalize (GObject *object)
{
	TrackerConfigFile *file;
	TrackerConfigFilePrivate *priv;

	file = TRACKER_CONFIG_FILE (object);
	priv = TRACKER_CONFIG_FILE_GET_PRIVATE (file);

	if (file->key_file) {
		g_key_file_free (file->key_file);
	}

	if (file->monitor) {
		g_object_unref (file->monitor);
	}

	if (file->file) {
		g_object_unref (file->file);
	}

	g_free (priv->domain);

	(G_OBJECT_CLASS (tracker_config_file_parent_class)->finalize) (object);
}

static void
config_constructed (GObject *object)
{
	config_load (TRACKER_CONFIG_FILE (object));
}

static gchar *
config_dir_ensure_exists_and_return (void)
{
	gchar *directory;

	directory = g_build_filename (g_get_user_config_dir (),
				      "tracker",
				      NULL);

	if (!g_file_test (directory, G_FILE_TEST_EXISTS)) {
		g_print ("Creating config directory:'%s'\n", directory);

		if (g_mkdir_with_parents (directory, 0700) == -1) {
			g_critical ("Could not create configuration directory");
			g_free (directory);
			return NULL;
		}
	}

	return directory;
}

static void
config_changed_cb (GFileMonitor     *monitor,
		   GFile	    *this_file,
		   GFile	    *other_file,
		   GFileMonitorEvent event_type,
		   gpointer	     user_data)
{
	TrackerConfigFile *file;
	gchar	             *filename;

	file = TRACKER_CONFIG_FILE (user_data);

	/* Do we recreate if the file is deleted? */

	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CHANGED:
	case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		file->file_exists = TRUE;

		filename = g_file_get_path (this_file);
		g_message ("Config file changed:'%s', reloading settings...",
			   filename);
		g_free (filename);

		config_load (file);
		break;

	case G_FILE_MONITOR_EVENT_DELETED:
		file->file_exists = FALSE;
		break;

	case G_FILE_MONITOR_EVENT_CREATED:
	case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
	case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
	case G_FILE_MONITOR_EVENT_UNMOUNTED:
		file->file_exists = TRUE;

	default:
		break;
	}
}

static void
config_load (TrackerConfigFile *file)
{
	TrackerConfigFilePrivate *priv;
	GError *error = NULL;
	gchar *basename;
	gchar *filename;
	gchar *directory;

	/* Check we have a config file and if not, create it based on
	 * the default settings.
	 */
	directory = config_dir_ensure_exists_and_return ();
	if (!directory) {
		return;
	}

	priv = TRACKER_CONFIG_FILE_GET_PRIVATE (file);

	basename = g_strdup_printf ("%s.cfg", priv->domain);
	filename = g_build_filename (directory, basename, NULL);
	g_free (basename);
	g_free (directory);

	/* Add file monitoring for changes */
	if (!file->file) {
		file->file = g_file_new_for_path (filename);
	}

	if (!file->monitor) {
		g_message ("Setting up monitor for changes to config file:'%s'",
			   filename);

		file->monitor = g_file_monitor_file (file->file,
						       G_FILE_MONITOR_NONE,
						       NULL,
						       NULL);

		g_signal_connect (file->monitor, "changed",
				  G_CALLBACK (config_changed_cb),
				  file);
	}

	/* Load options */
	g_key_file_load_from_file (file->key_file, 
				   filename, 
				   G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
				   &error);

	/* We force an overwrite in cases of error */
	file->file_exists = error ? FALSE : TRUE;

	if (error) {
		g_error_free (error);
	}

	g_free (filename);
}

static gboolean
config_save (TrackerConfigFile *file)
{
	GError *error = NULL;
	gchar *filename;
	gchar *data;
	gsize size;

	if (!file->key_file) {
		g_critical ("Could not save config, GKeyFile was NULL, has the config been loaded?");

		return FALSE;
	}

	g_message ("Setting details to GKeyFile object...");

	/* FIXME: Get to GKeyFile from object properties */

	g_message ("Saving config to disk...");

	/* Do the actual saving to disk now */
	data = g_key_file_to_data (file->key_file, &size, &error);
	if (error) {
		g_warning ("Could not get config data to write to file, %s",
			   error->message);
		g_error_free (error);

		return FALSE;
	}

	filename = g_file_get_path (file->file);

	g_file_set_contents (filename, data, size, &error);
	g_free (data);

	if (error) {
		g_warning ("Could not write %" G_GSIZE_FORMAT " bytes to file '%s', %s",
			   size,
			   filename,
			   error->message);
		g_free (filename);
		g_error_free (error);

		return FALSE;
	}

	g_message ("Wrote config to '%s' (%" G_GSIZE_FORMAT " bytes)",
		   filename, 
		   size);

	g_free (filename);

	return TRUE;
}

/**
 * tracker_config_file_new:
 *
 * Creates a new GObject for handling Tracker's config file.
 *
 * Return value: A new TrackerConfigFile object. Must be unreferenced when
 * finished with.
 */
TrackerConfigFile *
tracker_config_file_new (const gchar *domain)
{
	TrackerConfigFile *config;

	config = g_object_new (TRACKER_TYPE_CONFIG_FILE, 
			       "domain", domain,
			       NULL);

	return config;
}

/**
 * tracker_config_file_save:
 * @config: a #TrackerConfigFile
 *
 * Writes the configuration stored in TrackerConfigFile to disk.
 *
 * Return value: %TRUE on success, %FALSE otherwise.
 */
gboolean
tracker_config_file_save (TrackerConfigFile *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG_FILE (config), FALSE);

	return config_save (config);
}
