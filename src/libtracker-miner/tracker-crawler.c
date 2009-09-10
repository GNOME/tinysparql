/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include "tracker-crawler.h"
#include "tracker-marshal.h"
#include "tracker-utils.h"

#define TRACKER_CRAWLER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_CRAWLER, TrackerCrawlerPrivate))

#define FILE_ATTRIBUTES				\
	G_FILE_ATTRIBUTE_STANDARD_NAME ","	\
	G_FILE_ATTRIBUTE_STANDARD_TYPE

#define FILES_QUEUE_PROCESS_INTERVAL 2000
#define FILES_QUEUE_PROCESS_MAX      5000

/* This is the number of files to be called back with from GIO at a
 * time so we don't get called back for every file.
 */
#define FILES_GROUP_SIZE	     100

struct TrackerCrawlerPrivate {
	/* Found data */
	GQueue	       *found;

	/* Usable data */
	GQueue	       *directories;
	GQueue	       *files;

	GCancellable   *cancellable;

	/* Idle handler for processing found data */
	guint		idle_id;

	gboolean        recurse;

	/* Statistics */
	GTimer	       *timer;
	guint		directories_found;
	guint		directories_ignored;
	guint		files_found;
	guint		files_ignored;

	/* Status */
	gboolean	is_running;
	gboolean	is_finished;
	gboolean        is_paused;
	gboolean        was_started;
};

enum {
	CHECK_DIRECTORY,
	CHECK_FILE,
	FINISHED,
	LAST_SIGNAL
};

typedef struct {
	GFile          *child;
	gboolean        is_dir;
} EnumeratorChildData;

typedef struct {
	TrackerCrawler *crawler;
	GFile	       *parent;
	GHashTable     *children;
} EnumeratorData;

static void     crawler_finalize        (GObject         *object);
static gboolean check_defaults          (TrackerCrawler  *crawler,
					 GFile           *file);
static void     file_enumerate_next     (GFileEnumerator *enumerator,
					 EnumeratorData  *ed);
static void     file_enumerate_children (TrackerCrawler  *crawler,
					 GFile           *file);

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (TrackerCrawler, tracker_crawler, G_TYPE_OBJECT)

static void
tracker_crawler_class_init (TrackerCrawlerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
        TrackerCrawlerClass *crawler_class = TRACKER_CRAWLER_CLASS (klass);

	object_class->finalize = crawler_finalize;

	crawler_class->check_directory = check_defaults;
	crawler_class->check_file      = check_defaults;

	signals[CHECK_DIRECTORY] =
		g_signal_new ("check-directory",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerCrawlerClass, check_directory),
			      tracker_accumulator_check_file,
			      NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_FILE);
	signals[CHECK_FILE] =
		g_signal_new ("check-file",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerCrawlerClass, check_file),
			      tracker_accumulator_check_file,
			      NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_FILE);
	signals[FINISHED] =
		g_signal_new ("finished",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerCrawlerClass, finished),
			      NULL, NULL,
			      tracker_marshal_VOID__POINTER_BOOLEAN_UINT_UINT_UINT_UINT,
			      G_TYPE_NONE,
			      6,
			      G_TYPE_POINTER,
			      G_TYPE_BOOLEAN,
			      G_TYPE_UINT,
			      G_TYPE_UINT,
			      G_TYPE_UINT,
			      G_TYPE_UINT);

	g_type_class_add_private (object_class, sizeof (TrackerCrawlerPrivate));
}

static void
tracker_crawler_init (TrackerCrawler *object)
{
	TrackerCrawlerPrivate *priv;

	object->private = TRACKER_CRAWLER_GET_PRIVATE (object);

	priv = object->private;

	priv->found = g_queue_new ();

	priv->directories = g_queue_new ();
	priv->files = g_queue_new ();

	priv->cancellable = g_cancellable_new ();
}

static void
crawler_finalize (GObject *object)
{
	TrackerCrawlerPrivate *priv;

	priv = TRACKER_CRAWLER_GET_PRIVATE (object);

	if (priv->timer) {
		g_timer_destroy (priv->timer);
	}

	if (priv->idle_id) {
		g_source_remove (priv->idle_id);
	}

	g_object_unref (priv->cancellable);

	g_queue_foreach (priv->found, (GFunc) g_object_unref, NULL);
	g_queue_free (priv->found);

	g_queue_foreach (priv->files, (GFunc) g_object_unref, NULL);
	g_queue_free (priv->files);

	g_queue_foreach (priv->directories, (GFunc) g_object_unref, NULL);
	g_queue_free (priv->directories);

	G_OBJECT_CLASS (tracker_crawler_parent_class)->finalize (object);
}

static gboolean 
check_defaults (TrackerCrawler *crawler,
		GFile          *file)
{
	return TRUE;
}

TrackerCrawler *
tracker_crawler_new (void)
{
	TrackerCrawler *crawler;

	crawler = g_object_new (TRACKER_TYPE_CRAWLER, NULL);

	return crawler;
}

static void
add_file (TrackerCrawler *crawler,
	  GFile		 *file)
{
	g_return_if_fail (G_IS_FILE (file));

	g_queue_push_tail (crawler->private->files, g_object_ref (file));
}

static void
add_directory (TrackerCrawler *crawler,
	       GFile	      *file,
	       gboolean        override)
{
	g_return_if_fail (G_IS_FILE (file));

	if (crawler->private->recurse || override) {
		g_queue_push_tail (crawler->private->directories, g_object_ref (file));
	}
}

static gboolean
check_file (TrackerCrawler *crawler,
	    GFile          *file)
{
	gboolean use = FALSE;

	g_signal_emit (crawler, signals[CHECK_FILE], 0, file, &use);

	crawler->private->files_found++;

	if (!use) {
		crawler->private->files_ignored++;
	}

	return use;
}

static gboolean
check_directory (TrackerCrawler *crawler,
		 GFile          *file)
{
	gboolean use = FALSE;

	g_signal_emit (crawler, signals[CHECK_DIRECTORY], 0, file, &use);

	crawler->private->directories_found++;

	if (use) {
		file_enumerate_children (crawler, file);
	} else {
		crawler->private->directories_ignored++;
	}

	return use;
}

static gboolean
process_func (gpointer data)
{
	TrackerCrawler	      *crawler;
	TrackerCrawlerPrivate *priv;
	GFile		      *file;

	crawler = TRACKER_CRAWLER (data);
	priv = crawler->private;

	if (priv->is_paused) {
		/* Stop the idle func for now until we are unpaused */
		priv->idle_id = 0;

		return FALSE;
	}

	/* Throttle the crawler, with testing, throttling every item
	 * took the time to crawl 130k files from 7 seconds up to 68
	 * seconds. So it is important to get this figure right.
	 */
#ifdef FIX
	tracker_throttle (priv->config, 25);
#endif

	/* Crawler files */
	file = g_queue_pop_head (priv->files);

	if (file) {
		if (check_file (crawler, file)) {
			g_queue_push_tail (priv->found, file);
		} else {
			g_object_unref (file);
		}

		return TRUE;
	}

	/* Crawler directories */
	file = g_queue_pop_head (priv->directories);

	if (file) {
		if (check_directory (crawler, file)) {
			g_queue_push_tail (priv->found, file);

			/* directory is being iterated, this idle function
			 * will be re-enabled right after it finishes.
			 */
			priv->idle_id = 0;

			return FALSE;
		} else {
			g_object_unref (file);
			return TRUE;
		}
	}

	priv->idle_id = 0;
	priv->is_finished = TRUE;

	tracker_crawler_stop (crawler);

	return FALSE;
}

static gboolean
process_func_start (TrackerCrawler *crawler)
{
	if (crawler->private->is_paused) {
		return FALSE;
	}

	if (crawler->private->is_finished) {
		return FALSE;
	}

	if (crawler->private->idle_id == 0) {
		crawler->private->idle_id = g_idle_add (process_func, crawler);
	}

	return TRUE;
}

static void
process_func_stop (TrackerCrawler *crawler)
{
	if (crawler->private->idle_id != 0) {
		g_source_remove (crawler->private->idle_id);
		crawler->private->idle_id = 0;
	}
}

static EnumeratorChildData *
enumerator_child_data_new (GFile    *child,
			   gboolean  is_dir)
{
	EnumeratorChildData *cd;

	cd = g_slice_new (EnumeratorChildData);

	cd->child = g_object_ref (child);
	cd->is_dir = is_dir;

	return cd;
}

static void
enumerator_child_data_free (EnumeratorChildData *cd)
{
	g_object_unref (cd->child);
	g_slice_free (EnumeratorChildData, cd);
}

static EnumeratorData *
enumerator_data_new (TrackerCrawler *crawler,
		     GFile	    *parent)
{
	EnumeratorData *ed;

	ed = g_slice_new0 (EnumeratorData);

	ed->crawler = g_object_ref (crawler);
	ed->parent = g_object_ref (parent);
	ed->children = g_hash_table_new_full (g_str_hash,
					      g_str_equal,
					      (GDestroyNotify) g_free,
					      (GDestroyNotify) enumerator_child_data_free);
	return ed;
}

static void
enumerator_data_add_child (EnumeratorData *ed,
			   const gchar    *name,
			   GFile          *file,
			   gboolean        is_dir)
{
	g_hash_table_insert (ed->children,
			     g_strdup (name),
			     enumerator_child_data_new (file, is_dir));
}

static void
enumerator_data_process (EnumeratorData *ed)
{
	TrackerCrawler *crawler;
	GHashTableIter iter;
	EnumeratorChildData *cd;

	crawler = ed->crawler;

#ifdef FIX
	GList *l;

	/* Ignore directory if its contents match something we should ignore */
	for (l = crawler->private->ignored_directories_with_content; l; l = l->next) {
		if (g_hash_table_lookup (ed->children, l->data)) {
			gchar *path;

			path = g_file_get_path (ed->parent);

			crawler->private->directories_ignored++;
			g_debug ("Ignoring directory '%s' since it contains a file named '%s'", path, (gchar *) l->data);
			g_free (path);

			return;
		}
	}
#endif

	g_hash_table_iter_init (&iter, ed->children);

	while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &cd)) {
		if (cd->is_dir) {
			add_directory (crawler, cd->child, FALSE);
		} else {
			add_file (crawler, cd->child);
		}
	}
}

static void
enumerator_data_free (EnumeratorData *ed)
{
	g_object_unref (ed->parent);
	g_object_unref (ed->crawler);
	g_hash_table_destroy (ed->children);
	g_slice_free (EnumeratorData, ed);
}

static void
file_enumerator_close_cb (GObject      *enumerator,
			  GAsyncResult *result,
			  gpointer	user_data)
{
	TrackerCrawler *crawler;
	GError *error = NULL;

	crawler = TRACKER_CRAWLER (user_data);

	if (!g_file_enumerator_close_finish (G_FILE_ENUMERATOR (enumerator),
					     result,
					     &error)) {
		g_warning ("Couldn't close GFileEnumerator (%p): %s", enumerator,
			   (error) ? error->message : "No reason");

		g_clear_error (&error);
	}

	/* Processing of directory is now finished,
	 * continue with queued files/directories.
	 */
	process_func_start (crawler);
}

static void
file_enumerate_next_cb (GObject      *object,
			GAsyncResult *result,
			gpointer      user_data)
{
	TrackerCrawler *crawler;
	EnumeratorData *ed;
	GFileEnumerator *enumerator;
	GFile *parent, *child;
	GFileInfo *info;
	GList *files, *l;
	GError *error = NULL;
	gboolean cancelled;

	enumerator = G_FILE_ENUMERATOR (object);

	ed = (EnumeratorData*) user_data;
	crawler = ed->crawler;
	parent = ed->parent;
	cancelled = g_cancellable_is_cancelled (crawler->private->cancellable);

	files = g_file_enumerator_next_files_finish (enumerator,
						     result,
						     &error);

	if (error || !files || !crawler->private->is_running) {
		if (error && !cancelled) {
			g_critical ("Could not crawl through directory: %s", error->message);
			g_error_free (error);
		}

		/* No more files or we are stopping anyway, so clean
		 * up and close all file enumerators.
		 */
		if (files) {
			g_list_foreach (files, (GFunc) g_object_unref, NULL);
			g_list_free (files);
		}

		if (!cancelled) {
			enumerator_data_process (ed);
		}

		enumerator_data_free (ed);
		g_file_enumerator_close_async (enumerator,
					       G_PRIORITY_DEFAULT,
					       NULL,
					       file_enumerator_close_cb,
					       crawler);
		g_object_unref (enumerator);

		return;
	}

	for (l = files; l; l = l->next) {
		const gchar *child_name;
		gboolean is_dir;

		info = l->data;

		child_name = g_file_info_get_name (info);
		child = g_file_get_child (parent, child_name);
		is_dir = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;

		enumerator_data_add_child (ed, child_name, child, is_dir);

		g_object_unref (child);
		g_object_unref (info);
	}

	g_list_free (files);

	/* Get next files */
	file_enumerate_next (enumerator, ed);
}

static void
file_enumerate_next (GFileEnumerator *enumerator,
		     EnumeratorData  *ed)
{
	g_file_enumerator_next_files_async (enumerator,
					    FILES_GROUP_SIZE,
					    G_PRIORITY_DEFAULT,
					    ed->crawler->private->cancellable,
					    file_enumerate_next_cb,
					    ed);
}

static void
file_enumerate_children_cb (GObject	 *file,
			    GAsyncResult *result,
			    gpointer	  user_data)
{
	TrackerCrawler *crawler;
	EnumeratorData *ed;
	GFileEnumerator *enumerator;
	GFile *parent;
	GError *error = NULL;
	gboolean cancelled;

	parent = G_FILE (file);
	ed = (EnumeratorData*) user_data;
	crawler = ed->crawler;
	cancelled = g_cancellable_is_cancelled (crawler->private->cancellable);
	enumerator = g_file_enumerate_children_finish (parent, result, &error);

	if (!enumerator) {
		if (error && !cancelled) {
			gchar *path;

			path = g_file_get_path (parent);

			g_warning ("Could not open directory '%s': %s",
				   path, error->message);

			g_error_free (error);
			g_free (path);
		}

		enumerator_data_free (ed);
		process_func_start (crawler);
		return;
	}

	/* Start traversing the directory's files */
	file_enumerate_next (enumerator, ed);
}

static void
file_enumerate_children (TrackerCrawler *crawler,
			 GFile		*file)
{
	EnumeratorData *ed;

	ed = enumerator_data_new (crawler, file);

	g_file_enumerate_children_async (file,
					 FILE_ATTRIBUTES,
					 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					 G_PRIORITY_LOW,
					 crawler->private->cancellable,
					 file_enumerate_children_cb,
					 ed);
}

gboolean
tracker_crawler_start (TrackerCrawler *crawler,
		       GFile          *file,
		       gboolean        recurse)
{
	TrackerCrawlerPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CRAWLER (crawler), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	priv = crawler->private;

	priv->was_started = TRUE;
	priv->recurse = recurse;

	if (g_cancellable_is_cancelled (priv->cancellable)) {
		g_cancellable_reset (priv->cancellable);
	}

	if (!g_file_query_exists (file, NULL)) {
		/* We return TRUE because this is likely a config
		 * option and we only return FALSE when we expect to
		 * not fail.
		 */
		return TRUE;
	}

	/* Time the event */
	if (priv->timer) {
		g_timer_destroy (priv->timer);
	}

	priv->timer = g_timer_new ();

	/* Set as running now */
	priv->is_running = TRUE;
	priv->is_finished = FALSE;

	/* Reset stats */
	priv->directories_found = 0;
	priv->directories_ignored = 0;
	priv->files_found = 0;
	priv->files_ignored = 0;

	/* Start things off */
	add_directory (crawler, file, TRUE);
	process_func_start (crawler);

	return TRUE;
}

void
tracker_crawler_stop (TrackerCrawler *crawler)
{
	TrackerCrawlerPrivate *priv;

	g_return_if_fail (TRACKER_IS_CRAWLER (crawler));

	priv = crawler->private;

	priv->is_running = FALSE;
	g_cancellable_cancel (priv->cancellable);

	process_func_stop (crawler);

	if (priv->timer) {
		g_timer_destroy (priv->timer);
		priv->timer = NULL;
	}

	g_signal_emit (crawler, signals[FINISHED], 0,
		       priv->found,
		       !priv->is_finished,
		       priv->directories_found,
		       priv->directories_ignored,
		       priv->files_found,
		       priv->files_ignored);

	/* Clean up queue */
	g_queue_foreach (priv->found, (GFunc) g_object_unref, NULL);
	g_queue_clear (priv->found);

	/* We don't free the queue in case the crawler is reused, it
	 * is only freed in finalize.
	 */
}

void
tracker_crawler_pause (TrackerCrawler *crawler)
{
	g_return_if_fail (TRACKER_IS_CRAWLER (crawler));
	
	crawler->private->is_paused = TRUE;

	if (crawler->private->is_running) {
		g_timer_stop (crawler->private->timer);
		process_func_stop (crawler);
	}

	g_message ("Crawler is paused, %s", 
		   crawler->private->is_running ? "currently running" : "not running");
}

void
tracker_crawler_resume (TrackerCrawler *crawler)
{
	g_return_if_fail (TRACKER_IS_CRAWLER (crawler));

	crawler->private->is_paused = FALSE;

	if (crawler->private->is_running) {
		g_timer_continue (crawler->private->timer);
		process_func_start (crawler);
	}

	g_message ("Crawler is resuming, %s", 
		   crawler->private->is_running ? "currently running" : "not running");
}
