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
	GFileEnumerator *file_enumerator;
};

/**
 * SECTION:tracker-file-enumerator
 * @short_description: File based enumerator for file:// descendant URIs
 * @include: libtracker-miner/miner.h
 *
 * #TrackerFileEnumerator is a local file implementation of the
 * #TrackerEnumerator interface, charged with handling all file:// type URIs.
 *
 * Underneath it all, this implementation makes use of the
 * #GFileEnumerator APIs.
 *
 * Since: 1.2
 **/

G_DEFINE_TYPE_WITH_CODE (TrackerFileEnumerator, tracker_file_enumerator, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_ENUMERATOR,
                                                tracker_file_enumerator_file_iface_init))

static void
tracker_file_enumerator_finalize (GObject *object)
{
	TrackerFileEnumerator *tfe = TRACKER_FILE_ENUMERATOR (object);

	if (tfe->file_enumerator) {
		g_object_unref (tfe->file_enumerator);
	}

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

static gpointer
file_enumerator_next (TrackerEnumerator  *enumerator,
                      GCancellable       *cancellable,
                      GError            **error)
{
	TrackerFileEnumerator *tfe;
	GFileInfo *info = NULL;
	GError *local_error = NULL;

	if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
		return NULL;
	}

	tfe = TRACKER_FILE_ENUMERATOR (enumerator);
	info = g_file_enumerator_next_file (tfe->file_enumerator, cancellable, &local_error);

	/* FIXME: Do we need a ->is_running check here like before? */
	if (local_error || !info) {
		if (local_error) {
			g_critical ("Could not crawl through directory: %s", local_error->message);
			g_propagate_error (error, local_error);
		}

		/* No more files or we are stopping anyway, so clean
		 * up and close all file enumerators.
		 */
		if (info) {
			g_object_unref (info);
		}

		return NULL;
	}

	/* FIXME: We need some check here to call
	 * enumerator_data_process which signals
	 * CHECK_DIRECTORY_CONTENTS
	 */
	g_debug ("--> Found:'%s' (%s)",
	         g_file_info_get_name (info),
	         g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY ? "Dir" : "File");

	return info;
}

static void
file_enumerator_next_thread (GTask        *task,
                             gpointer      source_object,
                             gpointer      task_data,
                             GCancellable *cancellable)
{
	TrackerEnumerator *enumerator = source_object;
	GFileInfo *info;
	GError *error = NULL;

	info = file_enumerator_next (enumerator, cancellable, &error);

	if (error) {
		g_task_return_error (task, error);
	} else {
		g_task_return_pointer (task, info, (GDestroyNotify) g_object_unref);
	}
}

static void
file_enumerator_next_async (TrackerEnumerator    *enumerator,
                            gint                  io_priority,
                            GCancellable         *cancellable,
                            GAsyncReadyCallback   callback,
                            gpointer              user_data)
{
	GTask *task;

	task = g_task_new (enumerator, cancellable, callback, user_data);
	g_task_set_priority (task, io_priority);
	g_task_run_in_thread (task, file_enumerator_next_thread);
	g_object_unref (task);
}

static gpointer
file_enumerator_next_finish (TrackerEnumerator  *enumerator,
                             GAsyncResult       *result,
                             GError            **error)
{
	g_return_val_if_fail (g_task_is_valid (result, enumerator), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

static void
tracker_file_enumerator_file_iface_init (TrackerEnumeratorIface *iface)
{
	iface->next = file_enumerator_next;
	iface->next_async = file_enumerator_next_async;
	iface->next_finish = file_enumerator_next_finish;
}

/**
 * tracker_file_enumerator_new:
 * @file_enumerator: the #GFileEnumerator used to enumerate with
 *
 * Creates a new TrackerEnumerator which can be used to create new
 * #TrackerMinerFS classes. See #TrackerMinerFS for an example of how
 * to use your #TrackerEnumerator.
 *
 * Returns: (transfer full): a #TrackerEnumerator which must be
 * unreferenced with g_object_unref().
 *
 * Since: 1.2
 **/
TrackerEnumerator *
tracker_file_enumerator_new (GFileEnumerator *file_enumerator)
{
	TrackerFileEnumerator *tfe;

	g_return_val_if_fail (G_IS_FILE_ENUMERATOR (file_enumerator), NULL);

	tfe = g_object_new (TRACKER_TYPE_FILE_ENUMERATOR, NULL);
	if (!tfe) {
		return NULL;
	}

	tfe->file_enumerator = g_object_ref (file_enumerator);

	return TRACKER_ENUMERATOR (tfe);
}
