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
 * @short_description: Enumerate URI locations
 * @include: libtracker-miner/miner.h
 *
 * #TrackerEnumerator allows you to operate on a set of #GFiles,
 * returning a #GFileInfo structure for each file enumerated (e.g.
 * tracker_enumerator_get_children() will return a #GSList with the
 * children within a parent directory or structure for the URI.
 *
 * This function is similar to g_file_enumerator_next_files_async(),
 * unlike the g_file_enumerator_next() which will return one #GFile at
 * a time, this will return all children at once.
 *
 * The ordering of returned files is unspecified and dependent on
 * implementation.
 *
 * If your application needs a specific ordering, such as by name or
 * modification time, you will have to implement that in your
 * application code.
 *
 * There is also a #TrackerFileEnumerator which is an implementation
 * of this #TrackerEnumerator interface. This makes use of the
 * #GFileEnumerator API as an example of how to implement your own
 * enumerator class. Typically, an implementation class (like
 * #TrackerFileEnumerator) would be used with TrackerCrawler (an
 * internal class) which feeds URI data for Tracker to insert into the
 * database.
 *
 * The #TrackerMinerFS class which is a subclass to #TrackerMiner
 * takes a #TrackerEnumerator property which is passed down to the
 * TrackerCrawler created upon instantiation. This property is
 * #TrackerMinerFS:enumerator.
 *
 * What is important to note about implementations is what is expected
 * to be returned by an enumerator. There are some simple rules:
 *
 * 1. Tracker expects children on a per directory basis only, i.e. not
 * recursively given for a top level URI. This allows Tracker to
 * properly construct the database and carry out white/black listing
 * correctly, amongst other things.
 * 2. Tracker does not expect the top level URL to be reported in the
 * children returned. This is considered an error.
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
 * tracker_enumerator_get_children:
 * @enumerator: a #TrackerEnumerator
 * @dir: a #GFile to enumerate
 * @attributes: an attribute query string
 * @flags: a set of GFileQueryInfoFlags
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Enumerates children at the URI provided by @dir.
 *
 * The attributes value is a string that specifies the file attributes
 * that should be gathered. It is not an error if it's not possible to
 * read a particular requested attribute from a file - it just won't
 * be set. attributes should be a comma-separated list of attributes
 * or attribute wildcards. The wildcard "*" means all attributes, and
 * a wildcard like "standard::*" means all attributes in the standard
 * namespace. An example attribute query be "standard::*,owner::user".
 * The standard attributes are available as defines, like
 * G_FILE_ATTRIBUTE_STANDARD_NAME. See g_file_enumerate_children() for
 * more details.
 *
 * Returns: (transfer full) (element-type Gio.FileInfo): a #GSList of
 * #GFileInfo pointers. Both must be freed with g_slist_free() and
 * g_object_unref() when finished with.
 *
 * Since: 1.2
 **/
GSList *
tracker_enumerator_get_children (TrackerEnumerator    *enumerator,
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

	if (iface->get_children == NULL) {
		g_set_error_literal (error,
		                     G_IO_ERROR,
		                     G_IO_ERROR_NOT_SUPPORTED,
		                     _("Operation not supported"));
		return NULL;
	}

	return (* iface->get_children) (enumerator, dir, attributes, flags, cancellable, error);
}

/**
 * tracker_enumerator_get_children_async:
 * @enumerator: a #TrackerEnumerator.
 * @dir: a #GFile to enumerate
 * @attributes: an attribute query string
 * @flags: a set of GFileQueryInfoFlags
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to
 * ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the
 * request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Precisely the same operation as tracker_enumerator_get_children()
 * is performing, but asynchronously.
 *
 * When all i/o for the operation is finished the @callback will be
 * called with the requested information.

 * See the documentation of #TrackerEnumerator for information about the
 * order of returned files.
 *
 * In case of a partial error the callback will be called with any
 * succeeding items and no error, and on the next request the error
 * will be reported. If a request is cancelled the callback will be
 * called with %G_IO_ERROR_CANCELLED.
 *
 * During an async request no other sync and async calls are allowed,
 * and will result in %G_IO_ERROR_PENDING errors.
 *
 * Any outstanding i/o request with higher priority (lower numerical
 * value) will be executed before an outstanding request with lower
 * priority. Default priority is %G_PRIORITY_DEFAULT.
 *
 * Since: 1.2
 **/
void
tracker_enumerator_get_children_async (TrackerEnumerator    *enumerator,
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

	if (iface->get_children_async == NULL) {
		g_critical (_("Operation not supported"));
		return;
	}

	(* iface->get_children_async) (enumerator, dir, attributes, flags, io_priority, cancellable, callback, user_data);
}

/**
 * tracker_enumerator_get_children_finish:
 * @enumerator: a #TrackerEnumerator.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL
 * to ignore.
 *
 * Finishes the asynchronous operation started with
 * tracker_enumerator_get_children_async().
 *
 * Returns: (transfer full) (element-type Gio.FileInfo): a #GSList of
 * #GFileInfos. You must free the list with g_slist_free() and
 * unref the infos with g_object_unref() when you're done with
 * them.
 *
 * Since: 1.2
 **/
GSList *
tracker_enumerator_get_children_finish (TrackerEnumerator  *enumerator,
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

	return (* iface->get_children_finish) (enumerator, result, error);
}
