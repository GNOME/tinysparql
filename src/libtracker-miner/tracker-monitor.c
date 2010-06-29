/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-keyfile-object.h>

#include "tracker-monitor.h"
#include "tracker-marshal.h"

#define TRACKER_MONITOR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_MONITOR, TrackerMonitorPrivate))

/* The life time of an item in the cache */
#define CACHE_LIFETIME_SECONDS 1

/* When we receive IO monitor events, we pause sending information to
 * the indexer for a few seconds before continuing. We have to receive
 * NO events for at least a few seconds before unpausing.
 */
#define PAUSE_ON_IO_SECONDS    5

/* If this is defined, we pause the indexer when we get events. If it
 * is not, we don't do any pausing.
 */
#undef  PAUSE_ON_IO

struct TrackerMonitorPrivate {
	GHashTable    *monitors;

	gboolean       enabled;
	gint           scan_timeout;
	gint           cache_timeout;

	GType          monitor_backend;

	guint          monitor_limit;
	gboolean       monitor_limit_warned;
	guint          monitors_ignored;

	/* For FAM, the _CHANGES_DONE event is not signalled, so we
	 * have to just use the _CHANGED event instead.
	 */
	gboolean       use_changed_event;

#ifdef PAUSE_ON_IO
	/* Timeout id for pausing when we get IO */
	guint          unpause_timeout_id;
#endif /* PAUSE_ON_IO */

	GHashTable    *event_pairs;
	guint          event_pairs_timeout_id;
};

typedef struct {
	GFile    *file;
	GTimeVal  start_time;
	GTimeVal  last_time;
	guint32   event_type;
} EventData;

enum {
	ITEM_CREATED,
	ITEM_UPDATED,
	ITEM_DELETED,
	ITEM_MOVED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_ENABLED,
	PROP_SCAN_TIMEOUT,
	PROP_CACHE_TIMEOUT
};

static void           tracker_monitor_finalize     (GObject        *object);
static void           tracker_monitor_set_property (GObject        *object,
                                                    guint           prop_id,
                                                    const GValue   *value,
                                                    GParamSpec     *pspec);
static void           tracker_monitor_get_property (GObject        *object,
                                                    guint           prop_id,
                                                    GValue         *value,
                                                    GParamSpec     *pspec);
static guint          get_inotify_limit            (void);
static GFileMonitor * directory_monitor_new        (TrackerMonitor *monitor,
						    GFile          *file);
static void           directory_monitor_cancel     (GFileMonitor     *dir_monitor);


static void           event_data_free              (gpointer        data);


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
		              tracker_marshal_VOID__OBJECT_BOOLEAN,
		              G_TYPE_NONE,
		              2,
		              G_TYPE_OBJECT,
		              G_TYPE_BOOLEAN);
	signals[ITEM_UPDATED] =
		g_signal_new ("item-updated",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              tracker_marshal_VOID__OBJECT_BOOLEAN,
		              G_TYPE_NONE,
		              2,
		              G_TYPE_OBJECT,
		              G_TYPE_BOOLEAN);
	signals[ITEM_DELETED] =
		g_signal_new ("item-deleted",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              tracker_marshal_VOID__OBJECT_BOOLEAN,
		              G_TYPE_NONE,
		              2,
		              G_TYPE_OBJECT,
		              G_TYPE_BOOLEAN);
	signals[ITEM_MOVED] =
		g_signal_new ("item-moved",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              tracker_marshal_VOID__OBJECT_OBJECT_BOOLEAN_BOOLEAN,
		              G_TYPE_NONE,
		              4,
		              G_TYPE_OBJECT,
		              G_TYPE_OBJECT,
		              G_TYPE_BOOLEAN,
		              G_TYPE_BOOLEAN);

	g_object_class_install_property (object_class,
	                                 PROP_ENABLED,
	                                 g_param_spec_boolean ("enabled",
	                                                       "Enabled",
	                                                       "Enabled",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_SCAN_TIMEOUT,
	                                 g_param_spec_int ("scan-timeout",
	                                                   "Scan Timeout",
	                                                   "Time in seconds between same events to prevent flooding (0->1000)",
	                                                   0,
	                                                   1000,
	                                                   0,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_CACHE_TIMEOUT,
	                                 g_param_spec_int ("cache-timeout",
	                                                   "Scan Timeout",
	                                                   "Time in seconds for events to be cached (0->1000)",
	                                                   0,
	                                                   1000,
	                                                   60,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerMonitorPrivate));
}

static void
tracker_monitor_init (TrackerMonitor *object)
{
	TrackerMonitorPrivate *priv;
	GFile                 *file;
	GFileMonitor          *monitor;
	const gchar           *name;

	object->private = TRACKER_MONITOR_GET_PRIVATE (object);

	priv = object->private;

	/* By default we enable monitoring */
	priv->enabled = TRUE;

	/* Create monitors table for this module */
	priv->monitors =
		g_hash_table_new_full (g_file_hash,
		                       (GEqualFunc) g_file_equal,
		                       (GDestroyNotify) g_object_unref,
		                       (GDestroyNotify) directory_monitor_cancel);

	priv->event_pairs =
		g_hash_table_new_full (g_file_hash,
				       (GEqualFunc) g_file_equal,
				       (GDestroyNotify) g_object_unref,
				       event_data_free);

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
			g_message ("Monitor backend is Inotify");

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

	g_file_monitor_cancel (monitor);
	g_object_unref (monitor);
	g_object_unref (file);

	g_message ("Monitor limit is %d", priv->monitor_limit);
}

static void
tracker_monitor_finalize (GObject *object)
{
	TrackerMonitorPrivate *priv;

	priv = TRACKER_MONITOR_GET_PRIVATE (object);

#ifdef PAUSE_ON_IO
	if (priv->unpause_timeout_id) {
		g_source_remove (priv->unpause_timeout_id);
	}
#endif /* PAUSE_ON_IO */

	g_hash_table_unref (priv->event_pairs);
	g_hash_table_unref (priv->monitors);

	G_OBJECT_CLASS (tracker_monitor_parent_class)->finalize (object);
}

static void
tracker_monitor_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_ENABLED:
		tracker_monitor_set_enabled (TRACKER_MONITOR (object),
		                             g_value_get_boolean (value));
		break;
	case PROP_SCAN_TIMEOUT:
		tracker_monitor_set_scan_timeout (TRACKER_MONITOR (object),
		                                  g_value_get_int (value));
		break;
	case PROP_CACHE_TIMEOUT:
		tracker_monitor_set_cache_timeout (TRACKER_MONITOR (object),
		                                   g_value_get_int (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_monitor_get_property (GObject      *object,
                              guint         prop_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
	TrackerMonitorPrivate *priv;

	priv = TRACKER_MONITOR_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_ENABLED:
		g_value_set_boolean (value, priv->enabled);
		break;
	case PROP_SCAN_TIMEOUT:
		g_value_set_int (value, priv->scan_timeout);
		break;
	case PROP_CACHE_TIMEOUT:
		g_value_set_int (value, priv->cache_timeout);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static guint
get_inotify_limit (void)
{
	GError      *error = NULL;
	const gchar *filename;
	gchar       *contents = NULL;
	guint        limit;

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

#ifdef PAUSE_ON_IO

static gboolean
unpause_cb (gpointer data)
{
	TrackerMonitor *monitor;

	monitor = data;

	g_message ("Resuming indexing now we have stopped "
	           "receiving monitor events for %d seconds",
	           PAUSE_ON_IO_SECONDS);

	monitor->private->unpause_timeout_id = 0;
	tracker_status_set_is_paused_for_io (FALSE);

	return FALSE;
}

#endif /* PAUSE_ON_IO */

static gboolean
check_is_directory (TrackerMonitor *monitor,
		    GFile          *file)
{
	GFileType file_type;

	file_type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL);

	if (file_type == G_FILE_TYPE_DIRECTORY)
		return TRUE;

	if (file_type == G_FILE_TYPE_UNKNOWN) {
		/* Whatever it was, it's gone. Check the monitors
		 * hashtable to know whether it was a directory
		 * we knew about
		 */
		if (g_hash_table_lookup (monitor->private->monitors, file) != NULL)
			return TRUE;
	}

	return FALSE;
}

static EventData *
event_data_new (GFile   *file,
                guint32  event_type)
{
	EventData *event;
	GTimeVal now;

	event = g_slice_new0 (EventData);
	g_get_current_time (&now);

	event->file = g_object_ref (file);
	event->start_time = now;
	event->last_time = now;
	event->event_type = event_type;

	return event;
}

static void
event_data_free (gpointer data)
{
	EventData *event;

	event = data;

	g_object_unref (event->file);
	g_slice_free (EventData, data);
}

static gboolean
tracker_monitor_move (TrackerMonitor *monitor,
                      GFile          *old_file,
                      GFile          *new_file)
{
	GHashTableIter iter;
	GHashTable *new_monitors;
	gchar *old_prefix;
	gpointer iter_file, iter_file_monitor;
	guint items_moved = 0;

	/* So this is tricky. What we have to do is:
	 *
	 * 1) Add all monitors for the new_file directory hierarchy
	 * 2) Then remove the monitors for old_file
	 *
	 * This order is necessary because inotify can reuse watch
	 * descriptors, and libinotify will remove handles
	 * asynchronously on IN_IGNORE, so the opposite sequence
	 * may possibly remove valid, just added, monitors.
	 */
	new_monitors = g_hash_table_new_full (g_file_hash,
	                                      (GEqualFunc) g_file_equal,
	                                      (GDestroyNotify) g_object_unref,
	                                      NULL);
	old_prefix = g_file_get_path (old_file);

	/* Find out which subdirectories should have a file monitor added */
	g_hash_table_iter_init (&iter, monitor->private->monitors);
	while (g_hash_table_iter_next (&iter, &iter_file, &iter_file_monitor)) {
		GFile *f;
		gchar *old_path, *new_path;
		gchar *new_prefix;
		gchar *p;

		if (!g_file_has_prefix (iter_file, old_file) &&
		    !g_file_equal (iter_file, old_file)) {
			continue;
		}

		old_path = g_file_get_path (iter_file);
		p = strstr (old_path, old_prefix);

		if (!p || strcmp (p, old_prefix) == 0) {
			g_free (old_path);
			continue;
		}

		/* Move to end of prefix */
		p += strlen (old_prefix) + 1;

		/* Check this is not the end of the string */
		if (*p == '\0') {
			g_free (old_path);
			continue;
		}

		new_prefix = g_file_get_path (new_file);
		new_path = g_build_path (G_DIR_SEPARATOR_S, new_prefix, p, NULL);
		g_free (new_prefix);

		f = g_file_new_for_path (new_path);
		g_free (new_path);

		if (!g_hash_table_lookup (new_monitors, f)) {
			g_hash_table_insert (new_monitors, f, GINT_TO_POINTER (1));
		} else {
			g_object_unref (f);
		}

		g_free (old_path);
		items_moved++;
	}

	/* Add a new monitor for the top level directory */
	tracker_monitor_add (monitor, new_file);

	/* Add a new monitor for all subdirectories */
	g_hash_table_iter_init (&iter, new_monitors);
	while (g_hash_table_iter_next (&iter, &iter_file, NULL)) {
		tracker_monitor_add (monitor, iter_file);
		g_hash_table_iter_remove (&iter);
	}

	/* Remove the monitor for the old top level directory hierarchy */
	tracker_monitor_remove_recursively (monitor, old_file);

	g_hash_table_unref (new_monitors);
	g_free (old_prefix);

	return items_moved > 0;
}

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
#if GLIB_CHECK_VERSION (2, 23, 6)
	case G_FILE_MONITOR_EVENT_MOVED:
		return "G_FILE_MONITOR_EVENT_MOVED";
#endif /* GLIB_CHECK_VERSION */
	}

	return "unknown";
}

static void
emit_signal_for_event (TrackerMonitor *monitor,
		       EventData      *event_data)
{
	gboolean is_directory;

	is_directory = check_is_directory (monitor, event_data->file);

	if (event_data->event_type == G_FILE_MONITOR_EVENT_CREATED) {
		g_signal_emit (monitor,
			       signals[ITEM_CREATED], 0,
			       event_data->file,
			       is_directory);
	} else {
		g_signal_emit (monitor,
			       signals[ITEM_UPDATED], 0,
			       event_data->file,
			       is_directory);
	}
}

static gboolean
event_pairs_timeout_cb (gpointer user_data)
{
	TrackerMonitor *monitor;
	GHashTableIter iter;
	gpointer key, value;
	GTimeVal now;

	monitor = user_data;
	g_hash_table_iter_init (&iter, monitor->private->event_pairs);
	g_get_current_time (&now);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GFile *file = key;
		EventData *event_data = value;
		glong seconds;
		gchar *uri;

		seconds = now.tv_sec - event_data->start_time.tv_sec;

		if (seconds < 2) {
			continue;
		}

		uri = g_file_get_uri (file);

		g_debug ("Event '%s' for URI '%s' has timed out (%ld seconds have elapsed)",
		         monitor_event_to_string (event_data->event_type),
			 uri, seconds);

		g_free (uri);

		emit_signal_for_event (monitor, event_data);
		g_hash_table_iter_remove (&iter);
	}

	if (g_hash_table_size (monitor->private->event_pairs) > 0) {
		return TRUE;
	}

	g_debug ("No more events to pair");
	monitor->private->event_pairs_timeout_id = 0;
	return FALSE;
}

static void
monitor_event_cb (GFileMonitor	    *file_monitor,
		  GFile		    *file,
		  GFile		    *other_file,
		  GFileMonitorEvent  event_type,
		  gpointer	     user_data)
{
	TrackerMonitor *monitor;
	gchar	       *str1;
	gchar	       *str2;
	gboolean	is_directory;
	EventData      *event_data;

	monitor = user_data;

	if (G_UNLIKELY (!monitor->private->enabled)) {
		g_debug ("Silently dropping monitor event, monitor disabled for now");
		return;
	}

	str1 = g_file_get_path (file);

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

	is_directory = check_is_directory (monitor, file);

#ifdef PAUSE_ON_IO
	if (monitor->private->unpause_timeout_id != 0) {
		g_source_remove (monitor->private->unpause_timeout_id);
	} else {
		g_message ("Pausing indexing because we are "
			   "receiving monitor events");

		tracker_status_set_is_paused_for_io (TRUE);
	}

	monitor->private->unpause_timeout_id =
		g_timeout_add_seconds (PAUSE_ON_IO_SECONDS,
				       unpause_cb,
				       monitor);
#endif /* PAUSE_ON_IO */

	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CHANGED:
		if (!monitor->private->use_changed_event) {
			/* Do nothing (using CHANGES_DONE_HINT) */
			break;
		}

		/* Fall through */
	case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
		if (!g_hash_table_lookup (monitor->private->event_pairs, file)) {
			g_hash_table_insert (monitor->private->event_pairs,
					     g_object_ref (file),
					     event_data_new (file, event_type));
		}

		break;
	case G_FILE_MONITOR_EVENT_CREATED:
		if (!g_hash_table_lookup (monitor->private->event_pairs, file)) {
			g_hash_table_insert (monitor->private->event_pairs,
					     g_object_ref (file),
					     event_data_new (file, event_type));
		}

		break;

	case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		/* Note: the general case is that this event is the last
		 * one after a series of EVENT_CHANGED ones. It may be the
		 * case that after this one, an ATTRIBUTE_CHANGED one is
		 * issued, as when downloading a file with wget, where
		 * mtime of the file is modified. But this last case is
		 * quite specific, so we will anyway signal the event
		 * right away instead of adding it to the HT.
		 */

		event_data = g_hash_table_lookup (monitor->private->event_pairs, file);

		if (event_data) {
			emit_signal_for_event (monitor, event_data);
			g_hash_table_remove (monitor->private->event_pairs, file);
		} else {
			event_data = event_data_new (file, event_type);
			emit_signal_for_event (monitor, event_data);
			event_data_free (event_data);
		}

		break;
	case G_FILE_MONITOR_EVENT_DELETED:
		if (is_directory) {
			tracker_monitor_remove_recursively (monitor, file);
		}

		g_signal_emit (monitor,
			       signals[ITEM_DELETED], 0,
			       file,
			       is_directory);
		break;

#if GLIB_CHECK_VERSION (2, 23, 6)
	case G_FILE_MONITOR_EVENT_MOVED:
		g_signal_emit (monitor,
			       signals[ITEM_MOVED], 0,
			       file,
			       other_file,
			       is_directory,
			       TRUE);

		if (is_directory) {
			tracker_monitor_move (monitor, file, other_file);
		}

		break;
#endif /* GLIB_CHECK_VERSION */

	case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
	case G_FILE_MONITOR_EVENT_UNMOUNTED:
		/* Do nothing */
		break;
	}

	if (g_hash_table_size (monitor->private->event_pairs) > 0) {
		if (monitor->private->event_pairs_timeout_id == 0) {
			g_debug ("Waiting for event pairs");
			monitor->private->event_pairs_timeout_id =
				g_timeout_add_seconds (CACHE_LIFETIME_SECONDS,
						       event_pairs_timeout_cb,
						       monitor);
		}
	} else {
		if (monitor->private->event_pairs_timeout_id != 0) {
			g_source_remove (monitor->private->event_pairs_timeout_id);
		}

		monitor->private->event_pairs_timeout_id = 0;
	}

	g_free (str1);
	g_free (str2);
}

static GFileMonitor *
directory_monitor_new (TrackerMonitor *monitor,
		       GFile          *file)
{
	GFileMonitor *file_monitor;
	GError *error = NULL;

	file_monitor = g_file_monitor_directory (file,
#if GLIB_CHECK_VERSION (2, 23, 6)
						 G_FILE_MONITOR_SEND_MOVED |
#endif /* GLIB_CHECK_VERSION */
						 G_FILE_MONITOR_WATCH_MOUNTS,
						 NULL,
						 &error);

	if (error) {
		gchar *path;

		path = g_file_get_path (file);
		g_warning ("Could not add monitor for path:'%s', %s",
			   path, error->message);

		g_error_free (error);
		g_free (path);

		return NULL;
	}

	g_signal_connect (file_monitor, "changed",
			  G_CALLBACK (monitor_event_cb),
			  monitor);

	return file_monitor;
}

static void
directory_monitor_cancel (GFileMonitor *monitor)
{
	if (monitor) {
		g_file_monitor_cancel (G_FILE_MONITOR (monitor));
		g_object_unref (monitor);
	}
}

TrackerMonitor *
tracker_monitor_new (void)
{
	return g_object_new (TRACKER_TYPE_MONITOR, NULL);
}

gboolean
tracker_monitor_get_enabled (TrackerMonitor *monitor)
{
	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), FALSE);

	return monitor->private->enabled;
}

gint
tracker_monitor_get_scan_timeout (TrackerMonitor *monitor)
{
	TrackerMonitorPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), 0);

	priv = TRACKER_MONITOR_GET_PRIVATE (monitor);

	return priv->scan_timeout;
}

gint
tracker_monitor_get_cache_timeout (TrackerMonitor *monitor)
{
	TrackerMonitorPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), 60);

	priv = TRACKER_MONITOR_GET_PRIVATE (monitor);

	return priv->cache_timeout;
}

void
tracker_monitor_set_enabled (TrackerMonitor *monitor,
                             gboolean        enabled)
{
	GList *keys, *k;

	g_return_if_fail (TRACKER_IS_MONITOR (monitor));

	/* Don't replace all monitors if we are already
	 * enabled/disabled.
	 */
	if (monitor->private->enabled == enabled) {
		return;
	}

	monitor->private->enabled = enabled;
	g_object_notify (G_OBJECT (monitor), "enabled");

	keys = g_hash_table_get_keys (monitor->private->monitors);

	/* Update state on all monitored dirs */
	for (k = keys; k != NULL; k = k->next) {
		GFile *file;

		file = k->data;

		if (enabled) {
			GFileMonitor *dir_monitor;

			dir_monitor = directory_monitor_new (monitor, file);
			g_hash_table_replace (monitor->private->monitors,
			                      g_object_ref (file), dir_monitor);
		} else {
			/* Remove monitor */
			g_hash_table_replace (monitor->private->monitors,
			                      g_object_ref (file), NULL);
		}
	}

	g_list_free (keys);
}

void
tracker_monitor_set_scan_timeout (TrackerMonitor *monitor,
                                  gint            value)
{
	TrackerMonitorPrivate *priv;

	g_return_if_fail (TRACKER_IS_MONITOR (monitor));

	if (!tracker_keyfile_object_validate_int (monitor, "scan-timeout", value)) {
		return;
	}

	priv = TRACKER_MONITOR_GET_PRIVATE (monitor);

	priv->scan_timeout = value;
	g_object_notify (G_OBJECT (monitor), "scan-timeout");
}

void
tracker_monitor_set_cache_timeout (TrackerMonitor *monitor,
                                   gint            value)
{
	TrackerMonitorPrivate *priv;

	g_return_if_fail (TRACKER_IS_MONITOR (monitor));

	if (!tracker_keyfile_object_validate_int (monitor, "cache-timeout", value)) {
		return;
	}

	priv = TRACKER_MONITOR_GET_PRIVATE (monitor);

	priv->cache_timeout = value;
	g_object_notify (G_OBJECT (monitor), "cache-timeout");
}

gboolean
tracker_monitor_add (TrackerMonitor *monitor,
                     GFile          *file)
{
	GFileMonitor *dir_monitor = NULL;
	gchar *path;

	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	if (g_hash_table_lookup (monitor->private->monitors, file)) {
		return TRUE;
	}

	/* Cap the number of monitors */
	if (g_hash_table_size (monitor->private->monitors) >= monitor->private->monitor_limit) {
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

	if (monitor->private->enabled) {
		/* We don't check if a file exists or not since we might want
		 * to monitor locations which don't exist yet.
		 *
		 * Also, we assume ALL paths passed are directories.
		 */
		dir_monitor = directory_monitor_new (monitor, file);

		if (!dir_monitor) {
			g_warning ("Could not add monitor for path:'%s'",
			           path);
			g_free (path);
			return FALSE;
		}
	}

	/* NOTE: it is ok to add a NULL file_monitor, when our
	 * enabled/disabled state changes, we iterate all keys and
	 * add or remove monitors.
	 */
	g_hash_table_insert (monitor->private->monitors,
	                     g_object_ref (file),
	                     dir_monitor);

	g_debug ("Added monitor for path:'%s', total monitors:%d",
	         path,
	         g_hash_table_size (monitor->private->monitors));

	g_free (path);

	return TRUE;
}

gboolean
tracker_monitor_remove (TrackerMonitor *monitor,
                        GFile          *file)
{
	gboolean removed;

	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	removed = g_hash_table_remove (monitor->private->monitors, file);

	if (removed) {
		gchar *path;

		path = g_file_get_path (file);
		g_debug ("Removed monitor for path:'%s', total monitors:%d",
		         path,
		         g_hash_table_size (monitor->private->monitors));

		g_free (path);
	}

	return removed;
}

gboolean
tracker_monitor_remove_recursively (TrackerMonitor *monitor,
                                    GFile          *file)
{
	GHashTableIter iter;
	gpointer iter_file, iter_file_monitor;
	guint items_removed = 0;

	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	g_hash_table_iter_init (&iter, monitor->private->monitors);
	while (g_hash_table_iter_next (&iter, &iter_file, &iter_file_monitor)) {
		gchar *path;

		if (!g_file_has_prefix (iter_file, file) &&
		    !g_file_equal (iter_file, file)) {
			continue;
		}

		path = g_file_get_path (iter_file);

		g_hash_table_iter_remove (&iter);

		g_debug ("Removed monitor for path:'%s', total monitors:%d",
		         path,
		         g_hash_table_size (monitor->private->monitors));

		g_free (path);

		/* We reset this because now it is possible we have limit - 1 */
		monitor->private->monitor_limit_warned = FALSE;
		items_removed++;
	}

	return items_removed > 0;
}

gboolean
tracker_monitor_is_watched (TrackerMonitor *monitor,
                            GFile          *file)
{
	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	return g_hash_table_lookup (monitor->private->monitors, file) != NULL;
}

gboolean
tracker_monitor_is_watched_by_string (TrackerMonitor *monitor,
                                      const gchar    *path)
{
	GFile      *file;
	gboolean    watched;

	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	file = g_file_new_for_path (path);
	watched = g_hash_table_lookup (monitor->private->monitors, file) != NULL;
	g_object_unref (file);

	return watched;
}

guint
tracker_monitor_get_count (TrackerMonitor *monitor)
{
	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), 0);

	return g_hash_table_size (monitor->private->monitors);
}

guint
tracker_monitor_get_ignored (TrackerMonitor *monitor)
{
	g_return_val_if_fail (TRACKER_IS_MONITOR (monitor), 0);

	return monitor->private->monitors_ignored;
}
