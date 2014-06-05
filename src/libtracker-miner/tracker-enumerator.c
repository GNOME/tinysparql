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

typedef TrackerEnumeratorIface TrackerEnumeratorInterface;
G_DEFINE_INTERFACE (TrackerEnumerator, tracker_enumerator, G_TYPE_OBJECT)

static void
tracker_enumerator_default_init (TrackerEnumeratorInterface *iface)
{
}

GSList *
tracker_enumerator_start (TrackerEnumerator    *enumerator,
                          GFile                *dir,
                          const gchar          *attributes,
                          GFileQueryInfoFlags   flags,
                          GCancellable         *cancellable,
                          GError              **error)
{
	TrackerEnumeratorIface *iface;

	g_return_val_if_fail (TRACKER_IS_ENUMERATOR (enumerator), NULL);

	if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
		return NULL;
	}

	iface = TRACKER_ENUMERATOR_GET_IFACE (enumerator);

	if (iface->start == NULL) {
		g_set_error_literal (error,
		                     G_IO_ERROR,
		                     G_IO_ERROR_NOT_SUPPORTED,
		                     _("Operation not supported"));
		return NULL;
	}

	return (* iface->start) (enumerator, dir, attributes, flags, cancellable, error);
}

void
tracker_enumerator_start_async (TrackerEnumerator    *enumerator,
                                GFile                *dir,
                                const gchar          *attributes,
                                GFileQueryInfoFlags   flags,
                                int                   io_priority,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data)
{
	TrackerEnumeratorIface *iface;

	g_return_if_fail (TRACKER_IS_ENUMERATOR (enumerator));

	iface = TRACKER_ENUMERATOR_GET_IFACE (enumerator);

	if (iface->start_async == NULL) {
		g_critical (_("Operation not supported"));
		return;
	}

	(* iface->start_async) (enumerator, dir, attributes, flags, io_priority, cancellable, callback, user_data);
}

GSList *
tracker_enumerator_start_finish (TrackerEnumerator  *enumerator,
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

	return (* iface->start_finish) (enumerator, result, error);
}
