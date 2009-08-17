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

#include <libtracker-common/tracker-keyfile-object.h>
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
#define DEFAULT_ENABLE_MONITORS			 TRUE
#define DEFAULT_THROTTLE			 0	  /* 0->20 */
#define DEFAULT_SCAN_TIMEOUT			 0	  /* 0->1000 */
#define DEFAULT_CACHE_TIMEOUT			 60	  /* 0->1000 */
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

	/* Monitors */
	gboolean  enable_monitors;
	GSList   *monitor_directories;
	GSList   *monitor_directories_ignored;
	GSList   *monitor_recurse_directories;
	gint      scan_timeout;
	gint      cache_timeout;

	/* Indexing */
	gint	  throttle;
	gboolean  enable_thumbnails;
	gboolean  disable_indexing_on_battery;
	gboolean  disable_indexing_on_battery_init;
	gint	  low_disk_space_limit;
	GSList	 *index_directories;
	GSList   *ignored_directories;
	GSList   *ignored_directories_with_content;
	GSList   *ignored_files;
	GSList   *ignored_directory_patterns;
	GSList   *ignored_file_patterns;
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

	/* Monitors */
	PROP_ENABLE_MONITORS,
	PROP_MONITOR_DIRECTORIES,
	PROP_MONITOR_DIRECTORIES_IGNORED,
	PROP_MONITOR_RECURSE_DIRECTORIES,
	PROP_SCAN_TIMEOUT,
	PROP_CACHE_TIMEOUT,

	/* Indexing */
	PROP_THROTTLE,
	PROP_ENABLE_THUMBNAILS,
	PROP_DISABLE_INDEXING_ON_BATTERY,
	PROP_DISABLE_INDEXING_ON_BATTERY_INIT,
	PROP_LOW_DISK_SPACE_LIMIT,
	PROP_INDEX_DIRECTORIES,
	PROP_IGNORED_DIRECTORIES,
	PROP_IGNORED_DIRECTORIES_WITH_CONTENT,
	PROP_IGNORED_FILES,
	PROP_INDEX_MOUNTED_DIRECTORIES,
	PROP_INDEX_REMOVABLE_DEVICES,
};

static ObjectToKeyFile conversions[] = {
	/* General */
	{ G_TYPE_INT,     "verbosity",                        GROUP_GENERAL,  "Verbosity"                 },
	{ G_TYPE_INT,     "initial-sleep",                    GROUP_GENERAL,  "InitialSleep"              },
	/* Monitors */
	{ G_TYPE_BOOLEAN, "enable-monitors",                  GROUP_MONITORS, "EnableMonitors"            },
	{ G_TYPE_POINTER, "monitor-directories",              GROUP_MONITORS, "MonitorDirectories"        },
	{ G_TYPE_POINTER, "monitor-directories-ignored",      GROUP_MONITORS, "MonitorDirectoriesIgnored" },
	{ G_TYPE_POINTER, "monitor-recurse-directories",      GROUP_MONITORS, "MonitorRecurseDirectories" },
	{ G_TYPE_INT,     "scan-timeout",                     GROUP_MONITORS, "ScanTimeout"               },
	{ G_TYPE_INT,     "cache-timeout",                    GROUP_MONITORS, "CacheTimeout"              },
	/* Indexing */
	{ G_TYPE_INT,     "throttle",                         GROUP_INDEXING, "Throttle"                  },
	{ G_TYPE_BOOLEAN, "enable-thumbnails",                GROUP_INDEXING, "EnableThumbnails"          },
	{ G_TYPE_BOOLEAN, "disable-indexing-on-battery",      GROUP_INDEXING, "BatteryIndex"              },
	{ G_TYPE_BOOLEAN, "disable-indexing-on-battery-init", GROUP_INDEXING, "BatteryIndexInitial"       },
	{ G_TYPE_INT,     "low-disk-space-limit",             GROUP_INDEXING, "LowDiskSpaceLimit"         },
	{ G_TYPE_POINTER, "index-directories",                GROUP_INDEXING, "IndexDirectories"          },
	{ G_TYPE_POINTER, "ignored-directories",              GROUP_INDEXING, "IgnoredDirectories"        },
	{ G_TYPE_POINTER, "ignored-directories-with-content", GROUP_INDEXING, "IgnoredDirectoriesWithContent" },
	{ G_TYPE_POINTER, "ignored-files",                    GROUP_INDEXING, "IgnoredFiles"              },
	{ G_TYPE_BOOLEAN, "index-mounted-directories",        GROUP_INDEXING, "IndexMountedDirectories"   },
	{ G_TYPE_BOOLEAN, "index-removable-devices",          GROUP_INDEXING, "IndexRemovableMedia"       },
};

G_DEFINE_TYPE (TrackerConfig, tracker_config, TRACKER_TYPE_CONFIG_FILE);

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
					 PROP_ENABLE_MONITORS,
					 g_param_spec_boolean ("enable-monitors",
							       "Enable monitors",
							       " Set to false to completely disable any monitoring",
							       DEFAULT_ENABLE_MONITORS,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_MONITOR_DIRECTORIES,
					 g_param_spec_pointer ("monitor-directories",
							       "Monitor directories",
							       " List of directories to monitor for changes (separator=;)",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_MONITOR_DIRECTORIES_IGNORED,
					 g_param_spec_pointer ("monitor-directories-ignored",
							       "Monitor directories ignored",
							       " List of directories to NOT monitor for changes (separator=;)",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_MONITOR_RECURSE_DIRECTORIES,
					 g_param_spec_pointer ("monitor-recurse-directories",
							       "Monitor recurse directories",
							       " List of directories to monitor recursively for changes (separator=;)",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_SCAN_TIMEOUT,
					 g_param_spec_int ("scan-timeout",
							   "Scan Timeout",
							   " Time in seconds between same events to prevent flooding (0->1000)",
							   0,
							   1000,
							   DEFAULT_SCAN_TIMEOUT,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_CACHE_TIMEOUT,
					 g_param_spec_int ("cache-timeout",
							   "Scan Timeout",
							   " Time in seconds for events to be cached (0->1000)",
							   0,
							   1000,
							   DEFAULT_CACHE_TIMEOUT,
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
					 PROP_INDEX_DIRECTORIES,
					 g_param_spec_pointer ("index-directories",
							       "Index directories",
							       " List of directories to crawl for indexing (separator=;)",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_IGNORED_DIRECTORIES,
					 g_param_spec_pointer ("ignored-directories",
							       "Ignored directories",
							       " List of directories to NOT crawl for indexing (separator=;)",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_IGNORED_DIRECTORIES_WITH_CONTENT,
					 g_param_spec_pointer ("ignored-directories-with-content",
							       "Ignored directories with content",
							       " List of directories to NOT crawl for indexing based on child files (separator=;)",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_IGNORED_FILES,
					 g_param_spec_pointer ("ignored-files",
							       "Ignored files",
							       " List of files to NOT index (separator=;)",
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

		/* Monitors */
	case PROP_ENABLE_MONITORS:
		tracker_config_set_enable_monitors (TRACKER_CONFIG (object),
						    g_value_get_boolean (value));
		break;
	case PROP_SCAN_TIMEOUT:
		tracker_config_set_scan_timeout (TRACKER_CONFIG (object),
						 g_value_get_int (value));
		break;
	case PROP_CACHE_TIMEOUT:
		tracker_config_set_cache_timeout (TRACKER_CONFIG (object),
						  g_value_get_int (value));
		break;
	case PROP_MONITOR_DIRECTORIES:    
		tracker_config_set_monitor_directories (TRACKER_CONFIG (object),
							g_value_get_pointer (value));
		break;
	case PROP_MONITOR_DIRECTORIES_IGNORED:    
		tracker_config_set_monitor_directories_ignored (TRACKER_CONFIG (object),
								g_value_get_pointer (value));
		break;
	case PROP_MONITOR_RECURSE_DIRECTORIES:    
		tracker_config_set_monitor_recurse_directories (TRACKER_CONFIG (object),
								g_value_get_pointer (value));
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
	case PROP_INDEX_DIRECTORIES:    
		tracker_config_set_index_directories (TRACKER_CONFIG (object),
						      g_value_get_pointer (value));
		break;
	case PROP_IGNORED_DIRECTORIES:    
		tracker_config_set_ignored_directories (TRACKER_CONFIG (object),
							g_value_get_pointer (value));
		break;
	case PROP_IGNORED_DIRECTORIES_WITH_CONTENT:    
		tracker_config_set_ignored_directories_with_content (TRACKER_CONFIG (object),
								     g_value_get_pointer (value));
		break;
	case PROP_IGNORED_FILES:    
		tracker_config_set_ignored_files (TRACKER_CONFIG (object),
						  g_value_get_pointer (value));
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

		/* Montors */
	case PROP_ENABLE_MONITORS:
		g_value_set_boolean (value, priv->enable_monitors);
		break;
	case PROP_SCAN_TIMEOUT:
		g_value_set_int (value, priv->scan_timeout);
		break;
	case PROP_CACHE_TIMEOUT:
		g_value_set_int (value, priv->cache_timeout);
		break;
	case PROP_MONITOR_DIRECTORIES:
		g_value_set_pointer (value, priv->monitor_directories);
		break;
	case PROP_MONITOR_DIRECTORIES_IGNORED:
		g_value_set_pointer (value, priv->monitor_directories_ignored);
		break;
	case PROP_MONITOR_RECURSE_DIRECTORIES:
		g_value_set_pointer (value, priv->monitor_recurse_directories);
		break;

		/* Indexing */
	case PROP_THROTTLE:
		g_value_set_int (value, priv->throttle);
		break;
	case PROP_ENABLE_THUMBNAILS:
		g_value_set_boolean (value, priv->enable_thumbnails);
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
	case PROP_INDEX_DIRECTORIES:
		g_value_set_pointer (value, priv->index_directories);
		break;
	case PROP_IGNORED_DIRECTORIES:
		g_value_set_pointer (value, priv->ignored_directories);
		break;
	case PROP_IGNORED_DIRECTORIES_WITH_CONTENT:
		g_value_set_pointer (value, priv->ignored_directories_with_content);
		break;
	case PROP_IGNORED_FILES:
		g_value_set_pointer (value, priv->ignored_files);
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

	g_slist_foreach (priv->ignored_file_patterns,
			 (GFunc) g_pattern_spec_free,
			 NULL);
	g_slist_free (priv->ignored_file_patterns);

	g_slist_foreach (priv->ignored_directory_patterns,
			 (GFunc) g_pattern_spec_free,
			 NULL);
	g_slist_free (priv->ignored_directory_patterns);

	g_slist_foreach (priv->ignored_files, (GFunc) g_free, NULL);
	g_slist_free (priv->ignored_files);

	g_slist_foreach (priv->ignored_directories_with_content, (GFunc) g_free, NULL);
	g_slist_free (priv->ignored_directories_with_content);

	g_slist_foreach (priv->ignored_directories, (GFunc) g_free, NULL);
	g_slist_free (priv->ignored_directories);

	g_slist_foreach (priv->index_directories, (GFunc) g_free, NULL);
	g_slist_free (priv->index_directories);

	g_slist_foreach (priv->monitor_recurse_directories, (GFunc) g_free, NULL);
	g_slist_free (priv->monitor_recurse_directories);

	g_slist_foreach (priv->monitor_directories_ignored, (GFunc) g_free, NULL);
	g_slist_free (priv->monitor_directories_ignored);

	g_slist_foreach (priv->monitor_directories, (GFunc) g_free, NULL);
	g_slist_free (priv->monitor_directories);

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

		case G_TYPE_BOOLEAN:
			g_key_file_set_boolean (key_file, 
						conversions[i].group, 
						conversions[i].key, 
						tracker_keyfile_object_default_boolean (config, 
											conversions[i].property));
			break;

		case G_TYPE_POINTER:
			/* Special case string lists */
			if (g_strcmp0 (conversions[i].property, "index-directories") == 0) {
				const gchar *string_list[] = { NULL, NULL };

				string_list[0] = g_get_home_dir ();

				g_key_file_set_string_list (key_file, 
							    conversions[i].group, 
							    conversions[i].key, 
							    string_list, 
							    G_N_ELEMENTS (string_list));
			} else if (g_strcmp0 (conversions[i].property, "ignored-directories") == 0) {
				const gchar *string_list[] = { 
					"po", "CVS", ".svn", ".git", "core-dumps",
					NULL
				};

				g_key_file_set_string_list (key_file, 
							    conversions[i].group, 
							    conversions[i].key, 
							    string_list, 
							    G_N_ELEMENTS (string_list));
			} else if (g_strcmp0 (conversions[i].property, "ignored-directories-with-content") == 0) {
				const gchar *string_list[] = { 
					"backup.metadata",
					NULL
				};

				g_key_file_set_string_list (key_file, 
							    conversions[i].group, 
							    conversions[i].key, 
							    string_list, 
							    G_N_ELEMENTS (string_list));
			} else if (g_strcmp0 (conversions[i].property, "ignored-files") == 0) {
				const gchar *string_list[] = { 
					"*~", "*.o", "*.la", "*.lo", "*.loT", "*.in",
					"*.csproj", "*.m4", "*.rej", "*.gmo", "*.orig",
					"*.pc",	"*.omf", "*.aux", "*.tmp", "*.po",
					"*.vmdk", "*.vm*", "*.nvram", "*.part",
					"*.rcore", "lzo", "autom4te", "conftest",
					"confstat", "Makefile",	"SCCS",	"litmain.sh",
					"libtool", "config.status", "confdefs.h",
					NULL
				};

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
					tracker_keyfile_object_blurb (config,
								      conversions[i].property), 
					NULL);
	}
}

static void
config_set_ignored_file_patterns (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;
	GPatternSpec *spec;
	GSList *l;
	GSList *patterns = NULL;

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	g_slist_foreach (priv->ignored_file_patterns,
			 (GFunc) g_pattern_spec_free,
			 NULL);
	g_slist_free (priv->ignored_file_patterns);

	for (l = priv->ignored_files; l; l = l->next) {
		/* g_message ("  Adding file ignore pattern:'%s'", */
		/* 	   (gchar *) l->data); */
		spec = g_pattern_spec_new (l->data);
		patterns = g_slist_prepend (patterns, spec);
	}

	priv->ignored_file_patterns = g_slist_reverse (patterns);
}

static void
config_set_ignored_directory_patterns (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;
	GPatternSpec *spec;
	GSList *l;
	GSList *patterns = NULL;

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	g_slist_foreach (priv->ignored_directory_patterns,
			 (GFunc) g_pattern_spec_free,
			 NULL);
	g_slist_free (priv->ignored_directory_patterns);

	for (l = priv->ignored_directories; l; l = l->next) {
		/* g_message ("  Adding directory ignore pattern:'%s'", */
		/* 	   (gchar *) l->data); */
		spec = g_pattern_spec_new (l->data);
		patterns = g_slist_prepend (patterns, spec);
	}

	priv->ignored_directory_patterns = g_slist_reverse (patterns);
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
		gboolean is_directory_list;
		
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

		case G_TYPE_POINTER:
			if (strcmp (conversions[i].property, "ignored-files") == 0) {
				is_directory_list = FALSE;
			} else {
				is_directory_list = TRUE;
			}

			tracker_keyfile_object_load_string_list (G_OBJECT (file), 
								 conversions[i].property,
								 file->key_file, 
								 conversions[i].group, 
								 conversions[i].key,
								 is_directory_list);
			break;
		}
	}

	config_set_ignored_file_patterns (config);
	config_set_ignored_directory_patterns (config);
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

		case G_TYPE_BOOLEAN:
			tracker_keyfile_object_save_boolean (file,
							     conversions[i].property, 
							     file->key_file,
							     conversions[i].group, 
							     conversions[i].key);
			break;

		case G_TYPE_POINTER:
			tracker_keyfile_object_save_string_list (file,
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

gint
tracker_config_get_initial_sleep (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INITIAL_SLEEP);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->initial_sleep;
}

gboolean
tracker_config_get_enable_monitors (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_MONITORS);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->enable_monitors;
}

GSList *
tracker_config_get_monitor_directories (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->monitor_directories;
}

GSList *
tracker_config_get_monitor_directories_ignored (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->monitor_directories_ignored;
}

GSList *
tracker_config_get_monitor_recurse_directories (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->monitor_recurse_directories;
}

gint
tracker_config_get_scan_timeout (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_SCAN_TIMEOUT);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->scan_timeout;
}

gint
tracker_config_get_cache_timeout (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_CACHE_TIMEOUT);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->cache_timeout;
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

	if (!tracker_keyfile_object_validate_int (config, "verbosity", value)) {
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

	if (!tracker_keyfile_object_validate_int (config, "initial-sleep", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->initial_sleep = value;
	g_object_notify (G_OBJECT (config), "initial-sleep");
}

void
tracker_config_set_enable_monitors (TrackerConfig *config,
				    gboolean       value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->enable_monitors = value;
	g_object_notify (G_OBJECT (config), "enable-monitors");
}

void
tracker_config_set_scan_timeout (TrackerConfig *config,
				 gint           value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!tracker_keyfile_object_validate_int (config, "scan-timeout", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->scan_timeout = value;
	g_object_notify (G_OBJECT (config), "scan-timeout");
}

void
tracker_config_set_cache_timeout (TrackerConfig *config,
				  gint           value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!tracker_keyfile_object_validate_int (config, "cache-timeout", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->cache_timeout = value;
	g_object_notify (G_OBJECT (config), "cache-timeout");
}

void	       
tracker_config_set_monitor_directories (TrackerConfig *config,
					GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList               *l;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);
	
	l = priv->monitor_directories;

	if (!roots) {
		priv->monitor_directories = NULL;
	} else {
		priv->monitor_directories = 
			tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "monitor-directories");
}

void	       
tracker_config_set_monitor_directories_ignored (TrackerConfig *config,
						GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList               *l;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);
	
	l = priv->monitor_directories_ignored;

	if (!roots) {
		priv->monitor_directories_ignored = NULL;
	} else {
		priv->monitor_directories_ignored = 
			tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "monitor-directories-ignored");
}

void	       
tracker_config_set_monitor_recurse_directories (TrackerConfig *config,
						GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList               *l;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);
	
	l = priv->monitor_recurse_directories;

	if (!roots) {
		priv->monitor_recurse_directories = NULL;
	} else {
		priv->monitor_recurse_directories = 
			tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "monitor-recurse-directories");
}

void
tracker_config_set_throttle (TrackerConfig *config,
			     gint	    value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!tracker_keyfile_object_validate_int (config, "throttle", value)) {
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

	if (!tracker_keyfile_object_validate_int (config, "low-disk-space-limit", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->low_disk_space_limit = value;
	g_object_notify (G_OBJECT (config), "low-disk-space-limit");
}


void	       
tracker_config_set_index_directories (TrackerConfig *config,
				      GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList               *l;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);
	
	l = priv->index_directories;

	if (!roots) {
		priv->index_directories = NULL;
	} else {
		priv->index_directories = 
			tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "index-directories");
}

void	       
tracker_config_set_ignored_directories (TrackerConfig *config,
					GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList               *l;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);
	
	l = priv->ignored_directories;

	if (!roots) {
		priv->ignored_directories = NULL;
	} else {
		priv->ignored_directories = 
			tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "ignored-directories");
}

void	       
tracker_config_set_ignored_directories_with_content (TrackerConfig *config,
						     GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList               *l;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);
	
	l = priv->ignored_directories_with_content;

	if (!roots) {
		priv->ignored_directories_with_content = NULL;
	} else {
		priv->ignored_directories_with_content = 
			tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "ignored-directories-with-content");
}

void	       
tracker_config_set_ignored_files (TrackerConfig *config,
				  GSList        *files)
{
	TrackerConfigPrivate *priv;
	GSList               *l;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);
	
	l = priv->ignored_files;

	if (!files) {
		priv->ignored_files = NULL;
	} else {
		priv->ignored_files = 
			tracker_gslist_copy_with_string_data (files);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "ignored-files");
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
