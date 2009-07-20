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

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-config-utils.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-type-utils.h>

#include "tracker-config.h"

#define TRACKER_CONFIG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_CONFIG, TrackerConfigPrivate))

/* GKeyFile defines */
#define GROUP_GENERAL				 "General"
#define GROUP_MONITORS				 "Monitors"
#define GROUP_INDEXING				 "Indexing"

/* Default values */
#define DEFAULT_VERBOSITY			 0
#define DEFAULT_INITIAL_SLEEP			 15	  /* 0->1000 */
#define DEFAULT_ENABLE_WATCHES			 TRUE
#define DEFAULT_THROTTLE			 0	  /* 0->20 */
#define DEFAULT_ENABLE_THUMBNAILS		 TRUE
#define DEFAULT_DISABLE_INDEXING_ON_BATTERY	 TRUE
#define DEFAULT_DISABLE_INDEXING_ON_BATTERY_INIT FALSE
#define DEFAULT_INDEX_MOUNTED_DIRECTORIES	 TRUE
#define DEFAULT_INDEX_REMOVABLE_DEVICES		 TRUE
#define DEFAULT_LOW_DISK_SPACE_LIMIT		 1	  /* 0->100 / -1 */

typedef struct {
	/* General */
	gint	  verbosity;
	gint	  initial_sleep;

	/* Watches */
	GSList	 *watch_directory_roots;
	GSList	 *crawl_directory_roots;
	GSList	 *no_watch_directory_roots;
	gboolean  enable_watches;

	/* Indexing */
	gint	  throttle;
	gboolean  enable_thumbnails;
	GSList   *disabled_modules;

	gboolean  disable_indexing_on_battery;
	gboolean  disable_indexing_on_battery_init;
	gint	  low_disk_space_limit;
	gboolean  index_mounted_directories;
	gboolean  index_removable_devices;
} TrackerConfigPrivate;

typedef struct {
	GType  type;
	gchar *property;
	gchar *group;
	gchar *key;
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
	PROP_INITIAL_SLEEP,

	/* Watches */
	PROP_ENABLE_WATCHES,
	PROP_WATCH_DIRECTORY_ROOTS,
	PROP_CRAWL_DIRECTORY_ROOTS,
	PROP_NO_WATCH_DIRECTORY_ROOTS,

	/* Indexing */
	PROP_THROTTLE,
	PROP_ENABLE_THUMBNAILS,
	PROP_DISABLED_MODULES,
	PROP_DISABLE_INDEXING_ON_BATTERY,
	PROP_DISABLE_INDEXING_ON_BATTERY_INIT,
	PROP_LOW_DISK_SPACE_LIMIT,
	PROP_INDEX_MOUNTED_DIRECTORIES,
	PROP_INDEX_REMOVABLE_DEVICES,
};


static ObjectToKeyFile conversions[] = {
	{ G_TYPE_INT,     "verbosity",                        GROUP_GENERAL,  "Verbosity"               },
	{ G_TYPE_INT,     "initial-sleep",                    GROUP_GENERAL,  "InitialSleep"            },
	{ G_TYPE_BOOLEAN, "enable-watches",                   GROUP_MONITORS, "EnableWatches"           },
	{ G_TYPE_POINTER, "watch-directory-roots",            GROUP_MONITORS, "WatchDirectoryRoots"     },
	{ G_TYPE_POINTER, "crawl-directory-roots",            GROUP_MONITORS, "CrawlDirectoryRoots"     },
	{ G_TYPE_POINTER, "no-watch-directory-roots",         GROUP_MONITORS, "NoWatchDirectory"        },
	{ G_TYPE_INT,     "throttle",                         GROUP_INDEXING, "Throttle"                },
	{ G_TYPE_BOOLEAN, "enable-thumbnails",                GROUP_INDEXING, "EnableThumbnails"        },
	{ G_TYPE_POINTER, "disabled-modules",                 GROUP_INDEXING, "DisabledModules"         },
	{ G_TYPE_BOOLEAN, "disable-indexing-on-battery",      GROUP_INDEXING, "BatteryIndex"            },
	{ G_TYPE_BOOLEAN, "disable-indexing-on-battery-init", GROUP_INDEXING, "BatteryIndexInitial"     },
	{ G_TYPE_INT,     "low-disk-space-limit",             GROUP_INDEXING, "LowDiskSpaceLimit"       },
	{ G_TYPE_BOOLEAN, "index-mounted-directories",        GROUP_INDEXING, "IndexMountedDirectories" },
	{ G_TYPE_BOOLEAN, "index-removable-devices",          GROUP_INDEXING, "IndexRemovableMedia"     },
};

G_DEFINE_TYPE (TrackerConfig, tracker_config, TRACKER_TYPE_CONFIG_MANAGER);

static void
tracker_config_class_init (TrackerConfigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = config_set_property;
	object_class->get_property = config_get_property;
	object_class->finalize	   = config_finalize;
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
					 PROP_INITIAL_SLEEP,
					 g_param_spec_int ("initial-sleep",
							   "Initial sleep",
							   " Time in seconds before crawling filesystem (0->1000)",
							   0,
							   1000,
							   DEFAULT_INITIAL_SLEEP,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/* Monitors */
	g_object_class_install_property (object_class,
					 PROP_ENABLE_WATCHES,
					 g_param_spec_boolean ("enable-watches",
							       "Enable watches",
							       " Set to false to completely disable any watching",
							       DEFAULT_ENABLE_WATCHES,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_WATCH_DIRECTORY_ROOTS,
					 g_param_spec_pointer ("watch-directory-roots",
							       "Watched directory roots",
							       " List of directory roots to index and watch (separator=;)",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_CRAWL_DIRECTORY_ROOTS,
					 g_param_spec_pointer ("crawl-directory-roots",
							       "Crawl directory roots",
							       " List of directory roots to index but NOT watch (separator=;)",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_NO_WATCH_DIRECTORY_ROOTS,
					 g_param_spec_pointer ("no-watch-directory-roots",
							       "Not watched directory roots",
							       " List of directory roots NOT to index and NOT to watch (separator=;)",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/* Indexing */
	g_object_class_install_property (object_class,
					 PROP_THROTTLE,
					 g_param_spec_int ("throttle",
							   "Throttle",
							   " Sets the indexing speed (0->20, where 20=slowest speed)",
							   0,
							   20,
							   DEFAULT_THROTTLE,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_ENABLE_THUMBNAILS,
					 g_param_spec_boolean ("enable-thumbnails",
							       "Enable thumbnails",
							       " Set to false to completely disable thumbnail generation",
							       DEFAULT_ENABLE_THUMBNAILS,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_DISABLED_MODULES,
					 g_param_spec_pointer ("disabled-modules",
							       "Disabled modules",
							       " List of disabled modules (separator=;)\n"
							       " The modules that are indexed are kept in $prefix/lib/tracker/indexer-modules",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
;
	g_object_class_install_property (object_class,
					 PROP_DISABLE_INDEXING_ON_BATTERY,
					 g_param_spec_boolean ("disable-indexing-on-battery",
							       "Disable indexing on battery",
							       " Set to true to disable indexing when running on battery",
							       DEFAULT_DISABLE_INDEXING_ON_BATTERY,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_DISABLE_INDEXING_ON_BATTERY_INIT,
					 g_param_spec_boolean ("disable-indexing-on-battery-init",
							       "Disable indexing on battery",
							       " Set to true to disable initial indexing when running on battery",
							       DEFAULT_DISABLE_INDEXING_ON_BATTERY_INIT,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_LOW_DISK_SPACE_LIMIT,
					 g_param_spec_int ("low-disk-space-limit",
							   "Low disk space limit",
							   " Pause indexer when disk space is <= this value\n"
							   " (0->100, value is in % of $HOME file system, -1=disable pausing)",
							   -1,
							   100,
							   DEFAULT_LOW_DISK_SPACE_LIMIT,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_INDEX_MOUNTED_DIRECTORIES,
					 g_param_spec_boolean ("index-mounted-directories",
							       "Index mounted directories",
							       " Set to true to enable traversing mounted directories on other file systems\n"
							       " (this excludes removable devices)",
							       DEFAULT_INDEX_MOUNTED_DIRECTORIES,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_INDEX_REMOVABLE_DEVICES,
					 g_param_spec_boolean ("index-removable-devices",
							       "index removable devices",
							       " Set to true to enable traversing mounted directories for removable devices",
							       DEFAULT_INDEX_REMOVABLE_DEVICES,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerConfigPrivate));
}

static void
tracker_config_init (TrackerConfig *object)
{
}

static void
config_set_property (GObject	  *object,
		     guint	   param_id,
		     const GValue *value,
		     GParamSpec	  *pspec)
{
	switch (param_id) {
		/* General */
	case PROP_VERBOSITY:
		tracker_config_set_verbosity (TRACKER_CONFIG (object),
					      g_value_get_int (value));
		break;
	case PROP_INITIAL_SLEEP:
		tracker_config_set_initial_sleep (TRACKER_CONFIG (object),
						  g_value_get_int (value));
		break;

		/* Watches */
	case PROP_WATCH_DIRECTORY_ROOTS:    
		tracker_config_set_watch_directory_roots (TRACKER_CONFIG (object),
							  g_value_get_pointer (value));
		break;

	case PROP_CRAWL_DIRECTORY_ROOTS:    
		tracker_config_set_crawl_directory_roots (TRACKER_CONFIG (object),
							  g_value_get_pointer (value));
		break;
	case PROP_NO_WATCH_DIRECTORY_ROOTS: 
		tracker_config_set_no_watch_directory_roots (TRACKER_CONFIG (object),
							     g_value_get_pointer (value));
		break;
	case PROP_ENABLE_WATCHES:
		tracker_config_set_enable_watches (TRACKER_CONFIG (object),
						   g_value_get_boolean (value));
		break;

		/* Indexing */
	case PROP_THROTTLE:
		tracker_config_set_throttle (TRACKER_CONFIG (object),
					     g_value_get_int (value));
		break;
	case PROP_ENABLE_THUMBNAILS:
		tracker_config_set_enable_thumbnails (TRACKER_CONFIG (object),
						      g_value_get_boolean (value));
		break;
	case PROP_DISABLED_MODULES:
		tracker_config_set_disabled_modules (TRACKER_CONFIG (object),
						     g_value_get_pointer (value));
		break;
	case PROP_DISABLE_INDEXING_ON_BATTERY:
		tracker_config_set_disable_indexing_on_battery (TRACKER_CONFIG (object),
								g_value_get_boolean (value));
		break;
	case PROP_DISABLE_INDEXING_ON_BATTERY_INIT:
		tracker_config_set_disable_indexing_on_battery_init (TRACKER_CONFIG (object),
								     g_value_get_boolean (value));
		break;
	case PROP_LOW_DISK_SPACE_LIMIT:
		tracker_config_set_low_disk_space_limit (TRACKER_CONFIG (object),
							 g_value_get_int (value));
		break;
	case PROP_INDEX_MOUNTED_DIRECTORIES:
		tracker_config_set_index_mounted_directories (TRACKER_CONFIG (object),
							      g_value_get_boolean (value));
		break;
	case PROP_INDEX_REMOVABLE_DEVICES:
		tracker_config_set_index_removable_devices (TRACKER_CONFIG (object),
								g_value_get_boolean (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
config_get_property (GObject	*object,
		     guint	 param_id,
		     GValue	*value,
		     GParamSpec *pspec)
{
	TrackerConfigPrivate *priv;

	priv = TRACKER_CONFIG_GET_PRIVATE (object);

	switch (param_id) {
		/* General */
	case PROP_VERBOSITY:
		g_value_set_int (value, priv->verbosity);
		break;
	case PROP_INITIAL_SLEEP:
		g_value_set_int (value, priv->initial_sleep);
		break;

		/* Watches */
	case PROP_WATCH_DIRECTORY_ROOTS:
		g_value_set_pointer (value, priv->watch_directory_roots);
		break;
	case PROP_CRAWL_DIRECTORY_ROOTS:
		g_value_set_pointer (value, priv->crawl_directory_roots);
		break;
	case PROP_NO_WATCH_DIRECTORY_ROOTS:
		g_value_set_pointer (value, priv->no_watch_directory_roots);
		break;
	case PROP_ENABLE_WATCHES:
		g_value_set_boolean (value, priv->enable_watches);
		break;

		/* Indexing */
	case PROP_THROTTLE:
		g_value_set_int (value, priv->throttle);
		break;
	case PROP_ENABLE_THUMBNAILS:
		g_value_set_boolean (value, priv->enable_thumbnails);
		break;
	case PROP_DISABLED_MODULES:
		g_value_set_pointer (value, priv->disabled_modules);
		break;
	case PROP_DISABLE_INDEXING_ON_BATTERY:
		g_value_set_boolean (value, priv->disable_indexing_on_battery);
		break;
	case PROP_DISABLE_INDEXING_ON_BATTERY_INIT:
		g_value_set_boolean (value, priv->disable_indexing_on_battery_init);
		break;
	case PROP_LOW_DISK_SPACE_LIMIT:
		g_value_set_int (value, priv->low_disk_space_limit);
		break;
	case PROP_INDEX_MOUNTED_DIRECTORIES:
		g_value_set_boolean (value, priv->index_mounted_directories);
		break;
	case PROP_INDEX_REMOVABLE_DEVICES:
		g_value_set_boolean (value, priv->index_removable_devices);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
config_finalize (GObject *object)
{
	TrackerConfigPrivate *priv;

	priv = TRACKER_CONFIG_GET_PRIVATE (object);

	g_slist_foreach (priv->watch_directory_roots, (GFunc) g_free, NULL);
	g_slist_free (priv->watch_directory_roots);

	g_slist_foreach (priv->crawl_directory_roots, (GFunc) g_free, NULL);
	g_slist_free (priv->crawl_directory_roots);

	g_slist_foreach (priv->no_watch_directory_roots, (GFunc) g_free, NULL);
	g_slist_free (priv->no_watch_directory_roots);

	g_slist_foreach (priv->disabled_modules, (GFunc) g_free, NULL);
	g_slist_free (priv->disabled_modules);

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
						tracker_config_default_int (config, 
									    conversions[i].property));
			break;

		case G_TYPE_BOOLEAN:
			g_key_file_set_boolean (key_file, 
						conversions[i].group, 
						conversions[i].key, 
						tracker_config_default_boolean (config, 
										conversions[i].property));
			break;

		case G_TYPE_POINTER:
			/* Special case string lists */
			if (g_strcmp0 (conversions[i].property, "watch-directory-roots") == 0) {
				const gchar *string_list[] = { NULL, NULL };

				string_list[0] = g_get_home_dir ();

				g_key_file_set_string_list (key_file, 
							    conversions[i].group, 
							    conversions[i].key, 
							    string_list, 
							    G_N_ELEMENTS (string_list));
			} else {
				const gchar *string_list[] = { NULL };

				g_key_file_set_string_list (key_file, 
							    conversions[i].group, 
							    conversions[i].key, 
							    string_list, 
							    G_N_ELEMENTS (string_list));
			}

			break;

		default:
			g_assert_not_reached ();
		}

		g_key_file_set_comment (key_file, 
					conversions[i].group, 
					conversions[i].key, 
					tracker_config_blurb (config,
							      conversions[i].property), 
					NULL);
	}
}

static void
config_load (TrackerConfig *config)
{
	TrackerConfigManager *manager;
	gint i;

	manager = TRACKER_CONFIG_MANAGER (config);
	config_create_with_defaults (config, manager->key_file, FALSE);

	if (!manager->file_exists) {
		tracker_config_manager_save (manager);
	}

	for (i = 0; i < G_N_ELEMENTS (conversions); i++) {
		gboolean has_key;
		gboolean is_directory_list;
		
		has_key = g_key_file_has_key (manager->key_file, 
					      conversions[i].group, 
					      conversions[i].key, 
					      NULL);
	
		switch (conversions[i].type) {
		case G_TYPE_INT:
			tracker_config_load_int (G_OBJECT (manager), 
						 conversions[i].property,
						 manager->key_file,
						 conversions[i].group, 
						 conversions[i].key);
			break;

		case G_TYPE_BOOLEAN:
			tracker_config_load_boolean (G_OBJECT (manager), 
						     conversions[i].property,
						     manager->key_file,
						     conversions[i].group, 
						     conversions[i].key);
			break;

		case G_TYPE_POINTER:
			if (g_strcmp0 (conversions[i].property, "disabled-modules") == 0) {
				is_directory_list = FALSE;
			} else {
				is_directory_list = TRUE;
			}

			tracker_config_load_string_list (G_OBJECT (manager), 
							 conversions[i].property,
							 manager->key_file, 
							 conversions[i].group, 
							 conversions[i].key,
							 is_directory_list);
			break;
		}
	}
}

static gboolean
config_save (TrackerConfig *config)
{
	TrackerConfigManager *manager;
	gint i;

	manager = TRACKER_CONFIG_MANAGER (config);

	if (!manager->key_file) {
		g_critical ("Could not save config, GKeyFile was NULL, has the config been loaded?");

		return FALSE;
	}

	g_message ("Setting details to GKeyFile object...");

	for (i = 0; i < G_N_ELEMENTS (conversions); i++) {
		switch (conversions[i].type) {
		case G_TYPE_INT:
			tracker_config_save_int (manager,
						 conversions[i].property, 
						 manager->key_file,
						 conversions[i].group, 
						 conversions[i].key);
			break;

		case G_TYPE_BOOLEAN:
			tracker_config_save_boolean (manager,
						     conversions[i].property, 
						     manager->key_file,
						     conversions[i].group, 
						     conversions[i].key);
			break;

		case G_TYPE_POINTER:
			tracker_config_save_string_list (manager,
							 conversions[i].property, 
							 manager->key_file,
							 conversions[i].group, 
							 conversions[i].key);
			break;

		default:
			g_assert_not_reached ();
			break;
		}
	}

	return tracker_config_manager_save (manager);
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

gint
tracker_config_get_initial_sleep (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INITIAL_SLEEP);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->initial_sleep;
}

GSList *
tracker_config_get_watch_directory_roots (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->watch_directory_roots;
}

GSList *
tracker_config_get_crawl_directory_roots (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->crawl_directory_roots;
}

GSList *
tracker_config_get_no_watch_directory_roots (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->no_watch_directory_roots;
}

gboolean
tracker_config_get_enable_watches (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_WATCHES);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->enable_watches;
}

gint
tracker_config_get_throttle (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_THROTTLE);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->throttle;
}

gboolean
tracker_config_get_enable_thumbnails (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_THUMBNAILS);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->enable_thumbnails;
}

GSList *
tracker_config_get_disabled_modules (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->disabled_modules;
}

gboolean
tracker_config_get_disable_indexing_on_battery (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_DISABLE_INDEXING_ON_BATTERY);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->disable_indexing_on_battery;
}

gboolean
tracker_config_get_disable_indexing_on_battery_init (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_DISABLE_INDEXING_ON_BATTERY_INIT);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->disable_indexing_on_battery_init;
}

gint
tracker_config_get_low_disk_space_limit (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_LOW_DISK_SPACE_LIMIT);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->low_disk_space_limit;
}

gboolean
tracker_config_get_index_mounted_directories (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_MOUNTED_DIRECTORIES);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->index_mounted_directories;
}

gboolean
tracker_config_get_index_removable_devices (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_REMOVABLE_DEVICES);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->index_removable_devices;
}

void
tracker_config_set_verbosity (TrackerConfig *config,
			      gint	     value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!tracker_config_validate_int (config, "verbosity", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->verbosity = value;
	g_object_notify (G_OBJECT (config), "verbosity");
}

void
tracker_config_set_initial_sleep (TrackerConfig *config,
				  gint		 value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!tracker_config_validate_int (config, "initial-sleep", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->initial_sleep = value;
	g_object_notify (G_OBJECT (config), "initial-sleep");
}

void
tracker_config_set_enable_watches (TrackerConfig *config,
				   gboolean	  value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->enable_watches = value;
	g_object_notify (G_OBJECT (config), "enable-watches");
}

void
tracker_config_set_throttle (TrackerConfig *config,
			     gint	    value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!tracker_config_validate_int (config, "throttle", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->throttle = value;
	g_object_notify (G_OBJECT (config), "throttle");
}

void
tracker_config_set_enable_thumbnails (TrackerConfig *config,
				      gboolean	     value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->enable_thumbnails = value;
	g_object_notify (G_OBJECT (config), "enable-thumbnails");
}

void
tracker_config_set_disable_indexing_on_battery (TrackerConfig *config,
						gboolean       value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->disable_indexing_on_battery = value;
	g_object_notify (G_OBJECT (config), "disable-indexing-on-battery");
}

void
tracker_config_set_disable_indexing_on_battery_init (TrackerConfig *config,
						     gboolean	    value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->disable_indexing_on_battery_init = value;
	g_object_notify (G_OBJECT (config), "disable-indexing-on-battery-init");
}

void
tracker_config_set_low_disk_space_limit (TrackerConfig *config,
					 gint		value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!tracker_config_validate_int (config, "low-disk-space-limit", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->low_disk_space_limit = value;
	g_object_notify (G_OBJECT (config), "low-disk-space-limit");
}

void
tracker_config_set_index_mounted_directories (TrackerConfig *config,
					      gboolean	     value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->index_mounted_directories = value;
	g_object_notify (G_OBJECT (config), "index-mounted-directories");
}

void
tracker_config_set_index_removable_devices (TrackerConfig *config,
					  gboolean	 value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->index_removable_devices = value;
	g_object_notify (G_OBJECT (config), "index-removable-devices");
}

void	       
tracker_config_set_watch_directory_roots (TrackerConfig *config,
					  GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList               *l;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	l = priv->watch_directory_roots;

	if (!roots) {
		priv->watch_directory_roots = NULL;
	} else {
		priv->watch_directory_roots = tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "watch-directory-roots");
}

void	       
tracker_config_set_crawl_directory_roots (TrackerConfig *config,
					  GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList               *l;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	l = priv->crawl_directory_roots;

	if (!roots) {
		priv->crawl_directory_roots = NULL;
	} else {
		priv->crawl_directory_roots = tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "crawl-directory-roots");
}

void	       
tracker_config_set_no_watch_directory_roots (TrackerConfig *config,
					     GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList               *l;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);
	
	l = priv->no_watch_directory_roots;

	if (!roots) {
		priv->no_watch_directory_roots = NULL;
	} else {
		priv->no_watch_directory_roots = tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "no-watch-directory-roots");
}

void	       
tracker_config_set_disabled_modules (TrackerConfig *config,
				     GSList        *modules)
{
	TrackerConfigPrivate *priv;
	GSList               *l;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	l = priv->disabled_modules;

	if (!modules) {
		priv->disabled_modules = NULL;
	} else {
		priv->disabled_modules = tracker_gslist_copy_with_string_data (modules);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "disabled-modules");
}
