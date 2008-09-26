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

#include <string.h>
#include <stdlib.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-module-config.h>

#ifdef HAVE_INOTIFY
#include <linux/inotify.h>
#include <libinotify/libinotify.h>
#define USE_LIBINOTIFY
#endif /* HAVE_INOTIFY */

#include "tracker-monitor.h"
#include "tracker-dbus.h"
#include "tracker-indexer-client.h"
#include "tracker-marshal.h"
#include "tracker-status.h"

#define TRACKER_MONITOR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_MONITOR, TrackerMonitorPrivate))

/* Each file that a monitor event is received for is added to the
 * black list with a number. This number is the count for how many
 * events we have received for that file. If the count is above this
 * number below, then it is officially black listed and we don't
 * propagate events up the stack.
 */
#define BLACK_LIST_MAX_HITS    5

/* Each file is only on the black list for a certain time before it
 * is removed. When it is removed, a signal is emitted to force the
 * indexer to check the file out. This means, for example, if you
 * download a file, the indexer only indexes it AFTER this timeout
 * below AFTER it has stopped getting events. The timeout is in
 * seconds.
 */
#define BLACK_LIST_MAX_SECONDS 30

/* When we receive IO monitor events, we pause sending information to
 * the indexer for a few seconds before continuing. We have to receive
 * NO events for at least a few seconds before unpausing.
 */
#define PAUSE_FOR_IO_SECONDS   5

struct _TrackerMonitorPrivate {
	TrackerConfig *config;

	GHashTable    *modules;

	gboolean       enabled;

	guint	       black_list_timeout_id;
	GHashTable    *black_list_count;
	GHashTable    *black_list_timestamps;

	GType	       monitor_backend;

	guint	       monitor_limit;
	gboolean       monitor_limit_warned;
	guint	       monitors_ignored;

	/* For FAM, the _CHANGES_DONE event is not signalled, so we
	 * have to just use the _CHANGED event instead.
	 */
	gboolean       use_changed_event;

	/* Timeout id for pausing when we get IO */
	guint	       unpause_timeout_id;

#ifdef USE_LIBINOTIFY
	GHashTable    *event_pairs;
	GHashTable    *event_time_by_cookie;
	GHashTable    *event_type_by_cookie;

	guint	       event_check_timeout_id;
#endif /* USE_LIBINOTIFY */
};

enum {
	ITEM_CREATED,
	ITEM_UPDATED,
	ITEM_DELETED,
	ITEM_MOVED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_ENABLED
};

static void	      tracker_monitor_finalize	   (GObject	   *object);
static void	      tracker_monitor_set_property (GObject	   *object,
						    guint	    prop_id,
						    const GValue   *value,
						    GParamSpec	   *pspec);
static void	      tracker_monitor_get_property (GObject	   *object,
						    guint	    prop_id,
						    GValue	   *value,
						    GParamSpec	   *pspec);
static gboolean       black_list_check_items_cb    (gpointer	    data);
static void	      black_list_print_all	   (TrackerMonitor *monitor);
static guint	      get_inotify_limit		   (void);

#ifdef USE_LIBINOTIFY
static INotifyHandle *libinotify_monitor_directory (TrackerMonitor *monitor,
						    GFile	   *file);
static void	      libinotify_monitor_cancel    (gpointer	    data);
#endif /* USE_LIBINOTIFY */

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(TrackerMonitor, tracker_monitor, G_TYPE_OBJECT)

static void
tracker_monitor_class_init (TrackerMonitorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_monitor_finalize;
	object_class->set_property = tracker_monitor_set_property;
	object_class->get_property = tracker_monitor_get_property;

	signals[ITEM_CREATED] =
		g_signal_new ("item-created",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_OBJECT_BOOLEAN,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_STRING,
			      G_TYPE_OBJECT,
			      G_TYPE_BOOLEAN);
	signals[ITEM_UPDATED] =
		g_signal_new ("item-updated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_OBJECT_BOOLEAN,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_STRING,
			      G_TYPE_OBJECT,
			      G_TYPE_BOOLEAN);
	signals[ITEM_DELETED] =
		g_signal_new ("item-deleted",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_OBJECT_BOOLEAN,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_STRING,
			      G_TYPE_OBJECT,
			      G_TYPE_BOOLEAN);
	signals[ITEM_MOVED] =
		g_signal_new ("item-moved",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      tracker_marshal_VOID__STRING_OBJECT_OBJECT_BOOLEAN,
			      G_TYPE_NONE,
			      4,
			      G_TYPE_STRING,
			      G_TYPE_OBJECT,
			      G_TYPE_OBJECT,
			      G_TYPE_BOOLEAN);

	g_object_class_install_property (object_class,
					 PROP_ENABLED,
					 g_param_spec_boolean ("enabled",
							       "Enabled",
							       "Enabled",
							       TRUE,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerMonitorPrivate));
}

static void
tracker_monitor_init (TrackerMonitor *object)
{
	TrackerMonitorPrivate *priv;
	GFile		      *file;
	GFileMonitor	      *monitor;
	GList		      *all_modules, *l;
	const gchar	      *name;

	object->private = TRACKER_MONITOR_GET_PRIVATE (object);

	priv = object->private;

	/* By default we enable monitoring */
	priv->enabled = TRUE;

	/* For each module we create a hash table for monitors */
	priv->modules =
		g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       g_free,
				       (GDestroyNotify) g_hash_table_unref);

	/* The black list is for files which are CONSTANTLY being
	 * updated causing a lot of traffic and processing. We add
	 * these to the black list so we don't keep pushing events up
	 * the stack. This is black list is not saved, it is per
	 * session for the daemon only.
	 */
	priv->black_list_count =
		g_hash_table_new_full (g_file_hash,
				       (GEqualFunc) g_file_equal,
				       g_object_unref,
				       NULL);
	priv->black_list_timestamps =
		g_hash_table_new_full (g_file_hash,
				       (GEqualFunc) g_file_equal,
				       g_object_unref,
				       NULL);

#ifdef USE_LIBINOTIFY
	/* We have a hash table with cookies so we can pair up move
	 * events.
	 */
	priv->event_pairs =
		g_hash_table_new_full (g_direct_hash,
				       g_direct_equal,
				       NULL,
				       g_object_unref);
	priv->event_time_by_cookie =
		g_hash_table_new_full (g_direct_hash,
				       g_direct_equal,
				       NULL,
				       NULL);
	priv->event_type_by_cookie =
		g_hash_table_new_full (g_direct_hash,
				       g_direct_equal,
				       NULL,
				       NULL);
#endif /* USE_LIBINOTIFY */

	all_modules = tracker_module_config_get_modules ();

	for (l = all_modules; l; l = l->next) {
		GHashTable *monitors;

		/* Create monitors table for this module */
#ifdef USE_LIBINOTIFY
		monitors =
			g_hash_table_new_full (g_file_hash,
					       (GEqualFunc) g_file_equal,
					       (GDestroyNotify) g_object_unref,
					       (GDestroyNotify) libinotify_monitor_cancel);
#else  /* USE_LIBINOTIFY */
		monitors =
			g_hash_table_new_full (g_file_hash,
					       (GEqualFunc) g_file_equal,
					       (GDestroyNotify) g_object_unref,
					       (GDestroyNotify) g_file_monitor_cancel);
#endif /* USE_LIBINOTIFY */

		g_hash_table_insert (priv->modules, g_strdup (l->data), monitors);
	}

	g_list_free (all_modules);

	/* For the first monitor we get the type and find out if we
	 * are using inotify, FAM, polling, etc.
	 */
	file = g_file_new_for_path (g_get_home_dir ());
	monitor = g_file_monitor_directory (file,
					    G_FILE_MONITOR_WATCH_MOUNTS,
					    NULL,
					    NULL);

	priv->monitor_backend = G_OBJECT_TYPE (monitor);

	/* We use the name because the type itself is actually
	 * private and not available publically. Note this is
	 * subject to change, but unlikely of course.
	 */
	name = g_type_name (priv->monitor_backend);
	if (name) {
		/* Set limits based on backend... */
		if (strcmp (name, "GInotifyDirectoryMonitor") == 0) {
			/* Using inotify */
			g_message ("Monitor backend is INotify");

			/* Setting limit based on kernel
			 * settings in /proc...
			 */
			priv->monitor_limit = get_inotify_limit ();

			/* We don't use 100% of the monitors, we allow other
			 * applications to have at least 500 or so to use
			 * between them selves. This only
			 * applies to inotify because it is a
			 * user shared resource.
			 */
			priv->monitor_limit -= 500;

			/* Make sure we don't end up with a
			 * negative maximum.
			 */
			priv->monitor_limit = MAX (priv->monitor_limit, 0);
		}
		else if (strcmp (name, "GFamDirectoryMonitor") == 0) {
			/* Using Fam */
			g_message ("Monitor backend is Fam");

			/* Setting limit to an arbitary limit
			 * based on testing
			 */
			priv->monitor_limit = 400;
			priv->use_changed_event = TRUE;
		}
		else if (strcmp (name, "GFenDirectoryMonitor") == 0) {
			/* Using Fen, what is this? */
			g_message ("Monitor backend is Fen");

			/* Guessing limit... */
			priv->monitor_limit = 8192;
		}
		else if (strcmp (name, "GWin32DirectoryMonitor") == 0) {
			/* Using Windows */
			g_message ("Monitor backend is Windows");

			/* Guessing limit... */
			priv->monitor_limit = 8192;
		}
		else {
			/* Unknown */
			g_warning ("Monitor backend:'%s' is unknown, we have no limits "
				   "in place because we don't know what we are dealing with!",
				   name);

			/* Guessing limit... */
			priv->monitor_limit = 100;
		}
	}

	g_message ("Monitor limit is %d", priv->monitor_limit);

	g_file_monitor_cancel (monitor);
	g_object_unref (monitor);
	g_object_unref (file);
}

static void
tracker_monitor_finalize (GObject *object)
{
	TrackerMonitorPrivate *priv;

	priv = TRACKER_MONITOR_GET_PRIVATE (object);

	if (priv->unpause_timeout_id) {
		g_source_remove (priv->unpause_timeout_id);
	}

	black_list_print_all (TRACKER_MONITOR (object));

	g_hash_table_unref (priv->black_list_timestamps);
	g_hash_table_unref (priv->black_list_count);

#ifdef USE_LIBINOTIFY
	if (priv->event_check_timeout_id) {
		g_source_remove (priv->event_check_timeout_id);
	}

	g_hash_table_unref (priv->event_type_by_cookie);
	g_hash_table_unref (priv->event_time_by_cookie);
	g_hash_table_unref (priv->event_pairs);
#endif /* USE_LIBINOTIFY */

	if (priv->black_list_timeout_id) {
		g_source_remove (priv->black_list_timeout_id);
	}

	g_hash_table_unref (priv->modules);

	g_object_unref (priv->config);

	G_OBJECT_CLASS (tracker_monitor_parent_class)->finalize (object);
}

static void
tracker_monitor_set_property (GObject	   *object,
			      guint	    prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_ENABLED:
		tracker_monitor_set_enabled (TRACKER_MONITOR (object),
					     g_value_get_boolean (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_monitor_get_property (GObject	   *object,
			      guint	    prop_id,
			      GValue	   *value,
			      GParamSpec   *pspec)
{
	TrackerMonitorPrivate *priv;

	priv = TRACKER_MONITOR_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_ENABLED:
		g_value_set_boolean (value, priv->enabled);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static guint
get_inotify_limit (void)
{
	GError	    *error = NULL;
	const gchar *filename;
	gchar	    *contents = NULL;
	guint	     limit;

	filename = "/proc/sys/fs/inotify/max_user_watches";

	if (!g_file_get_contents (filename,
				  &contents,
				  NULL,
				  &error)) {
		g_warning ("Couldn't get INotify monitor limit from:'%s', %s",
			   filename,
			   error ? error->message : "no error given");
		g_clear_error (&error);

		/* Setting limit to an arbitary limit */
		limit = 8192;
	} else {
		limit = atoi (contents);
		g_free (contents);
	}

	return limit;
}

static const gchar *
get_queue_from_gfile (GHashTable *modules,
		      GFile	 *file)
{
	GHashTable  *hash_table;
	GList	    *all_modules, *l;
	const gchar *module_name = NULL;

	all_modules = g_hash_table_get_keys (modules);

	for (l = all_modules; l && !module_name; l = l->next) {
		hash_table = g_hash_table_lookup (modules, l->data);

		if (g_hash_table_lookup (hash_table, file)) {
			module_name = l->data;
		}
	}

	g_list_free (all_modules);

	return module_name;
}

static const gchar *
get_module_name_from_gfile (TrackerMonitor *monitor,
			    GFile	   *file,
			    gboolean	   *is_directory)
{
	const gchar *module_name;

	if (is_directory) {
		*is_directory = TRUE;
	}

	/* First try to get the module name from the file, this will
	 * only work if the event we received is for a directory.
	 */
	module_name = get_queue_from_gfile (monitor->private->modules, file);
	if (!module_name) {
		GFile *parent;

		/* Second we try to get the module name from the base
		 * name of the file.
		 */
		parent = g_file_get_parent (file);
		module_name = get_queue_from_gfile (monitor->private->modules, parent);

		if (!module_name) {
			gchar *child_path;
			gchar *parent_path;

			child_path = g_file_get_path (file);
			parent_path = g_file_get_path (parent);

			g_warning ("Could not get module name from GFile (path:'%s' or parent:'%s')",
				   child_path,
				   parent_path);

			g_free (parent_path);
			g_free (child_path);

			return NULL;
		}

		if (is_directory) {
			*is_directory = FALSE;
		}
	}

	return module_name;
}

static gboolean
black_list_check_items_cb (gpointer data)
{
	TrackerMonitor *monitor;
	GHashTableIter	iter;
	GTimeVal	t;
	gchar	       *path;
	gpointer	key;
	gpointer	value;
	gsize		seconds;
	gsize		seconds_now;
	gsize		seconds_diff;
	guint		count;

	monitor = data;

	g_get_current_time (&t);
	seconds_now = t.tv_sec;

	g_hash_table_iter_init (&iter, monitor->private->black_list_timestamps);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		seconds = GPOINTER_TO_SIZE (value);
		seconds_diff = seconds_now - seconds;

		/* Do absolutely nothing if this file hasn't been
		 * black listed for at least BLACK_LIST_MAX_SECONDS.
		 */
		if (seconds_diff < BLACK_LIST_MAX_SECONDS) {
			continue;
		}

		path = g_file_get_path (key);
		g_debug ("Removing '%s' from black list count", path);
		g_free (path);

		/* Only signal if count >= BLACK_LIST_MAX_HITS, since
		 * we have mitigated signals due to spamming, so we
		 * make sure that we signal the indexer that this
		 * file needs a check.
		 */
		value = g_hash_table_lookup (monitor->private->black_list_count, key);
		count = GPOINTER_TO_UINT (value);

		if (count >= BLACK_LIST_MAX_HITS) {
			const gchar *module_name;
			gboolean     is_directory;

			module_name = get_module_name_from_gfile (data,
								  key,
								  &is_directory);
			if (!module_name) {
				continue;
			}

			g_signal_emit (data,
				       signals[ITEM_CREATED], 0,
				       module_name,
				       key,
				       is_directory);
		}

		/* Remove from hash tables (i.e. white list it) */
		g_hash_table_remove (monitor->private->black_list_count, key);
		g_hash_table_remove (monitor->private->black_list_timestamps, key);

		/* Reset iterators */
		g_hash_table_iter_init (&iter, monitor->private->black_list_timestamps);
	}

	/* If the hash tables are empty, don't keep calling this
	 * callback, this interrupts and wastes battery. Set up the
	 * timeout again when we need it instead.
	 */
	if (g_hash_table_size (monitor->private->black_list_count) < 1) {
		g_debug ("No further items on the black list, removing check timeout");
		monitor->private->black_list_timeout_id = 0;
		return FALSE;
	}

	return TRUE;
}

static gboolean
black_list_file_check (TrackerMonitor *monitor,
		       GFile	      *file)
{
	gpointer data;
	guint	 count;

	data = g_hash_table_lookup (monitor->private->black_list_count, file);
	count = GPOINTER_TO_UINT (data);

	return count >= BLACK_LIST_MAX_HITS;
}

static void
black_list_file_increment (TrackerMonitor *monitor,
			   GFile	  *file)
{
	GTimeVal  t;
	gchar	 *path;
	gpointer  data;
	guint	  count;

	data = g_hash_table_lookup (monitor->private->black_list_count, file);
	count = GPOINTER_TO_UINT (data);

	/* Replace the black listed item with the updated count for
	 * how many times an event has occurred for this path
	 */
	count++;
	g_get_current_time (&t);

	g_hash_table_replace (monitor->private->black_list_count,
			      g_object_ref (file),
			      GUINT_TO_POINTER (count));
	g_hash_table_replace (monitor->private->black_list_timestamps,
			      g_object_ref (file),
			      GSIZE_TO_POINTER (t.tv_sec));

	if (monitor->private->black_list_timeout_id == 0) {
		monitor->private->black_list_timeout_id =
			g_timeout_add_seconds (1,
					       black_list_check_items_cb,
					       monitor);
	}

	path = g_file_get_path (file);
	g_debug ("Adding '%s' to black list count:%d (MAX is %d)",
		 path,
		 count,
		 BLACK_LIST_MAX_HITS);
	g_free (path);
}

static void
black_list_print_all (TrackerMonitor *monitor)
{
	GHashTableIter	iter;
	gchar	       *path;
	gpointer	key;
	gpointer	value;
	guint		count;
	gboolean	none = TRUE;

	g_message ("Summary of black list: (with >= %d events)",
		   BLACK_LIST_MAX_HITS);

	g_hash_table_iter_init (&iter, monitor->private->black_list_count);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		count = GPOINTER_TO_UINT (value);

		if (count < BLACK_LIST_MAX_HITS) {
			continue;
		}

		none = FALSE;

		path = g_file_get_path (key);
		g_message ("  %s (%d events)", path, count);
		g_free (path);
	}

	if (none) {
		g_message ("  NONE");
	}
}

static void
indexer_pause_cb (DBusGProxy *proxy,
		  GError     *error,
		  gpointer    user_data)
{
	if (error) {
		g_message ("Could not pause the indexer, %s",
			   error->message);
	}
}

static void
indexer_continue_cb (DBusGProxy *proxy,
		     GError	*error,
		     gpointer	 user_data)
{
	if (error) {
		g_message ("Could not continue the indexer, %s",
			   error->message);
	}
}

static gboolean
unpause_cb (gpointer data)
{
	TrackerMonitor *monitor;

	monitor = data;

	monitor->private->unpause_timeout_id = 0;
	tracker_status_set_is_paused_for_io (FALSE);

	g_message ("Resumming indexing now we have stopped "
		   "receiving monitor events for %d seconds",
		   PAUSE_FOR_IO_SECONDS);

	if (!tracker_status_get_is_paused_manually ()) {
		org_freedesktop_Tracker_Indexer_continue_async (tracker_dbus_indexer_get_proxy (),
								indexer_continue_cb,
								NULL);
	}

	return FALSE;
}

#ifdef USE_LIBINOTIFY

static gchar *
libinotify_monitor_event_to_string (guint32 event_type)
{
	GString *s;

	s = g_string_new ("");

	if (event_type & IN_ACCESS) {
		s = g_string_append (s, "IN_ACCESS | ");
	}
	if (event_type & IN_MODIFY) {
		s = g_string_append (s, "IN_MODIFY | ");
	}
	if (event_type & IN_ATTRIB) {
		s = g_string_append (s, "IN_ATTRIB | ");
	}
	if (event_type & IN_CLOSE_WRITE) {
		s = g_string_append (s, "IN_CLOSE_WRITE | ");
	}
	if (event_type & IN_CLOSE_NOWRITE) {
		s = g_string_append (s, "IN_CLOSE_NOWRITE | ");
	}
	if (event_type & IN_OPEN) {
		s = g_string_append (s, "IN_OPEN | ");
	}
	if (event_type & IN_MOVED_FROM) {
		s = g_string_append (s, "IN_MOVED_FROM | ");
	}
	if (event_type & IN_MOVED_TO) {
		s = g_string_append (s, "IN_MOVED_TO | ");
	}
	if (event_type & IN_CREATE) {
		s = g_string_append (s, "IN_CREATE | ");
	}
	if (event_type & IN_DELETE) {
		s = g_string_append (s, "IN_DELETE | ");
	}
	if (event_type & IN_DELETE_SELF) {
		s = g_string_append (s, "IN_DELETE_SELF | ");
	}
	if (event_type & IN_MOVE_SELF) {
		s = g_string_append (s, "IN_MOVE_SELF | ");
	}
	if (event_type & IN_UNMOUNT) {
		s = g_string_append (s, "IN_UNMOUNT | ");
	}
	if (event_type & IN_Q_OVERFLOW) {
		s = g_string_append (s, "IN_Q_OVERFLOW | ");
	}
	if (event_type & IN_IGNORED) {
		s = g_string_append (s, "IN_IGNORED | ");
	}

	/* helper events */
	if (event_type & IN_CLOSE) {
		s = g_string_append (s, "IN_CLOSE* | ");
	}
	if (event_type & IN_MOVE) {
		s = g_string_append (s, "IN_MOVE* | ");
	}

	/* special flags */
	if (event_type & IN_MASK_ADD) {
		s = g_string_append (s, "IN_MASK_ADD^ | ");
	}
	if (event_type & IN_ISDIR) {
		s = g_string_append (s, "IN_ISDIR^ | ");
	}
	if (event_type & IN_ONESHOT) {
		s = g_string_append (s, "IN_ONESHOT^ | ");
	}

	s->str[s->len - 3] = '\0';

	return g_string_free (s, FALSE);
}

static gboolean
libinotify_event_check_timeout_cb (gpointer data)
{
	TrackerMonitor *monitor;
	GTimeVal	t;
	GHashTableIter	iter;
	gpointer	key, value;

	monitor = data;

	g_debug ("Checking for event pairs which have timed out...");

	g_get_current_time (&t);

	g_hash_table_iter_init (&iter, monitor->private->event_time_by_cookie);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		glong	     seconds;
		glong	     seconds_then;
		guint32      event_type;
		GFile	    *file;
		const gchar *module_name;
		gboolean     is_directory;
		gpointer     p;

		seconds_then = GPOINTER_TO_SIZE (value);

		seconds  = t.tv_sec;
		seconds -= seconds_then;

		g_debug ("Comparing now:%ld to then:%ld, diff:%ld",
			 t.tv_sec,
			 seconds_then,
			 seconds);

		if (seconds < 2) {
			continue;
		}

		file = g_hash_table_lookup (monitor->private->event_pairs, key);
		p = g_hash_table_lookup (monitor->private->event_type_by_cookie, key);
		event_type = GPOINTER_TO_UINT (p);

		/* We didn't receive an event pair for this
		 * cookie, so we just generate the CREATE or
		 * DELETE event for the file we know about.
		 */
		g_debug ("Event:%d with cookie:%d has timed out (%ld seconds have elapsed)",
			 event_type,
			 GPOINTER_TO_UINT (key),
			 seconds);

		module_name = get_module_name_from_gfile (monitor,
							  file,
							  &is_directory);

		switch (event_type) {
		case IN_MOVE_SELF:
		case IN_MOVED_FROM:
		case IN_DELETE:
		case IN_DELETE_SELF:
			/* So we new the source, but not the
			 * target location for the event.
			 */
			g_signal_emit (monitor,
				       signals[ITEM_DELETED], 0,
				       module_name,
				       file,
				       is_directory);
			break;

		case IN_CREATE:
		case IN_MOVED_TO:
			/* So we new the target, but not the
			 * source location for the event.
			 */
			g_signal_emit (monitor,
				       signals[ITEM_CREATED], 0,
				       module_name,
				       file,
				       is_directory);
			break;
		}

		/* Clean up */
		g_hash_table_remove (monitor->private->event_pairs, key);
		g_hash_table_remove (monitor->private->event_time_by_cookie, key);
		g_hash_table_remove (monitor->private->event_type_by_cookie, key);

		/* Reset the iter, so we start from the top */
		g_hash_table_iter_init (&iter, monitor->private->event_time_by_cookie);
	}

	if (g_hash_table_size (monitor->private->event_time_by_cookie) < 1) {
		g_debug ("No more events to pair, removing timeout");
		monitor->private->event_check_timeout_id = 0;
		return FALSE;
	}

	return TRUE;
}

static void
libinotify_monitor_event_cb (INotifyHandle *handle,
			     const char    *monitor_name,
			     const char    *filename,
			     guint32	    event_type,
			     guint32	    cookie,
			     gpointer	    user_data)
{
	TrackerMonitor *monitor;
	GFile	       *file;
	GFile	       *other_file;
	const gchar    *module_name;
	gchar	       *str1;
	gchar	       *str2;
	gboolean	is_directory;
	gchar	       *event_type_str;

	monitor = user_data;

	switch (event_type) {
	case IN_Q_OVERFLOW:
	case IN_OPEN:
	case IN_CLOSE_NOWRITE:
	case IN_ACCESS:
	case IN_IGNORED:
		/* Return, otherwise we spam with messages for every
		 * file we open and look at.
		 */
		return;
	default:
		break;
	}

	if (G_UNLIKELY (!monitor->private->enabled)) {
		g_debug ("Silently dropping monitor event, monitor disabled for now");
		return;
	}

	if (monitor_name) {
		str1 = g_build_filename (monitor_name, filename, NULL);
		file = g_file_new_for_path (str1);
	} else {
		str1 = NULL;
		file = g_file_new_for_path (filename);
	}

	other_file = NULL;

	module_name = get_module_name_from_gfile (monitor,
						  file,
						  &is_directory);
	if (!module_name) {
		g_free (str1);
		g_object_unref (file);
		return;
	}

	if (!str1) {
		str1 = g_file_get_path (file);
	}

	/* This doesn't outright black list a file, it purely adds
	 * each file to the hash table so we have a count for every
	 * path we get an event for. If the count is too high, we
	 * then don't propagate the event up.
	 */
	switch (event_type) {
	case IN_MOVE_SELF:
	case IN_MOVED_FROM:
	case IN_MOVED_TO:
		/* If the event is a move type, we don't increment the
		 * black list count to avoid missing the second event
		 * to pair the two up.
		 */
		break;

	default:
		black_list_file_increment (monitor, file);
		break;
	}

	if (other_file) {
		str2 = g_file_get_path (other_file);
	} else {
		str2 = NULL;
	}

	event_type_str = libinotify_monitor_event_to_string (event_type);
	g_message ("Received monitor event:%d->'%s' for file:'%s' (cookie:%d)",
		   event_type,
		   event_type_str,
		   str1 ? str1 : "",
		   cookie);
	g_free (event_type_str);

	if (!black_list_file_check (monitor, file)) {
		if (monitor->private->unpause_timeout_id != 0) {
			g_source_remove (monitor->private->unpause_timeout_id);
		} else {
			g_message ("Pausing indexing because we are "
				   "receiving monitor events");

			tracker_status_set_is_paused_for_io (TRUE);

			org_freedesktop_Tracker_Indexer_pause_async (tracker_dbus_indexer_get_proxy (),
								     indexer_pause_cb,
								     NULL);
		}

		monitor->private->unpause_timeout_id =
			g_timeout_add_seconds (PAUSE_FOR_IO_SECONDS,
					       unpause_cb,
					       monitor);

		if (cookie > 0) {
			/* First check if we already have a file in
			 * the event pairs hash table.
			 */
			other_file = g_hash_table_lookup (monitor->private->event_pairs,
							  GINT_TO_POINTER (cookie));
			if (!other_file) {
				GTimeVal t;

				g_get_current_time (&t);
				g_hash_table_insert (monitor->private->event_pairs,
						     GUINT_TO_POINTER (cookie),
						     g_object_ref (file));
				g_hash_table_insert (monitor->private->event_time_by_cookie,
						     GUINT_TO_POINTER (cookie),
						     GSIZE_TO_POINTER (t.tv_sec));
				g_hash_table_insert (monitor->private->event_type_by_cookie,
						     GUINT_TO_POINTER (cookie),
						     GUINT_TO_POINTER (event_type));
			} else {
				gchar *other_path;

				other_path = g_file_get_path (other_file);
				g_free (other_path);
			}

			/* Add a check for old cookies we didn't
			 * receive the follow up pair event for.
			 */
			if (!monitor->private->event_check_timeout_id) {
				g_debug ("Setting up event pair timeout check");

				monitor->private->event_check_timeout_id =
					g_timeout_add_seconds (2,
							       libinotify_event_check_timeout_cb,
							       monitor);
			}
		}

		switch (event_type) {
		case IN_MODIFY:
			if (!monitor->private->use_changed_event) {
				/* Do nothing */
				break;
			}

		case IN_CLOSE_WRITE:
		case IN_ATTRIB:
			g_signal_emit (monitor,
				       signals[ITEM_UPDATED], 0,
				       module_name,
				       file,
				       is_directory);
			break;

		case IN_MOVE_SELF:
		case IN_MOVED_FROM:
		case IN_DELETE:
		case IN_DELETE_SELF:
			if (cookie == 0) {
				g_signal_emit (monitor,
					       signals[ITEM_DELETED], 0,
					       module_name,
					       file,
					       is_directory);
			} else if (other_file) {
				g_signal_emit (monitor,
					       signals[ITEM_MOVED], 0,
					       module_name,
					       file,
					       other_file,
					       is_directory);
				g_hash_table_remove (monitor->private->event_pairs,
						     GUINT_TO_POINTER (cookie));
				g_hash_table_remove (monitor->private->event_time_by_cookie,
						     GUINT_TO_POINTER (cookie));
				g_hash_table_remove (monitor->private->event_type_by_cookie,
						     GUINT_TO_POINTER (cookie));
			}

			break;

		case IN_CREATE:
		case IN_MOVED_TO:
			/* FIXME: What if we don't monitor the other
			 * location?
			 */
			if (cookie == 0) {
				g_signal_emit (monitor,
					       signals[ITEM_CREATED], 0,
					       module_name,
					       file,
					       is_directory);
			} else if (other_file) {
				g_signal_emit (monitor,
					       signals[ITEM_MOVED], 0,
					       module_name,
					       other_file,
					       file,
					       is_directory);
				g_hash_table_remove (monitor->private->event_pairs,
						     GUINT_TO_POINTER (cookie));
				g_hash_table_remove (monitor->private->event_time_by_cookie,
						     GUINT_TO_POINTER (cookie));
				g_hash_table_remove (monitor->private->event_type_by_cookie,
						     GUINT_TO_POINTER (cookie));
			}

			break;

		case IN_UNMOUNT:
			g_signal_emit (monitor,
				       signals[ITEM_DELETED], 0,
				       module_name,
				       file,
				       is_directory);
			break;

		case IN_Q_OVERFLOW:
		case IN_OPEN:
		case IN_CLOSE_NOWRITE:
		case IN_ACCESS:
		case IN_IGNORED:
			/* Do nothing */
			break;
		}
	} else {
		g_message ("Not propagating event, file is black listed");
	}

	g_free (str1);
	g_free (str2);
	g_object_unref (file);
}

static INotifyHandle *
libinotify_monitor_directory (TrackerMonitor *monitor,
			      GFile	     *file)
{
	INotifyHandle *handle;
	gchar	      *path;
	guint32	       mask;
	unsigned long  flags;
	gboolean       is_directory;

	is_directory = TRUE;
	flags = 0;
	mask = IN_ALL_EVENTS;

	/* For files */
	if (is_directory) {
		flags &= ~IN_FLAG_FILE_BASED;
	} else {
		flags |= IN_FLAG_FILE_BASED;
	}

	path = g_file_get_path (file);
	handle = inotify_monitor_add (path,
				      mask,
				      flags,
				      libinotify_monitor_event_cb,
				      monitor);
	g_free (path);

	if (!handle) {
		return NULL;
	}

	return handle;
}

static void
libinotify_monitor_cancel (gpointer data)
{
	inotify_monitor_remove (data);
}

#else  /* USE_LIBINOTIFY */

static const gchar *
monitor_event_to_string (GFileMonitorEvent event_type)
{
	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CHANGED:
		return "G_FILE_MONITOR_EVENT_CHANGED";
	case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		return "G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT";
	case G_FILE_MONITOR_EVENT_DELETED:
		return "G_FILE_MONITOR_EVENT_DELETED";
	case G_FILE_MONITOR_EVENT_CREATED:
		return "G_FILE_MONITOR_EVENT_CREATED";
	case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
		return "G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED";
	case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
		return "G_FILE_MONITOR_EVENT_PRE_UNMOUNT";
	case G_FILE_MONITOR_EVENT_UNMOUNTED:
		return "G_FILE_MONITOR_EVENT_UNMOUNTED";
	}

	return "unknown";
}

static void
monitor_event_cb (GFileMonitor	    *file_monitor,
		  GFile		    *file,
		  GFile		    *other_file,
		  GFileMonitorEvent  event_type,
		  gpointer	     user_data)
{
	TrackerMonitor *monitor;
	const gchar    *module_name;
	gchar	       *str1;
	gchar	       *str2;
	gboolean	is_directory;

	monitor = user_data;

	if (G_UNLIKELY (!monitor->private->enabled)) {
		g_debug ("Silently dropping monitor event, monitor disabled for now");
		return;
	}

	module_name = get_module_name_from_gfile (monitor,
						  file,
						  &is_directory);
	if (!module_name) {
		return;
	}

	str1 = g_file_get_path (file);

	/* This doesn't outright black list a file, it purely adds
	 * each file to the hash table so we have a count for every
	 * path we get an event for. If the count is too high, we
	 * then don't propagate the event up.
	 */
	black_list_file_increment (monitor, file);

	if (other_file) {
		str2 = g_file_get_path (other_file);
	} else {
		str2 = NULL;
	}

	g_message ("Received monitor event:%d->'%s' for file:'%s' and other file:'%s'",
		   event_type,
		   monitor_event_to_string (event_type),
		   str1,
		   str2 ? str2 : "");

	if (!black_list_file_check (monitor, file)) {
		if (monitor->private->unpause_timeout_id != 0) {
			g_source_remove (monitor->private->unpause_timeout_id);
		} else {
			g_message ("Pausing indexing because we are "
				   "receiving monitor events");

			tracker_status_set_is_paused_for_io (TRUE);

			org_freedesktop_Tracker_Indexer_pause_async (tracker_dbus_indexer_get_proxy (),
								     indexer_pause_cb,
								     NULL);
		}

		monitor->private->unpause_timeout_id =
			g_timeout_add_seconds (PAUSE_FOR_IO_SECONDS,
					       unpause_cb,
					       monitor);

		switch (event_type) {
		case G_FILE_MONITOR_EVENT_CHANGED:
			if (!monitor->private->use_changed_event) {
				/* Do nothing */
				break;
			}

		case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
			g_signal_emit (monitor,
				       signals[ITEM_UPDATED], 0,
				       module_name,
				       file,
				       is_directory);
			break;

		case G_FILE_MONITOR_EVENT_DELETED:
			g_signal_emit (monitor,
				       signals[ITEM_DELETED], 0,
				       module_name,
				       file,
				       is_directory);
			break;

		case G_FILE_MONITOR_EVENT_CREATED:
			g_signal_emit (monitor,
				       signals[ITEM_CREATED], 0,
				       module_name,
				       file,
				       is_directory);
			break;

		case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
			g_signal_emit (monitor,
				       signals[ITEM_DELETED], 0,
				       module_name,
				       file,
				       is_directory);
			break;

		case G_FILE_MONITOR_EVENT_UNMOUNTED:
			/* Do nothing */
			break;
		}
	} else {
		g_message ("Not propagating event, file is black listed");
	}

	g_free (str1);
	g_free (str2);
}

#endif /* USE_LIBINOTIFY */

TrackerMonitor *
tracker_monitor_new (TrackerConfig *config)
{
	TrackerMonitor	      *monitor;
	TrackerMonitorPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), NULL);

	monitor = g_object_new (TRACKER_TYPE_MONITOR, NULL);

	priv = monitor->private;

	priv->config = g_object_ref (config);

	return monitor;
}

gboolean
tracker_monitor_get_enabled (TrackerMonitor *monitor)
{
	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), FALSE);

	return monitor->private->enabled;
}

void
tracker_monitor_set_enabled (TrackerMonitor *monitor,
			     gboolean	     enabled)
{
	g_return_if_fail (TRACKER_IS_MONITOR (monitor));

	monitor->private->enabled = enabled;

	g_object_notify (G_OBJECT (monitor), "enabled");
}

gboolean
tracker_monitor_add (TrackerMonitor *monitor,
		     const gchar    *module_name,
		     GFile	    *file)
{
#ifdef USE_LIBINOTIFY
	INotifyHandle *file_monitor;
#else  /* USE_LIBINOTIFY */
	GFileMonitor *file_monitor;
	GError	     *error = NULL;
#endif /* USE_LIBINOTIFY */
	GHashTable   *monitors;
	GSList	     *ignored_roots;
	GSList	     *l;
	gchar	     *path;

	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), FALSE);
	g_return_val_if_fail (module_name != NULL, FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	if (!tracker_config_get_enable_watches (monitor->private->config)) {
		return TRUE;
	}

	monitors = g_hash_table_lookup (monitor->private->modules, module_name);
	if (!monitors) {
		g_warning ("No monitor hash table for module:'%s'",
			   module_name);
		return FALSE;
	}

	if (g_hash_table_lookup (monitors, file)) {
		return TRUE;
	}

	/* Cap the number of monitors */
	if (g_hash_table_size (monitors) >= monitor->private->monitor_limit) {
		monitor->private->monitors_ignored++;

		if (!monitor->private->monitor_limit_warned) {
			g_warning ("The maximum number of monitors to set (%d) "
				   "has been reached, not adding any new ones",
				   monitor->private->monitor_limit);
			monitor->private->monitor_limit_warned = TRUE;
		}

		return FALSE;
	}

	path = g_file_get_path (file);

	ignored_roots = tracker_config_get_no_watch_directory_roots (monitor->private->config);

	/* Check this location isn't excluded in the config */
	for (l = ignored_roots; l; l = l->next) {
		if (strcmp (path, l->data) == 0) {
			g_message ("Not adding montior for:'%s', path is in config ignore list",
				   path);
			g_free (path);
			return FALSE;
		}
	}

	/* We don't check if a file exists or not since we might want
	 * to monitor locations which don't exist yet.
	 *
	 * Also, we assume ALL paths passed are directories.
	 */
#ifdef USE_LIBINOTIFY
	file_monitor = libinotify_monitor_directory (monitor, file);

	if (!file_monitor) {
		g_warning ("Could not add monitor for path:'%s'",
			   path);
		g_free (path);
		return FALSE;
	}

	g_hash_table_insert (monitors,
			     g_object_ref (file),
			     file_monitor);
#else  /* USE_LIBINOTIFY */
	file_monitor = g_file_monitor_directory (file,
						 G_FILE_MONITOR_WATCH_MOUNTS,
						 NULL,
						 &error);

	if (error) {
		g_warning ("Could not add monitor for path:'%s', %s",
			   path,
			   error->message);
		g_free (path);
		g_error_free (error);
		return FALSE;
	}

	g_signal_connect (file_monitor, "changed",
			  G_CALLBACK (monitor_event_cb),
			  monitor);

	g_hash_table_insert (monitors,
			     g_object_ref (file),
			     file_monitor);
#endif /* USE_LIBINOTIFY */

	g_debug ("Added monitor for module:'%s', path:'%s', total monitors:%d",
		 module_name,
		 path,
		 g_hash_table_size (monitors));

	g_free (path);

	return TRUE;
}

gboolean
tracker_monitor_remove (TrackerMonitor *monitor,
			const gchar    *module_name,
			GFile	       *file)
{
	GFileMonitor *file_monitor;
	GHashTable   *monitors;
	gchar	     *path;

	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), FALSE);
	g_return_val_if_fail (module_name != NULL, FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	if (!tracker_config_get_enable_watches (monitor->private->config)) {
		return TRUE;
	}

	monitors = g_hash_table_lookup (monitor->private->modules, module_name);
	if (!monitors) {
		g_warning ("No monitor hash table for module:'%s'",
			   module_name);
		return FALSE;
	}

	file_monitor = g_hash_table_lookup (monitors, file);
	if (!file_monitor) {
		return TRUE;
	}

	/* We reset this because now it is possible we have limit - 1 */
	monitor->private->monitor_limit_warned = FALSE;

	g_hash_table_remove (monitors, file);

	path = g_file_get_path (file);

	g_debug ("Removed monitor for module:'%s', path:'%s', total monitors:%d",
		 module_name,
		 path,
		 g_hash_table_size (monitors));

	g_free (path);

	return TRUE;
}

gboolean
tracker_monitor_is_watched (TrackerMonitor *monitor,
			    const gchar    *module_name,
			    GFile	   *file)
{
	GHashTable *monitors;

	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), FALSE);
	g_return_val_if_fail (module_name != NULL, FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	monitors = g_hash_table_lookup (monitor->private->modules, module_name);
	if (!monitors) {
		g_warning ("No monitor hash table for module:'%s'",
			   module_name);
		return FALSE;
	}

	return g_hash_table_lookup (monitors, file) != NULL;
}

gboolean
tracker_monitor_is_watched_by_string (TrackerMonitor *monitor,
				      const gchar    *module_name,
				      const gchar    *path)
{
	GFile	   *file;
	GHashTable *monitors;
	gboolean    watched;

	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), FALSE);
	g_return_val_if_fail (module_name != NULL, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	monitors = g_hash_table_lookup (monitor->private->modules, module_name);
	if (!monitors) {
		g_warning ("No monitor hash table for module:'%s'",
			   module_name);
		return FALSE;
	}

	file = g_file_new_for_path (path);
	watched = g_hash_table_lookup (monitors, file) != NULL;
	g_object_unref (file);

	return watched;
}

guint
tracker_monitor_get_count (TrackerMonitor *monitor,
			   const gchar	  *module_name)
{
	guint count;

	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), 0);

	if (module_name) {
		GHashTable *monitors;

		monitors = g_hash_table_lookup (monitor->private->modules, module_name);
		if (!monitors) {
			g_warning ("No monitor hash table for module:'%s'",
				   module_name);
			return 0;
		}

		count = g_hash_table_size (monitors);
	} else {
		GList *all_modules, *l;

		all_modules = g_hash_table_get_values (monitor->private->modules);

		for (l = all_modules, count = 0; l; l = l->next) {
			count += g_hash_table_size (l->data);
		}

		g_list_free (all_modules);
	}

	return count;
}

guint
tracker_monitor_get_ignored (TrackerMonitor *monitor)
{
	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), 0);

	return monitor->private->monitors_ignored;
}
