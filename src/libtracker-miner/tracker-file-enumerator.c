/*
 * Copyright (C) 2014, Softathome <contact@softathome.com>
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
 *
 * Author: Martyn Russell <martyn@lanedo.com>
 */

#include "config.h"

#include "tracker-file-enumerator.h"

static void tracker_file_enumerator_file_iface_init (TrackerEnumeratorIface *iface);

struct _TrackerFileEnumerator {
	GObject parent_instance;
};

typedef struct {
	GFile *dir;
	gchar *attributes;
	GFileQueryInfoFlags flags;
} StartData;

G_DEFINE_TYPE_WITH_CODE (TrackerFileEnumerator, tracker_file_enumerator, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_ENUMERATOR,
                                                tracker_file_enumerator_file_iface_init))

static void
tracker_file_enumerator_finalize (GObject *object)
{
	G_OBJECT_CLASS (tracker_file_enumerator_parent_class)->finalize (object);
}

static void
tracker_file_enumerator_class_init (TrackerFileEnumeratorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = tracker_file_enumerator_finalize;
}

static void
tracker_file_enumerator_init (TrackerFileEnumerator *fe)
{
}

static StartData *
start_data_new (GFile               *dir,
                const gchar         *attributes,
                GFileQueryInfoFlags  flags)
{
	StartData *data;

	data = g_slice_new0 (StartData);
	data->dir = g_object_ref (dir);
	/* FIXME: inefficient */
	data->attributes = g_strdup (attributes);
	data->flags = flags;

	return data;
}

static void
start_data_free (StartData *data)
{
	if (!data) {
		return;
	}

	g_object_unref (data->dir);
	g_free (data->attributes);
	g_slice_free (StartData, data);
}

static GSList *
file_enumerator_start (TrackerEnumerator    *enumerator,
                       GFile                *dir,
                       const gchar          *attributes,
                       GFileQueryInfoFlags   flags,
                       GCancellable         *cancellable,
                       GError              **error)
{
	GFileEnumerator *fe;
	GSList *files;
	GError *local_error = NULL;
	gboolean cancelled;

	fe = g_file_enumerate_children (dir,
	                                attributes,
	                                flags,
	                                cancellable,
	                                &local_error);


	cancelled = g_cancellable_is_cancelled (cancellable);

	if (!enumerator) {
		if (local_error && !cancelled) {
			gchar *path;

			path = g_file_get_path (dir);

			g_warning ("Could not open directory '%s': %s",
			           path, local_error->message);

			g_propagate_error (error, local_error);
			g_free (path);
		}

		return NULL;
	}

	files = NULL;

	/* May as well be while TRUE ... */
	while (!cancelled) {
		GFileInfo *info;

		info = g_file_enumerator_next_file (fe, cancellable, &local_error);

		/* FIXME: Do we need a ->is_running check here like before? */
		if (local_error || !info) {
			if (local_error && !cancelled) {
				g_critical ("Could not crawl through directory: %s", local_error->message);
				g_propagate_error (error, local_error);
			}

			/* No more files or we are stopping anyway, so clean
			 * up and close all file enumerators.
			 */
			if (info) {
				g_object_unref (info);
			}

			/* FIXME: We need some check here to call
			 * enumerator_data_process which signals
			 * CHECK_DIRECTORY_CONTENTS
			 */
			g_file_enumerator_close (fe, NULL, &local_error);

			if (local_error) {
				g_warning ("Couldn't close GFileEnumerator (%p): %s", fe,
				           local_error ? local_error->message : "No reason");
				g_propagate_error (error, local_error);
			}

			g_object_unref (fe);

			break;
		}

		files = g_slist_prepend (files, info);
	}

	return g_slist_reverse (files);
}

static void
start_async_thread_op_free (GSList *files)
{
	g_slist_free_full (files, g_object_unref);
}

static void
start_async_thread (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
	TrackerEnumerator *enumerator = source_object;
	StartData *data = task_data;
	GSList *files = NULL;
	GError *error = NULL;

	if (g_cancellable_set_error_if_cancelled (cancellable, &error)) {
		files = NULL;
	} else {
		files = file_enumerator_start (enumerator,
		                               data->dir,
		                               data->attributes,
		                               data->flags,
		                               cancellable,
		                               &error);
	}

	if (error) {
		g_task_return_error (task, error);
	} else {
		g_task_return_pointer (task, files, (GDestroyNotify) start_async_thread_op_free);
	}
}

static void
file_enumerator_start_async (TrackerEnumerator    *enumerator,
                             GFile                *dir,
                             const gchar          *attributes,
                             GFileQueryInfoFlags   flags,
                             int                   io_priority,
                             GCancellable         *cancellable,
                             GAsyncReadyCallback   callback,
                             gpointer              user_data)
{
	GTask *task;

	task = g_task_new (enumerator, cancellable, callback, user_data);
	g_task_set_task_data (task, start_data_new (dir, attributes, flags), (GDestroyNotify) start_data_free);
	g_task_set_priority (task, io_priority);

	g_task_run_in_thread (task, start_async_thread);
	g_object_unref (task);
}

static GSList *
file_enumerator_start_finish (TrackerEnumerator  *enumerator,
                              GAsyncResult       *result,
                              GError            **error)
{
	g_return_val_if_fail (g_task_is_valid (result, enumerator), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}


static void
tracker_file_enumerator_file_iface_init (TrackerEnumeratorIface *iface)
{
	iface->start = file_enumerator_start;
	iface->start_async = file_enumerator_start_async;
	iface->start_finish = file_enumerator_start_finish;
}

TrackerEnumerator *
tracker_file_enumerator_new (void)
{
	TrackerFileEnumerator *tfe;

	tfe = g_object_new (TRACKER_TYPE_FILE_ENUMERATOR, NULL);

	return TRACKER_ENUMERATOR (tfe);
}
