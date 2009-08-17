/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <gio/gio.h>

#include "tracker-crawler.h"
#include "tracker-marshal.h"

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
	GQueue	       *directories;
	GQueue	       *files;

	/* Idle handler for processing found data */
	guint		idle_id;

	gboolean        recurse;

	/* Statistics */
	GTimer	       *timer;
	guint		enumerations;
	guint		directories_found;
	guint		directories_ignored;
	guint		files_found;
	guint		files_ignored;

	/* Status */
	gboolean	is_running;
	gboolean	is_finished;
	gboolean        was_started;
};

enum {
	PROCESS_DIRECTORY,
	PROCESS_FILE,
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

static void tracker_crawler_finalize (GObject	      *object);
static void file_enumerate_next      (GFileEnumerator *enumerator,
				      EnumeratorData  *ed);
static void file_enumerate_children  (TrackerCrawler  *crawler,
				      GFile	      *file);

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (TrackerCrawler, tracker_crawler, G_TYPE_OBJECT)

static void
tracker_crawler_class_init (TrackerCrawlerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_crawler_finalize;

	signals[PROCESS_DIRECTORY] =
		g_signal_new ("process-directory",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_OBJECT);
	signals[PROCESS_FILE] =
		g_signal_new ("process-file",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_OBJECT);
	signals[FINISHED] =
		g_signal_new ("finished",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__UINT_UINT_UINT_UINT,
			      G_TYPE_NONE,
			      4,
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

	priv->directories = g_queue_new ();
	priv->files = g_queue_new ();
}

static void
tracker_crawler_finalize (GObject *object)
{
	TrackerCrawlerPrivate *priv;

	priv = TRACKER_CRAWLER_GET_PRIVATE (object);

	if (priv->timer) {
		g_timer_destroy (priv->timer);
	}

	if (priv->idle_id) {
		g_source_remove (priv->idle_id);
	}

	g_queue_foreach (priv->files, (GFunc) g_object_unref, NULL);
	g_queue_free (priv->files);

	g_queue_foreach (priv->directories, (GFunc) g_object_unref, NULL);
	g_queue_free (priv->directories);

	G_OBJECT_CLASS (tracker_crawler_parent_class)->finalize (object);
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
process_file (TrackerCrawler *crawler,
	      GFile	     *file)
{
	gchar *path;
	gboolean should_process = TRUE;

	g_signal_emit (crawler, signals[PROCESS_FILE], 0, file, &should_process);

	path = g_file_get_path (file);

	crawler->private->files_found++;

	if (should_process) {
		g_debug ("Found  :'%s' (%d)",
			 path,
			 crawler->private->enumerations);

	} else {
		g_debug ("Ignored:'%s' (%d)",
			 path,
			 crawler->private->enumerations);

		crawler->private->files_ignored++;
	}

	g_free (path);
	
	return should_process;
}

static gboolean
process_directory (TrackerCrawler *crawler,
		   GFile	  *file)
{
	gchar *path;
	gboolean should_process = TRUE;

	g_signal_emit (crawler, signals[PROCESS_DIRECTORY], 0, file, &should_process);

	path = g_file_get_path (file);

	crawler->private->directories_found++;

	if (should_process) {
		g_debug ("Found  :'%s' (%d)",
			 path,
			 crawler->private->enumerations);

		file_enumerate_children (crawler, file);
	} else {
		g_debug ("Ignored:'%s' (%d)",
			 path,
			 crawler->private->enumerations);

		crawler->private->directories_ignored++;
	}

	g_free (path);

	return should_process;
}

static gboolean
process_func (gpointer data)
{
	TrackerCrawler	      *crawler;
	TrackerCrawlerPrivate *priv;
	GFile		      *file;

	crawler = TRACKER_CRAWLER (data);
	priv = crawler->private;

#ifdef FIX
	/* If manually paused, we hold off until unpaused */
	if (tracker_status_get_is_paused_manually () ||
	    tracker_status_get_is_paused_for_io ()) {
		return TRUE;
	}
#endif

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
		process_file (crawler, file);
		g_object_unref (file);

		return TRUE;
	}

	/* Crawler directories */
	file = g_queue_pop_head (priv->directories);

	if (file) {
		process_directory (crawler, file);
		g_object_unref (file);

		return TRUE;
	}

	/* If we still have some async operations in progress, wait
	 * for them to finish, if not, we are truly done.
	 */
	if (priv->enumerations > 0) {
		return TRUE;
	}

	priv->idle_id = 0;
	priv->is_finished = TRUE;

	tracker_crawler_stop (crawler);

	return FALSE;
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
	crawler->private->enumerations--;

	if (!g_file_enumerator_close_finish (G_FILE_ENUMERATOR (enumerator),
					     result,
					     &error)) {
		g_warning ("Couldn't close GFileEnumerator (%p): %s", enumerator,
			   (error) ? error->message : "No reason");

		g_clear_error (&error);
	}
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

	enumerator = G_FILE_ENUMERATOR (object);

	ed = (EnumeratorData*) user_data;
	crawler = ed->crawler;
	parent = ed->parent;

	files = g_file_enumerator_next_files_finish (enumerator,
						     result,
						     &error);

	if (error || !files || !crawler->private->is_running) {
		if (error) {
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

		enumerator_data_process (ed);
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
					    G_PRIORITY_LOW,
					    NULL,
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

	parent = G_FILE (file);
	ed = (EnumeratorData*) user_data;
	crawler = ed->crawler;
	enumerator = g_file_enumerate_children_finish (parent, result, &error);

	if (!enumerator) {
		if (error) {
			gchar *path;

			path = g_file_get_path (parent);

			g_critical ("Could not open directory '%s': %s",
				    path, error->message);

			g_error_free (error);
			g_free (path);
		}

		crawler->private->enumerations--;
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

	crawler->private->enumerations++;

	ed = enumerator_data_new (crawler, file);

	g_file_enumerate_children_async (file,
					 FILE_ATTRIBUTES,
					 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					 G_PRIORITY_DEFAULT,
					 NULL,
					 file_enumerate_children_cb,
					 ed);
}

gboolean
tracker_crawler_start (TrackerCrawler *crawler,
		       const gchar    *path, 
		       gboolean        recurse)
{
	TrackerCrawlerPrivate *priv;
	GFile *file;

	g_return_val_if_fail (TRACKER_IS_CRAWLER (crawler), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	priv = crawler->private;

	priv->was_started = TRUE;
	priv->recurse = recurse;

	file = g_file_new_for_path (path);

	if (!g_file_query_exists (file, NULL)) {
		g_message ("NOT crawling directory %s:'%s' - path does not exist",
			   recurse ? "recursively" : "non-recursively",
			   path);

		
		g_object_unref (file);

		/* We return TRUE because this is likely a config
		 * option and we only return FALSE when we expect to
		 * not fail.
		 */
		return TRUE;
	}

	g_message ("Crawling directory %s:'%s'",
		   recurse ? "recursively" : "non-recursively",
		   path);

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

	/* Set idle handler to process directories and files found */
	priv->idle_id = g_idle_add (process_func, crawler);

	/* Start things off */
	add_directory (crawler, file, TRUE);
	g_object_unref (file);

	return TRUE;
}

void
tracker_crawler_stop (TrackerCrawler *crawler)
{
	TrackerCrawlerPrivate *priv;

	g_return_if_fail (TRACKER_IS_CRAWLER (crawler));

	priv = crawler->private;

	g_message ("%s crawling files in %4.4f seconds",
		   priv->is_finished ? "Finished" : "Stopped",
		   g_timer_elapsed (priv->timer, NULL));
	g_message ("  Found %d directories, ignored %d directories",
		   priv->directories_found,
		   priv->directories_ignored);
	g_message ("  Found %d files, ignored %d files",
		   priv->files_found,
		   priv->files_ignored);

	priv->is_running = FALSE;

	if (priv->idle_id) {
		g_source_remove (priv->idle_id);
		priv->idle_id = 0;
	}

	if (priv->timer) {
		g_timer_destroy (priv->timer);
		priv->timer = NULL;
	}

	g_signal_emit (crawler, signals[FINISHED], 0,
		       priv->directories_found,
		       priv->directories_ignored,
		       priv->files_found,
		       priv->files_ignored);
}
