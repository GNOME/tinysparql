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

#ifndef __LIBTRACKER_MINER_ENUMERATOR_H__
#define __LIBTRACKER_MINER_ENUMERATOR_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <gio/gio.h>

#include "tracker-miner-enums.h"

G_BEGIN_DECLS

#define TRACKER_TYPE_ENUMERATOR           (tracker_enumerator_get_type ())
#define TRACKER_ENUMERATOR(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_ENUMERATOR, TrackerEnumerator))
#define TRACKER_IS_ENUMERATOR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_ENUMERATOR))
#define TRACKER_ENUMERATOR_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), TRACKER_TYPE_ENUMERATOR, TrackerEnumeratorIface))

/**
 * TrackerEnumerator:
 *
 * An interface to enumerate URIs and feed the data to Tracker.
 **/
typedef struct _TrackerEnumerator TrackerEnumerator;
typedef struct _TrackerEnumeratorIface TrackerEnumeratorIface;

/**
 * TrackerEnumeratorIface:
 * @g_iface: Parent interface type.
 * @next: Called when the enumerator is synchronously
 * retrieving the next resource to be iterated.
 * @next_async: Called when the enumerator is asynchronously
 * retrieving the next resource to be iterated. Completed using
 * @next_finish.
 * @next_finish: Called when the enumerator is completing the
 * asynchronous operation provided by @next_async.
 *
 * Virtual methods left to implement.
 **/
struct _TrackerEnumeratorIface {
	GTypeInterface g_iface;

	/* Virtual Table */
	gpointer          (* next)        (TrackerEnumerator    *enumerator,
	                                   GCancellable         *cancellable,
	                                   GError              **error);
	void              (* next_async)  (TrackerEnumerator    *enumerator,
	                                   gint                  io_priority,
	                                   GCancellable         *cancellable,
	                                   GAsyncReadyCallback   callback,
	                                   gpointer              user_data);
	gpointer          (* next_finish) (TrackerEnumerator    *enumerator,
	                                   GAsyncResult         *result,
	                                   GError              **error);

	/*< private >*/
	/* Padding for future expansion */
	void (*_tracker_reserved1) (void);
	void (*_tracker_reserved2) (void);
	void (*_tracker_reserved3) (void);
	void (*_tracker_reserved4) (void);
	void (*_tracker_reserved5) (void);
	void (*_tracker_reserved6) (void);
	void (*_tracker_reserved7) (void);
	void (*_tracker_reserved8) (void);
};

GType             tracker_enumerator_get_type        (void) G_GNUC_CONST;
gpointer          tracker_enumerator_next            (TrackerEnumerator    *enumerator,
                                                      GCancellable         *cancellable,
                                                      GError              **error);
void              tracker_enumerator_next_async      (TrackerEnumerator    *enumerator,
                                                      gint                  io_priority,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
gpointer          tracker_enumerator_next_finish     (TrackerEnumerator    *enumerator,
                                                      GAsyncResult         *result,
                                                      GError              **error);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_ENUMERATOR_H__ */
