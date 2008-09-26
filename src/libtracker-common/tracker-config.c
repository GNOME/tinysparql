/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007, Michal Pryc (Michal.Pryc@Sun.Com)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
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

#include "tracker-language.h"
#include "tracker-config.h"
#include "tracker-file-utils.h"

#define TRACKER_CONFIG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_CONFIG, TrackerConfigPrivate))

/* GKeyFile defines */
#define GROUP_GENERAL				 "General"
#define KEY_VERBOSITY				 "Verbosity"
#define KEY_INITIAL_SLEEP			 "InitialSleep"
#define KEY_LOW_MEMORY_MODE			 "LowMemoryMode"
#define KEY_NFS_LOCKING				 "NFSLocking"
#define GROUP_WATCHES				 "Watches"
#define KEY_WATCH_DIRECTORY_ROOTS		 "WatchDirectoryRoots"
#define KEY_CRAWL_DIRECTORY_ROOTS		 "CrawlDirectory"
#define KEY_NO_WATCH_DIRECTORY_ROOTS		 "NoWatchDirectory"
#define KEY_ENABLE_WATCHES			 "EnableWatching"

#define GROUP_INDEXING				 "Indexing"
#define KEY_THROTTLE				 "Throttle"
#define KEY_ENABLE_INDEXING			 "EnableIndexing"
#define KEY_ENABLE_CONTENT_INDEXING		 "EnableFileContentIndexing"
#define KEY_ENABLE_THUMBNAILS			 "EnableThumbnails"
#define KEY_DISABLED_MODULES			 "DisabledModules"
#define KEY_FAST_MERGES				 "FastMerges"
#define KEY_NO_INDEX_FILE_TYPES			 "NoIndexFileTypes"
#define KEY_MIN_WORD_LENGTH			 "MinWorldLength"
#define KEY_MAX_WORD_LENGTH			 "MaxWorldLength"
#define KEY_LANGUAGE				 "Language"
#define KEY_ENABLE_STEMMER			 "EnableStemmer"
#define KEY_DISABLE_INDEXING_ON_BATTERY		 "BatteryIndex"
#define KEY_DISABLE_INDEXING_ON_BATTERY_INIT	 "BatteryIndexInitial"
#define KEY_LOW_DISK_SPACE_LIMIT		 "LowDiskSpaceLimit"
#define KEY_INDEX_MOUNTED_DIRECTORIES		 "IndexMountedDirectories"
#define KEY_INDEX_REMOVABLE_DEVICES		 "IndexRemovableMedia"

#define GROUP_PERFORMANCE			 "Performance"
#define KEY_MAX_TEXT_TO_INDEX			 "MaxTextToIndex"
#define KEY_MAX_WORDS_TO_INDEX			 "MaxWordsToIndex"
#define KEY_MAX_BUCKET_COUNT			 "MaxBucketCount"
#define KEY_MIN_BUCKET_COUNT			 "MinBucketCount"

#define GROUP_SERVICES				 "Services"
#define KEY_ENABLE_XESAM			 "EnableXesam"

/* Default values */
#define DEFAULT_VERBOSITY			 0
#define DEFAULT_INITIAL_SLEEP			 45	  /* 0->1000 */
#define DEFAULT_LOW_MEMORY_MODE			 FALSE
#define DEFAULT_NFS_LOCKING			 FALSE
#define DEFAULT_ENABLE_WATCHES			 TRUE
#define DEFAULT_THROTTLE			 0	  /* 0->20 */
#define DEFAULT_ENABLE_INDEXING			 TRUE
#define DEFAULT_ENABLE_XESAM			 FALSE
#define DEFAULT_ENABLE_CONTENT_INDEXING		 TRUE
#define DEFAULT_ENABLE_THUMBNAILS		 TRUE
#define DEFAULT_FAST_MERGES			 FALSE
#define DEFAULT_MIN_WORD_LENGTH			 3	  /* 0->30 */
#define DEFAULT_MAX_WORD_LENGTH			 30	  /* 0->200 */
#define DEFAULT_ENABLE_STEMMER			 TRUE
#define DEFAULT_DISABLE_INDEXING_ON_BATTERY	 TRUE
#define DEFAULT_DISABLE_INDEXING_ON_BATTERY_INIT FALSE
#define DEFAULT_INDEX_MOUNTED_DIRECTORIES	 TRUE
#define DEFAULT_INDEX_REMOVABLE_DEVICES		 TRUE
#define DEFAULT_LOW_DISK_SPACE_LIMIT		 1	  /* 0->100 / -1 */
#define DEFAULT_MAX_TEXT_TO_INDEX		 1048576  /* Bytes */
#define DEFAULT_MAX_WORDS_TO_INDEX		 10000
#define DEFAULT_MAX_BUCKET_COUNT		 524288
#define DEFAULT_MIN_BUCKET_COUNT		 65536

typedef struct _TrackerConfigPrivate TrackerConfigPrivate;

struct _TrackerConfigPrivate {
	GFile	     *file;
	GFileMonitor *monitor;

	/* General */
	gint	      verbosity;
	gint	      initial_sleep;
	gboolean      low_memory_mode;
	gboolean      nfs_locking;

	/* Watches */
	GSList	     *watch_directory_roots;
	GSList	     *crawl_directory_roots;
	GSList	     *no_watch_directory_roots;
	gboolean      enable_watches;

	/* Indexing */
	gint	      throttle;
	gboolean      enable_indexing;
	gboolean      enable_content_indexing;
	gboolean      enable_thumbnails;
	GSList	     *disabled_modules;
	gboolean      fast_merges;
	GSList	     *no_index_file_types;
	gint	      min_word_length;
	gint	      max_word_length;
	gchar	     *language;
	gboolean      enable_stemmer;
	gboolean      disable_indexing_on_battery;
	gboolean      disable_indexing_on_battery_init;
	gint	      low_disk_space_limit;
	gboolean      index_mounted_directories;
	gboolean      index_removable_devices;

	/* Performance */
	gint	      max_text_to_index;
	gint	      max_words_to_index;
	gint	      max_bucket_count;
	gint	      min_bucket_count;

	/* Services*/
	gboolean      enable_xesam;
};

static void config_finalize	(GObject      *object);
static void config_get_property (GObject      *object,
				 guint	       param_id,
				 GValue	      *value,
				 GParamSpec   *pspec);
static void config_set_property (GObject      *object,
				 guint	       param_id,
				 const GValue *value,
				 GParamSpec   *pspec);
static void config_load		(TrackerConfig *config);

enum {
	PROP_0,

	/* General */
	PROP_VERBOSITY,
	PROP_INITIAL_SLEEP,
	PROP_LOW_MEMORY_MODE,
	PROP_NFS_LOCKING,

	/* Watches */
	PROP_WATCH_DIRECTORY_ROOTS,
	PROP_CRAWL_DIRECTORY_ROOTS,
	PROP_NO_WATCH_DIRECTORY_ROOTS,
	PROP_ENABLE_WATCHES,

	/* Indexing */
	PROP_THROTTLE,
	PROP_ENABLE_INDEXING,
	PROP_ENABLE_CONTENT_INDEXING,
	PROP_ENABLE_THUMBNAILS,
	PROP_DISABLED_MODULES,
	PROP_FAST_MERGES,
	PROP_NO_INDEX_FILE_TYPES,
	PROP_MIN_WORD_LENGTH,
	PROP_MAX_WORD_LENGTH,
	PROP_LANGUAGE,
	PROP_ENABLE_STEMMER,
	PROP_DISABLE_INDEXING_ON_BATTERY,
	PROP_DISABLE_INDEXING_ON_BATTERY_INIT,
	PROP_LOW_DISK_SPACE_LIMIT,
	PROP_INDEX_MOUNTED_DIRECTORIES,
	PROP_INDEX_REMOVABLE_DEVICES,

	/* Performance */
	PROP_MAX_TEXT_TO_INDEX,
	PROP_MAX_WORDS_TO_INDEX,
	PROP_MAX_BUCKET_COUNT,
	PROP_MIN_BUCKET_COUNT,

	/* Services*/
	PROP_ENABLE_XESAM
};

G_DEFINE_TYPE (TrackerConfig, tracker_config, G_TYPE_OBJECT);

static void
tracker_config_class_init (TrackerConfigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = config_finalize;
	object_class->get_property = config_get_property;
	object_class->set_property = config_set_property;

	/* General */
	g_object_class_install_property (object_class,
					 PROP_VERBOSITY,
					 g_param_spec_int ("verbosity",
							   "Log verbosity",
							   "How much logging we have "
							   "(0=errors, 1=minimal, 2=detailed, 3=debug)",
							   0,
							   3,
							   DEFAULT_VERBOSITY,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_INITIAL_SLEEP,
					 g_param_spec_int ("initial-sleep",
							   "Initial sleep",
							   "Initial sleep time in seconds "
							   "(0->1000)",
							   0,
							   1000,
							   DEFAULT_INITIAL_SLEEP,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_LOW_MEMORY_MODE,
					 g_param_spec_boolean ("low-memory-mode",
							       "Use extra memory",
							       "Use extra memory at the "
							       "expense of indexing speed",
							       DEFAULT_LOW_MEMORY_MODE,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_NFS_LOCKING,
					 g_param_spec_boolean ("nfs-locking",
							       "Use NFS friendly location for lock file",
							       "In NFS filesystems is not safe to have "
							       "the lock file in the home directory",
							       DEFAULT_NFS_LOCKING,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/* Watches */
	g_object_class_install_property (object_class,
					 PROP_WATCH_DIRECTORY_ROOTS,
					 g_param_spec_pointer ("watch-directory-roots",
							       "Watched directory roots",
							       "This is a GSList of directory roots "
							       "to index and watch",
							       G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					 PROP_CRAWL_DIRECTORY_ROOTS,
					 g_param_spec_pointer ("crawl-directory-roots",
							       "Crawl directory roots",
							       "This is a GSList of directory roots "
							       "to index but NOT watch",
							       G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					 PROP_NO_WATCH_DIRECTORY_ROOTS,
					 g_param_spec_pointer ("no-watch-directory-roots",
							       "Not watched directory roots",
							       "This is a GSList of directory roots "
							       "to NOT index and NOT watch",
							       G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					 PROP_ENABLE_WATCHES,
					 g_param_spec_boolean ("enable-watches",
							       "Enable watches",
							       "You can disable all watches "
							       "by setting this FALSE",
							       DEFAULT_ENABLE_WATCHES,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/* Indexing */
	g_object_class_install_property (object_class,
					 PROP_THROTTLE,
					 g_param_spec_int ("throttle",
							   "Throttle",
							   "Throttle indexing, higher value "
							   "is slower (0->20)",
							   0,
							   20,
							   DEFAULT_THROTTLE,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_ENABLE_INDEXING,
					 g_param_spec_boolean ("enable-indexing",
							       "Enable indexing",
							       "All indexing",
							       DEFAULT_ENABLE_INDEXING,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_ENABLE_CONTENT_INDEXING,
					 g_param_spec_boolean ("enable-content-indexing",
							       "Enable content indexing",
							       "Content specific indexing "
							       "(i.e. file content)",
							       DEFAULT_ENABLE_CONTENT_INDEXING,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_ENABLE_THUMBNAILS,
					 g_param_spec_boolean ("enable-thumbnails",
							       "Enable thumbnails",
							       "Create thumbnails from image based files",
							       DEFAULT_ENABLE_THUMBNAILS,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_DISABLED_MODULES,
					 g_param_spec_pointer ("disabled-modules",
							       "Disabled modules",
							       "Modules to disable, like 'files', etc.",
							       G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					 PROP_FAST_MERGES,
					 g_param_spec_boolean ("fast-merges",
							       "Fast merges",
							       "Spends more disk usage if TRUE",
							       DEFAULT_FAST_MERGES,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_NO_INDEX_FILE_TYPES,
					 g_param_spec_pointer ("no-index-file-types",
							       "File types to not index",
							       "This is a GSList of file types "
							       "to NOT index",
							       G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					 PROP_MIN_WORD_LENGTH,
					 g_param_spec_int ("min-word-length",
							   "Minimum word length",
							   "Minimum word length used to index "
							   "(0->30)",
							   0,
							   30,
							   DEFAULT_MIN_WORD_LENGTH,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_MAX_WORD_LENGTH,
					 g_param_spec_int ("max-word-length",
							   "Maximum word length",
							   "Maximum word length used to index",
							   0,
							   200, /* Is this a reasonable limit? */
							   DEFAULT_MAX_WORD_LENGTH,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_LANGUAGE,
					 g_param_spec_string ("language",
							      "Language",
							      "Language to use with stemming "
							      "('en', 'fr', 'sv', etc)",
							      "en",
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_ENABLE_STEMMER,
					 g_param_spec_boolean ("enable-stemmer",
							       "Enable stemmer",
							       "Language specific stemmer",
							       DEFAULT_ENABLE_STEMMER,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_DISABLE_INDEXING_ON_BATTERY,
					 g_param_spec_boolean ("disable-indexing-on-battery",
							       "Disable indexing on battery",
							       "Don't index when using AC battery",
							       DEFAULT_DISABLE_INDEXING_ON_BATTERY,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_DISABLE_INDEXING_ON_BATTERY_INIT,
					 g_param_spec_boolean ("disable-indexing-on-battery-init",
							       "Disable indexing on battery",
							       "Don't index when using AC "
							       "battery initially",
							       DEFAULT_DISABLE_INDEXING_ON_BATTERY_INIT,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_LOW_DISK_SPACE_LIMIT,
					 g_param_spec_int ("low-disk-space-limit",
							   "Low disk space limit",
							   "Pause the indexer when the "
							   "disk space is below this percentage "
							   "(-1=off, 0->100)",
							   -1,
							   100,
							   DEFAULT_LOW_DISK_SPACE_LIMIT,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_INDEX_MOUNTED_DIRECTORIES,
					 g_param_spec_boolean ("index-mounted-directories",
							       "Index mounted directories",
							       "Don't traverse mounted directories "
							       "which are not on the same file system",
							       DEFAULT_INDEX_MOUNTED_DIRECTORIES,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_INDEX_REMOVABLE_DEVICES,
					 g_param_spec_boolean ("index-removable-devices",
							       "index removable devices",
							       "Don't traverse mounted directories "
							       "which are for removable devices",
							       DEFAULT_INDEX_REMOVABLE_DEVICES,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/* Performance */
	g_object_class_install_property (object_class,
					 PROP_MAX_TEXT_TO_INDEX,
					 g_param_spec_int ("max-text-to-index",
							   "Maximum text to index",
							   "Maximum text in bytes to index "
							   "from file's content",
							   0,
							   G_MAXINT,
							   DEFAULT_MAX_TEXT_TO_INDEX,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_MAX_WORDS_TO_INDEX,
					 g_param_spec_int ("max-words-to-index",
							   "Maximum words to index",
							   "Maximum unique words to index "
							   "from file's content",
							   0,
							   G_MAXINT,
							   DEFAULT_MAX_WORDS_TO_INDEX,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_MAX_BUCKET_COUNT,
					 g_param_spec_int ("max-bucket-count",
							   "Maximum bucket count",
							   "Maximum bucket count (1000->524288)",
							   1000,
							   G_MAXINT, /* FIXME: Is this reasonable? */
							   DEFAULT_MAX_BUCKET_COUNT,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_MIN_BUCKET_COUNT,
					 g_param_spec_int ("min-bucket-count",
							   "Minimum bucket count",
							   "Minimum bucket count (1000->65536)",
							   1000,
							   G_MAXINT,
							   DEFAULT_MIN_BUCKET_COUNT,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/* Services */
	g_object_class_install_property (object_class,
					 PROP_ENABLE_XESAM,
					 g_param_spec_boolean ("enable-xesam",
							       "Enable Xesam",
							       "Xesam DBus service",
							       DEFAULT_ENABLE_XESAM,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerConfigPrivate));
}

static void
tracker_config_init (TrackerConfig *object)
{
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

	g_slist_foreach (priv->no_index_file_types, (GFunc) g_free, NULL);
	g_slist_free (priv->no_index_file_types);

	g_free (priv->language);

	if (priv->monitor) {
		g_object_unref (priv->monitor);
	}

	if (priv->file) {
		g_object_unref (priv->file);
	}

	(G_OBJECT_CLASS (tracker_config_parent_class)->finalize) (object);
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
	case PROP_LOW_MEMORY_MODE:
		g_value_set_boolean (value, priv->low_memory_mode);
		break;
	case PROP_NFS_LOCKING:
		g_value_set_boolean (value, priv->nfs_locking);
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
	case PROP_ENABLE_INDEXING:
		g_value_set_boolean (value, priv->enable_indexing);
		break;
	case PROP_ENABLE_CONTENT_INDEXING:
		g_value_set_boolean (value, priv->enable_content_indexing);
		break;
	case PROP_ENABLE_THUMBNAILS:
		g_value_set_boolean (value, priv->enable_thumbnails);
		break;
	case PROP_DISABLED_MODULES:
		g_value_set_pointer (value, priv->disabled_modules);
		break;
	case PROP_FAST_MERGES:
		g_value_set_boolean (value, priv->fast_merges);
		break;
	case PROP_NO_INDEX_FILE_TYPES:
		g_value_set_pointer (value, priv->no_index_file_types);
		break;
	case PROP_MIN_WORD_LENGTH:
		g_value_set_int (value, priv->min_word_length);
		break;
	case PROP_MAX_WORD_LENGTH:
		g_value_set_int (value, priv->max_word_length);
		break;
	case PROP_LANGUAGE:
		g_value_set_string (value, priv->language);
		break;
	case PROP_ENABLE_STEMMER:
		g_value_set_boolean (value, priv->enable_stemmer);
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

		/* Performance */
	case PROP_MAX_TEXT_TO_INDEX:
		g_value_set_int (value, priv->max_text_to_index);
		break;
	case PROP_MAX_WORDS_TO_INDEX:
		g_value_set_int (value, priv->max_words_to_index);
		break;
	case PROP_MAX_BUCKET_COUNT:
		g_value_set_int (value, priv->max_bucket_count);
		break;
	case PROP_MIN_BUCKET_COUNT:
		g_value_set_int (value, priv->min_bucket_count);
		break;

	/* Services */
	case PROP_ENABLE_XESAM:
		g_value_set_boolean (value, priv->enable_xesam);
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
	TrackerConfigPrivate *priv;

	priv = TRACKER_CONFIG_GET_PRIVATE (object);

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
	case PROP_LOW_MEMORY_MODE:
		tracker_config_set_low_memory_mode (TRACKER_CONFIG (object),
						    g_value_get_boolean (value));
		break;
	case PROP_NFS_LOCKING:
		tracker_config_set_nfs_locking (TRACKER_CONFIG (object),
						g_value_get_boolean (value));
		break;

		/* Watches */
	case PROP_WATCH_DIRECTORY_ROOTS:    /* Not writable */
	case PROP_CRAWL_DIRECTORY_ROOTS:    /* Not writable */
	case PROP_NO_WATCH_DIRECTORY_ROOTS: /* Not writable */
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
	case PROP_ENABLE_INDEXING:
		tracker_config_set_enable_indexing (TRACKER_CONFIG (object),
						    g_value_get_boolean (value));
		break;
	case PROP_ENABLE_CONTENT_INDEXING:
		tracker_config_set_enable_content_indexing (TRACKER_CONFIG (object),
							    g_value_get_boolean (value));
		break;
	case PROP_ENABLE_THUMBNAILS:
		tracker_config_set_enable_thumbnails (TRACKER_CONFIG (object),
						      g_value_get_boolean (value));
		break;
	case PROP_DISABLED_MODULES:
		/* Not writable */
		break;
	case PROP_FAST_MERGES:
		tracker_config_set_fast_merges (TRACKER_CONFIG (object),
						g_value_get_boolean (value));
		break;
	case PROP_NO_INDEX_FILE_TYPES:
		/* Not writable */
		break;
	case PROP_MIN_WORD_LENGTH:
		tracker_config_set_min_word_length (TRACKER_CONFIG (object),
						    g_value_get_int (value));
		break;
	case PROP_MAX_WORD_LENGTH:
		tracker_config_set_max_word_length (TRACKER_CONFIG (object),
						    g_value_get_int (value));
		break;
	case PROP_LANGUAGE:
		tracker_config_set_language (TRACKER_CONFIG (object),
					     g_value_get_string (value));
		break;
	case PROP_ENABLE_STEMMER:
		tracker_config_set_enable_stemmer (TRACKER_CONFIG (object),
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
	case PROP_INDEX_MOUNTED_DIRECTORIES:
		tracker_config_set_index_mounted_directories (TRACKER_CONFIG (object),
							      g_value_get_boolean (value));
		break;
	case PROP_INDEX_REMOVABLE_DEVICES:
		tracker_config_set_index_removable_devices (TRACKER_CONFIG (object),
								g_value_get_boolean (value));
		break;

		/* Performance */
	case PROP_MAX_TEXT_TO_INDEX:
		tracker_config_set_max_text_to_index (TRACKER_CONFIG (object),
						      g_value_get_int (value));
		break;
	case PROP_MAX_WORDS_TO_INDEX:
		tracker_config_set_max_words_to_index (TRACKER_CONFIG (object),
						       g_value_get_int (value));
		break;
	case PROP_MAX_BUCKET_COUNT:
		tracker_config_set_max_bucket_count (TRACKER_CONFIG (object),
						     g_value_get_int (value));
		break;
	case PROP_MIN_BUCKET_COUNT:
		tracker_config_set_min_bucket_count (TRACKER_CONFIG (object),
						     g_value_get_int (value));
		break;

	/* Services */
	case PROP_ENABLE_XESAM:
		tracker_config_set_enable_xesam (TRACKER_CONFIG (object),
						    g_value_get_boolean (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
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
			g_warning ("Could not create configuration directory");
			g_free (directory);
			return NULL;
		}
	}

	return directory;
}

static gboolean
config_create_with_defaults (const gchar *filename,
			     GKeyFile	 *key_file)
{
	GError	     *error = NULL;
	gchar	     *content = NULL;
	gchar	     *language;
	const gchar  *watch_directory_roots[2] = { NULL, NULL };
	const gchar  *empty_string_list[] = { NULL };

	/* Get default values */
	language = tracker_language_get_default_code ();

	watch_directory_roots[0] = g_get_home_dir ();

	/* General */
	g_key_file_set_integer (key_file, GROUP_GENERAL, KEY_VERBOSITY, DEFAULT_VERBOSITY);
	g_key_file_set_comment (key_file, GROUP_GENERAL, KEY_VERBOSITY,
				" Log Verbosity (0=errors, 1=minimal, 2=detailed, 3=debug)",
				NULL);
	g_key_file_set_integer (key_file, GROUP_GENERAL, KEY_INITIAL_SLEEP, DEFAULT_INITIAL_SLEEP);
	g_key_file_set_comment (key_file, GROUP_GENERAL, KEY_INITIAL_SLEEP,
				" Initial sleep time in seconds (0->1000)",
				NULL);
	g_key_file_set_boolean (key_file, GROUP_GENERAL, KEY_LOW_MEMORY_MODE, DEFAULT_LOW_MEMORY_MODE);
	g_key_file_set_comment (key_file, GROUP_GENERAL, KEY_LOW_MEMORY_MODE,
				" Minimizes memory use at the expense of indexing speed",
				NULL);
	g_key_file_set_boolean (key_file, GROUP_GENERAL, KEY_NFS_LOCKING, DEFAULT_NFS_LOCKING);
	g_key_file_set_comment (key_file, GROUP_GENERAL, KEY_NFS_LOCKING,
				" Set to TRUE when the home directory is in a NFS filesystem",
				NULL);


	/* Watches */
	g_key_file_set_string_list (key_file, GROUP_WATCHES, KEY_WATCH_DIRECTORY_ROOTS,
				    watch_directory_roots, 1);
	g_key_file_set_comment (key_file, GROUP_WATCHES, KEY_WATCH_DIRECTORY_ROOTS,
				" List of directory roots to index and watch (separator=;)",
				NULL);
	g_key_file_set_string_list (key_file, GROUP_WATCHES, KEY_CRAWL_DIRECTORY_ROOTS,
				    empty_string_list, 0);
	g_key_file_set_comment (key_file, GROUP_WATCHES, KEY_CRAWL_DIRECTORY_ROOTS,
				" List of directory roots to index but NOT watch (separator=;)",
				NULL);
	g_key_file_set_string_list (key_file, GROUP_WATCHES, KEY_NO_WATCH_DIRECTORY_ROOTS,
				    empty_string_list, 0);
	g_key_file_set_comment (key_file, GROUP_WATCHES, KEY_NO_WATCH_DIRECTORY_ROOTS,
				" List of directory roots NOT to index and NOT to watch (separator=;)",
				NULL);
	g_key_file_set_boolean (key_file, GROUP_WATCHES, KEY_ENABLE_WATCHES, DEFAULT_ENABLE_WATCHES);
	g_key_file_set_comment (key_file, GROUP_WATCHES, KEY_ENABLE_WATCHES,
				" Set to false to completely disable any watching",
				NULL);

	/* Indexing */
	g_key_file_set_integer (key_file, GROUP_INDEXING, KEY_THROTTLE, DEFAULT_THROTTLE);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_THROTTLE,
				" Sets the indexing speed (0->20, where 20=slowest speed)",
				NULL);

	g_key_file_set_boolean (key_file, GROUP_INDEXING, KEY_ENABLE_INDEXING, DEFAULT_ENABLE_INDEXING);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_ENABLE_INDEXING,
				" Set to false to completely disable any indexing",
				NULL);
	g_key_file_set_boolean (key_file, GROUP_INDEXING, KEY_ENABLE_CONTENT_INDEXING, DEFAULT_ENABLE_CONTENT_INDEXING);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_ENABLE_CONTENT_INDEXING,
				" Set to false to completely disable file content indexing",
				NULL);
	g_key_file_set_boolean (key_file, GROUP_INDEXING, KEY_ENABLE_THUMBNAILS, DEFAULT_ENABLE_THUMBNAILS);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_ENABLE_THUMBNAILS,
				" Set to false to completely disable thumbnail generation",
				NULL);
	g_key_file_set_string_list (key_file, GROUP_INDEXING, KEY_DISABLED_MODULES,
				    empty_string_list, 0);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_DISABLED_MODULES,
				" List of disabled modules (separator=;)\n"
				" The modules that are indexed are kept in $prefix/share/tracker/modules",
				NULL);
	g_key_file_set_boolean (key_file, GROUP_INDEXING, KEY_FAST_MERGES, DEFAULT_FAST_MERGES);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_FAST_MERGES,
				" Set to false to NOT hog the disk for extended periods",
				NULL);
	g_key_file_set_string_list (key_file, GROUP_INDEXING, KEY_NO_INDEX_FILE_TYPES,
				    empty_string_list, 0);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_NO_INDEX_FILE_TYPES,
				" List of partial file pattern globs (separator=;)\n"
				" This is for files to NOT index\n"
				" (basic stat info is only extended for files that match the patterns)",
				NULL);
	g_key_file_set_integer (key_file, GROUP_INDEXING, KEY_MIN_WORD_LENGTH, DEFAULT_MIN_WORD_LENGTH);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_MIN_WORD_LENGTH,
				" Set the minimum length of words to index (0->30, default=3)",
				NULL);
	g_key_file_set_integer (key_file, GROUP_INDEXING, KEY_MAX_WORD_LENGTH, DEFAULT_MAX_WORD_LENGTH);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_MAX_WORD_LENGTH,
				" Set the maximum length of words to index (0->200, default=30)",
				NULL);
	g_key_file_set_string (key_file, GROUP_INDEXING, KEY_LANGUAGE, language);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_LANGUAGE,
				" Set the language specific stemmer and stopword list to use\n"
				" Values include:\n"
				" - en (English)\n"
				" - da (Danish)\n"
				" - nl (Dutch)\n"
				" - fi (Finish)\n"
				" - fr (French)\n"
				" - de (German)\n"
				" - it (Italian)\n"
				" - nb (Norwegian)\n"
				" - pt (Portugese)\n"
				" - ru (Russian)\n"
				" - es (Spanish)\n"
				" - sv (Swedish)",
				NULL);
	g_key_file_set_boolean (key_file, GROUP_INDEXING, KEY_ENABLE_STEMMER, DEFAULT_ENABLE_STEMMER);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_ENABLE_STEMMER,
				" Set to false to disable language specific stemmer",
				NULL);
	g_key_file_set_boolean (key_file, GROUP_INDEXING, KEY_DISABLE_INDEXING_ON_BATTERY, DEFAULT_DISABLE_INDEXING_ON_BATTERY);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_DISABLE_INDEXING_ON_BATTERY,
				" Set to true to disable indexing when running on battery",
				NULL);
	g_key_file_set_boolean (key_file, GROUP_INDEXING, KEY_DISABLE_INDEXING_ON_BATTERY_INIT, DEFAULT_DISABLE_INDEXING_ON_BATTERY_INIT);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_DISABLE_INDEXING_ON_BATTERY_INIT,
				" Set to true to disable initial indexing when running on battery",
				NULL);
	g_key_file_set_integer (key_file, GROUP_INDEXING, KEY_LOW_DISK_SPACE_LIMIT, DEFAULT_LOW_DISK_SPACE_LIMIT);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_LOW_DISK_SPACE_LIMIT,
				" Pause indexer when disk space is <= this value\n"
				" (0->100, value is in % of $HOME file system, -1=disable pausing)",
				NULL);
	g_key_file_set_boolean (key_file, GROUP_INDEXING, KEY_INDEX_MOUNTED_DIRECTORIES, DEFAULT_INDEX_MOUNTED_DIRECTORIES);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_INDEX_MOUNTED_DIRECTORIES,
				" Set to true to enable traversing mounted directories on other file systems\n"
				" (this excludes removable devices)",
				NULL);
	g_key_file_set_boolean (key_file, GROUP_INDEXING, KEY_INDEX_REMOVABLE_DEVICES, DEFAULT_INDEX_REMOVABLE_DEVICES);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_INDEX_REMOVABLE_DEVICES,
				" Set to true to enable traversing mounted directories for removable devices",
				NULL);

	/* Performance */
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_MAX_TEXT_TO_INDEX, DEFAULT_MAX_TEXT_TO_INDEX);
	g_key_file_set_comment (key_file, GROUP_PERFORMANCE, KEY_MAX_TEXT_TO_INDEX,
				" Maximum text size in bytes to index from a file's content",
				NULL);
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_MAX_WORDS_TO_INDEX, DEFAULT_MAX_WORDS_TO_INDEX);
	g_key_file_set_comment (key_file, GROUP_PERFORMANCE, KEY_MAX_WORDS_TO_INDEX,
				" Maximum unique words to index from a file's content",
				NULL);
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_MAX_BUCKET_COUNT, DEFAULT_MAX_BUCKET_COUNT);
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_MIN_BUCKET_COUNT, DEFAULT_MIN_BUCKET_COUNT);

	/* Services */
	g_key_file_set_boolean (key_file, GROUP_SERVICES, KEY_ENABLE_XESAM, DEFAULT_ENABLE_XESAM);
	g_key_file_set_comment (key_file, GROUP_SERVICES, KEY_ENABLE_XESAM,
				" Xesam DBus service.\n",
				NULL);

	content = g_key_file_to_data (key_file, NULL, &error);
	g_free (language);

	if (error) {
		g_warning ("Couldn't produce default configuration, %s", error->message);
		g_clear_error (&error);
		return FALSE;
	}

	if (!g_file_set_contents (filename, content, -1, &error)) {
		g_warning ("Couldn't write default configuration, %s", error->message);
		g_clear_error (&error);
		g_free (content);
		return FALSE;
	}

	g_print ("Writting default configuration to file:'%s'\n", filename);
	g_free (content);

	return TRUE;
}

static GSList *
config_string_list_to_gslist (const gchar **value,
			      gboolean	    is_directory_list)
{
	GSList *list = NULL;
	gint	i;

	for (i = 0; value[i]; i++) {
		const gchar *str;
		gchar	    *validated;

		str = value[i];
		if (!str || str[0] == '\0') {
			continue;
		}

		if (!is_directory_list) {
			list = g_slist_prepend (list, g_strdup (str));
			continue;
		}

		/* For directories we validate any special characters,
		 * for example '~' and '../../'
		 */
		validated = tracker_path_evaluate_name (str);
		if (validated) {
			list = g_slist_prepend (list, validated);
		}
	}

	return g_slist_reverse (list);
}

static void
config_load_int (TrackerConfig *config,
		 const gchar   *property,
		 GKeyFile      *key_file,
		 const gchar   *group,
		 const gchar   *key)
{
	GError *error = NULL;
	gint	value;

	value = g_key_file_get_integer (key_file, group, key, &error);
	if (!error) {
		g_object_set (G_OBJECT (config), property, value, NULL);
	} else {
		g_message ("Couldn't load config option '%s' (int) in group '%s', %s",
			   property, group, error->message);
		g_error_free (error);
	}
}

static void
config_load_boolean (TrackerConfig *config,
		     const gchar   *property,
		     GKeyFile	   *key_file,
		     const gchar   *group,
		     const gchar   *key)
{
	GError	 *error = NULL;
	gboolean  value;

	value = g_key_file_get_boolean (key_file, group, key, &error);
	if (!error) {
		g_object_set (G_OBJECT (config), property, value, NULL);
	} else {
		g_message ("Couldn't load config option '%s' (bool) in group '%s', %s",
			   property, group, error->message);
		g_error_free (error);
	}
}

static void
config_load_string (TrackerConfig *config,
		    const gchar	  *property,
		    GKeyFile	  *key_file,
		    const gchar	  *group,
		    const gchar	  *key)
{
	GError *error = NULL;
	gchar  *value;

	value = g_key_file_get_string (key_file, group, key, &error);
	if (!error) {
		g_object_set (G_OBJECT (config), property, value, NULL);
	} else {
		g_message ("Couldn't load config option '%s' (string) in group '%s', %s",
			   property, group, error->message);
		g_error_free (error);
	}

	g_free (value);
}

static void
config_load_string_list (TrackerConfig *config,
			 const gchar   *property,
			 GKeyFile      *key_file,
			 const gchar   *group,
			 const gchar   *key)
{
	TrackerConfigPrivate  *priv;
	GSList		      *l;
	gchar		     **value;

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	value = g_key_file_get_string_list (key_file, group, key, NULL, NULL);

	if (strcmp (property, "watch-directory-roots") == 0) {
		if (value) {
			priv->watch_directory_roots = l =
				config_string_list_to_gslist ((const gchar **) value, TRUE);
			priv->watch_directory_roots =
				tracker_path_list_filter_duplicates (priv->watch_directory_roots);

			g_slist_foreach (l, (GFunc) g_free, NULL);
			g_slist_free (l);
		}
	}
	else if (strcmp (property, "crawl-directory-roots") == 0) {
		if (value) {
			priv->crawl_directory_roots = l =
				config_string_list_to_gslist ((const gchar **) value, TRUE);
			priv->crawl_directory_roots =
				tracker_path_list_filter_duplicates (priv->crawl_directory_roots);

			g_slist_foreach (l, (GFunc) g_free, NULL);
			g_slist_free (l);
		}
	}
	else if (strcmp (property, "no-watch-directory-roots") == 0) {
		if (value) {
			priv->no_watch_directory_roots = l =
				config_string_list_to_gslist ((const gchar **) value, TRUE);
			priv->no_watch_directory_roots =
				tracker_path_list_filter_duplicates (priv->no_watch_directory_roots);

			g_slist_foreach (l, (GFunc) g_free, NULL);
			g_slist_free (l);
		}
	}
	else if (strcmp (property, "no-index-file-types") == 0) {
		if (value) {
			priv->no_index_file_types =
				config_string_list_to_gslist ((const gchar **) value, FALSE);
		}
	}
	else if (strcmp (property, "disabled-modules") == 0) {
		if (value) {
			priv->disabled_modules =
				config_string_list_to_gslist ((const gchar **) value, FALSE);
		}
	}
	else {
		g_warning ("Property '%s' not recognized to set string list from key '%s'",
			   property, key);
	}

	g_strfreev (value);
}

static void
config_changed_cb (GFileMonitor     *monitor,
		   GFile	    *file,
		   GFile	    *other_file,
		   GFileMonitorEvent event_type,
		   gpointer	     user_data)
{
	TrackerConfig *config;
	gchar	      *filename;

	config = TRACKER_CONFIG (user_data);

	/* Do we recreate if the file is deleted? */

	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CHANGED:
	case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		filename = g_file_get_path (file);
		g_message ("Config file changed:'%s', reloading settings...",
			   filename);
		g_free (filename);

		config_load (config);
		break;

	default:
		break;
	}
}

static void
config_load (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;
	GKeyFile	     *key_file;
	GError		     *error = NULL;
	gchar		     *filename;
	gchar		     *directory;
	gboolean	      value;

	key_file = g_key_file_new ();

	/* Check we have a config file and if not, create it based on
	 * the default settings.
	 */
	directory = config_dir_ensure_exists_and_return ();
	if (!directory) {
		return;
	}

	filename = g_build_filename (directory, "tracker.cfg", NULL);
	g_free (directory);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	/* Add file monitoring for changes */
	if (!priv->file) {
		priv->file = g_file_new_for_path (filename);
	}

	if (!priv->monitor) {
		g_message ("Setting up monitor for changes to config file:'%s'",
			   filename);

		priv->monitor = g_file_monitor_file (priv->file,
						     G_FILE_MONITOR_NONE,
						     NULL,
						     NULL);

		g_signal_connect (priv->monitor, "changed",
				  G_CALLBACK (config_changed_cb),
				  config);
	}

	/* Load options */
	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, &error);
	if (error) {
		config_create_with_defaults (filename, key_file);
		g_clear_error (&error);
	}

	g_free (filename);

	/* General */
	config_load_int (config, "verbosity", key_file, GROUP_GENERAL, KEY_VERBOSITY);
	config_load_int (config, "initial-sleep", key_file, GROUP_GENERAL, KEY_INITIAL_SLEEP);
	config_load_boolean (config, "low-memory-mode", key_file, GROUP_GENERAL, KEY_LOW_MEMORY_MODE);
	config_load_boolean (config, "nfs-locking", key_file, GROUP_GENERAL, KEY_NFS_LOCKING);

	/* Watches */
	config_load_string_list (config, "watch-directory-roots", key_file, GROUP_WATCHES, KEY_WATCH_DIRECTORY_ROOTS);
	config_load_string_list (config, "crawl-directory-roots", key_file, GROUP_WATCHES, KEY_CRAWL_DIRECTORY_ROOTS);
	config_load_string_list (config, "no-watch-directory-roots", key_file, GROUP_WATCHES, KEY_NO_WATCH_DIRECTORY_ROOTS);
	config_load_boolean (config, "enable-watches", key_file, GROUP_WATCHES, KEY_ENABLE_WATCHES);

	/* Indexing */
	config_load_int (config, "throttle", key_file, GROUP_INDEXING, KEY_THROTTLE);
	config_load_boolean (config, "enable-indexing", key_file, GROUP_INDEXING, KEY_ENABLE_INDEXING);
	config_load_boolean (config, "enable-content-indexing", key_file, GROUP_INDEXING, KEY_ENABLE_CONTENT_INDEXING);
	config_load_boolean (config, "enable-thumbnails", key_file, GROUP_INDEXING, KEY_ENABLE_THUMBNAILS);
	config_load_string_list (config, "disabled-modules", key_file, GROUP_INDEXING, KEY_DISABLED_MODULES);
	config_load_boolean (config, "fast-merges", key_file, GROUP_INDEXING, KEY_FAST_MERGES);
	config_load_string_list (config, "no-index-file-types", key_file, GROUP_INDEXING, KEY_NO_INDEX_FILE_TYPES);
	config_load_int (config, "min-word-length", key_file, GROUP_INDEXING, KEY_MIN_WORD_LENGTH);
	config_load_int (config, "max-word-length", key_file, GROUP_INDEXING, KEY_MAX_WORD_LENGTH);
	config_load_string (config, "language", key_file, GROUP_INDEXING, KEY_LANGUAGE);
	config_load_boolean (config, "enable-stemmer", key_file, GROUP_INDEXING, KEY_ENABLE_STEMMER);
	config_load_boolean (config, "disable-indexing-on-battery", key_file, GROUP_INDEXING, KEY_DISABLE_INDEXING_ON_BATTERY);
	config_load_boolean (config, "disable-indexing-on-battery-init", key_file, GROUP_INDEXING, KEY_DISABLE_INDEXING_ON_BATTERY_INIT);
	config_load_int (config, "low-disk-space-limit", key_file, GROUP_INDEXING, KEY_LOW_DISK_SPACE_LIMIT);
	config_load_boolean (config, "index-mounted-directories", key_file, GROUP_INDEXING, KEY_INDEX_MOUNTED_DIRECTORIES);
	config_load_boolean (config, "index-removable-devices", key_file, GROUP_INDEXING, KEY_INDEX_REMOVABLE_DEVICES);

	/* Performance */
	config_load_int (config, "max-text-to-index", key_file, GROUP_PERFORMANCE, KEY_MAX_TEXT_TO_INDEX);
	config_load_int (config, "max-words-to-index", key_file, GROUP_PERFORMANCE, KEY_MAX_WORDS_TO_INDEX);
	config_load_int (config, "max-bucket-count", key_file, GROUP_PERFORMANCE, KEY_MAX_BUCKET_COUNT);
	config_load_int (config, "min-bucket-count", key_file, GROUP_PERFORMANCE, KEY_MIN_BUCKET_COUNT);

	/* Services */
	config_load_boolean (config, "enable-xesam", key_file, GROUP_SERVICES, KEY_ENABLE_XESAM);

	/*
	 * Legacy options no longer supported:
	 */
	value = g_key_file_get_boolean (key_file, "Emails", "IndexEvolutionEmails", &error);
	if (!error) {
		gchar * const modules[2] = { "evolution", NULL };

		g_message ("Legacy config option 'IndexEvolutionEmails' found");
		g_message ("  This option has been replaced by 'DisabledModules'");

		if (!value) {
			tracker_config_add_disabled_modules (config, modules);
			g_message ("  Option 'DisabledModules' added '%s'", modules[0]);
		} else {
			tracker_config_remove_disabled_modules (config, modules[0]);
			g_message ("  Option 'DisabledModules' removed '%s'", modules[0]);
		}
	} else {
		g_clear_error (&error);
	}

	value = g_key_file_get_boolean (key_file, "Emails", "IndexThunderbirdEmails", &error);
	if (!error) {
		g_message ("Legacy config option 'IndexThunderbirdEmails' found");
		g_message ("  This option is no longer supported and has no effect");
	} else {
		g_clear_error (&error);
	}

	value = g_key_file_get_boolean (key_file, "Indexing", "SkipMountPoints", &error);
	if (!error) {
		g_message ("Legacy config option 'SkipMountPoints' found");
		tracker_config_set_index_mounted_directories (config, !value);
		g_message ("  Option 'IndexMountedDirectories' set to %s", !value ? "true" : "false");
	} else {
		g_clear_error (&error);
	}

	g_key_file_free (key_file);
}

static gboolean
config_int_validate (TrackerConfig *config,
		     const gchar   *property,
		     gint	    value)
{
#ifdef G_DISABLE_CHECKS
	GParamSpec *spec;
	GValue	    value = { 0 };
	gboolean    valid;

	spec = g_object_class_find_property (G_OBJECT_CLASS (config), property);
	g_return_val_if_fail (spec != NULL, FALSE);

	g_value_init (&value, spec->value_type);
	g_value_set_int (&value, verbosity);
	valid = g_param_value_validate (spec, &value);
	g_value_unset (&value);

	g_return_val_if_fail (valid != TRUE, FALSE);
#endif

	return TRUE;
}

TrackerConfig *
tracker_config_new (void)
{
	TrackerConfig *config;

	config = g_object_new (TRACKER_TYPE_CONFIG, NULL);
	config_load (config);

	return config;
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
tracker_config_get_low_memory_mode (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_LOW_MEMORY_MODE);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->low_memory_mode;
}

gboolean
tracker_config_get_nfs_locking (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_NFS_LOCKING);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->nfs_locking;
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
tracker_config_get_enable_indexing (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_INDEXING);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->enable_indexing;
}

gboolean
tracker_config_get_enable_xesam (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_XESAM);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->enable_xesam;
}

gboolean
tracker_config_get_enable_content_indexing (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_CONTENT_INDEXING);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->enable_content_indexing;
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
tracker_config_get_fast_merges (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_FAST_MERGES);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->fast_merges;
}

GSList *
tracker_config_get_no_index_file_types (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->no_index_file_types;
}

gint
tracker_config_get_min_word_length (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MIN_WORD_LENGTH);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->min_word_length;
}

gint
tracker_config_get_max_word_length (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MAX_WORD_LENGTH);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->max_word_length;
}

const gchar *
tracker_config_get_language (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), "en");

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->language;
}

gboolean
tracker_config_get_enable_stemmer (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_STEMMER);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->enable_stemmer;
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

gint
tracker_config_get_max_text_to_index (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MAX_TEXT_TO_INDEX);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->max_text_to_index;
}

gint
tracker_config_get_max_words_to_index (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MAX_WORDS_TO_INDEX);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->max_words_to_index;
}

gint
tracker_config_get_max_bucket_count (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MAX_BUCKET_COUNT);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->max_bucket_count;
}

gint
tracker_config_get_min_bucket_count (TrackerConfig *config)
{
	TrackerConfigPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MIN_BUCKET_COUNT);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	return priv->min_bucket_count;
}

void
tracker_config_set_verbosity (TrackerConfig *config,
			      gint	     value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "verbosity", value)) {
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

	if (!config_int_validate (config, "initial-sleep", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->initial_sleep = value;
	g_object_notify (G_OBJECT (config), "initial-sleep");
}

void
tracker_config_set_low_memory_mode (TrackerConfig *config,
				    gboolean	   value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->low_memory_mode = value;
	g_object_notify (G_OBJECT (config), "low-memory-mode");
}

void
tracker_config_set_nfs_locking (TrackerConfig *config,
				gboolean      value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->nfs_locking = value;
	g_object_notify (G_OBJECT (config), "nfs-locking");
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

	if (!config_int_validate (config, "throttle", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->throttle = value;
	g_object_notify (G_OBJECT (config), "throttle");
}

void
tracker_config_set_enable_indexing (TrackerConfig *config,
				    gboolean	   value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->enable_indexing = value;
	g_object_notify (G_OBJECT (config), "enable-indexing");
}

void
tracker_config_set_enable_xesam (TrackerConfig *config,
				 gboolean	   value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->enable_xesam = value;
	g_object_notify (G_OBJECT (config), "enable-xesam");
}

void
tracker_config_set_enable_content_indexing (TrackerConfig *config,
					    gboolean	   value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->enable_content_indexing = value;
	g_object_notify (G_OBJECT (config), "enable-content-indexing");
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
tracker_config_set_fast_merges (TrackerConfig *config,
				gboolean       value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->fast_merges = value;
	g_object_notify (G_OBJECT (config), "fast-merges");
}

void
tracker_config_set_min_word_length (TrackerConfig *config,
				    gint	   value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "min-word-length", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->min_word_length = value;
	g_object_notify (G_OBJECT (config), "min-word-length");
}

void
tracker_config_set_max_word_length (TrackerConfig *config,
				    gint	   value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "max-word-length", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->max_word_length = value;
	g_object_notify (G_OBJECT (config), "max-word-length");
}

void
tracker_config_set_language (TrackerConfig *config,
			     const gchar   *value)
{
	TrackerConfigPrivate *priv;
	gboolean	      use_default = FALSE;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	g_free (priv->language);

	/* Validate language */
	use_default |= !value;
	use_default |= strlen (value) < 2;
	use_default |= !tracker_language_check_exists (value);

	if (use_default) {
		priv->language = tracker_language_get_default_code ();
	} else {
		priv->language = g_strdup (value);
	}

	g_object_notify (G_OBJECT (config), "language");
}

void
tracker_config_set_enable_stemmer (TrackerConfig *config,
				   gboolean	  value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->enable_stemmer = value;
	g_object_notify (G_OBJECT (config), "enable-stemmer");
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

	if (!config_int_validate (config, "low-disk-space-limit", value)) {
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
tracker_config_set_max_text_to_index (TrackerConfig *config,
				      gint	     value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "max-text-to-index", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->max_text_to_index = value;
	g_object_notify (G_OBJECT (config), "max-text-to-index");
}

void
tracker_config_set_max_words_to_index (TrackerConfig *config,
				       gint	      value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "max-words-to-index", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->max_words_to_index = value;
	g_object_notify (G_OBJECT (config), "max-words-to-index");
}

void
tracker_config_set_max_bucket_count (TrackerConfig *config,
				     gint	    value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "max-bucket-count", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->max_bucket_count = value;
	g_object_notify (G_OBJECT (config), "max-bucket-count");
}

void
tracker_config_set_min_bucket_count (TrackerConfig *config,
				     gint	    value)
{
	TrackerConfigPrivate *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "min-bucket-count", value)) {
		return;
	}

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	priv->min_bucket_count = value;
	g_object_notify (G_OBJECT (config), "min-bucket-count");
}

void
tracker_config_add_watch_directory_roots (TrackerConfig *config,
					  gchar * const *roots)
{
	TrackerConfigPrivate *priv;
	GSList		     *l;
	gchar		     *validated_root;
	gchar * const	     *p;

	g_return_if_fail (TRACKER_IS_CONFIG (config));
	g_return_if_fail (roots != NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	for (p = roots; *p; p++) {
		validated_root = tracker_path_evaluate_name (*p);
		if (!validated_root) {
			g_print ("Root '%s' is not valid to add to watch directory list\n",
				 validated_root);
			continue;
		}

		priv->watch_directory_roots = g_slist_append (priv->watch_directory_roots,
							      validated_root);
	}

	l = priv->watch_directory_roots;
	priv->watch_directory_roots =
		tracker_path_list_filter_duplicates (priv->watch_directory_roots);

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "watch-directory-roots");
}

void
tracker_config_add_crawl_directory_roots (TrackerConfig *config,
					  gchar * const *roots)
{
	TrackerConfigPrivate *priv;
	GSList		     *l;
	gchar		     *validated_root;
	gchar * const	     *p;

	g_return_if_fail (TRACKER_IS_CONFIG (config));
	g_return_if_fail (roots != NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	for (p = roots; *p; p++) {
		validated_root = tracker_path_evaluate_name (*p);
		if (!validated_root) {
			g_print ("Root '%s' is not valid to add to crawl directory list\n",
				 validated_root);
			continue;
		}

		priv->crawl_directory_roots = g_slist_append (priv->crawl_directory_roots,
							      validated_root);
	}

	l = priv->crawl_directory_roots;
	priv->crawl_directory_roots =
		tracker_path_list_filter_duplicates (priv->crawl_directory_roots);

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "crawl-directory-roots");
}

void
tracker_config_add_no_watch_directory_roots (TrackerConfig *config,
					     gchar * const *roots)
{
	TrackerConfigPrivate *priv;
	GSList		     *l;
	gchar		     *validated_root;
	gchar * const	     *p;

	g_return_if_fail (TRACKER_IS_CONFIG (config));
	g_return_if_fail (roots != NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	for (p = roots; *p; p++) {
		validated_root = tracker_path_evaluate_name (*p);
		if (!validated_root) {
			g_print ("Root '%s' is not valid to add to no_watch directory list\n",
				 validated_root);
			continue;
		}

		priv->no_watch_directory_roots = g_slist_append (priv->no_watch_directory_roots,
								 validated_root);
	}

	l = priv->no_watch_directory_roots;
	priv->no_watch_directory_roots =
		tracker_path_list_filter_duplicates (priv->no_watch_directory_roots);

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	g_object_notify (G_OBJECT (config), "no-watch-directory-roots");
}

void
tracker_config_add_disabled_modules (TrackerConfig *config,
				     gchar * const *modules)
{
	TrackerConfigPrivate *priv;
	GSList		     *new_modules;
	gchar * const	     *p;

	g_return_if_fail (TRACKER_IS_CONFIG (config));
	g_return_if_fail (modules != NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	new_modules = NULL;

	for (p = modules; *p; p++) {
		if (g_slist_find_custom (priv->disabled_modules,
					 *p,
					 (GCompareFunc) strcmp)) {
			continue;
		}

		new_modules = g_slist_append (new_modules, g_strdup (*p));
	}

	priv->disabled_modules = g_slist_concat (priv->disabled_modules,
						 new_modules);

	g_object_notify (G_OBJECT (config), "disabled-modules");
}

void
tracker_config_remove_disabled_modules (TrackerConfig *config,
					const gchar   *module)
{
	TrackerConfigPrivate *priv;
	GSList		     *l;

	g_return_if_fail (TRACKER_IS_CONFIG (config));
	g_return_if_fail (module != NULL);

	priv = TRACKER_CONFIG_GET_PRIVATE (config);

	l = g_slist_find_custom (priv->disabled_modules,
				 module,
				 (GCompareFunc) strcmp);

	if (l) {
		g_free (l->data);
		priv->disabled_modules = g_slist_delete_link (priv->disabled_modules, l);
		g_object_notify (G_OBJECT (config), "disabled-modules");
	}
}
