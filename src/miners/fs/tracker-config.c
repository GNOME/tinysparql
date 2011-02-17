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
#include <libtracker-common/tracker-enum-types.h>

#include "tracker-config.h"

/* Default values */
#define DEFAULT_VERBOSITY                        0
#define DEFAULT_INITIAL_SLEEP                    15       /* 0->1000 */
#define DEFAULT_ENABLE_MONITORS                  TRUE
#define DEFAULT_THROTTLE                         0        /* 0->20 */
#define DEFAULT_INDEX_REMOVABLE_DEVICES          TRUE
#define DEFAULT_INDEX_OPTICAL_DISCS              FALSE
#define DEFAULT_INDEX_ON_BATTERY                 FALSE
#define DEFAULT_INDEX_ON_BATTERY_FIRST_TIME      TRUE
#define DEFAULT_LOW_DISK_SPACE_LIMIT             1        /* 0->100 / -1 */
#define DEFAULT_CRAWLING_INTERVAL                -1       /* 0->365 / -1 / -2 */
#define DEFAULT_REMOVABLE_DAYS_THRESHOLD         3        /* 1->365 / 0  */

typedef struct {
	GSList   *index_recursive_directories;
	GSList	 *index_recursive_directories_unfiltered;
	GSList   *index_single_directories;
	GSList	 *index_single_directories_unfiltered;
	GSList   *ignored_directories;
	GSList   *ignored_directories_with_content;
	GSList   *ignored_files;

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

enum {
	PROP_0,

	/* General */
	PROP_VERBOSITY,
	PROP_INITIAL_SLEEP,

	/* Monitors */
	PROP_ENABLE_MONITORS,

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
	PROP_CRAWLING_INTERVAL,
	PROP_REMOVABLE_DAYS_THRESHOLD
};

static TrackerConfigMigrationEntry migration[] = {
	{ G_TYPE_ENUM,    "General",  "Verbosity",                     "verbosity"                        },
	{ G_TYPE_INT,     "General",  "InitialSleep",                  "initial-sleep"                    },
	{ G_TYPE_BOOLEAN, "Monitors", "EnableMonitors",                "enable-monitors"                  },
	{ G_TYPE_INT,     "Indexing", "Throttle",                      "throttle"                         },
	{ G_TYPE_BOOLEAN, "Indexing", "IndexOnBattery",                "index-on-battery"                 },
	{ G_TYPE_BOOLEAN, "Indexing", "IndexOnBatteryFirstTime",       "index-on-battery-first-time"      },
	{ G_TYPE_BOOLEAN, "Indexing", "IndexRemovableMedia",           "index-removable-devices"          },
	{ G_TYPE_BOOLEAN, "Indexing", "IndexOpticalDiscs",             "index-optical-discs"              },
	{ G_TYPE_INT,     "Indexing", "LowDiskSpaceLimit",             "low-disk-space-limit"             },
	{ G_TYPE_POINTER, "Indexing", "IndexRecursiveDirectories",     "index-recursive-directories"      },
	{ G_TYPE_POINTER, "Indexing", "IndexSingleDirectories",        "index-single-directories"         },
	{ G_TYPE_POINTER, "Indexing", "IgnoredDirectories",            "ignored-directories"              },
	{ G_TYPE_POINTER, "Indexing", "IgnoredDirectoriesWithContent", "ignored-directories-with-content" },
	{ G_TYPE_POINTER, "Indexing", "IgnoredFiles",                  "ignored-files"                    },
	{ G_TYPE_INT,	  "Indexing", "CrawlingInterval",              "crawling-interval"                },
	{ G_TYPE_INT,	  "Indexing", "RemovableDaysThreshold",        "removable-days-threshold"         },
	{ 0 }
};

G_DEFINE_TYPE (TrackerConfig, tracker_config, G_TYPE_SETTINGS)

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
	                                 PROP_INITIAL_SLEEP,
	                                 g_param_spec_int ("initial-sleep",
	                                                   "Initial sleep",
	                                                   "Time in seconds before crawling filesystem (0->1000)",
	                                                   0,
	                                                   1000,
	                                                   DEFAULT_INITIAL_SLEEP,
	                                                   G_PARAM_READWRITE));

	/* Monitors */
	g_object_class_install_property (object_class,
	                                 PROP_ENABLE_MONITORS,
	                                 g_param_spec_boolean ("enable-monitors",
	                                                       "Enable monitors",
	                                                       "Set to false to completely disable any monitoring",
	                                                       DEFAULT_ENABLE_MONITORS,
	                                                       G_PARAM_READWRITE));

	/* Indexing */
	g_object_class_install_property (object_class,
	                                 PROP_THROTTLE,
	                                 g_param_spec_int ("throttle",
	                                                   "Throttle",
	                                                   "Sets the indexing speed (0->20, where 20=slowest speed)",
	                                                   0,
	                                                   20,
	                                                   DEFAULT_THROTTLE,
	                                                   G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_INDEX_ON_BATTERY,
	                                 g_param_spec_boolean ("index-on-battery",
	                                                       "Index on battery",
	                                                       "Set to true to index while running on battery",
	                                                       DEFAULT_INDEX_ON_BATTERY,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_INDEX_ON_BATTERY_FIRST_TIME,
	                                 g_param_spec_boolean ("index-on-battery-first-time",
	                                                       "Index on battery first time",
	                                                       "Set to true to index while running on battery for the first time only",
	                                                       DEFAULT_INDEX_ON_BATTERY_FIRST_TIME,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_INDEX_REMOVABLE_DEVICES,
	                                 g_param_spec_boolean ("index-removable-devices",
	                                                       "index removable devices",
	                                                       "Set to true to enable traversing mounted directories for removable devices\n"
	                                                       "(this includes optical discs)",
	                                                       DEFAULT_INDEX_REMOVABLE_DEVICES,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_INDEX_OPTICAL_DISCS,
	                                 g_param_spec_boolean ("index-optical-discs",
	                                                       "index optical discs",
	                                                       "Set to true to enable traversing CDs, DVDs, and generally optical media\n"
	                                                       "(if removable devices are not indexed, optical discs won't be either)",
	                                                       DEFAULT_INDEX_OPTICAL_DISCS,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_LOW_DISK_SPACE_LIMIT,
	                                 g_param_spec_int ("low-disk-space-limit",
	                                                   "Low disk space limit",
	                                                   "Pause indexer when disk space is <= this value\n"
	                                                   "(0->100, value is in % of $HOME file system, -1=disable pausing)",
	                                                   -1,
	                                                   100,
	                                                   DEFAULT_LOW_DISK_SPACE_LIMIT,
	                                                   G_PARAM_READWRITE));
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
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_INDEX_SINGLE_DIRECTORIES,
	                                 g_param_spec_pointer ("index-single-directories",
	                                                       "Index single directories",
	                                                       " List of directories to index but not sub-directories for changes (separator=;)\n"
	                                                       " Special values used for IndexRecursiveDirectories can also be used here",
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_IGNORED_DIRECTORIES,
	                                 g_param_spec_pointer ("ignored-directories",
	                                                       "Ignored directories",
	                                                       " List of directories to NOT crawl for indexing (separator=;)",
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_IGNORED_DIRECTORIES_WITH_CONTENT,
	                                 g_param_spec_pointer ("ignored-directories-with-content",
	                                                       "Ignored directories with content",
	                                                       " List of directories to NOT crawl for indexing based on child files (separator=;)",
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_IGNORED_FILES,
	                                 g_param_spec_pointer ("ignored-files",
	                                                       "Ignored files",
	                                                       " List of files to NOT index (separator=;)",
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_CRAWLING_INTERVAL,
	                                 g_param_spec_int ("crawling-interval",
	                                                   "Crawling interval",
	                                                   " Interval in days to check the filesystem is up to date in the database,"
	                                                   " maximum is 365, default is -1.\n"
	                                                   "   -2 = crawling is disabled entirely\n"
	                                                   "   -1 = crawling *may* occur on startup (if not cleanly shutdown)\n"
	                                                   "    0 = crawling is forced",
	                                                   -2,
	                                                   365,
	                                                   DEFAULT_CRAWLING_INTERVAL,
	                                                   G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_REMOVABLE_DAYS_THRESHOLD,
	                                 g_param_spec_int ("removable-days-threshold",
	                                                   "Removable days threshold",
	                                                   " Threshold in days after which files from removables devices"
	                                                   " will be removed from database if not mounted. 0 means never, "
	                                                   " maximum is 365.",
	                                                   0,
	                                                   365,
	                                                   DEFAULT_REMOVABLE_DAYS_THRESHOLD,
	                                                   G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (TrackerConfigPrivate));
}

static void
tracker_config_init (TrackerConfig *object)
{
	object->priv = G_TYPE_INSTANCE_GET_PRIVATE (object, TRACKER_TYPE_CONFIG, TrackerConfigPrivate);
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
		                              g_value_get_enum (value));
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
	case PROP_REMOVABLE_DAYS_THRESHOLD:
		tracker_config_set_removable_days_threshold (TRACKER_CONFIG (object),
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
	TrackerConfig *config = TRACKER_CONFIG (object);

	switch (param_id) {
		/* General */
	case PROP_VERBOSITY:
		g_value_set_enum (value, tracker_config_get_verbosity (config));
		break;
	case PROP_INITIAL_SLEEP:
		g_value_set_int (value, tracker_config_get_initial_sleep (config));
		break;

		/* Montors */
	case PROP_ENABLE_MONITORS:
		g_value_set_boolean (value, tracker_config_get_enable_monitors (config));
		break;

		/* Indexing */
	case PROP_THROTTLE:
		g_value_set_int (value, tracker_config_get_throttle (config));
		break;
	case PROP_INDEX_ON_BATTERY:
		g_value_set_boolean (value, tracker_config_get_index_on_battery (config));
		break;
	case PROP_INDEX_ON_BATTERY_FIRST_TIME:
		g_value_set_boolean (value, tracker_config_get_index_on_battery_first_time (config));
		break;
	case PROP_INDEX_REMOVABLE_DEVICES:
		g_value_set_boolean (value, tracker_config_get_index_removable_devices (config));
		break;
	case PROP_INDEX_OPTICAL_DISCS:
		g_value_set_boolean (value, tracker_config_get_index_optical_discs (config));
		break;
	case PROP_LOW_DISK_SPACE_LIMIT:
		g_value_set_int (value, tracker_config_get_low_disk_space_limit (config));
		break;
	case PROP_INDEX_RECURSIVE_DIRECTORIES:
		g_value_set_pointer (value, tracker_config_get_index_recursive_directories (config));
		break;
	case PROP_INDEX_SINGLE_DIRECTORIES:
		g_value_set_pointer (value, tracker_config_get_index_single_directories (config));
		break;
	case PROP_IGNORED_DIRECTORIES:
		g_value_set_pointer (value, tracker_config_get_ignored_directories (config));
		break;
	case PROP_IGNORED_DIRECTORIES_WITH_CONTENT:
		g_value_set_pointer (value, tracker_config_get_ignored_directories_with_content (config));
		break;
	case PROP_IGNORED_FILES:
		g_value_set_pointer (value, tracker_config_get_ignored_files (config));
		break;
	case PROP_CRAWLING_INTERVAL:
		g_value_set_int (value, tracker_config_get_crawling_interval (config));
		break;
	case PROP_REMOVABLE_DAYS_THRESHOLD:
		g_value_set_int (value, tracker_config_get_removable_days_threshold (config));
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

	priv = TRACKER_CONFIG (object)->priv;

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

static gboolean
settings_get_dir_mapping (GVariant *value,
                          gpointer *result,
                          gpointer  user_data)
{
	gchar **strv;
	gboolean is_recursive;
	GSList *dirs, *l;
	gsize len;

	strv = (gchar **) g_variant_get_strv (value, &len);
	dirs = tracker_string_list_to_gslist (strv, len);
	g_free (strv);

	is_recursive = GPOINTER_TO_INT (user_data);

	if (dirs) {
		GSList *filtered;

		filtered = tracker_path_list_filter_duplicates (dirs, ".", is_recursive);

		g_slist_foreach (dirs, (GFunc) g_free, NULL);
		g_slist_free (dirs);

		dirs = filtered;
	}

	for (l = dirs; l; l = l->next) {
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

	*result = dirs;

	return TRUE;
}

static gboolean
settings_get_strv_mapping (GVariant *value,
                           gpointer *result,
                           gpointer  user_data)
{
	gchar **strv;
	gsize len;

	strv = (gchar **) g_variant_get_strv (value, &len);
	*result = tracker_string_list_to_gslist (strv, len);
	g_free (strv);

	return TRUE;
}

static void
config_set_ignored_file_conveniences (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;
	GSList *l;
	GSList *paths = NULL;
	GSList *patterns = NULL;

	priv = config->priv;

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

	priv = config->priv;

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
config_constructed (GObject *object)
{
	TrackerConfigFile *config_file;
	TrackerConfigPrivate *priv;

	(G_OBJECT_CLASS (tracker_config_parent_class)->constructed) (object);

	priv = TRACKER_CONFIG (object)->priv;
	g_settings_delay (G_SETTINGS (object));

	/* Migrate keyfile-based configuration */
	config_file = tracker_config_file_new ();

	if (config_file) {
		tracker_config_file_migrate (config_file, G_SETTINGS (object), migration);
		g_object_unref (config_file);
	}

	/* Get cached values */
	priv->index_recursive_directories = g_settings_get_mapped (G_SETTINGS (object),
	                                                           "index-recursive-directories",
	                                                           settings_get_dir_mapping,
	                                                           GUINT_TO_POINTER (TRUE));

	priv->index_single_directories = g_settings_get_mapped (G_SETTINGS (object),
	                                                        "index-single-directories",
	                                                        settings_get_dir_mapping,
	                                                        GUINT_TO_POINTER (FALSE));

	priv->ignored_directories = g_settings_get_mapped (G_SETTINGS (object),
	                                                   "ignored-directories",
	                                                   settings_get_dir_mapping,
	                                                   GUINT_TO_POINTER (FALSE));

	priv->ignored_directories_with_content = g_settings_get_mapped (G_SETTINGS (object),
	                                                                "ignored-directories-with-content",
	                                                                settings_get_strv_mapping, NULL);

	priv->ignored_files = g_settings_get_mapped (G_SETTINGS (object),
	                                             "ignored-files",
	                                             settings_get_strv_mapping, NULL);

	config_set_ignored_file_conveniences (TRACKER_CONFIG (object));
	config_set_ignored_directory_conveniences (TRACKER_CONFIG (object));
}

TrackerConfig *
tracker_config_new (void)
{
	return g_object_new (TRACKER_TYPE_CONFIG,
	                     "schema", "org.freedesktop.Tracker.Miner.FileSystem",
	                     "path", "/org/freedesktop/tracker/miners/fs/",
	                     NULL);
}

static gchar **
directories_to_strv (GSList *dirs)
{
	gchar **strv;
	guint i = 0;
	GSList *l;

	strv = g_new0 (gchar*, g_slist_length (dirs) + 1);

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
			path_to_use = l->data;
		}

		strv[i] = g_strdup (path_to_use);
		i++;
	}

	return strv;
}

gboolean
tracker_config_save (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;
	gchar **strv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), FALSE);

	priv = config->priv;

	/* Store cached values */
	strv = tracker_gslist_to_string_list (priv->ignored_directories_with_content);
	g_settings_set_strv (G_SETTINGS (config),
	                     "ignored-directories-with-content",
	                     (const gchar * const *) strv);
	g_strfreev (strv);

	strv = tracker_gslist_to_string_list (priv->ignored_files);
	g_settings_set_strv (G_SETTINGS (config),
	                     "ignored-files",
	                     (const gchar * const *) strv);
	g_strfreev (strv);

	strv = tracker_gslist_to_string_list (priv->ignored_directories);
	g_settings_set_strv (G_SETTINGS (config),
	                     "ignored-directories",
	                     (const gchar * const *) strv);
	g_strfreev (strv);

	strv = directories_to_strv (priv->index_recursive_directories);
	g_settings_set_strv (G_SETTINGS (config),
	                     "index-recursive-directories",
	                     (const gchar * const *) strv);
	g_strfreev (strv);

	strv = directories_to_strv (priv->index_single_directories);
	g_settings_set_strv (G_SETTINGS (config),
	                     "index-single-directories",
	                     (const gchar * const *) strv);
	g_strfreev (strv);

	/* And then apply the config */
	g_settings_apply (G_SETTINGS (config));
	return TRUE;
}

gint
tracker_config_get_verbosity (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_VERBOSITY);

	return g_settings_get_enum (G_SETTINGS (config), "verbosity");
}

gint
tracker_config_get_initial_sleep (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INITIAL_SLEEP);

	return g_settings_get_int (G_SETTINGS (config), "initial-sleep");
}

gboolean
tracker_config_get_enable_monitors (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_MONITORS);

	return g_settings_get_boolean (G_SETTINGS (config), "enable-monitors");
}

gint
tracker_config_get_throttle (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_THROTTLE);

	return g_settings_get_int (G_SETTINGS (config), "throttle");
}

gboolean
tracker_config_get_index_on_battery (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_ON_BATTERY);

	return g_settings_get_boolean (G_SETTINGS (config), "index-on-battery");
}

gboolean
tracker_config_get_index_on_battery_first_time (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_ON_BATTERY_FIRST_TIME);

	return g_settings_get_boolean (G_SETTINGS (config), "index-on-battery-first-time");
}

gboolean
tracker_config_get_index_removable_devices (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_REMOVABLE_DEVICES);

	return g_settings_get_boolean (G_SETTINGS (config), "index-removable-devices");
}

gboolean
tracker_config_get_index_optical_discs (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_OPTICAL_DISCS);

	return g_settings_get_boolean (G_SETTINGS (config), "index-optical-discs");
}

gint
tracker_config_get_low_disk_space_limit (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_LOW_DISK_SPACE_LIMIT);

	return g_settings_get_int (G_SETTINGS (config), "low-disk-space-limit");
}

GSList *
tracker_config_get_index_recursive_directories (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = config->priv;

	return priv->index_recursive_directories;
}

GSList *
tracker_config_get_index_recursive_directories_unfiltered (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = config->priv;

	return priv->index_recursive_directories_unfiltered;
}

GSList *
tracker_config_get_index_single_directories (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = config->priv;

	return priv->index_single_directories;
}

GSList *
tracker_config_get_index_single_directories_unfiltered (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = config->priv;

	return priv->index_single_directories_unfiltered;
}

GSList *
tracker_config_get_ignored_directories (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = config->priv;

	return priv->ignored_directories;
}

GSList *
tracker_config_get_ignored_directories_with_content (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = config->priv;

	return priv->ignored_directories_with_content;
}

GSList *
tracker_config_get_ignored_files (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = config->priv;

	return priv->ignored_files;
}

gint
tracker_config_get_crawling_interval (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), 0);

	return g_settings_get_int (G_SETTINGS (config), "crawling-interval");
}

gint
tracker_config_get_removable_days_threshold (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), 0);

	return g_settings_get_int (G_SETTINGS (config), "removable-days-threshold");
}

void
tracker_config_set_verbosity (TrackerConfig *config,
                              gint           value)
{

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_enum (G_SETTINGS (config), "verbosity", value);
	g_object_notify (G_OBJECT (config), "verbosity");
}

void
tracker_config_set_initial_sleep (TrackerConfig *config,
                                  gint           value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_int (G_SETTINGS (config), "initial-sleep", value);
	g_object_notify (G_OBJECT (config), "initial-sleep");
}

void
tracker_config_set_enable_monitors (TrackerConfig *config,
                                    gboolean       value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_boolean (G_SETTINGS (config), "enable-monitors", value);
	g_object_notify (G_OBJECT (config), "enable-monitors");
}

void
tracker_config_set_throttle (TrackerConfig *config,
                             gint           value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_int (G_SETTINGS (config), "throttle", value);
	g_object_notify (G_OBJECT (config), "throttle");
}

void
tracker_config_set_index_on_battery (TrackerConfig *config,
                                     gboolean       value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_boolean (G_SETTINGS (config), "index-on-battery", value);
	g_object_notify (G_OBJECT (config), "index-on-battery");
}

void
tracker_config_set_index_on_battery_first_time (TrackerConfig *config,
                                                gboolean       value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_boolean (G_SETTINGS (config), "index-on-battery-first-time", value);
	g_object_notify (G_OBJECT (config), "index-on-battery-first-time");
}

void
tracker_config_set_index_removable_devices (TrackerConfig *config,
                                            gboolean       value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_boolean (G_SETTINGS (config), "index-removable-devices", value);
	g_object_notify (G_OBJECT (config), "index-removable-devices");
}

void
tracker_config_set_index_optical_discs (TrackerConfig *config,
                                        gboolean       value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_boolean (G_SETTINGS (config), "index-optical-discs", value);
	g_object_notify (G_OBJECT (config), "index-optical-discs");
}

void
tracker_config_set_low_disk_space_limit (TrackerConfig *config,
                                         gint           value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_int (G_SETTINGS (config), "low-disk-space-limit", value);
	g_object_notify (G_OBJECT (config), "low-disk-space-limit");
}

static void
rebuild_filtered_lists (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;
	GSList *old_list;

	priv = config->priv;

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

	priv = config->priv;

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

	priv = config->priv;

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

	priv = config->priv;

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

	priv = config->priv;

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

	priv = config->priv;

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
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_int (G_SETTINGS (config), "crawling-interval", interval);
	g_object_notify (G_OBJECT (config), "crawling-interval");
}

void
tracker_config_set_removable_days_threshold (TrackerConfig *config,
                                             gint           value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	g_settings_set_int (G_SETTINGS (config), "removable-days-threshold", value);
	g_object_notify (G_OBJECT (config), "removable-days-threshold");
}

/*
 * Convenience functions
 */

GSList *
tracker_config_get_ignored_directory_patterns (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = config->priv;

	return priv->ignored_directory_patterns;
}

GSList *
tracker_config_get_ignored_file_patterns (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = config->priv;

	return priv->ignored_file_patterns;
}

GSList *
tracker_config_get_ignored_directory_paths (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = config->priv;

	return priv->ignored_directory_paths;
}

GSList *
tracker_config_get_ignored_file_paths (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = config->priv;

	return priv->ignored_file_paths;
}
