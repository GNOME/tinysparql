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

#include "tracker-file-data-provider.h"

static void tracker_file_data_provider_file_iface_init (TrackerDataProviderIface *iface);

struct _TrackerFileDataProvider {
	GObject parent_instance;
};

/**
 * SECTION:tracker-file-data-provider
 * @short_description: File based data provider for file:// descendant URIs
 * @include: libtracker-miner/miner.h
 *
 * #TrackerFileDataProvider is a local file implementation of the
 * #TrackerDataProvider interface, charged with handling all file:// type URIs.
 *
 * Underneath it all, this implementation makes use of GIO-based
 * #GFileEnumerator<!-- -->s.
 *
 * Since: 1.2
 **/

G_DEFINE_TYPE_WITH_CODE (TrackerFileDataProvider, tracker_file_data_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_DATA_PROVIDER,
                                                tracker_file_data_provider_file_iface_init))

static void
tracker_file_data_provider_finalize (GObject *object)
{
	G_OBJECT_CLASS (tracker_file_data_provider_parent_class)->finalize (object);
}

static void
tracker_file_data_provider_class_init (TrackerFileDataProviderClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = tracker_file_data_provider_finalize;
}

static void
tracker_file_data_provider_init (TrackerFileDataProvider *fe)
{
}

static GFileEnumerator *
file_data_provider_begin (TrackerDataProvider    *data_provider,
                          GFile                  *url,
                          const gchar            *attributes,
                          TrackerDirectoryFlags   flags,
                          GCancellable           *cancellable,
                          GError                **error)
{
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

	return fe;
}

static void
enumerate_children_cb (GObject       *source_object,
                       GAsyncResult  *res,
                       gpointer       user_data)
{
	GFile *url = G_FILE (source_object);
	GFileEnumerator *enumerator = NULL;
	GTask *task = G_TASK (user_data);
	GError *error = NULL;

	enumerator = g_file_enumerate_children_finish (url, res, &error);
	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			gchar *uri;

			uri = g_file_get_uri (url);
			g_warning ("Could not open directory '%s': %s",
			           uri, error->message);
			g_free (uri);
		}

		g_task_return_error (task, error);
	} else {
		g_task_return_pointer (task, enumerator, (GDestroyNotify) g_object_unref);
	}

	g_object_unref (task);
}

static void
file_data_provider_begin_async (TrackerDataProvider   *data_provider,
                                GFile                 *url,
                                const gchar           *attributes,
                                TrackerDirectoryFlags  flags,
                                int                    io_priority,
                                GCancellable          *cancellable,
                                GAsyncReadyCallback    callback,
                                gpointer               user_data)
{
	GFileQueryInfoFlags file_flags;
	GTask *task;

	task = g_task_new (data_provider, cancellable, callback, user_data);

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

	g_file_enumerate_children_async (url,
	                                 attributes,
	                                 file_flags,
	                                 io_priority,
	                                 cancellable,
	                                 enumerate_children_cb,
	                                 g_object_ref (task));

	g_object_unref (task);
}

static GFileEnumerator *
file_data_provider_begin_finish (TrackerDataProvider  *data_provider,
                                 GAsyncResult         *result,
                                 GError              **error)
{
	g_return_val_if_fail (g_task_is_valid (result, data_provider), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

static void
tracker_file_data_provider_file_iface_init (TrackerDataProviderIface *iface)
{
	iface->begin = file_data_provider_begin;
	iface->begin_async = file_data_provider_begin_async;
	iface->begin_finish = file_data_provider_begin_finish;
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
