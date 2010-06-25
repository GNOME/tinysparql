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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.          See the GNU
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
#define GROUP_GENERAL                            "General"
#define GROUP_MONITORS                           "Monitors"
#define GROUP_INDEXING                           "Indexing"
#define GROUP_CRAWLING                           "Crawling"

/* Default values */
#define DEFAULT_VERBOSITY                        0
#define DEFAULT_INITIAL_SLEEP                    15       /* 0->1000 */
#define DEFAULT_ENABLE_MONITORS                  TRUE
#define DEFAULT_THROTTLE                         0        /* 0->20 */
#define DEFAULT_SCAN_TIMEOUT                     0        /* 0->1000 */
#define DEFAULT_CACHE_TIMEOUT                    60       /* 0->1000 */
#define DEFAULT_INDEX_REMOVABLE_DEVICES          TRUE
#define DEFAULT_INDEX_OPTICAL_DISCS              FALSE
#define DEFAULT_INDEX_ON_BATTERY                 FALSE
#define DEFAULT_INDEX_ON_BATTERY_FIRST_TIME      TRUE
#define DEFAULT_LOW_DISK_SPACE_LIMIT             1        /* 0->100 / -1 */
#define DEFAULT_CRAWLING_INTERVAL                0        /* 0->365 / -1 */

typedef struct {
	/* General */
	gint      verbosity;
	gint      initial_sleep;

	/* Monitors */
	gboolean  enable_monitors;
	gint      scan_timeout;
	gint      cache_timeout;

	/* Indexing */
	gint      throttle;
	gboolean  index_on_battery;
	gboolean  index_on_battery_first_time;
	gboolean  index_removable_devices;
	gboolean  index_optical_discs;
	gint      low_disk_space_limit;
	GSList   *index_recursive_directories;
	GSList	 *index_recursive_directories_unfiltered;
	GSList   *index_single_directories;
	GSList	 *index_single_directories_unfiltered;
	GSList   *ignored_directories;
	GSList   *ignored_directories_with_content;
	GSList   *ignored_files;
	gint	  crawling_interval;

	/* Convenience data */
	GSList   *ignored_directory_patterns;
	GSList   *ignored_directory_paths;
	GSList   *ignored_file_patterns;
	GSList   *ignored_file_paths;
} TrackerConfigPrivate;

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
static void     config_finalize             (GObject           *object);
static void     config_constructed          (GObject           *object);
static void     config_changed              (TrackerConfigFile *file);
static void     config_load                 (TrackerConfig     *config,
                                             gboolean           use_defaults);
static gboolean config_save                 (TrackerConfig     *config);
static void     config_create_with_defaults (TrackerConfig     *config,
                                             GKeyFile          *key_file,
                                             gboolean           overwrite);

enum {
	PROP_0,

	/* General */
	PROP_VERBOSITY,
	PROP_INITIAL_SLEEP,

	/* Monitors */
	PROP_ENABLE_MONITORS,
	PROP_SCAN_TIMEOUT,
	PROP_CACHE_TIMEOUT,

	/* Indexing */
	PROP_THROTTLE,
	PROP_INDEX_ON_BATTERY,
	PROP_INDEX_ON_BATTERY_FIRST_TIME,
	PROP_INDEX_REMOVABLE_DEVICES,
	PROP_INDEX_OPTICAL_DISCS,
	PROP_LOW_DISK_SPACE_LIMIT,
	PROP_INDEX_RECURSIVE_DIRECTORIES,
	PROP_INDEX_SINGLE_DIRECTORIES,
	PROP_IGNORED_DIRECTORIES,
	PROP_IGNORED_DIRECTORIES_WITH_CONTENT,
	PROP_IGNORED_FILES,
	PROP_CRAWLING_INTERVAL
};

static ObjectToKeyFile conversions[] = {
	/* General */
	{ G_TYPE_INT,     "verbosity",                        GROUP_GENERAL,  "Verbosity"                 },
	{ G_TYPE_INT,     "initial-sleep",                    GROUP_GENERAL,  "InitialSleep"              },
	/* Monitors */
	{ G_TYPE_BOOLEAN, "enable-monitors",                  GROUP_MONITORS, "EnableMonitors"            },
	{ G_TYPE_INT,     "scan-timeout",                     GROUP_MONITORS, "ScanTimeout"               },
	{ G_TYPE_INT,     "cache-timeout",                    GROUP_MONITORS, "CacheTimeout"              },
	/* Indexing */
	{ G_TYPE_INT,     "throttle",                         GROUP_INDEXING, "Throttle"                  },
	{ G_TYPE_BOOLEAN, "index-on-battery",                 GROUP_INDEXING, "IndexOnBattery"            },
	{ G_TYPE_BOOLEAN, "index-on-battery-first-time",      GROUP_INDEXING, "IndexOnBatteryFirstTime"   },
	{ G_TYPE_BOOLEAN, "index-removable-devices",          GROUP_INDEXING, "IndexRemovableMedia"       },
	{ G_TYPE_BOOLEAN, "index-optical-discs",              GROUP_INDEXING, "IndexOpticalDiscs"         },
	{ G_TYPE_INT,     "low-disk-space-limit",             GROUP_INDEXING, "LowDiskSpaceLimit"         },
	{ G_TYPE_POINTER, "index-recursive-directories",      GROUP_INDEXING, "IndexRecursiveDirectories" },
	{ G_TYPE_POINTER, "index-single-directories",         GROUP_INDEXING, "IndexSingleDirectories"    },
	{ G_TYPE_POINTER, "ignored-directories",              GROUP_INDEXING, "IgnoredDirectories"        },
	{ G_TYPE_POINTER, "ignored-directories-with-content", GROUP_INDEXING, "IgnoredDirectoriesWithContent" },
	{ G_TYPE_POINTER, "ignored-files",                    GROUP_INDEXING, "IgnoredFiles"              },
	{ G_TYPE_INT,	  "crawling-interval",		      GROUP_INDEXING, "CrawlingInterval"	  }
};

G_DEFINE_TYPE (TrackerConfig, tracker_config, TRACKER_TYPE_CONFIG_FILE);

static void
tracker_config_class_init (TrackerConfigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerConfigFileClass *config_file_class = TRACKER_CONFIG_FILE_CLASS (klass);

	object_class->set_property = config_set_property;
	object_class->get_property = config_get_property;
	object_class->finalize     = config_finalize;
	object_class->constructed  = config_constructed;

	config_file_class->changed = config_changed;

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
	                                 PROP_INDEX_ON_BATTERY,
	                                 g_param_spec_boolean ("index-on-battery",
	                                                       "Index on battery",
	                                                       " Set to true to index while running on battery",
	                                                       DEFAULT_INDEX_ON_BATTERY,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_INDEX_ON_BATTERY_FIRST_TIME,
	                                 g_param_spec_boolean ("index-on-battery-first-time",
	                                                       "Index on battery first time",
	                                                       " Set to true to index while running on battery for the first time only",
	                                                       DEFAULT_INDEX_ON_BATTERY_FIRST_TIME,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_INDEX_REMOVABLE_DEVICES,
	                                 g_param_spec_boolean ("index-removable-devices",
	                                                       "index removable devices",
	                                                       " Set to true to enable traversing mounted directories for removable devices\n"
	                                                       " (this includes optical discs)",
	                                                       DEFAULT_INDEX_REMOVABLE_DEVICES,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_INDEX_OPTICAL_DISCS,
	                                 g_param_spec_boolean ("index-optical-discs",
	                                                       "index optical discs",
	                                                       " Set to true to enable traversing CDs, DVDs, and generally optical media\n"
	                                                       " (if removable devices are not indexed, optical discs won't be either)",
	                                                       DEFAULT_INDEX_OPTICAL_DISCS,
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
	                                 PROP_INDEX_RECURSIVE_DIRECTORIES,
	                                 g_param_spec_pointer ("index-recursive-directories",
	                                                       "Index recursive directories",
	                                                       " List of directories to crawl recursively for indexing (separator=;)\n"
	                                                       " Special values include: (see /etc/xdg/user-dirs.defaults & $HOME/.config/user-dirs.default)\n"
	                                                       "   &DESKTOP\n"
	                                                       "   &DOCUMENTS\n"
	                                                       "   &DOWNLOAD\n"
	                                                       "   &MUSIC\n"
	                                                       "   &PICTURES\n"
	                                                       "   &PUBLIC_SHARE\n"
	                                                       "   &TEMPLATES\n"
	                                                       "   &VIDEOS\n"
	                                                       " If $HOME is the default below, it is because $HOME/.config/user-dirs.default was missing.",
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_INDEX_SINGLE_DIRECTORIES,
	                                 g_param_spec_pointer ("index-single-directories",
	                                                       "Index single directories",
	                                                       " List of directories to index but not sub-directories for changes (separator=;)\n"
	                                                       " Special values used for IndexRecursiveDirectories can also be used here",
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
                                         PROP_CRAWLING_INTERVAL,
	                                 g_param_spec_int ("crawling-interval",
	                                                   "Crawling interval",
                                                           " Interval in days to check the filesystem is up to date in the database."
                                                           " If set to 0, crawling always occurs on startup, if -1 crawling is"
                                                           " disabled entirely. Maximum is 365.",
	                                                   -1,
	                                                   365,
	                                                   DEFAULT_CRAWLING_INTERVAL,
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
                     GParamSpec           *pspec)
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

		/* Indexing */
	case PROP_THROTTLE:
		tracker_config_set_throttle (TRACKER_CONFIG (object),
		                             g_value_get_int (value));
		break;
	case PROP_INDEX_ON_BATTERY:
		tracker_config_set_index_on_battery (TRACKER_CONFIG (object),
		                                     g_value_get_boolean (value));
		break;
	case PROP_INDEX_ON_BATTERY_FIRST_TIME:
		tracker_config_set_index_on_battery_first_time (TRACKER_CONFIG (object),
		                                                g_value_get_boolean (value));
		break;
	case PROP_INDEX_REMOVABLE_DEVICES:
		tracker_config_set_index_removable_devices (TRACKER_CONFIG (object),
		                                            g_value_get_boolean (value));
		break;
	case PROP_INDEX_OPTICAL_DISCS:
		tracker_config_set_index_optical_discs (TRACKER_CONFIG (object),
		                                        g_value_get_boolean (value));
		break;
	case PROP_LOW_DISK_SPACE_LIMIT:
		tracker_config_set_low_disk_space_limit (TRACKER_CONFIG (object),
		                                         g_value_get_int (value));
		break;
	case PROP_INDEX_RECURSIVE_DIRECTORIES:
		tracker_config_set_index_recursive_directories (TRACKER_CONFIG (object),
		                                                g_value_get_pointer (value));
		break;
	case PROP_INDEX_SINGLE_DIRECTORIES:
		tracker_config_set_index_single_directories (TRACKER_CONFIG (object),
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
	case PROP_CRAWLING_INTERVAL:
		tracker_config_set_crawling_interval (TRACKER_CONFIG (object),
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

		/* Indexing */
	case PROP_THROTTLE:
		g_value_set_int (value, priv->throttle);
		break;
	case PROP_INDEX_ON_BATTERY:
		g_value_set_boolean (value, priv->index_on_battery);
		break;
	case PROP_INDEX_ON_BATTERY_FIRST_TIME:
		g_value_set_boolean (value, priv->index_on_battery_first_time);
		break;
	case PROP_INDEX_REMOVABLE_DEVICES:
		g_value_set_boolean (value, priv->index_removable_devices);
		break;
	case PROP_INDEX_OPTICAL_DISCS:
		g_value_set_boolean (value, priv->index_optical_discs);
		break;
	case PROP_LOW_DISK_SPACE_LIMIT:
		g_value_set_int (value, priv->low_disk_space_limit);
		break;
	case PROP_INDEX_RECURSIVE_DIRECTORIES:
		g_value_set_pointer (value, priv->index_recursive_directories);
		break;
	case PROP_INDEX_SINGLE_DIRECTORIES:
		g_value_set_pointer (value, priv->index_single_directories);
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
	case PROP_CRAWLING_INTERVAL:
		g_value_set_int (value, priv->crawling_interval);
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

	g_slist_foreach (priv->ignored_file_paths,
	                 (GFunc) g_free,
	                 NULL);
	g_slist_free (priv->ignored_file_paths);

	g_slist_foreach (priv->ignored_directory_patterns,
	                 (GFunc) g_pattern_spec_free,
	                 NULL);
	g_slist_free (priv->ignored_directory_patterns);

	g_slist_foreach (priv->ignored_directory_paths,
	                 (GFunc) g_free,
	                 NULL);
	g_slist_free (priv->ignored_directory_paths);

	g_slist_foreach (priv->ignored_files, (GFunc) g_free, NULL);
	g_slist_free (priv->ignored_files);

	g_slist_foreach (priv->ignored_directories_with_content, (GFunc) g_free, NULL);
	g_slist_free (priv->ignored_directories_with_content);

	g_slist_foreach (priv->ignored_directories, (GFunc) g_free, NULL);
	g_slist_free (priv->ignored_directories);

	g_slist_foreach (priv->index_single_directories, (GFunc) g_free, NULL);
	g_slist_free (priv->index_single_directories);

	g_slist_foreach (priv->index_single_directories_unfiltered, (GFunc) g_free, NULL);
	g_slist_free (priv->index_single_directories_unfiltered);

	g_slist_foreach (priv->index_recursive_directories, (GFunc) g_free, NULL);
	g_slist_free (priv->index_recursive_directories);

	g_slist_foreach (priv->index_recursive_directories_unfiltered, (GFunc) g_free, NULL);
	g_slist_free (priv->index_recursive_directories_unfiltered);

	(G_OBJECT_CLASS (tracker_config_parent_class)->finalize) (object);
}

static void
config_constructed (GObject *object)
{
	(G_OBJECT_CLASS (tracker_config_parent_class)->constructed) (object);

	config_load (TRACKER_CONFIG (object), TRUE);
}

static void
config_changed (TrackerConfigFile *file)
{
	/* Reload config */
	config_load (TRACKER_CONFIG (file), FALSE);
}

static void
config_create_with_defaults (TrackerConfig *config,
                             GKeyFile      *key_file,
                             gboolean       overwrite)
{
	gboolean added_home_recursively = FALSE;
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
			if (g_strcmp0 (conversions[i].property, "index-recursive-directories") == 0) {
				/* Since we can't know if
				 * $HOME/.config/user-dirs.dirs exists
				 * or not, we check that the GLib API
				 * returns meaningful data.
				 */
				GUserDirectory dir;
				GSList *paths = NULL;
				GStrv string_list;

				for (dir = 0; dir < G_USER_N_DIRECTORIES; dir++) {
					const gchar *path;

					path = g_get_user_special_dir (dir);
					if (path == NULL || *path == '\0') {
						continue;
					}

					switch (dir) {
					case G_USER_DIRECTORY_DESKTOP:
						path = "&DESKTOP";
						break;
					case G_USER_DIRECTORY_DOCUMENTS:
						path = "&DOCUMENTS";
						break;
					case G_USER_DIRECTORY_DOWNLOAD:
						path = "&DOWNLOAD";
						break;
					case G_USER_DIRECTORY_MUSIC:
						path = "&MUSIC";
						break;
					case G_USER_DIRECTORY_PICTURES:
						path = "&PICTURES";
						break;
					case G_USER_DIRECTORY_VIDEOS:
						path = "&VIDEOS";
						break;

						/* We are not interested in
						 * TEMPLATES or PUBLIC_SHAREs
						 */
					case G_USER_DIRECTORY_PUBLIC_SHARE:
					case G_USER_DIRECTORY_TEMPLATES:
					case G_USER_N_DIRECTORIES:
						continue;
					}

					paths = g_slist_prepend (paths, (gpointer) path);
				}

				paths = g_slist_reverse (paths);

				/* If we only found DESKTOP which is
				 * always defined it seems, then add
				 * $HOME.
				 */
				if (g_slist_length (paths) < 2) {
					g_slist_free (paths);
					paths = g_slist_prepend (NULL, (gpointer) "$HOME");
					added_home_recursively = TRUE;
				}

				string_list = tracker_gslist_to_string_list (paths);
				g_slist_free (paths);

				g_key_file_set_string_list (key_file,
				                            conversions[i].group,
				                            conversions[i].key,
				                            (const gchar * const *) string_list,
				                            g_strv_length (string_list));

				g_strfreev (string_list);
			} else if (g_strcmp0 (conversions[i].property, "index-single-directories") == 0) {
				GSList *paths = NULL;
				GStrv string_list;

				if (!added_home_recursively) {
					paths = g_slist_prepend (paths, (gpointer) "$HOME");
				}

				string_list = tracker_gslist_to_string_list (paths);
				g_slist_free (paths);

				g_key_file_set_string_list (key_file,
				                            conversions[i].group,
				                            conversions[i].key,
				                            (const gchar * const *) string_list,
				                            g_strv_length (string_list));

				g_strfreev (string_list);
			} else if (g_strcmp0 (conversions[i].property, "ignored-directories") == 0) {
				const gchar *string_list[] = {
					"po", "CVS", "core-dumps", "lost+found",
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
					"*.pc", "*.omf", "*.aux", "*.tmp", "*.po",
					"*.vmdk", "*.vm*", "*.nvram", "*.part",
					"*.rcore", "lzo", "autom4te", "conftest",
					"confstat", "Makefile", "SCCS", "litmain.sh",
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
config_set_ignored_file_conveniences (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;
	GSList *l;
	GSList *paths = NULL;
	GSList *patterns = NULL;

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	g_slist_foreach (priv->ignored_file_patterns,
	                 (GFunc) g_pattern_spec_free,
	                 NULL);
	g_slist_free (priv->ignored_file_patterns);

	g_slist_foreach (priv->ignored_file_paths,
	                 (GFunc) g_free,
	                 NULL);
	g_slist_free (priv->ignored_file_paths);

	for (l = priv->ignored_files; l; l = l->next) {
		const gchar *str = l->data;

		if (!str) {
			continue;
		}

		if (G_LIKELY (*str != G_DIR_SEPARATOR)) {
			GPatternSpec *spec;

			spec = g_pattern_spec_new (l->data);
			patterns = g_slist_prepend (patterns, spec);
		} else {
			paths = g_slist_prepend (paths, g_strdup (l->data));
		}
	}

	priv->ignored_file_patterns = g_slist_reverse (patterns);
	priv->ignored_file_paths = g_slist_reverse (paths);
}

static void
config_set_ignored_directory_conveniences (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;
	GSList *l;
	GSList *patterns = NULL;
	GSList *paths = NULL;

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	g_slist_foreach (priv->ignored_directory_patterns,
	                 (GFunc) g_pattern_spec_free,
	                 NULL);
	g_slist_free (priv->ignored_directory_patterns);

	g_slist_foreach (priv->ignored_directory_paths,
	                 (GFunc) g_free,
	                 NULL);
	g_slist_free (priv->ignored_directory_paths);

	for (l = priv->ignored_directories; l; l = l->next) {
		const gchar *str = l->data;

		if (!str) {
			continue;
		}

		if (G_LIKELY (*str != G_DIR_SEPARATOR)) {
			GPatternSpec *spec;

			spec = g_pattern_spec_new (l->data);
			patterns = g_slist_prepend (patterns, spec);
		} else {
			paths = g_slist_prepend (paths, g_strdup (l->data));
		}
	}

	priv->ignored_directory_patterns = g_slist_reverse (patterns);
	priv->ignored_directory_paths = g_slist_reverse (paths);
}

static void
config_load (TrackerConfig *config,
             gboolean       use_defaults)
{
	TrackerConfigFile *file;
	gint i;

	file = TRACKER_CONFIG_FILE (config);

        if (use_defaults) {
                config_create_with_defaults (config, file->key_file, FALSE);
        }

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

		case G_TYPE_BOOLEAN:
			tracker_keyfile_object_load_boolean (G_OBJECT (file),
			                                     conversions[i].property,
			                                     file->key_file,
			                                     conversions[i].group,
			                                     conversions[i].key);
			break;

		case G_TYPE_POINTER: {
			GSList *new_dirs, *old_dirs, *l;
			gboolean is_recursive = TRUE;
			gboolean equal;

			if (strcmp (conversions[i].property, "index-recursive-directories") == 0 ||
			    strcmp (conversions[i].property, "index-single-directories") == 0 ||
			    strcmp (conversions[i].property, "ignored-directories") == 0) {
				is_recursive = strcmp (conversions[i].property, "index-recursive-directories") == 0;
				tracker_keyfile_object_load_directory_list (G_OBJECT (file),
				                                            conversions[i].property,
				                                            file->key_file,
				                                            conversions[i].group,
				                                            conversions[i].key,
				                                            is_recursive,
				                                            &new_dirs);

				for (l = new_dirs; l; l = l->next) {
					const gchar *path_to_use;

					/* Must be a special dir */
					if (strcmp (l->data, "&DESKTOP") == 0) {
						path_to_use = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
					} else if (strcmp (l->data, "&DOCUMENTS") == 0) {
						path_to_use = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
					} else if (strcmp (l->data, "&DOWNLOAD") == 0) {
						path_to_use = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
					} else if (strcmp (l->data, "&MUSIC") == 0) {
						path_to_use = g_get_user_special_dir (G_USER_DIRECTORY_MUSIC);
					} else if (strcmp (l->data, "&PICTURES") == 0) {
						path_to_use = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
					} else if (strcmp (l->data, "&PUBLIC_SHARE") == 0) {
						path_to_use = g_get_user_special_dir (G_USER_DIRECTORY_PUBLIC_SHARE);
					} else if (strcmp (l->data, "&TEMPLATES") == 0) {
						path_to_use = g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES);
					} else if (strcmp (l->data, "&VIDEOS") == 0) {
						path_to_use = g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS);
					} else {
						path_to_use = NULL;
					}

					if (path_to_use) {
						g_free (l->data);
						l->data = g_strdup (path_to_use);
					}
				}
			} else {
				tracker_keyfile_object_load_string_list (G_OBJECT (file),
				                                         conversions[i].property,
				                                         file->key_file,
				                                         conversions[i].group,
				                                         conversions[i].key,
				                                         &new_dirs);
			}

			g_object_get (config, conversions[i].property, &old_dirs, NULL);

			equal = tracker_gslist_with_string_data_equal (new_dirs, old_dirs);

			if (!equal) {
				g_object_set (config, conversions[i].property, new_dirs, NULL);
                        }

			g_slist_foreach (new_dirs, (GFunc) g_free, NULL);
			g_slist_free (new_dirs);

			break;
		}
		}
	}

	config_set_ignored_file_conveniences (config);
	config_set_ignored_directory_conveniences (config);
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
			if (strcmp (conversions[i].property, "index-recursive-directories") == 0 ||
			    strcmp (conversions[i].property, "index-single-directories") == 0) {
				GSList *dirs, *l;

				g_object_get (config, conversions[i].property, &dirs, NULL);

				for (l = dirs; l; l = l->next) {
					const gchar *path_to_use;

					/* FIXME: This doesn't work
					 * perfectly, what if DESKTOP
					 * and DOCUMENTS are in the
					 * same place? Then this
					 * breaks. Need a better
					 * solution at some point.
					 */
					if (g_strcmp0 (l->data, g_get_home_dir ()) == 0) {
						/* Home dir gets straight into configuration,
						 * regardless of having XDG dirs pointing to it.
						 */
						path_to_use = "$HOME";
					} else if (g_strcmp0 (l->data, g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP)) == 0) {
						path_to_use = "&DESKTOP";
					} else if (g_strcmp0 (l->data, g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS)) == 0) {
						path_to_use = "&DOCUMENTS";
					} else if (g_strcmp0 (l->data, g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD)) == 0) {
						path_to_use = "&DOWNLOAD";
					} else if (g_strcmp0 (l->data, g_get_user_special_dir (G_USER_DIRECTORY_MUSIC)) == 0) {
						path_to_use = "&MUSIC";
					} else if (g_strcmp0 (l->data, g_get_user_special_dir (G_USER_DIRECTORY_PICTURES)) == 0) {
						path_to_use = "&PICTURES";
					} else if (g_strcmp0 (l->data, g_get_user_special_dir (G_USER_DIRECTORY_PUBLIC_SHARE)) == 0) {
						path_to_use = "&PUBLIC_SHARE";
					} else if (g_strcmp0 (l->data, g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES)) == 0) {
						path_to_use = "&TEMPLATES";
					} else if (g_strcmp0 (l->data, g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS)) == 0) {
						path_to_use = "&VIDEOS";
					} else {
						path_to_use = NULL;
					}

					if (path_to_use) {
						g_free (l->data);
						l->data = g_strdup (path_to_use);
					}
				}
			}

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


TrackerConfig *
tracker_config_new_with_domain (const gchar *domain)
{
	return g_object_new (TRACKER_TYPE_CONFIG,  "domain", domain, NULL);
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
tracker_config_get_index_on_battery (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_ON_BATTERY);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->index_on_battery;
}

gboolean
tracker_config_get_index_on_battery_first_time (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_ON_BATTERY_FIRST_TIME);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->index_on_battery_first_time;
}

gboolean
tracker_config_get_index_removable_devices (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_REMOVABLE_DEVICES);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->index_removable_devices;
}

gboolean
tracker_config_get_index_optical_discs (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_OPTICAL_DISCS);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->index_optical_discs;
}

gint
tracker_config_get_low_disk_space_limit (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_LOW_DISK_SPACE_LIMIT);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->low_disk_space_limit;
}

GSList *
tracker_config_get_index_recursive_directories (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->index_recursive_directories;
}

GSList *
tracker_config_get_index_recursive_directories_unfiltered (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->index_recursive_directories_unfiltered;
}

GSList *
tracker_config_get_index_single_directories (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->index_single_directories;
}

GSList *
tracker_config_get_index_single_directories_unfiltered (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->index_single_directories_unfiltered;
}

GSList *
tracker_config_get_ignored_directories (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->ignored_directories;
}

GSList *
tracker_config_get_ignored_directories_with_content (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->ignored_directories_with_content;
}

GSList *
tracker_config_get_ignored_files (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->ignored_files;
}

gint
tracker_config_get_crawling_interval (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), 0);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->crawling_interval;
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

void
tracker_config_set_initial_sleep (TrackerConfig *config,
                                  gint           value)
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
tracker_config_set_throttle (TrackerConfig *config,
                             gint           value)
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
tracker_config_set_index_on_battery (TrackerConfig *config,
                                     gboolean       value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->index_on_battery = value;
	g_object_notify (G_OBJECT (config), "index-on-battery");
}

void
tracker_config_set_index_on_battery_first_time (TrackerConfig *config,
                                                gboolean       value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->index_on_battery_first_time = value;
	g_object_notify (G_OBJECT (config), "index-on-battery-first-time");
}

void
tracker_config_set_index_removable_devices (TrackerConfig *config,
                                            gboolean       value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->index_removable_devices = value;
	g_object_notify (G_OBJECT (config), "index-removable-devices");
}

void
tracker_config_set_index_optical_discs (TrackerConfig *config,
                                        gboolean       value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->index_optical_discs = value;
	g_object_notify (G_OBJECT (config), "index-optical-discs");
}

void
tracker_config_set_low_disk_space_limit (TrackerConfig *config,
                                         gint           value)
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

static void
rebuild_filtered_lists (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;
	GSList *old_list;

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	/* Filter single directories first, checking duplicates */
	old_list = priv->index_single_directories;
	priv->index_single_directories = NULL;

	if (priv->index_single_directories_unfiltered) {
		priv->index_single_directories =
		        tracker_path_list_filter_duplicates (priv->index_single_directories_unfiltered,
		                                             ".", FALSE);
	}

	if (!tracker_gslist_with_string_data_equal (old_list, priv->index_single_directories)) {
		g_object_notify (G_OBJECT (config), "index-single-directories");
	}

	if (old_list) {
		g_slist_foreach (old_list, (GFunc) g_free, NULL);
		g_slist_free (old_list);
	}

	/* Filter recursive directories */
	old_list = priv->index_recursive_directories;
	priv->index_recursive_directories = NULL;

	if (priv->index_recursive_directories_unfiltered) {
		GSList *l, *new_list = NULL;

		/* Remove elements already in single directories */
		for (l = priv->index_recursive_directories_unfiltered; l; l = l->next) {
			if (g_slist_find_custom (priv->index_single_directories,
			                         l->data,
			                         (GCompareFunc) g_strcmp0) != NULL) {
				g_message ("Path '%s' being removed from recursive directories "
				           "list, as it also exists in single directories list",
				           (gchar *) l->data);
			} else {
				new_list = g_slist_prepend (new_list, l->data);
			}
		}

		new_list = g_slist_reverse (new_list);

		priv->index_recursive_directories =
		        tracker_path_list_filter_duplicates (new_list, ".", TRUE);

		g_slist_free (new_list);
	}

	if (!tracker_gslist_with_string_data_equal (old_list, priv->index_recursive_directories)) {
		g_object_notify (G_OBJECT (config), "index-recursive-directories");
	}

	if (old_list) {
		g_slist_foreach (old_list, (GFunc) g_free, NULL);
		g_slist_free (old_list);
	}
}

void
tracker_config_set_index_recursive_directories (TrackerConfig *config,
                                                GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList *l;
        gboolean equal;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	l = priv->index_recursive_directories_unfiltered;

        equal = tracker_gslist_with_string_data_equal (roots, l);

	if (!roots) {
		priv->index_recursive_directories_unfiltered = NULL;
	} else {
		priv->index_recursive_directories_unfiltered =
			tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

        if (equal) {
                return;
        }

        rebuild_filtered_lists (config);
}

void
tracker_config_set_index_single_directories (TrackerConfig *config,
                                             GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList *l;
        gboolean equal;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	l = priv->index_single_directories_unfiltered;

        equal = tracker_gslist_with_string_data_equal (roots, l);

	if (!roots) {
		priv->index_single_directories_unfiltered = NULL;
	} else {
		priv->index_single_directories_unfiltered =
			tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

        if (equal) {
                return;
        }

        rebuild_filtered_lists (config);
}

void
tracker_config_set_ignored_directories (TrackerConfig *config,
                                        GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList *l;
        gboolean equal;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	l = priv->ignored_directories;

        equal = tracker_gslist_with_string_data_equal (roots, l);

	if (!roots) {
		priv->ignored_directories = NULL;
	} else {
		priv->ignored_directories =
			tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

        if (equal) {
                return;
        }

	/* Re-set up the GPatternSpec list */
	config_set_ignored_directory_conveniences (config);

	g_object_notify (G_OBJECT (config), "ignored-directories");
}

void
tracker_config_set_ignored_directories_with_content (TrackerConfig *config,
                                                     GSList        *roots)
{
	TrackerConfigPrivate *priv;
	GSList *l;
        gboolean equal;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	l = priv->ignored_directories_with_content;

        equal = tracker_gslist_with_string_data_equal (roots, l);

	if (!roots) {
		priv->ignored_directories_with_content = NULL;
	} else {
		priv->ignored_directories_with_content =
			tracker_gslist_copy_with_string_data (roots);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

        if (equal) {
                return;
        }

	g_object_notify (G_OBJECT (config), "ignored-directories-with-content");
}

void
tracker_config_set_ignored_files (TrackerConfig *config,
                                  GSList        *files)
{
	TrackerConfigPrivate *priv;
	GSList *l;
        gboolean equal;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	l = priv->ignored_files;

        equal = tracker_gslist_with_string_data_equal (files, l);

	if (!files) {
		priv->ignored_files = NULL;
	} else {
		priv->ignored_files =
			tracker_gslist_copy_with_string_data (files);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

        if (equal) {
                return;
        }

	/* Re-set up the GPatternSpec list */
	config_set_ignored_file_conveniences (config);

	g_object_notify (G_OBJECT (config), "ignored-files");
}

void
tracker_config_set_crawling_interval (TrackerConfig *config,
                                      gint           interval)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!tracker_keyfile_object_validate_int (config, "crawling-interval", interval)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->crawling_interval = interval;
	g_object_notify (G_OBJECT (config), "crawling-interval");
}

/*
 * Convenience functions
 */

GSList *
tracker_config_get_ignored_directory_patterns (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->ignored_directory_patterns;
}

GSList *
tracker_config_get_ignored_file_patterns (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->ignored_file_patterns;
}

GSList *
tracker_config_get_ignored_directory_paths (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->ignored_directory_paths;
}

GSList *
tracker_config_get_ignored_file_paths (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->ignored_file_paths;
}
