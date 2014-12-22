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

#ifndef __LIBTRACKER_MINER_DATA_PROVIDER_H__
#define __LIBTRACKER_MINER_DATA_PROVIDER_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <gio/gio.h>

#include "tracker-enumerator.h"
#include "tracker-miner-enums.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_DATA_PROVIDER           (tracker_data_provider_get_type ())
#define TRACKER_DATA_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_DATA_PROVIDER, TrackerDataProvider))
#define TRACKER_IS_DATA_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_DATA_PROVIDER))
#define TRACKER_DATA_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), TRACKER_TYPE_DATA_PROVIDER, TrackerDataProviderIface))

/**
 * TrackerDataProvider:
 *
 * An interface to enumerate URIs and feed the data to Tracker.
 **/
typedef struct _TrackerDataProvider TrackerDataProvider;
typedef struct _TrackerDataProviderIface TrackerDataProviderIface;

/**
 * TrackerDataProviderIface:
 * @g_iface: Parent interface type.
 * @begin: Called when the data_provider is synchronously
 * opening and starting the iteration of a given location.
 * @begin_async: Called when the data_provider is synchronously
 * opening and starting the iteration of a given location. Completed
 * using @begin_finish.
 * @begin_finish: Called when the data_provider is completing the
 * asynchronous operation provided by @begin_async.
 * @end: Called when the data_provider is synchronously
 * closing and cleaning up the iteration of a given location.
 * @end_async: Called when the data_provider is asynchronously
 * closing and cleaning up the iteration of a given location.
 * Completed using @end_finish.
 * @end_finish: Called when the data_provider is completing the
 * asynchronous operation provided by @end_async.
 * @add_monitor: Called when the data_provider is asked to monitor a
 * container for changes.
 * @remove_monitor: Called when the data_provider is asked to stop
 * monitoring a container for changes.
 * @item_created: Signalled when an item is created in a monitored
 * container. This can be another container or object itself. A
 * container could be a directory and an object could be a file in
 * a directory.
 * @item_updated: Signalled when an item is updated, this includes
 * both containers and objects. For example, the contents of an item
 * have changed.
 * @item_attribute_updated: Signalled when metadata changes are
 * made to a container or object. Example of this would be include
 * chmod, timestamps (utimensat), extended attributes (setxattr), link
 * counts and user/group ID (chown) updates.
 * @item_deleted: Signalled when a container or object is deleted.
 * @item_moved: Signalled when a container or object is moved. The
 * parameters provided give indication about what is known of source
 * and target items.
 *
 * Virtual methods to be implemented.
 *
 * The @item_created, @item_updated, @item_attribute_updated,
 * @item_deleted and @item_moved signals <emphasis>MUST NOT</emphasis>
 * be emitted unless the #TrackerDirectoryFlags used with the @begin
 * and @begin_async APIs include #TRACKER_DIRECTORY_FLAG_MONITOR.
 **/
struct _TrackerDataProviderIface {
	GTypeInterface g_iface;

	/* Virtual Table */

	/* Crawling API - for container/object traversal */
	TrackerEnumerator *   (* begin)              (TrackerDataProvider    *data_provider,
	                                              GFile                  *url,
	                                              const gchar            *attributes,
	                                              TrackerDirectoryFlags   flags,
	                                              GCancellable           *cancellable,
	                                              GError                **error);
	void                  (* begin_async)        (TrackerDataProvider    *data_provider,
	                                              GFile                  *url,
	                                              const gchar            *attributes,
	                                              TrackerDirectoryFlags   flags,
	                                              gint                    io_priority,
	                                              GCancellable           *cancellable,
	                                              GAsyncReadyCallback     callback,
	                                              gpointer                user_data);
	TrackerEnumerator *   (* begin_finish)       (TrackerDataProvider    *data_provider,
	                                              GAsyncResult           *result,
	                                              GError                **error);

	gboolean              (* end)                (TrackerDataProvider    *data_provider,
	                                              TrackerEnumerator      *enumerator,
	                                              GCancellable           *cancellable,
	                                              GError                **error);
	void                  (* end_async)          (TrackerDataProvider    *data_provider,
	                                              TrackerEnumerator      *enumerator,
	                                              gint                    io_priority,
	                                              GCancellable           *cancellable,
	                                              GAsyncReadyCallback     callback,
	                                              gpointer                user_data);
	gboolean              (* end_finish)         (TrackerDataProvider    *data_provider,
	                                              GAsyncResult           *result,
	                                              GError                **error);

	/* Monitoring API - to tell data provider you're interested */
	gboolean              (* monitor_add)        (TrackerDataProvider    *data_provider,
	                                              GFile                  *container,
	                                              GError                **error);
	gboolean              (* monitor_remove)     (TrackerDataProvider    *data_provider,
	                                              GFile                  *container,
	                                              gboolean                recursively,
	                                              GError                **error);

	/* Monitoring Signals - for container/object change notification */
	void                  (* item_created)       (TrackerDataProvider    *data_provider,
	                                              GFile                  *file,
	                                              gboolean                is_container);
	void                  (* item_updated)       (TrackerDataProvider    *data_provider,
	                                              GFile                  *file,
	                                              gboolean                is_container);
	void                  (* item_attribute_updated)
                                                     (TrackerDataProvider    *data_provider,
	                                              GFile                  *file,
	                                              gboolean                is_container);
	void                  (* item_deleted)       (TrackerDataProvider    *data_provider,
	                                              GFile                  *file,
	                                              gboolean                is_container);
	void                  (* item_moved)         (TrackerDataProvider    *data_provider,
	                                              GFile                  *file,
	                                              GFile                  *other_file,
	                                              gboolean                is_container);

	/*< private >*/
	/* Padding for future expansion */
	void (*_tracker_reserved1) (void);
};

GType              tracker_data_provider_get_type        (void) G_GNUC_CONST;
TrackerEnumerator *tracker_data_provider_begin           (TrackerDataProvider   *data_provider,
                                                          GFile                 *url,
                                                          const gchar           *attributes,
                                                          TrackerDirectoryFlags  flags,
                                                          GCancellable          *cancellable,
                                                          GError               **error);
void               tracker_data_provider_begin_async     (TrackerDataProvider   *data_provider,
                                                          GFile                 *url,
                                                          const gchar           *attributes,
                                                          TrackerDirectoryFlags  flags,
                                                          gint                   io_priority,
                                                          GCancellable          *cancellable,
                                                          GAsyncReadyCallback    callback,
                                                          gpointer               user_data);
TrackerEnumerator *tracker_data_provider_begin_finish    (TrackerDataProvider   *data_provider,
                                                          GAsyncResult          *result,
                                                          GError               **error);
gboolean           tracker_data_provider_end             (TrackerDataProvider   *data_provider,
                                                          TrackerEnumerator     *enumerator,
                                                          GCancellable          *cancellable,
                                                          GError               **error);
void               tracker_data_provider_end_async       (TrackerDataProvider   *data_provider,
                                                          TrackerEnumerator     *enumerator,
                                                          gint                   io_priority,
                                                          GCancellable          *cancellable,
                                                          GAsyncReadyCallback    callback,
                                                          gpointer               user_data);
gboolean           tracker_data_provider_end_finish      (TrackerDataProvider   *data_provider,
                                                          GAsyncResult          *result,
                                                          GError               **error);

gboolean           tracker_data_provider_monitor_add     (TrackerDataProvider  *data_provider,
                                                          GFile                *container,
                                                          GError              **error);
gboolean           tracker_data_provider_monitor_remove  (TrackerDataProvider  *data_provider,
                                                          GFile                *container,
                                                          gboolean              recursively,
                                                          GError              **error);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_DATA_PROVIDER_H__ */
