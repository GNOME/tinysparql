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

static void tracker_file_data_provider_file_iface_init (TrackerDataProviderIface *iface);

struct _TrackerFileDataProvider {
	GObject parent_instance;
	TrackerCrawlFlags crawl_flags;
};

typedef struct {
	GFile *url;
	gchar *attributes;
	GFileQueryInfoFlags flags;
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
	fe->crawl_flags = TRACKER_CRAWL_FLAG_NONE;
}

static TrackerCrawlFlags
file_data_provider_get_crawl_flags (TrackerDataProvider *data_provider)
{
	TrackerFileDataProvider *fe;

	fe = TRACKER_FILE_DATA_PROVIDER (data_provider);

	return fe->crawl_flags;
}

static void
file_data_provider_set_crawl_flags (TrackerDataProvider *data_provider,
                                 TrackerCrawlFlags  flags)
{
	TrackerFileDataProvider *fe;

	fe = TRACKER_FILE_DATA_PROVIDER (data_provider);

	fe->crawl_flags = flags;
}

static BeginData *
begin_data_new (GFile               *url,
                const gchar         *attributes,
                GFileQueryInfoFlags  flags)
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
file_data_provider_begin (TrackerDataProvider  *data_provider,
                          GFile                *url,
                          const gchar          *attributes,
                          GFileQueryInfoFlags   flags,
                          GCancellable         *cancellable,
                          GError              **error)
{
	TrackerEnumerator *enumerator;
	GFileEnumerator *fe;
	GError *local_error = NULL;

	if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
		return NULL;
	}

	fe = g_file_enumerate_children (url,
	                                attributes,
	                                flags,
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
file_data_provider_begin_async (TrackerDataProvider  *data_provider,
                                GFile                *dir,
                                const gchar          *attributes,
                                GFileQueryInfoFlags   flags,
                                int                   io_priority,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data)
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

static void
tracker_file_data_provider_file_iface_init (TrackerDataProviderIface *iface)
{
	iface->get_crawl_flags = file_data_provider_get_crawl_flags;
	iface->set_crawl_flags = file_data_provider_set_crawl_flags;
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
