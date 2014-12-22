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

#include <glib/gi18n.h>

#include "tracker-enumerator.h"

/**
 * SECTION:tracker-enumerator
 * @short_description: Enumerate resources
 * @include: libtracker-miner/miner.h
 *
 * #TrackerEnumerator allows you to iterate content or resources.
 *
 * The common example use case for #TrackerEnumerator would be to
 * operate on a set of #GFiles, returning a #GFileInfo structure for
 * each file enumerated (e.g. tracker_enumerator_next() would return a
 * #GFileInfo for each resource found in a parent folder or container).
 *
 * The ordering of returned content is unspecified and dependent on
 * implementation.
 *
 * If your application needs a specific ordering, such as by name or
 * modification time, you will have to implement that in your
 * application code.
 *
 * There is a #TrackerFileEnumerator which is an implementation
 * of the #TrackerEnumerator interface. This makes use of the
 * #GFileEnumerator API as an example of how to implement your own
 * enumerator class. Typically, an implementation class (like
 * #TrackerFileEnumerator) would be used with TrackerCrawler (an
 * internal class) which feeds URI data for Tracker to insert into the
 * database.
 *
 * What is important to note about implementations is what is expected
 * to be returned by an enumerator. There are some simple rules:
 *
 * 1. Tracker expects resources on a per container (or directory)
 * basis only, i.e. not recursively given for a top level URI. This
 * allows Tracker to properly construct the database and carry out
 * white/black listing correctly, amongst other things.
 * 2. Tracker does not expect the top level URL to be reported in the
 * children returned. This is considered an error.
 *
 * See the #TrackerDataProvider documentation for more details.
 *
 * Since: 1.2
 **/

typedef TrackerEnumeratorIface TrackerEnumeratorInterface;
G_DEFINE_INTERFACE (TrackerEnumerator, tracker_enumerator, G_TYPE_OBJECT)

static void
tracker_enumerator_default_init (TrackerEnumeratorInterface *iface)
{
}

/**
 * tracker_enumerator_next:
 * @enumerator: a #TrackerEnumerator
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Enumerates to the next piece of data according to the @enumerator
 * implementation.
 *
 * Returns: (transfer full): Returns a #gpointer with the next item
 * from the @enumerator, or %NULL when @error is set or the operation
 * was cancelled in @cancellable. The data must be freed. The function
 * to free depends on the data returned by the enumerator and the
 * #TrackerDataProvider that created the @enumerator.
 *
 * Since: 1.2
 **/
gpointer
tracker_enumerator_next (TrackerEnumerator  *enumerator,
                         GCancellable       *cancellable,
                         GError            **error)
{
	TrackerEnumeratorIface *iface;

	g_return_val_if_fail (TRACKER_IS_ENUMERATOR (enumerator), NULL);

	if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
		return NULL;
	}

	iface = TRACKER_ENUMERATOR_GET_IFACE (enumerator);

	if (iface->next == NULL) {
		g_set_error_literal (error,
		                     G_IO_ERROR,
		                     G_IO_ERROR_NOT_SUPPORTED,
		                     _("Operation not supported"));
		return NULL;
	}

	return (* iface->next) (enumerator, cancellable, error);
}

/**
 * tracker_enumerator_next_async:
 * @enumerator: a #TrackerEnumerator.
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to
 * ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the
 * request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Precisely the same operation as tracker_enumerator_next()
 * is performing, but asynchronously.
 *
 * When all i/o for the operation is finished the @callback will be
 * called with the requested information.
 *
 * In case of a partial error the callback will be called with any
 * succeeding items and no error, and on the next request the error
 * will be reported. If a request is cancelled the callback will be
 * called with #G_IO_ERROR_CANCELLED.
 *
 * During an async request no other sync and async calls are allowed,
 * and will result in #G_IO_ERROR_PENDING errors.
 *
 * Any outstanding i/o request with higher priority (lower numerical
 * value) will be executed before an outstanding request with lower
 * priority. Default priority is %G_PRIORITY_DEFAULT.
 *
 * Since: 1.2
 **/
void
tracker_enumerator_next_async (TrackerEnumerator    *enumerator,
                               gint                  io_priority,
                               GCancellable         *cancellable,
                               GAsyncReadyCallback   callback,
                               gpointer              user_data)
{
	TrackerEnumeratorIface *iface;

	g_return_if_fail (TRACKER_IS_ENUMERATOR (enumerator));

	iface = TRACKER_ENUMERATOR_GET_IFACE (enumerator);

	if (iface->next_async == NULL) {
		g_critical (_("Operation not supported"));
		return;
	}

	(* iface->next_async) (enumerator, io_priority, cancellable, callback, user_data);
}

/**
 * tracker_enumerator_next_finish:
 * @enumerator: a #TrackerEnumerator.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL
 * to ignore.
 *
 * Finishes the asynchronous operation started with
 * tracker_enumerator_next_async().
 *
 * Returns: (transfer full): Returns a #gpointer with the next item
 * from the @enumerator, or %NULL when @error is set or the operation
 * was cancelled in @cancellable. The data must be freed. The function
 * to free depends on the data returned by the enumerator and the
 * #TrackerDataProvider that created the @enumerator.
 *
 * Since: 1.2
 **/
gpointer
tracker_enumerator_next_finish (TrackerEnumerator  *enumerator,
                                GAsyncResult       *result,
                                GError            **error)
{
	TrackerEnumeratorIface *iface;

	g_return_val_if_fail (TRACKER_IS_ENUMERATOR (enumerator), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

	iface = TRACKER_ENUMERATOR_GET_IFACE (enumerator);

	if (g_async_result_legacy_propagate_error (result, error)) {
		return NULL;
	}

	return (* iface->next_finish) (enumerator, result, error);
}
