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
#include <libtracker-common/tracker-log.h>

#include "tracker-config.h"

/* Default values */
#define DEFAULT_VERBOSITY                        0
#define DEFAULT_SCHED_IDLE                       1
#define DEFAULT_INITIAL_SLEEP                    15       /* 0->1000 */
#define DEFAULT_ENABLE_MONITORS                  TRUE
#define DEFAULT_THROTTLE                         0        /* 0->20 */
#define DEFAULT_INDEX_REMOVABLE_DEVICES          FALSE
#define DEFAULT_INDEX_OPTICAL_DISCS              FALSE
#define DEFAULT_INDEX_ON_BATTERY                 FALSE
#define DEFAULT_INDEX_ON_BATTERY_FIRST_TIME      TRUE
#define DEFAULT_LOW_DISK_SPACE_LIMIT             1        /* 0->100 / -1 */
#define DEFAULT_CRAWLING_INTERVAL                -1       /* 0->365 / -1 / -2 */
#define DEFAULT_REMOVABLE_DAYS_THRESHOLD         3        /* 1->365 / 0  */
#define DEFAULT_ENABLE_WRITEBACK                 FALSE

typedef struct {
	/* NOTE: Only used with TRACKER_USE_CONFIG_FILES env var. */
	gpointer config_file;

	/* IMPORTANT: There are 3 versions of the directories:
	 * 1. a GStrv stored in GSettings
	 * 2. a GSList stored here which is the GStrv without any
	 *    aliases or duplicates resolved.
	 * 3. a GSList stored here which has duplicates and aliases
	 *    resolved.
	 */
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

static void config_set_property                         (GObject           *object,
                                                         guint              param_id,
                                                         const GValue      *value,
                                                         GParamSpec        *pspec);
static void config_get_property                         (GObject           *object,
                                                         guint              param_id,
                                                         GValue            *value,
                                                         GParamSpec        *pspec);
static void config_finalize                             (GObject           *object);
static void config_constructed                          (GObject           *object);
static void config_file_changed_cb                      (TrackerConfigFile *config,
                                                         gpointer           user_data);
static void config_set_index_recursive_directories      (TrackerConfig     *config,
                                                         GSList            *roots);
static void config_set_index_single_directories         (TrackerConfig     *config,
                                                         GSList            *roots);
static void config_set_ignored_directories              (TrackerConfig     *config,
                                                         GSList            *roots);
static void config_set_ignored_directories_with_content (TrackerConfig     *config,
                                                         GSList            *roots);
static void config_set_ignored_files                    (TrackerConfig     *config,
                                                         GSList            *files);

enum {
	PROP_0,

	/* General */
	PROP_VERBOSITY,
	PROP_SCHED_IDLE,
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
	PROP_REMOVABLE_DAYS_THRESHOLD,

	/* Writeback */
	PROP_ENABLE_WRITEBACK

};

static TrackerConfigMigrationEntry migration[] = {
	{ G_TYPE_ENUM,    "General",   "Verbosity",                     "verbosity",                        FALSE, FALSE },
	{ G_TYPE_ENUM,    "General",   "SchedIdle",                     "sched-idle",                       FALSE, FALSE },
	{ G_TYPE_INT,     "General",   "InitialSleep",                  "initial-sleep",                    FALSE, FALSE },
	{ G_TYPE_BOOLEAN, "Monitors",  "EnableMonitors",                "enable-monitors",                  FALSE, FALSE },
	{ G_TYPE_INT,     "Indexing",  "Throttle",                      "throttle",                         FALSE, FALSE },
	{ G_TYPE_BOOLEAN, "Indexing",  "IndexOnBattery",                "index-on-battery",                 FALSE, FALSE },
	{ G_TYPE_BOOLEAN, "Indexing",  "IndexOnBatteryFirstTime",       "index-on-battery-first-time",      FALSE, FALSE },
	{ G_TYPE_BOOLEAN, "Indexing",  "IndexRemovableMedia",           "index-removable-devices",          FALSE, FALSE },
	{ G_TYPE_BOOLEAN, "Indexing",  "IndexOpticalDiscs",             "index-optical-discs",              FALSE, FALSE },
	{ G_TYPE_INT,     "Indexing",  "LowDiskSpaceLimit",             "low-disk-space-limit",             FALSE, FALSE },
	{ G_TYPE_POINTER, "Indexing",  "IndexRecursiveDirectories",     "index-recursive-directories",      TRUE,  TRUE },
	{ G_TYPE_POINTER, "Indexing",  "IndexSingleDirectories",        "index-single-directories",         TRUE,  FALSE },
	{ G_TYPE_POINTER, "Indexing",  "IgnoredDirectories",            "ignored-directories",              FALSE, FALSE },
	{ G_TYPE_POINTER, "Indexing",  "IgnoredDirectoriesWithContent", "ignored-directories-with-content", FALSE, FALSE },
	{ G_TYPE_POINTER, "Indexing",  "IgnoredFiles",                  "ignored-files",                    FALSE, FALSE },
	{ G_TYPE_INT,     "Indexing",  "CrawlingInterval",              "crawling-interval",                FALSE, FALSE },
	{ G_TYPE_INT,     "Indexing",  "RemovableDaysThreshold",        "removable-days-threshold",         FALSE, FALSE },
	{ G_TYPE_BOOLEAN, "Writeback", "EnableWriteback",               "enable-writeback",                 FALSE, FALSE },
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
	                                                    DEFAULT_VERBOSITY,
	                                                    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_SCHED_IDLE,
	                                 g_param_spec_enum ("sched-idle",
	                                                    "Scheduler priority when idle",
	                                                    "Scheduler priority when idle (0=always, 1=first-index, 2=never)",
	                                                    TRACKER_TYPE_SCHED_IDLE,
	                                                    DEFAULT_SCHED_IDLE,
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
	                                 g_param_spec_boxed ("index-recursive-directories",
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
	                                                     G_TYPE_STRV,
	                                                     G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_INDEX_SINGLE_DIRECTORIES,
	                                 g_param_spec_boxed ("index-single-directories",
	                                                     "Index single directories",
	                                                     " List of directories to index but not sub-directories for changes (separator=;)\n"
	                                                     " Special values used for IndexRecursiveDirectories can also be used here",
	                                                     G_TYPE_STRV,
	                                                     G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_IGNORED_DIRECTORIES,
	                                 g_param_spec_boxed ("ignored-directories",
	                                                     "Ignored directories",
	                                                     " List of directories to NOT crawl for indexing (separator=;)",
	                                                     G_TYPE_STRV,
	                                                     G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_IGNORED_DIRECTORIES_WITH_CONTENT,
	                                 g_param_spec_boxed ("ignored-directories-with-content",
	                                                     "Ignored directories with content",
	                                                     " List of directories to NOT crawl for indexing based on child files (separator=;)",
	                                                     G_TYPE_STRV,
	                                                     G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_IGNORED_FILES,
	                                 g_param_spec_boxed ("ignored-files",
	                                                     "Ignored files",
	                                                     " List of files to NOT index (separator=;)",
	                                                     G_TYPE_STRV,
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

	/* Writeback */
	g_object_class_install_property (object_class,
	                                 PROP_ENABLE_WRITEBACK,
	                                 g_param_spec_boolean ("enable-writeback",
	                                                       "Enable Writeback",
	                                                       "Set to false to disable writeback",
	                                                       DEFAULT_ENABLE_WRITEBACK,
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
                     GParamSpec   *pspec)
{
	switch (param_id) {
		/* General */
		/* NOTE: We handle these because we have to be able
		 * to save these based on command line overrides.
		 */
	case PROP_VERBOSITY:
		tracker_config_set_verbosity (TRACKER_CONFIG (object),
		                              g_value_get_enum (value));
		break;
	case PROP_INITIAL_SLEEP:
		tracker_config_set_initial_sleep (TRACKER_CONFIG (object),
		                                  g_value_get_int (value));
		break;

		/* Indexing */
		/* NOTE: We handle these specifically because we
		 * create convenience data around these lists.
		 */
	case PROP_INDEX_RECURSIVE_DIRECTORIES: {
		GStrv strv = g_value_get_boxed (value);
		GSList *dirs = tracker_string_list_to_gslist (strv, -1);

		config_set_index_recursive_directories (TRACKER_CONFIG (object), dirs);

		g_slist_foreach (dirs, (GFunc) g_free, NULL);
		g_slist_free (dirs);

		break;
	}
	case PROP_INDEX_SINGLE_DIRECTORIES: {
		GStrv strv = g_value_get_boxed (value);
		GSList *dirs = tracker_string_list_to_gslist (strv, -1);

		config_set_index_single_directories (TRACKER_CONFIG (object), dirs);

		g_slist_foreach (dirs, (GFunc) g_free, NULL);
		g_slist_free (dirs);
		break;
	}
	case PROP_IGNORED_DIRECTORIES: {
		GStrv strv = g_value_get_boxed (value);
		GSList *dirs = tracker_string_list_to_gslist (strv, -1);

		config_set_ignored_directories (TRACKER_CONFIG (object), dirs);

		g_slist_foreach (dirs, (GFunc) g_free, NULL);
		g_slist_free (dirs);
		break;
	}
	case PROP_IGNORED_DIRECTORIES_WITH_CONTENT: {
		GStrv strv = g_value_get_boxed (value);
		GSList *dirs = tracker_string_list_to_gslist (strv, -1);

		config_set_ignored_directories_with_content (TRACKER_CONFIG (object), dirs);

		g_slist_foreach (dirs, (GFunc) g_free, NULL);
		g_slist_free (dirs);
		break;
	}
	case PROP_IGNORED_FILES: {
		GStrv strv = g_value_get_boxed (value);
		GSList *files = tracker_string_list_to_gslist (strv, -1);

		config_set_ignored_files (TRACKER_CONFIG (object), files);

		g_slist_foreach (files, (GFunc) g_free, NULL);
		g_slist_free (files);
		break;
	}
	default:
		/* We don't care about the others... we don't save anyway. */
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
	TrackerConfigPrivate *priv = config->priv;

	switch (param_id) {
		/* General */
	case PROP_VERBOSITY:
		g_value_set_enum (value, tracker_config_get_verbosity (config));
		break;
	case PROP_SCHED_IDLE:
		g_value_set_enum (value, tracker_config_get_sched_idle (config));
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
		g_value_take_boxed (value, tracker_gslist_to_string_list (priv->index_recursive_directories_unfiltered));
		break;
	case PROP_INDEX_SINGLE_DIRECTORIES:
		g_value_take_boxed (value, tracker_gslist_to_string_list (priv->index_single_directories_unfiltered));
		break;
	case PROP_IGNORED_DIRECTORIES:
		g_value_take_boxed (value, tracker_gslist_to_string_list (priv->ignored_directories));
		break;
	case PROP_IGNORED_DIRECTORIES_WITH_CONTENT:
		g_value_take_boxed (value, tracker_gslist_to_string_list (priv->ignored_directories_with_content));
		break;
	case PROP_IGNORED_FILES:
		g_value_take_boxed (value, tracker_gslist_to_string_list (priv->ignored_files));
		break;
	case PROP_CRAWLING_INTERVAL:
		g_value_set_int (value, tracker_config_get_crawling_interval (config));
		break;
	case PROP_REMOVABLE_DAYS_THRESHOLD:
		g_value_set_int (value, tracker_config_get_removable_days_threshold (config));
		break;

	/* Writeback */
	case PROP_ENABLE_WRITEBACK:
		g_value_set_boolean (value, tracker_config_get_enable_writeback (config));
		break;

	/* Did we miss any new properties? */
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

	if (priv->config_file) {
		g_signal_handlers_disconnect_by_func (priv->config_file,
		                                      config_file_changed_cb,
		                                      TRACKER_CONFIG (object));
		g_object_unref (priv->config_file);
		priv->config_file = NULL;
	}

	(G_OBJECT_CLASS (tracker_config_parent_class)->finalize) (object);
}

static gchar *
get_user_special_dir_if_not_home (GUserDirectory directory)
{
	const gchar *path;

	path = g_get_user_special_dir (directory);

	if (g_strcmp0 (path, g_get_home_dir ()) == 0) {
		/* ignore XDG directories set to $HOME */
		return NULL;
	} else {
		return g_strdup (path);
	}
}

static GSList *
dir_mapping_get (GSList   *dirs,
                 gboolean  is_recursive)
{
	GSList *filtered = NULL;
	GSList *evaluated_dirs, *l;

	if (dirs) {
		filtered = tracker_path_list_filter_duplicates (dirs, ".", is_recursive);
	}

	evaluated_dirs = NULL;

	for (l = filtered; l; l = l->next) {
		gchar *path_to_use;

		/* Must be a special dir */
		if (strcmp (l->data, "&DESKTOP") == 0) {
			path_to_use = get_user_special_dir_if_not_home (G_USER_DIRECTORY_DESKTOP);
		} else if (strcmp (l->data, "&DOCUMENTS") == 0) {
			path_to_use = get_user_special_dir_if_not_home (G_USER_DIRECTORY_DOCUMENTS);
		} else if (strcmp (l->data, "&DOWNLOAD") == 0) {
			path_to_use = get_user_special_dir_if_not_home (G_USER_DIRECTORY_DOWNLOAD);
		} else if (strcmp (l->data, "&MUSIC") == 0) {
			path_to_use = get_user_special_dir_if_not_home (G_USER_DIRECTORY_MUSIC);
		} else if (strcmp (l->data, "&PICTURES") == 0) {
			path_to_use = get_user_special_dir_if_not_home (G_USER_DIRECTORY_PICTURES);
		} else if (strcmp (l->data, "&PUBLIC_SHARE") == 0) {
			path_to_use = get_user_special_dir_if_not_home (G_USER_DIRECTORY_PUBLIC_SHARE);
		} else if (strcmp (l->data, "&TEMPLATES") == 0) {
			path_to_use = get_user_special_dir_if_not_home (G_USER_DIRECTORY_TEMPLATES);
		} else if (strcmp (l->data, "&VIDEOS") == 0) {
			path_to_use = get_user_special_dir_if_not_home (G_USER_DIRECTORY_VIDEOS);
		} else {
			path_to_use = tracker_path_evaluate_name (l->data);
		}

		if (path_to_use) {
			evaluated_dirs = g_slist_prepend (evaluated_dirs, path_to_use);
		}
	}

	g_slist_foreach (filtered, (GFunc) g_free, NULL);
	g_slist_free (filtered);

	return g_slist_reverse (evaluated_dirs);
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
	TrackerConfig *config;
	TrackerConfigFile *config_file;
	GSettings *settings;

	(G_OBJECT_CLASS (tracker_config_parent_class)->constructed) (object);

	config = TRACKER_CONFIG (object);
	settings = G_SETTINGS (object);

	/* NOTE: Without the _delay() call the updates to settings
	 * from tracker-preferences may not happen before we notify
	 * about the property change from _set_*() APIs. This is
	 * because the GValue in set_property() is not up to date at
	 * the time we are called back. Quite fscking stupid really if
	 * you ask me.
	 *
	 * NOTE: We need this. If we don't we can't have local
	 * settings which are *NOT* stored in the GSettings database.
	 * We need this for overriding things like verbosity on start
	 * up.
	 */
	g_settings_delay (settings);

	/* Set up bindings */
	g_settings_bind (settings, "verbosity", object, "verbosity", G_SETTINGS_BIND_GET_NO_CHANGES);
	g_settings_bind (settings, "sched-idle", object, "sched-idle", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "initial-sleep", object, "initial-sleep", G_SETTINGS_BIND_GET_NO_CHANGES);
	g_settings_bind (settings, "throttle", object, "throttle", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "low-disk-space-limit", object, "low-disk-space-limit", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "crawling-interval", object, "crawling-interval", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "low-disk-space-limit", object, "low-disk-space-limit", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "removable-days-threshold", object, "removable-days-threshold", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "enable-monitors", object, "enable-monitors", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "enable-writeback", object, "enable-writeback", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "index-removable-devices", object, "index-removable-devices", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "index-optical-discs", object, "index-optical-discs", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "index-on-battery", object, "index-on-battery", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "index-on-battery-first-time", object, "index-on-battery-first-time", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "index-recursive-directories", object, "index-recursive-directories", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "index-single-directories", object, "index-single-directories", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "ignored-files", object, "ignored-files", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "ignored-directories", object, "ignored-directories", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, "ignored-directories-with-content", object, "ignored-directories-with-content", G_SETTINGS_BIND_GET);

	/* Migrate keyfile-based configuration */
	config_file = tracker_config_file_new ();

	if (config_file) {
		/* NOTE: Migration works both ways... */
		tracker_config_file_migrate (config_file, settings, migration);

		if (G_UNLIKELY (g_getenv ("TRACKER_USE_CONFIG_FILES"))) {
			TrackerConfigPrivate *priv;

			tracker_config_file_load_from_file (config_file, G_OBJECT (config), migration);
			g_signal_connect (config_file, "changed", G_CALLBACK (config_file_changed_cb), config);

			priv = config->priv;
			priv->config_file = config_file;
		} else {
			g_object_unref (config_file);
		}
	}

	config_set_ignored_file_conveniences (TRACKER_CONFIG (object));
	config_set_ignored_directory_conveniences (TRACKER_CONFIG (object));
}

static void
config_file_changed_cb (TrackerConfigFile *config_file,
                        gpointer           user_data)
{
	GSettings *settings = G_SETTINGS (user_data);

	tracker_info ("Settings have changed in INI file, we need to restart to take advantage of those changes!");
	tracker_config_file_import_to_settings (config_file, settings, migration);
}

TrackerConfig *
tracker_config_new (void)
{
	return g_object_new (TRACKER_TYPE_CONFIG,
	                     "schema-id", "org.freedesktop.Tracker.Miner.Files",
	                     "path", "/org/freedesktop/tracker/miner/files/",
	                     NULL);
}

gint
tracker_config_get_verbosity (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_VERBOSITY);

	return g_settings_get_enum (G_SETTINGS (config), "verbosity");
}

gint
tracker_config_get_sched_idle (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_SCHED_IDLE);

	return g_settings_get_enum (G_SETTINGS (config), "sched-idle");
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

gboolean
tracker_config_get_enable_writeback (TrackerConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_WRITEBACK);

	return g_settings_get_boolean (G_SETTINGS (config), "enable-writeback");
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
tracker_config_get_index_single_directories (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = config->priv;

	return priv->index_single_directories;
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

	/* g_object_set (G_OBJECT (config), "verbosity", value, NULL); */
	g_settings_set_enum (G_SETTINGS (config), "verbosity", value);
	g_object_notify (G_OBJECT (config), "verbosity");
}

void
tracker_config_set_initial_sleep (TrackerConfig *config,
                                  gint           value)
{
	g_return_if_fail (TRACKER_IS_CONFIG (config));

	/* g_object_set (G_OBJECT (config), "initial-sleep", value, NULL); */
	g_settings_set_int (G_SETTINGS (config), "initial-sleep", value);
	g_object_notify (G_OBJECT (config), "initial-sleep");
}

static void
rebuild_filtered_lists (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;
	GSList *old_list;

	/* This function does 3 things:
	 * 1. Convert aliases like &DESKTOP to real paths
	 * 2. Filter and remove duplicates
	 * 3. Save the new list to the lists we return with public API
	 *
	 * Importantly, we:
	 * 1. Only notify on changes.
	 * 2. Don't update the unfiltered lists (since they have aliases)
	 */
	priv = config->priv;

	/* Filter single directories first, checking duplicates */
	old_list = priv->index_single_directories;
	priv->index_single_directories = NULL;

	if (priv->index_single_directories_unfiltered) {
		GSList *mapped_dirs = dir_mapping_get (priv->index_single_directories_unfiltered, FALSE);

		priv->index_single_directories =
			tracker_path_list_filter_duplicates (mapped_dirs, ".", FALSE);

		if (mapped_dirs) {
			g_slist_foreach (mapped_dirs, (GFunc) g_free, NULL);
			g_slist_free (mapped_dirs);
		}
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
		GSList *l, *checked_dirs = NULL;
		GSList *mapped_dirs;

		/* First, translate aliases */
		mapped_dirs = dir_mapping_get (priv->index_recursive_directories_unfiltered, TRUE);

		/* Second, remove elements already in single directories */
		for (l = mapped_dirs; l; l = l->next) {
			if (g_slist_find_custom (priv->index_single_directories,
			                         l->data,
			                         (GCompareFunc) g_strcmp0) != NULL) {
				g_message ("Path '%s' being removed from recursive directories "
				           "list, as it also exists in single directories list",
				           (gchar *) l->data);
			} else {
				checked_dirs = g_slist_prepend (checked_dirs, l->data);
			}
		}

		g_slist_free (mapped_dirs);
		checked_dirs = g_slist_reverse (checked_dirs);

		/* Third, clean up any duplicates */
		priv->index_recursive_directories =
			tracker_path_list_filter_duplicates (checked_dirs, ".", TRUE);

		g_slist_foreach (checked_dirs, (GFunc) g_free, NULL);
		g_slist_free (checked_dirs);
	}

	if (!tracker_gslist_with_string_data_equal (old_list, priv->index_recursive_directories)) {
		g_object_notify (G_OBJECT (config), "index-recursive-directories");
	}

	if (old_list) {
		g_slist_foreach (old_list, (GFunc) g_free, NULL);
		g_slist_free (old_list);
	}
}

static void
config_set_index_recursive_directories (TrackerConfig *config,
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

static void
config_set_index_single_directories (TrackerConfig *config,
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

static void
config_set_ignored_directories (TrackerConfig *config,
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

static void
config_set_ignored_directories_with_content (TrackerConfig *config,
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

static void
config_set_ignored_files (TrackerConfig *config,
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
