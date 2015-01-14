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
#include "tracker-file-data-provider.h"
#include "tracker-monitor.h"

static void tracker_file_data_provider_file_iface_init (TrackerDataProviderIface *iface);

struct _TrackerFileDataProvider {
	GObject parent_instance;
	TrackerIndexingTree *indexing_tree;
	TrackerMonitor *monitor;
};

typedef struct {
	GFile *url;
	gchar *attributes;
	TrackerDirectoryFlags flags;
} BeginData;

/**
 * SECTION:tracker-file-data-provider
 * @short_description: File based data provider for file:// descendant URIs
 * @include: libtracker-miner/miner.h
 *
 * #TrackerFileDataProvider is a local file implementation of the
 * #TrackerDataProvider interface, charged with handling all file:// type URIs.
 *
 * Underneath it all, this implementation makes use of the
 * #GFileEnumerator APIs.
 *
 * Since: 1.2
 **/

G_DEFINE_TYPE_WITH_CODE (TrackerFileDataProvider, tracker_file_data_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_DATA_PROVIDER,
                                                tracker_file_data_provider_file_iface_init))

static void
tracker_file_data_provider_finalize (GObject *object)
{
	TrackerFileDataProvider *fdp = TRACKER_FILE_DATA_PROVIDER (object);

	if (fdp->indexing_tree) {
		g_object_unref (fdp->indexing_tree);
	}

	if (fdp->monitor) {
		g_object_unref (fdp->monitor);
	}

	G_OBJECT_CLASS (tracker_file_data_provider_parent_class)->finalize (object);
}

static void
tracker_file_data_provider_class_init (TrackerFileDataProviderClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = tracker_file_data_provider_finalize;
}

static void
item_created_cb (TrackerMonitor *monitor,
                 GFile          *file,
                 gboolean        is_directory,
                 gpointer        user_data)
{
	TrackerDataProvider *dp = TRACKER_DATA_PROVIDER (user_data);
	g_signal_emit_by_name (dp, "item-created", file, is_directory);
}

static void
item_updated_cb (TrackerMonitor *monitor,
                 GFile          *file,
                 gboolean        is_directory,
                 gpointer        user_data)
{
	TrackerDataProvider *dp = TRACKER_DATA_PROVIDER (user_data);
	g_signal_emit_by_name (dp, "item-updated", file, is_directory);
}

static void
item_attribute_updated_cb (TrackerMonitor *monitor,
                           GFile          *file,
                           gboolean        is_directory,
                           gpointer        user_data)
{
	TrackerDataProvider *dp = TRACKER_DATA_PROVIDER (user_data);
	g_signal_emit_by_name (dp, "item-attribute-updated", file, is_directory);
}

static void
item_deleted_cb (TrackerMonitor *monitor,
                 GFile          *file,
                 gboolean        is_directory,
                 gpointer        user_data)
{
	TrackerDataProvider *dp = TRACKER_DATA_PROVIDER (user_data);
	g_signal_emit_by_name (dp, "item-deleted", file, is_directory);
}

static void
item_moved_cb (TrackerMonitor *monitor,
               GFile          *file,
               GFile          *other_file,
               gboolean        is_directory,
               gboolean        is_source_monitored,
               gpointer        user_data)
{
	TrackerDataProvider *dp = TRACKER_DATA_PROVIDER (user_data);
	g_signal_emit_by_name (dp, "item-moved", file, other_file, is_directory, is_source_monitored);
}

static void
tracker_file_data_provider_init (TrackerFileDataProvider *fe)
{
	fe->monitor = tracker_monitor_new ();
	g_signal_connect_object (fe->monitor, "item-created", G_CALLBACK (item_created_cb), fe, G_CONNECT_AFTER);
	g_signal_connect_object (fe->monitor, "item-updated", G_CALLBACK (item_updated_cb), fe, G_CONNECT_AFTER);
	g_signal_connect_object (fe->monitor, "item-deleted", G_CALLBACK (item_deleted_cb), fe, G_CONNECT_AFTER);
	g_signal_connect_object (fe->monitor, "item-attribute-updated", G_CALLBACK (item_attribute_updated_cb), fe, G_CONNECT_AFTER);
	g_signal_connect_object (fe->monitor, "item-moved", G_CALLBACK (item_moved_cb), fe, G_CONNECT_AFTER);
}

static BeginData *
begin_data_new (GFile                 *url,
                const gchar           *attributes,
                TrackerDirectoryFlags  flags)
{
	BeginData *data;

	data = g_slice_new0 (BeginData);
	data->url = g_object_ref (url);
	/* FIXME: inefficient */
	data->attributes = g_strdup (attributes);
	data->flags = flags;

	return data;
}

static void
begin_data_free (BeginData *data)
{
	if (!data) {
		return;
	}

	g_object_unref (data->url);
	g_free (data->attributes);
	g_slice_free (BeginData, data);
}

static TrackerEnumerator *
file_data_provider_begin (TrackerDataProvider    *data_provider,
                          GFile                  *url,
                          const gchar            *attributes,
                          TrackerDirectoryFlags   flags,
                          GCancellable           *cancellable,
                          GError                **error)
{
	TrackerEnumerator *enumerator;
	GFileQueryInfoFlags file_flags;
	GFileEnumerator *fe;
	GError *local_error = NULL;

	if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
		return NULL;
	}

	/* We ignore the TRACKER_DIRECTORY_FLAG_NO_STAT here, it makes
	 * no sense to be at this point with that flag. So we warn
	 * about it...
	 */
	if ((flags & TRACKER_DIRECTORY_FLAG_NO_STAT) != 0) {
		g_warning ("Did not expect to have TRACKER_DIRECTORY_FLAG_NO_STAT "
		           "flag in %s(), continuing anyway...",
		           __FUNCTION__);
	}

	file_flags = G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS;

	fe = g_file_enumerate_children (url,
	                                attributes,
	                                file_flags,
	                                cancellable,
	                                &local_error);

	if (local_error) {
		gchar *uri;

		uri = g_file_get_uri (url);

		g_warning ("Could not open directory '%s': %s",
		           uri, local_error->message);

		g_propagate_error (error, local_error);
		g_free (uri);

		return NULL;
	}

	enumerator = tracker_file_enumerator_new (fe);
	g_object_unref (fe);

	return TRACKER_ENUMERATOR (enumerator);
}

static void
file_data_provider_begin_thread (GTask        *task,
                                 gpointer      source_object,
                                 gpointer      task_data,
                                 GCancellable *cancellable)
{
	TrackerDataProvider *data_provider = source_object;
	TrackerEnumerator *enumerator = NULL;
	BeginData *data = task_data;
	GError *error = NULL;

	if (g_cancellable_set_error_if_cancelled (cancellable, &error)) {
		enumerator = NULL;
	} else {
		enumerator = file_data_provider_begin (data_provider,
		                                       data->url,
		                                       data->attributes,
		                                       data->flags,
		                                       cancellable,
		                                       &error);
	}

	if (error) {
		g_task_return_error (task, error);
	} else {
		g_task_return_pointer (task, enumerator, (GDestroyNotify) g_object_unref);
	}
}

static void
file_data_provider_begin_async (TrackerDataProvider   *data_provider,
                                GFile                 *dir,
                                const gchar           *attributes,
                                TrackerDirectoryFlags  flags,
                                int                    io_priority,
                                GCancellable          *cancellable,
                                GAsyncReadyCallback    callback,
                                gpointer               user_data)
{
	GTask *task;

	task = g_task_new (data_provider, cancellable, callback, user_data);
	g_task_set_task_data (task, begin_data_new (dir, attributes, flags), (GDestroyNotify) begin_data_free);
	g_task_set_priority (task, io_priority);
	g_task_run_in_thread (task, file_data_provider_begin_thread);
	g_object_unref (task);
}

static TrackerEnumerator *
file_data_provider_begin_finish (TrackerDataProvider  *data_provider,
                                 GAsyncResult         *result,
                                 GError              **error)
{
	g_return_val_if_fail (g_task_is_valid (result, data_provider), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

static gboolean
file_data_provider_end (TrackerDataProvider  *data_provider,
                        TrackerEnumerator    *enumerator,
                        GCancellable         *cancellable,
                        GError              **error)
{
	if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
		return FALSE;
	}

	return TRUE;
}

static void
file_data_provider_end_thread (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
	TrackerDataProvider *data_provider = source_object;
	TrackerEnumerator *enumerator = task_data;
	GError *error = NULL;
	gboolean success = FALSE;

	if (!g_cancellable_set_error_if_cancelled (cancellable, &error)) {
		success = file_data_provider_end (data_provider,
		                                  enumerator,
		                                  cancellable,
		                                  &error);
	}

	if (error) {
		g_task_return_error (task, error);
	} else {
		g_task_return_boolean (task, success);
	}
}

static void
file_data_provider_end_async (TrackerDataProvider  *data_provider,
                              TrackerEnumerator    *enumerator,
                              int                   io_priority,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data)
{
	GTask *task;

	task = g_task_new (data_provider, cancellable, callback, user_data);
	g_task_set_task_data (task, g_object_ref (enumerator), (GDestroyNotify) g_object_unref);
	g_task_set_priority (task, io_priority);
	g_task_run_in_thread (task, file_data_provider_end_thread);
	g_object_unref (task);
}

static gboolean
file_data_provider_end_finish (TrackerDataProvider  *data_provider,
                               GAsyncResult         *result,
                               GError              **error)
{
	g_return_val_if_fail (g_task_is_valid (result, data_provider), NULL);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
file_data_provider_monitor_add (TrackerDataProvider  *data_provider,
                                GFile                *container,
                                GError              **error)
{
	TrackerFileDataProvider *fdp;

	g_return_val_if_fail (TRACKER_IS_FILE_DATA_PROVIDER (data_provider), FALSE);

	fdp = TRACKER_FILE_DATA_PROVIDER (data_provider);

	return tracker_monitor_add (fdp->monitor, container);
}

static gboolean
file_data_provider_monitor_remove (TrackerDataProvider  *data_provider,
                                   GFile                *container,
                                   gboolean              recursively,
                                   GError              **error)
{
	TrackerFileDataProvider *fdp;

	g_return_val_if_fail (TRACKER_IS_FILE_DATA_PROVIDER (data_provider), FALSE);

	fdp = TRACKER_FILE_DATA_PROVIDER (data_provider);

	if (G_LIKELY (recursively)) {
		return tracker_monitor_remove_recursively (fdp->monitor, container);
	} else {
		return tracker_monitor_remove (fdp->monitor, container);
	}
}

static gboolean
file_data_provider_monitor_move (TrackerDataProvider  *data_provider,
                                 GFile                *container_from,
                                 GFile                *container_to,
                                 GError              **error)
{
	TrackerFileDataProvider *fdp;

	g_return_val_if_fail (TRACKER_IS_FILE_DATA_PROVIDER (data_provider), FALSE);
	g_return_val_if_fail (G_IS_FILE (container_from), FALSE);
	g_return_val_if_fail (G_IS_FILE (container_to), FALSE);

	fdp = TRACKER_FILE_DATA_PROVIDER (data_provider);

	return tracker_monitor_move (fdp->monitor, container_from, container_to);
}

static gboolean
file_data_provider_is_monitored (TrackerDataProvider  *data_provider,
                                 GFile                *container,
                                 GError              **error)
{
	TrackerFileDataProvider *fdp;

	g_return_val_if_fail (TRACKER_IS_FILE_DATA_PROVIDER (data_provider), FALSE);
	g_return_val_if_fail (G_IS_FILE (container), FALSE);

	fdp = TRACKER_FILE_DATA_PROVIDER (data_provider);

	return tracker_monitor_is_watched (fdp->monitor, container);
}

static gboolean
file_data_provider_is_monitored_by_path (TrackerDataProvider  *data_provider,
                                         const gchar          *container,
                                         GError              **error)
{
	TrackerFileDataProvider *fdp;

	g_return_val_if_fail (TRACKER_IS_FILE_DATA_PROVIDER (data_provider), FALSE);
	g_return_val_if_fail (container != NULL, FALSE);

	fdp = TRACKER_FILE_DATA_PROVIDER (data_provider);

	return tracker_monitor_is_watched_by_string (fdp->monitor, container);
}

static guint
file_data_provider_monitor_count (TrackerDataProvider  *data_provider,
                                  GError              **error)
{
	TrackerFileDataProvider *fdp;

	g_return_val_if_fail (TRACKER_IS_FILE_DATA_PROVIDER (data_provider), 0);

	fdp = TRACKER_FILE_DATA_PROVIDER (data_provider);

	return tracker_monitor_get_count (fdp->monitor);
}

static gboolean
file_data_provider_set_indexing_tree (TrackerDataProvider  *data_provider,
                                      TrackerIndexingTree  *indexing_tree,
                                      GError              **error)
{
	TrackerFileDataProvider *fdp;

	g_return_val_if_fail (TRACKER_IS_FILE_DATA_PROVIDER (data_provider), NULL);

	fdp = TRACKER_FILE_DATA_PROVIDER (data_provider);

	if (indexing_tree) {
		g_object_ref (indexing_tree);
	}

	if (fdp->indexing_tree) {
		g_object_unref (fdp->indexing_tree);
	}

	fdp->indexing_tree = indexing_tree;

	return TRUE;
}

static TrackerIndexingTree *
file_data_provider_get_indexing_tree (TrackerDataProvider  *data_provider,
                                      GError              **error)
{
	TrackerFileDataProvider *fdp;

	g_return_val_if_fail (TRACKER_IS_FILE_DATA_PROVIDER (data_provider), NULL);

	fdp = TRACKER_FILE_DATA_PROVIDER (data_provider);

	return fdp->indexing_tree;
}

static void
tracker_file_data_provider_file_iface_init (TrackerDataProviderIface *iface)
{
	iface->begin = file_data_provider_begin;
	iface->begin_async = file_data_provider_begin_async;
	iface->begin_finish = file_data_provider_begin_finish;
	iface->end = file_data_provider_end;
	iface->end_async = file_data_provider_end_async;
	iface->end_finish = file_data_provider_end_finish;
	iface->monitor_add = file_data_provider_monitor_add;
	iface->monitor_remove = file_data_provider_monitor_remove;
	iface->monitor_move = file_data_provider_monitor_move;
	iface->is_monitored = file_data_provider_is_monitored;
	iface->is_monitored_by_path = file_data_provider_is_monitored_by_path;
	iface->monitor_count = file_data_provider_monitor_count;
	iface->set_indexing_tree = file_data_provider_set_indexing_tree;
	iface->get_indexing_tree = file_data_provider_get_indexing_tree;
}

/**
 * tracker_file_data_provider_new:
 *
 * Creates a new TrackerDataProvider which can be used to create new
 * #TrackerMinerFS classes. See #TrackerMinerFS for an example of how
 * to use your #TrackerDataProvider.
 *
 * Returns: (transfer full): a #TrackerDataProvider which must be
 * unreferenced with g_object_unref().
 *
 * Since: 1.2:
 **/
TrackerDataProvider *
tracker_file_data_provider_new (void)
{
	TrackerFileDataProvider *tfdp;

	tfdp = g_object_new (TRACKER_TYPE_FILE_DATA_PROVIDER, NULL);

	return TRACKER_DATA_PROVIDER (tfdp);
}
