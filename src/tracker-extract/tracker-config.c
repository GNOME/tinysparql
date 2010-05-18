/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <libtracker-common/tracker-keyfile-object.h>

#include "tracker-config.h"

#define TRACKER_CONFIG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_CONFIG, TrackerConfigPrivate))

/* GKeyFile defines */
#define GROUP_GENERAL     "General"

/* Default values */
#define DEFAULT_VERBOSITY 0
#define DEFAULT_MAX_BYTES 1048576   /* 1Mbyte */
#define ABSOLUTE_MAX_BYTES 10485760 /* 10 Mbytes (GB#616845) */

typedef struct {
	/* General */
	gint verbosity;
	gint max_bytes;
} TrackerConfigPrivate;

typedef struct {
	GType  type;
	const gchar *property;
	const gchar *group;
	const gchar *key;
} ObjectToKeyFile;

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
static void     config_load                 (TrackerConfig *config);
static gboolean config_save                 (TrackerConfig *config);
static void     config_create_with_defaults (TrackerConfig *config,
                                             GKeyFile      *key_file,
                                             gboolean       overwrite);

enum {
	PROP_0,

	/* General */
	PROP_VERBOSITY,
	PROP_MAX_BYTES
};

static ObjectToKeyFile conversions[] = {
	{ G_TYPE_INT,     "verbosity",          GROUP_GENERAL,  "Verbosity"       },
	{ G_TYPE_INT,     "max-bytes",          GROUP_GENERAL,  "MaxBytes"        },
};

G_DEFINE_TYPE (TrackerConfig, tracker_config, TRACKER_TYPE_CONFIG_FILE);

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
	                                 g_param_spec_int ("verbosity",
	                                                   "Log verbosity",
	                                                   " Log verbosity (0=errors, 1=minimal, 2=detailed, 3=debug)",
	                                                   0,
	                                                   3,
	                                                   DEFAULT_VERBOSITY,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
	                                 PROP_MAX_BYTES,
	                                 g_param_spec_int ("max-bytes",
	                                                   "Max Bytes",
	                                                   " Maximum number of UTF-8 bytes to extract per file [0->10485760]",
	                                                   0,
	                                                   ABSOLUTE_MAX_BYTES,
	                                                   DEFAULT_MAX_BYTES,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerConfigPrivate));
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
		/* General */
	case PROP_VERBOSITY:
		tracker_config_set_verbosity (TRACKER_CONFIG (object),
		                              g_value_get_int (value));
		break;

	case PROP_MAX_BYTES:
		tracker_config_set_max_bytes (TRACKER_CONFIG (object),
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
	TrackerConfigPrivate *priv;

	priv = TRACKER_CONFIG_GET_PRIVATE (object);

	switch (param_id) {
		/* General */
	case PROP_VERBOSITY:
		g_value_set_int (value, priv->verbosity);
		break;

	case PROP_MAX_BYTES:
		g_value_set_int (value, priv->max_bytes);
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
	(G_OBJECT_CLASS (tracker_config_parent_class)->constructed) (object);

	config_load (TRACKER_CONFIG (object));
}

static void
config_create_with_defaults (TrackerConfig *config,
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

		default:
			g_assert_not_reached ();
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
config_load (TrackerConfig *config)
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

		default:
			g_assert_not_reached ();
			break;
		}
	}
}

static gboolean
config_save (TrackerConfig *config)
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

TrackerConfig *
tracker_config_new (void)
{
	return g_object_new (TRACKER_TYPE_CONFIG, NULL);
}

gboolean
tracker_config_save (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), FALSE);

	return config_save (config);
}

gint
tracker_config_get_verbosity (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_VERBOSITY);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->verbosity;
}

void
tracker_config_set_verbosity (TrackerConfig *config,
                              gint           value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!tracker_keyfile_object_validate_int (config, "verbosity", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->verbosity = value;
	g_object_notify (G_OBJECT (config), "verbosity");
}


gint
tracker_config_get_max_bytes (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MAX_BYTES);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->max_bytes;
}

void
tracker_config_set_max_bytes (TrackerConfig *config,
                              gint           value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!tracker_keyfile_object_validate_int (config, "max-bytes", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->max_bytes = value;
	g_object_notify (G_OBJECT (config), "max-bytes");
}
