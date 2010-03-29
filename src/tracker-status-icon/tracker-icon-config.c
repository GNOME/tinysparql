/*
 * Copyright (C) 2009, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-keyfile-object.h>

#include "tracker-icon-config.h"

#define TRACKER_ICON_CONFIG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_ICON_CONFIG, TrackerIconConfigPrivate))

/* GKeyFile defines */
#define GROUP_GENERAL                            "General"

/* Default values */
#define DEFAULT_VISIBILITY TRACKER_SHOW_ACTIVE

typedef struct {
	gint     visibility;
} TrackerIconConfigPrivate;

typedef struct {
	GType  type;
	const gchar *property;
	const gchar *group;
	const gchar *key;
} ObjectToKeyFile;

static void     config_set_property         (GObject           *object,
                                             guint              param_id,
                                             const GValue      *value,
                                             GParamSpec        *pspec);
static void     config_get_property         (GObject           *object,
                                             guint              param_id,
                                             GValue            *value,
                                             GParamSpec        *pspec);
static void     config_constructed          (GObject           *object);
static void     config_changed              (TrackerConfigFile *file);
static void     config_load                 (TrackerIconConfig *config);
static gboolean config_save                 (TrackerIconConfig *config);
static void     config_create_with_defaults (TrackerIconConfig *config,
                                             GKeyFile          *key_file,
                                             gboolean           overwrite);

enum {
	PROP_0,
	PROP_VISIBILITY
};

static ObjectToKeyFile conversions[] = {
	/* General */
	{ G_TYPE_INT, "visibility", GROUP_GENERAL,  "Visibility" }
};

G_DEFINE_TYPE (TrackerIconConfig, tracker_icon_config, TRACKER_TYPE_CONFIG_FILE);

static void
tracker_icon_config_class_init (TrackerIconConfigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerConfigFileClass *config_file_class = TRACKER_CONFIG_FILE_CLASS (klass);

	object_class->set_property = config_set_property;
	object_class->get_property = config_get_property;
	object_class->constructed  = config_constructed;

	config_file_class->changed = config_changed;

	/* General */
	g_object_class_install_property (object_class,
	                                 PROP_VISIBILITY,
	                                 g_param_spec_int ("visibility",
	                                                   "Status icon visibility",
	                                                   "Status icon visibility (0=Never, 1=When active, 2=Always)",
	                                                   TRACKER_SHOW_NEVER, TRACKER_SHOW_ALWAYS,
	                                                   DEFAULT_VISIBILITY,
	                                                   G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (TrackerIconConfigPrivate));
}

static void
tracker_icon_config_init (TrackerIconConfig *object)
{
}

static void
config_set_property (GObject      *object,
                     guint         param_id,
                     const GValue *value,
                     GParamSpec           *pspec)
{
	switch (param_id) {
		/* General */
	case PROP_VISIBILITY:
		tracker_icon_config_set_visibility (TRACKER_ICON_CONFIG (object),
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
	TrackerIconConfigPrivate *priv;

	priv = TRACKER_ICON_CONFIG_GET_PRIVATE (object);

	switch (param_id) {
		/* General */
	case PROP_VISIBILITY:
		g_value_set_int (value, priv->visibility);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
config_constructed (GObject *object)
{
	(G_OBJECT_CLASS (tracker_icon_config_parent_class)->constructed) (object);

	config_load (TRACKER_ICON_CONFIG (object));
}

static void
config_changed (TrackerConfigFile *file)
{
	/* Reload config */
	config_load (TRACKER_ICON_CONFIG (file));
}

static void
config_create_with_defaults (TrackerIconConfig *config,
                             GKeyFile          *key_file,
                             gboolean           overwrite)
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
		default:
			g_assert_not_reached ();
		}

		g_key_file_set_comment (key_file,
		                        conversions[i].group,
		                        conversions[i].key,
		                        tracker_keyfile_object_blurb (config, conversions[i].property),
		                        NULL);
	}
}

static void
config_load (TrackerIconConfig *config)
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
		}
	}
}

static gboolean
config_save (TrackerIconConfig *config)
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
		default:
			g_assert_not_reached ();
			break;
		}
	}

	return tracker_config_file_save (file);
}

TrackerIconConfig *
tracker_icon_config_new (void)
{
	return g_object_new (TRACKER_TYPE_ICON_CONFIG, NULL);
}

TrackerIconConfig *
tracker_icon_config_new_with_domain (const gchar *domain)
{
	return g_object_new (TRACKER_TYPE_ICON_CONFIG,  "domain", domain, NULL);
}

gboolean
tracker_icon_config_save (TrackerIconConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_ICON_CONFIG (config), FALSE);

	return config_save (config);
}

TrackerVisibility
tracker_icon_config_get_visibility (TrackerIconConfig *config)
{
	TrackerIconConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_ICON_CONFIG (config), DEFAULT_VISIBILITY);

	priv = TRACKER_ICON_CONFIG_GET_PRIVATE (config);

	return priv->visibility;
}

void
tracker_icon_config_set_visibility (TrackerIconConfig *config,
                                    TrackerVisibility  visibility)
{
	TrackerIconConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_ICON_CONFIG (config));

	priv = TRACKER_ICON_CONFIG_GET_PRIVATE (config);

	priv->visibility = visibility;
	g_object_notify (G_OBJECT (config), "visibility");
}
