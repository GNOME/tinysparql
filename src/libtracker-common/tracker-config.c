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

#include "tracker-language.h"
#include "tracker-config.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_CONFIG, TrackerConfigPriv))

/* GKeyFile defines */
#define GROUP_GENERAL				 "General"
#define KEY_VERBOSITY				 "Verbosity"
#define KEY_INITIAL_SLEEP			 "InitialSleep"
#define KEY_LOW_MEMORY_MODE			 "LowMemoryMode"

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
#define KEY_FAST_MERGES				 "FastMerges"
#define KEY_NO_INDEX_FILE_TYPES			 "NoIndexFileTypes"
#define KEY_MIN_WORD_LENGTH			 "MinWorldLength"
#define KEY_MAX_WORD_LENGTH			 "MaxWorldLength"
#define KEY_LANGUAGE				 "Language"
#define KEY_ENABLE_STEMMER			 "EnableStemmer"
#define KEY_SKIP_MOUNT_POINTS			 "SkipMountPoints"
#define KEY_DISABLE_INDEXING_ON_BATTERY		 "BatteryIndex"
#define KEY_DISABLE_INDEXING_ON_BATTERY_INIT	 "BatteryIndexInitial"
#define KEY_LOW_DISK_SPACE_LIMIT		 "LowDiskSpaceLimit"

#define GROUP_EMAILS				 "Emails"
#define KEY_INDEX_EVOLUTION_EMAILS		 "IndexEvolutionEmails"
#define KEY_INDEX_KMAIL_EMAILS			 "IndexKMailEmails"
#define KEY_INDEX_THUNDERBIRD_EMAILS		 "IndexThunderbirdEmails"

#define GROUP_PERFORMANCE			 "Performance"
#define KEY_MAX_TEXT_TO_INDEX			 "MaxTextToIndex"
#define KEY_MAX_WORDS_TO_INDEX			 "MaxWordsToIndex"
#define KEY_OPTIMIZATION_SWEEP_COUNT		 "OptimizationSweepCount"
#define KEY_MAX_BUCKET_COUNT			 "MaxBucketCount"
#define KEY_MIN_BUCKET_COUNT			 "MinBucketCount"
#define KEY_DIVISIONS				 "Divisions"
#define KEY_BUCKET_RATIO			 "BucketRatio"
#define KEY_PADDING				 "Padding"
#define KEY_THREAD_STACK_SIZE			 "ThreadStackSize"

/* Default values */
#define DEFAULT_VERBOSITY			 0
#define DEFAULT_INITIAL_SLEEP			 45	  /* 0->1000 */
#define DEFAULT_LOW_MEMORY_MODE			 TRUE
#define DEFAULT_ENABLE_WATCHES			 TRUE
#define DEFAULT_THROTTLE			 0	  /* 0->20 */
#define DEFAULT_ENABLE_INDEXING			 TRUE
#define DEFAULT_ENABLE_CONTENT_INDEXING		 TRUE
#define DEFAULT_ENABLE_THUMBNAILS		 TRUE
#define DEFAULT_FAST_MERGES			 FALSE
#define DEFAULT_MIN_WORD_LENGTH			 3	  /* 0->30 */
#define DEFAULT_MAX_WORD_LENGTH			 30	  /* 0->200 */
#define DEFAULT_ENABLE_STEMMER			 TRUE
#define DEFAULT_SKIP_MOUNT_POINTS		 FALSE
#define DEFAULT_DISABLE_INDEXING_ON_BATTERY	 TRUE
#define DEFAULT_DISABLE_INDEXING_ON_BATTERY_INIT FALSE
#define DEFAULT_INDEX_EVOLUTION_EMAILS		 TRUE
#define DEFAULT_INDEX_KMAIL_EMAILS		 TRUE
#define DEFAULT_INDEX_THUNDERBIRD_EMAILS	 TRUE
#define DEFAULT_LOW_DISK_SPACE_LIMIT		 1	  /* 0->100 / -1 */
#define DEFAULT_MAX_TEXT_TO_INDEX		 1048576  /* Bytes */
#define DEFAULT_MAX_WORDS_TO_INDEX		 10000
#define DEFAULT_OPTIMIZATION_SWEEP_COUNT	 10000
#define DEFAULT_MAX_BUCKET_COUNT		 524288
#define DEFAULT_MIN_BUCKET_COUNT		 65536
#define DEFAULT_DIVISIONS			 4	  /* 1->64 */
#define DEFAULT_BUCKET_RATIO			 1	  /* 0=50%, 1=100%, 2=200%, 3=300%, 4=400% */
#define DEFAULT_PADDING				 2	  /* 1->8 */
#define DEFAULT_THREAD_STACK_SIZE		 0	  /* 0 is the default for the platform */

typedef struct _ConfigLanguages	  ConfigLanguages;
typedef struct _TrackerConfigPriv TrackerConfigPriv;

struct _TrackerConfigPriv {
	/* General */
	gint	  verbosity;
	gint	  initial_sleep;
	gboolean  low_memory_mode;

	/* Watches */
	GSList	 *watch_directory_roots;
	GSList	 *crawl_directory_roots;
	GSList	 *no_watch_directory_roots;
	gboolean  enable_watches;

	/* Indexing */
	gint	  throttle;
	gboolean  enable_indexing;
	gboolean  enable_content_indexing;
	gboolean  enable_thumbnails;
	gboolean  fast_merges;
	GSList	 *no_index_file_types;
	gint	  min_word_length;
	gint	  max_word_length;
	gchar	 *language;
	gboolean  enable_stemmer;
	gboolean  skip_mount_points;
	gboolean  disable_indexing_on_battery;
	gboolean  disable_indexing_on_battery_init;
	gint	  low_disk_space_limit;

	/* Emails */
	gboolean  index_evolution_emails;
	gboolean  index_kmail_emails;
	gboolean  index_thunderbird_emails;

	/* Performance */
	gint	  max_text_to_index;
	gint	  max_words_to_index;
	gint	  optimization_sweep_count;
	gint	  max_bucket_count;
	gint	  min_bucket_count;
	gint	  divisions;
	gint	  bucket_ratio;
	gint	  padding;
	gint	  thread_stack_size;
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

/* GObject properties */
enum {
	PROP_0,

	/* General */
	PROP_VERBOSITY,
	PROP_INITIAL_SLEEP,
	PROP_LOW_MEMORY_MODE,

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
	PROP_FAST_MERGES,
	PROP_NO_INDEX_FILE_TYPES,
	PROP_MIN_WORD_LENGTH,
	PROP_MAX_WORD_LENGTH,
	PROP_LANGUAGE,
	PROP_ENABLE_STEMMER,
	PROP_SKIP_MOUNT_POINTS,
	PROP_DISABLE_INDEXING_ON_BATTERY,
	PROP_DISABLE_INDEXING_ON_BATTERY_INIT,
	PROP_LOW_DISK_SPACE_LIMIT,

	/* Emails */
	PROP_INDEX_EVOLUTION_EMAILS,
	PROP_INDEX_KMAIL_EMAILS,
	PROP_INDEX_THUNDERBIRD_EMAILS,

	/* Performance */
	PROP_MAX_TEXT_TO_INDEX,
	PROP_MAX_WORDS_TO_INDEX,
	PROP_OPTIMIZATION_SWEEP_COUNT,
	PROP_MAX_BUCKET_COUNT,
	PROP_MIN_BUCKET_COUNT,
	PROP_DIVISIONS,
	PROP_BUCKET_RATIO,
	PROP_PADDING,
	PROP_THREAD_STACK_SIZE
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
					 PROP_SKIP_MOUNT_POINTS,
					 g_param_spec_boolean ("skip-mount-points",
							       "Skip mount points",
							       "Don't traverse mount points "
							       "when indexing",
							       DEFAULT_SKIP_MOUNT_POINTS,
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

	/* Emails */
	g_object_class_install_property (object_class,
					 PROP_INDEX_EVOLUTION_EMAILS,
					 g_param_spec_boolean ("index-evolution-emails",
							       "Index evolution emails",
							       "Index evolution emails",
							       DEFAULT_INDEX_EVOLUTION_EMAILS,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_INDEX_KMAIL_EMAILS,
					 g_param_spec_boolean ("index-kmail-emails",
							       "Index kmail emails",
							       "Index kmail emails",
							       DEFAULT_INDEX_KMAIL_EMAILS,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_INDEX_THUNDERBIRD_EMAILS,
					 g_param_spec_boolean ("index-thunderbird-emails",
							       "Index thunderbird emails",
							       "Index thunderbird emails",
							       DEFAULT_INDEX_THUNDERBIRD_EMAILS,
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
					 PROP_MAX_TEXT_TO_INDEX,
					 g_param_spec_int ("max-words-to-index",
							   "Maximum words to index",
							   "Maximum unique words to index "
							   "from file's content",
							   0,
							   G_MAXINT,
							   DEFAULT_MAX_WORDS_TO_INDEX,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_OPTIMIZATION_SWEEP_COUNT,
					 g_param_spec_int ("optimization-sweep-count",
							   "Optimization Sweep Count",
							   "Number of entities to index "
							   "before deciding to try to optimize "
							   "(1000->10000)",
							   1000,
							   G_MAXINT,
							   DEFAULT_OPTIMIZATION_SWEEP_COUNT,
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
	g_object_class_install_property (object_class,
					 PROP_DIVISIONS,
					 g_param_spec_int ("divisions",
							   "Divisions",
							   "Number of divisions of the index file (1->64)",
							   1,
							   64,
							   DEFAULT_DIVISIONS,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_BUCKET_RATIO,
					 g_param_spec_int ("bucket-ratio",
							   "Bucket Ratio",
							   "Number of used records to buckets ratio "
							   "when optimizing the index "
							   "(0=50%, 1=100%, 2=200%, 3=300%, 4=400%)",
							   0,
							   4,
							   DEFAULT_BUCKET_RATIO,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_PADDING,
					 g_param_spec_int ("padding",
							   "Padding",
							   "How much space is used to prevent "
							   "index relocations, higher values use "
							   "more disk space (1->8)",
							   1,
							   8,
							   DEFAULT_PADDING,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_THREAD_STACK_SIZE,
					 g_param_spec_int ("thread-stack-size",
							   "Thread stack size",
							   "Thread stack size to use inside tracker. "
							   "Use this carefully, as it may lead to misterious crashes. "
							   "The default is 0, which uses the default for the platform.",
							   0, G_MAXINT,
							   DEFAULT_THREAD_STACK_SIZE,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerConfigPriv));
}

static void
tracker_config_init (TrackerConfig *config)
{
}

static void
config_finalize (GObject *object)
{
	TrackerConfigPriv *priv;

	priv = GET_PRIV (object);

	(G_OBJECT_CLASS (tracker_config_parent_class)->finalize) (object);
}

static void
config_get_property (GObject	*object,
		     guint	 param_id,
		     GValue	*value,
		     GParamSpec *pspec)
{
	TrackerConfigPriv *priv;

	priv = GET_PRIV (object);

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
	case PROP_SKIP_MOUNT_POINTS:
		g_value_set_boolean (value, priv->skip_mount_points);
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

		/* Emails */
	case PROP_INDEX_EVOLUTION_EMAILS:
		g_value_set_boolean (value, priv->index_evolution_emails);
		break;
	case PROP_INDEX_KMAIL_EMAILS:
		g_value_set_boolean (value, priv->index_kmail_emails);
		break;
	case PROP_INDEX_THUNDERBIRD_EMAILS:
		g_value_set_boolean (value, priv->index_thunderbird_emails);
		break;

		/* Performance */
	case PROP_MAX_TEXT_TO_INDEX:
		g_value_set_int (value, priv->max_text_to_index);
		break;
	case PROP_MAX_WORDS_TO_INDEX:
		g_value_set_int (value, priv->max_words_to_index);
		break;
	case PROP_OPTIMIZATION_SWEEP_COUNT:
		g_value_set_int (value, priv->optimization_sweep_count);
		break;
	case PROP_MAX_BUCKET_COUNT:
		g_value_set_int (value, priv->max_bucket_count);
		break;
	case PROP_MIN_BUCKET_COUNT:
		g_value_set_int (value, priv->min_bucket_count);
		break;
	case PROP_DIVISIONS:
		g_value_set_int (value, priv->divisions);
		break;
	case PROP_BUCKET_RATIO:
		g_value_set_int (value, priv->bucket_ratio);
		break;
	case PROP_PADDING:
		g_value_set_int (value, priv->padding);
		break;
	case PROP_THREAD_STACK_SIZE:
		g_value_set_int (value, priv->thread_stack_size);
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
	TrackerConfigPriv *priv;

	priv = GET_PRIV (object);

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
	case PROP_FAST_MERGES:
		tracker_config_set_fast_merges (TRACKER_CONFIG (object),
						g_value_get_boolean (value));
		break;
	case PROP_NO_INDEX_FILE_TYPES:	    /* Not writable */
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
	case PROP_SKIP_MOUNT_POINTS:
		tracker_config_set_skip_mount_points (TRACKER_CONFIG (object),
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

		/* Emails */
	case PROP_INDEX_EVOLUTION_EMAILS:
		tracker_config_set_index_evolution_emails (TRACKER_CONFIG (object),
							   g_value_get_boolean (value));
		break;
	case PROP_INDEX_KMAIL_EMAILS:
		tracker_config_set_index_kmail_emails (TRACKER_CONFIG (object),
						       g_value_get_boolean (value));
		break;
	case PROP_INDEX_THUNDERBIRD_EMAILS:
		tracker_config_set_index_thunderbird_emails (TRACKER_CONFIG (object),
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
	case PROP_OPTIMIZATION_SWEEP_COUNT:
		tracker_config_set_optimization_sweep_count (TRACKER_CONFIG (object),
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
	case PROP_DIVISIONS:
		tracker_config_set_divisions (TRACKER_CONFIG (object),
					      g_value_get_int (value));
		break;
	case PROP_BUCKET_RATIO:
		tracker_config_set_bucket_ratio (TRACKER_CONFIG (object),
						 g_value_get_int (value));
		break;
	case PROP_PADDING:
		tracker_config_set_padding (TRACKER_CONFIG (object),
					    g_value_get_int (value));
		break;
	case PROP_THREAD_STACK_SIZE:
		tracker_config_set_thread_stack_size (TRACKER_CONFIG (object),
						      g_value_get_int (value));
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

static gchar *
config_dir_validate_name (const gchar *original_path)
{
	gchar resolved_path[PATH_MAX + 2];

	if (!original_path || original_path[0] == '\0') {
		return NULL;
	}

	if (original_path[0] == '~') {
		const char *home = g_get_home_dir ();

		if (!home || home[0] == '\0') {
			return NULL;
		}

		return g_build_path (G_DIR_SEPARATOR_S,
				     home,
				     original_path + 1,
				     NULL);
	}

	return g_strdup (realpath (original_path, resolved_path));
}

static gboolean
config_dir_is_child_of (const char *dir,
			const char *dir_to_test)
{
	gchar	 *path;
	gboolean  result;

	if (!dir_to_test) {
		return FALSE;
	}

	if (dir[strlen (dir)-1] != '/') {
		path = g_strconcat (dir, "/", NULL);
	} else {
		path = g_strdup (dir);
	}

	result = g_str_has_prefix (dir_to_test, path);
	g_free (path);

	return result;
}

static void
config_dir_check_roots_for_conflicts (TrackerConfig *config)
{
	TrackerConfigPriv *priv;
	GSList		  *final_list = NULL;
	GSList		  *l1, *l2;

	priv = GET_PRIV (config);

	for (l1 = priv->watch_directory_roots; l1; l1 = l1->next) {
		gboolean add = TRUE;

		if (!final_list) {
			final_list = g_slist_prepend (NULL, l1->data);
			continue;
		}

		for (l2 = final_list; l2 && add; l2 = l2->next) {
			if (!l2->data) {
				continue;
			}

			/* Is new directory a child of another in
			 * current list already?
			 */
			if (config_dir_is_child_of (l2->data, l1->data)) {
				add = FALSE;
				continue;
			}

			/* Is current directory a child of the new
			 * directory we are adding?
			 */
			if (config_dir_is_child_of (l1->data, l2->data)) {
				l2->data = NULL;
			}
		}

		if (add) {
			final_list = g_slist_prepend (final_list, l1->data);
		}
	}

	g_slist_free (priv->watch_directory_roots);
	priv->watch_directory_roots = NULL;

	for (l1 = final_list; l1; l1 = l1->next) {
		gchar *root;

		root = l1->data;

		if (!root || root[0] != G_DIR_SEPARATOR) {
			continue;
		}

		priv->watch_directory_roots =
			g_slist_prepend (priv->watch_directory_roots, root);
	}

	priv->watch_directory_roots = g_slist_reverse (priv->watch_directory_roots);
	g_slist_free (final_list);
}

static gboolean
config_create_with_defaults (const gchar *filename)
{
	GKeyFile     *key_file;
	GError	     *error = NULL;
	gchar	     *content = NULL;
	gchar	     *language;
	const gchar  *watch_directory_roots[2] = { NULL, NULL };
	const gchar  *empty_string_list[] = { NULL };

	key_file = g_key_file_new ();

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
	g_key_file_set_boolean (key_file, GROUP_INDEXING, KEY_SKIP_MOUNT_POINTS, DEFAULT_SKIP_MOUNT_POINTS);
	g_key_file_set_comment (key_file, GROUP_INDEXING, KEY_SKIP_MOUNT_POINTS,
				" Set to true to disable traversing directories on mount points",
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

	/* Emails */
	g_key_file_set_boolean (key_file, GROUP_EMAILS, KEY_INDEX_EVOLUTION_EMAILS, DEFAULT_INDEX_EVOLUTION_EMAILS);
	g_key_file_set_boolean (key_file, GROUP_EMAILS, KEY_INDEX_KMAIL_EMAILS, DEFAULT_INDEX_KMAIL_EMAILS);
	g_key_file_set_boolean (key_file, GROUP_EMAILS, KEY_INDEX_THUNDERBIRD_EMAILS, DEFAULT_INDEX_THUNDERBIRD_EMAILS);

	/* Performance */
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_MAX_TEXT_TO_INDEX, DEFAULT_MAX_TEXT_TO_INDEX);
	g_key_file_set_comment (key_file, GROUP_PERFORMANCE, KEY_MAX_TEXT_TO_INDEX,
				" Maximum text size in bytes to index from a file's content",
				NULL);
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_MAX_WORDS_TO_INDEX, DEFAULT_MAX_WORDS_TO_INDEX);
	g_key_file_set_comment (key_file, GROUP_PERFORMANCE, KEY_MAX_WORDS_TO_INDEX,
				" Maximum unique words to index from a file's content",
				NULL);
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_OPTIMIZATION_SWEEP_COUNT, DEFAULT_OPTIMIZATION_SWEEP_COUNT);
	g_key_file_set_comment (key_file, GROUP_PERFORMANCE, KEY_OPTIMIZATION_SWEEP_COUNT,
				" Number of entities to index before deciding to trying to optimize"
				" (1000->10000)",
				NULL);
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_MAX_BUCKET_COUNT, DEFAULT_MAX_BUCKET_COUNT);
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_MIN_BUCKET_COUNT, DEFAULT_MIN_BUCKET_COUNT);
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_DIVISIONS, DEFAULT_DIVISIONS);
	g_key_file_set_comment (key_file, GROUP_PERFORMANCE, KEY_DIVISIONS,
				" Number of divisions of the index file (1->64, default=4)",
				NULL);
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_BUCKET_RATIO, DEFAULT_BUCKET_RATIO);
	g_key_file_set_comment (key_file, GROUP_PERFORMANCE, KEY_BUCKET_RATIO,
				" Number of used records to buckets ratio when optimising the index.\n"
				" (0=50%, 1=100%, 2=200%, 3=300%, 4=400%)",
				NULL);
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_PADDING, DEFAULT_PADDING);
	g_key_file_set_comment (key_file, GROUP_PERFORMANCE, KEY_PADDING,
				" How much space is used to prevent index relocations.\n"
				" Higher values improve indexing speed but waste more disk space.\n"
				" Values should be between 1 and 8.",
				NULL);
	g_key_file_set_integer (key_file, GROUP_PERFORMANCE, KEY_THREAD_STACK_SIZE, DEFAULT_THREAD_STACK_SIZE);
	g_key_file_set_comment (key_file, GROUP_PERFORMANCE, KEY_THREAD_STACK_SIZE,
				" Stack size to use in threads inside Tracker.\n"
				" Use this carefully, or expect misterious crashes.\n"
				" 0 uses the default stack size for this platform",
				NULL);

	content = g_key_file_to_data (key_file, NULL, &error);
	g_free (language);
	g_key_file_free (key_file);

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
		validated = config_dir_validate_name (str);
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
		g_clear_error (&error);
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
		g_clear_error (&error);
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
		g_clear_error (&error);
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
	TrackerConfigPriv  *priv;
	gchar		  **value;

	priv = GET_PRIV (config);

	value = g_key_file_get_string_list (key_file, group, key, NULL, NULL);

	if (strcmp (property, "watch-directory-roots") == 0) {
		if (!value) {
			priv->watch_directory_roots =
				g_slist_prepend (NULL, g_strdup (g_get_home_dir ()));
		} else {
			priv->watch_directory_roots =
				config_string_list_to_gslist ((const gchar **) value, TRUE);
		}

		/* We only do this for watch directory roots right now, not
		 * sure why.
		 */
		config_dir_check_roots_for_conflicts (config);
	}
	else if (strcmp (property, "crawl-directory-roots") == 0) {
		if (value) {
			priv->crawl_directory_roots =
				config_string_list_to_gslist ((const gchar **) value, TRUE);
		}
	}
	else if (strcmp (property, "no-watch-directory-roots") == 0) {
		if (value) {
			priv->no_watch_directory_roots =
				config_string_list_to_gslist ((const gchar **) value, TRUE);
		}
	}
	else if (strcmp (property, "no-index-file-types") == 0) {
		if (value) {
			priv->no_index_file_types =
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
config_load (TrackerConfig *config)
{
	GKeyFile *key_file;
	GError	 *error = NULL;
	gchar	 *filename;
	gchar	 *directory;

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

	/* Load options */
	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, &error);
	if (error) {
		config_create_with_defaults (filename);
		g_clear_error (&error);
	}

	g_free (filename);

	/* General */
	config_load_int (config, "verbosity", key_file, GROUP_GENERAL, KEY_VERBOSITY);
	config_load_int (config, "initial-sleep", key_file, GROUP_GENERAL, KEY_INITIAL_SLEEP);
	config_load_boolean (config, "low-memory-mode", key_file, GROUP_GENERAL, KEY_LOW_MEMORY_MODE);

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
	config_load_boolean (config, "fast-merges", key_file, GROUP_INDEXING, KEY_FAST_MERGES);
	config_load_string_list (config, "no-index-file-types", key_file, GROUP_INDEXING, KEY_NO_INDEX_FILE_TYPES);
	config_load_int (config, "min-word-length", key_file, GROUP_INDEXING, KEY_MIN_WORD_LENGTH);
	config_load_int (config, "max-word-length", key_file, GROUP_INDEXING, KEY_MAX_WORD_LENGTH);
	config_load_string (config, "language", key_file, GROUP_INDEXING, KEY_LANGUAGE);
	config_load_boolean (config, "enable-stemmer", key_file, GROUP_INDEXING, KEY_ENABLE_STEMMER);
	config_load_boolean (config, "skip-mount-points", key_file, GROUP_INDEXING, KEY_SKIP_MOUNT_POINTS);
	config_load_boolean (config, "disable-indexing-on-battery", key_file, GROUP_INDEXING, KEY_DISABLE_INDEXING_ON_BATTERY);
	config_load_boolean (config, "disable-indexing-on-battery-init", key_file, GROUP_INDEXING, KEY_DISABLE_INDEXING_ON_BATTERY_INIT);
	config_load_int (config, "low-disk-space-limit", key_file, GROUP_INDEXING, KEY_LOW_DISK_SPACE_LIMIT);

	/* Emails */
	config_load_boolean (config, "index-evolution-emails", key_file, GROUP_EMAILS, KEY_INDEX_EVOLUTION_EMAILS);
	config_load_boolean (config, "index-kmail-emails", key_file, GROUP_EMAILS, KEY_INDEX_KMAIL_EMAILS);
	config_load_boolean (config, "index-thunderbird-emails", key_file, GROUP_EMAILS, KEY_INDEX_THUNDERBIRD_EMAILS);

	/* Performance */
	config_load_int (config, "max-text-to-index", key_file, GROUP_PERFORMANCE, KEY_MAX_TEXT_TO_INDEX);
	config_load_int (config, "max-words-to-index", key_file, GROUP_PERFORMANCE, KEY_MAX_WORDS_TO_INDEX);
	config_load_int (config, "optimization-sweep-count", key_file, GROUP_PERFORMANCE, KEY_OPTIMIZATION_SWEEP_COUNT);
	config_load_int (config, "max-bucket-count", key_file, GROUP_PERFORMANCE, KEY_MAX_BUCKET_COUNT);
	config_load_int (config, "min-bucket-count", key_file, GROUP_PERFORMANCE, KEY_MIN_BUCKET_COUNT);
	config_load_int (config, "divisions", key_file, GROUP_PERFORMANCE, KEY_DIVISIONS);
	config_load_int (config, "bucket-ratio", key_file, GROUP_PERFORMANCE, KEY_BUCKET_RATIO);
	config_load_int (config, "padding", key_file, GROUP_PERFORMANCE, KEY_PADDING);
	config_load_int (config, "thread-stack-size", key_file, GROUP_PERFORMANCE, KEY_THREAD_STACK_SIZE);

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
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_VERBOSITY);

	priv = GET_PRIV (config);

	return priv->verbosity;
}

gint
tracker_config_get_initial_sleep (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INITIAL_SLEEP);

	priv = GET_PRIV (config);

	return priv->initial_sleep;
}

gboolean
tracker_config_get_low_memory_mode (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_LOW_MEMORY_MODE);

	priv = GET_PRIV (config);

	return priv->low_memory_mode;
}

GSList *
tracker_config_get_watch_directory_roots (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = GET_PRIV (config);

	return priv->watch_directory_roots;
}

GSList *
tracker_config_get_crawl_directory_roots (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = GET_PRIV (config);

	return priv->crawl_directory_roots;
}

GSList *
tracker_config_get_no_watch_directory_roots (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = GET_PRIV (config);

	return priv->no_watch_directory_roots;
}

gboolean
tracker_config_get_enable_watches (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_WATCHES);

	priv = GET_PRIV (config);

	return priv->enable_watches;
}

gint
tracker_config_get_throttle (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_THROTTLE);

	priv = GET_PRIV (config);

	return priv->throttle;
}

gboolean
tracker_config_get_enable_indexing (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_INDEXING);

	priv = GET_PRIV (config);

	return priv->enable_indexing;
}

gboolean
tracker_config_get_enable_content_indexing (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_CONTENT_INDEXING);

	priv = GET_PRIV (config);

	return priv->enable_content_indexing;
}

gboolean
tracker_config_get_enable_thumbnails (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_THUMBNAILS);

	priv = GET_PRIV (config);

	return priv->enable_thumbnails;
}

gboolean
tracker_config_get_fast_merges (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_FAST_MERGES);

	priv = GET_PRIV (config);

	return priv->fast_merges;
}

GSList *
tracker_config_get_no_index_file_types (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	priv = GET_PRIV (config);

	return priv->no_index_file_types;
}

gint
tracker_config_get_min_word_length (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MIN_WORD_LENGTH);

	priv = GET_PRIV (config);

	return priv->min_word_length;
}

gint
tracker_config_get_max_word_length (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MAX_WORD_LENGTH);

	priv = GET_PRIV (config);

	return priv->max_word_length;
}

const gchar *
tracker_config_get_language (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), "en");

	priv = GET_PRIV (config);

	return priv->language;
}

gboolean
tracker_config_get_enable_stemmer (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_ENABLE_STEMMER);

	priv = GET_PRIV (config);

	return priv->enable_stemmer;
}

gboolean
tracker_config_get_skip_mount_points (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_SKIP_MOUNT_POINTS);

	priv = GET_PRIV (config);

	return priv->skip_mount_points;
}

gboolean
tracker_config_get_disable_indexing_on_battery (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_DISABLE_INDEXING_ON_BATTERY);

	priv = GET_PRIV (config);

	return priv->disable_indexing_on_battery;
}

gboolean
tracker_config_get_disable_indexing_on_battery_init (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_DISABLE_INDEXING_ON_BATTERY_INIT);

	priv = GET_PRIV (config);

	return priv->disable_indexing_on_battery_init;
}

gint
tracker_config_get_low_disk_space_limit (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_LOW_DISK_SPACE_LIMIT);

	priv = GET_PRIV (config);

	return priv->low_disk_space_limit;
}

gboolean
tracker_config_get_index_evolution_emails (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_EVOLUTION_EMAILS);

	priv = GET_PRIV (config);

	return priv->index_evolution_emails;
}

gboolean
tracker_config_get_index_kmail_emails (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_KMAIL_EMAILS);

	priv = GET_PRIV (config);

	return priv->index_kmail_emails;
}

gboolean
tracker_config_get_index_thunderbird_emails (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_INDEX_THUNDERBIRD_EMAILS);

	priv = GET_PRIV (config);

	return priv->index_thunderbird_emails;
}

gint
tracker_config_get_max_text_to_index (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MAX_TEXT_TO_INDEX);

	priv = GET_PRIV (config);

	return priv->max_text_to_index;
}

gint
tracker_config_get_max_words_to_index (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MAX_WORDS_TO_INDEX);

	priv = GET_PRIV (config);

	return priv->max_words_to_index;
}

gint
tracker_config_get_optimization_sweep_count (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_OPTIMIZATION_SWEEP_COUNT);

	priv = GET_PRIV (config);

	return priv->optimization_sweep_count;
}

gint
tracker_config_get_max_bucket_count (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MAX_BUCKET_COUNT);

	priv = GET_PRIV (config);

	return priv->max_bucket_count;
}

gint
tracker_config_get_min_bucket_count (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_MIN_BUCKET_COUNT);

	priv = GET_PRIV (config);

	return priv->min_bucket_count;
}

gint
tracker_config_get_divisions (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_DIVISIONS);

	priv = GET_PRIV (config);

	return priv->divisions;
}

gint
tracker_config_get_bucket_ratio (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_BUCKET_RATIO);

	priv = GET_PRIV (config);

	return priv->bucket_ratio;
}

gint
tracker_config_get_padding (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_PADDING);

	priv = GET_PRIV (config);

	return priv->padding;
}

gint
tracker_config_get_thread_stack_size (TrackerConfig *config)
{
	TrackerConfigPriv *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), DEFAULT_THREAD_STACK_SIZE);

	priv = GET_PRIV (config);

	return priv->thread_stack_size;
}

void
tracker_config_set_verbosity (TrackerConfig *config,
			      gint	     value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "verbosity", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->verbosity = value;
	g_object_notify (G_OBJECT (config), "verbosity");
}

void
tracker_config_set_initial_sleep (TrackerConfig *config,
				  gint		 value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "initial-sleep", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->initial_sleep = value;
	g_object_notify (G_OBJECT (config), "initial-sleep");
}

void
tracker_config_set_low_memory_mode (TrackerConfig *config,
				    gboolean	   value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->low_memory_mode = value;
	g_object_notify (G_OBJECT (config), "low-memory-mode");
}

void
tracker_config_set_enable_watches (TrackerConfig *config,
				   gboolean	  value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->enable_watches = value;
	g_object_notify (G_OBJECT (config), "enable-watches");
}

void
tracker_config_set_throttle (TrackerConfig *config,
			     gint	    value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "throttle", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->throttle = value;
	g_object_notify (G_OBJECT (config), "throttle");
}

void
tracker_config_set_enable_indexing (TrackerConfig *config,
				    gboolean	   value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->enable_indexing = value;
	g_object_notify (G_OBJECT (config), "enable-indexing");
}

void
tracker_config_set_enable_content_indexing (TrackerConfig *config,
					    gboolean	   value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->enable_content_indexing = value;
	g_object_notify (G_OBJECT (config), "enable-content-indexing");
}

void
tracker_config_set_enable_thumbnails (TrackerConfig *config,
				      gboolean	     value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->enable_thumbnails = value;
	g_object_notify (G_OBJECT (config), "enable-thumbnails");
}

void
tracker_config_set_fast_merges (TrackerConfig *config,
				gboolean       value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->fast_merges = value;
	g_object_notify (G_OBJECT (config), "fast-merges");
}

void
tracker_config_set_min_word_length (TrackerConfig *config,
				    gint	   value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "min-word-length", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->min_word_length = value;
	g_object_notify (G_OBJECT (config), "min-word-length");
}

void
tracker_config_set_max_word_length (TrackerConfig *config,
				    gint	   value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "max-word-length", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->max_word_length = value;
	g_object_notify (G_OBJECT (config), "max-word-length");
}

void
tracker_config_set_language (TrackerConfig *config,
			     const gchar   *value)
{
	TrackerConfigPriv *priv;
	gboolean	   use_default = FALSE;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

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
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->enable_stemmer = value;
	g_object_notify (G_OBJECT (config), "enable-stemmer");
}

void
tracker_config_set_skip_mount_points (TrackerConfig *config,
				      gboolean	     value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->skip_mount_points = value;
	g_object_notify (G_OBJECT (config), "skip-mount-points");
}

void
tracker_config_set_disable_indexing_on_battery (TrackerConfig *config,
						gboolean       value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->disable_indexing_on_battery = value;
	g_object_notify (G_OBJECT (config), "disable-indexing-on-battery");
}

void
tracker_config_set_disable_indexing_on_battery_init (TrackerConfig *config,
						     gboolean	    value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->disable_indexing_on_battery_init = value;
	g_object_notify (G_OBJECT (config), "disable-indexing-on-battery-init");
}

void
tracker_config_set_low_disk_space_limit (TrackerConfig *config,
					 gint		value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "low-disk-space-limit", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->low_disk_space_limit = value;
	g_object_notify (G_OBJECT (config), "low-disk-space-limit");
}

void
tracker_config_set_index_evolution_emails (TrackerConfig *config,
					   gboolean	  value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->index_evolution_emails = value;
	g_object_notify (G_OBJECT (config), "index-evolution-emails");
}

void
tracker_config_set_index_kmail_emails (TrackerConfig *config,
				       gboolean	      value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->index_kmail_emails = value;
	g_object_notify (G_OBJECT (config), "index-kmail-emails");
}

void
tracker_config_set_index_thunderbird_emails (TrackerConfig *config,
					     gboolean	    value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	priv = GET_PRIV (config);

	priv->index_thunderbird_emails = value;
	g_object_notify (G_OBJECT (config), "index-thunderbird-emails");
}

void
tracker_config_set_max_text_to_index (TrackerConfig *config,
				      gint	     value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "max-text-to-index", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->max_text_to_index = value;
	g_object_notify (G_OBJECT (config), "max_text_to_index");
}

void
tracker_config_set_max_words_to_index (TrackerConfig *config,
				       gint	      value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "max-words-to-index", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->max_words_to_index = value;
	g_object_notify (G_OBJECT (config), "max_words_to_index");
}

void
tracker_config_set_optimization_sweep_count (TrackerConfig *config,
					     gint	    value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "optimization-sweep-count", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->optimization_sweep_count = value;
	g_object_notify (G_OBJECT (config), "optimization-sweep-count");
}

void
tracker_config_set_max_bucket_count (TrackerConfig *config,
				     gint	    value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "max-bucket-count", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->max_bucket_count = value;
	g_object_notify (G_OBJECT (config), "max-bucket-count");
}

void
tracker_config_set_min_bucket_count (TrackerConfig *config,
				     gint	    value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "min-bucket-count", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->min_bucket_count = value;
	g_object_notify (G_OBJECT (config), "min-bucket-count");
}

void
tracker_config_set_divisions (TrackerConfig *config,
			      gint	     value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "divisions", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->divisions = value;
	g_object_notify (G_OBJECT (config), "divisions");
}

void
tracker_config_set_bucket_ratio (TrackerConfig *config,
				 gint		value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "bucket-ratio", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->bucket_ratio = value;
	g_object_notify (G_OBJECT (config), "bucket-ratio");
}

void
tracker_config_set_padding (TrackerConfig *config,
			    gint	   value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "padding", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->padding = value;
	g_object_notify (G_OBJECT (config), "padding");
}

void
tracker_config_set_thread_stack_size (TrackerConfig *config,
				      gint	     value)
{
	TrackerConfigPriv *priv;

	g_return_if_fail (TRACKER_IS_CONFIG (config));

	if (!config_int_validate (config, "thread-stack-size", value)) {
		return;
	}

	priv = GET_PRIV (config);

	priv->padding = value;
	g_object_notify (G_OBJECT (config), "thread-stack-size");
}

void
tracker_config_add_watch_directory_roots (TrackerConfig	 *config,
					  gchar * const	 *roots)
{
	TrackerConfigPriv  *priv;
	gchar		   *validated_root;
	gchar * const	   *p;

	g_return_if_fail (TRACKER_IS_CONFIG (config));
	g_return_if_fail (roots != NULL);

	priv = GET_PRIV (config);

	for (p = roots; *p; p++) {
		validated_root = config_dir_validate_name (*p);
		if (!validated_root) {
			g_print ("Root '%s' is not valid to add to watch directory list\n",
				 validated_root);
			continue;
		}

		priv->watch_directory_roots = g_slist_append (priv->watch_directory_roots,
							      validated_root);
	}

	/* We only do this for watch directory roots right now, not
	 * sure why.
	 */
	config_dir_check_roots_for_conflicts (config);

	g_object_notify (G_OBJECT (config), "watch-directory-roots");
}

void
tracker_config_add_crawl_directory_roots (TrackerConfig	 *config,
					  gchar * const	 *roots)
{
	TrackerConfigPriv  *priv;
	gchar		   *validated_root;
	gchar * const	   *p;

	g_return_if_fail (TRACKER_IS_CONFIG (config));
	g_return_if_fail (roots != NULL);

	priv = GET_PRIV (config);

	for (p = roots; *p; p++) {
		validated_root = config_dir_validate_name (*p);
		if (!validated_root) {
			g_print ("Root '%s' is not valid to add to crawl directory list\n",
				 validated_root);
			continue;
		}

		priv->crawl_directory_roots = g_slist_append (priv->crawl_directory_roots,
							      validated_root);
	}

	g_object_notify (G_OBJECT (config), "crawl-directory-roots");
}

void
tracker_config_add_no_watch_directory_roots (TrackerConfig  *config,
					     gchar * const  *roots)
{
	TrackerConfigPriv  *priv;
	gchar		   *validated_root;
	gchar * const	   *p;

	g_return_if_fail (TRACKER_IS_CONFIG (config));
	g_return_if_fail (roots != NULL);

	priv = GET_PRIV (config);

	for (p = roots; *p; p++) {
		validated_root = config_dir_validate_name (*p);
		if (!validated_root) {
			g_print ("Root '%s' is not valid to add to no_watch directory list\n",
				 validated_root);
			continue;
		}

		priv->no_watch_directory_roots = g_slist_append (priv->no_watch_directory_roots,
								 validated_root);
	}

	g_object_notify (G_OBJECT (config), "no-watch-directory-roots");
}
