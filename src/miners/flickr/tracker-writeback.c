/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2010, Codeminded BVBA <abustany@gnome.org>
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
 */

#include "config.h"

#include <libtracker-common/tracker-common.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-writeback.h"

typedef struct {
	guint id;
	TrackerWritebackCallback func;
	gpointer data;
} WritebackCallback;

static DBusGConnection *connection;
static DBusGProxy *proxy_resources;
static GList *writeback_callbacks;
static guint writeback_callback_id;

static void
writeback_cb (DBusGProxy       *proxy,
              const GHashTable *resources,
              gpointer          user_data)
{
	WritebackCallback *cb;
	GList *l;

	g_return_if_fail (resources != NULL);

	for (l = writeback_callbacks; l; l = l->next) {
		cb = l->data;
		cb->func (resources, cb->data);
	}
}

void
tracker_writeback_init (void)
{
	GError *error = NULL;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection || error) {
		g_warning ("Could not connect to D-Bus session bus, %s\n",
			   error ? error->message : "no error given");
		g_clear_error (&error);
		return;
	}

	proxy_resources =
		dbus_g_proxy_new_for_name (connection,
		                           TRACKER_DBUS_SERVICE,
		                           TRACKER_DBUS_OBJECT_RESOURCES,
		                           TRACKER_DBUS_INTERFACE_RESOURCES);

	dbus_g_proxy_add_signal (proxy_resources,
	                         "Writeback",
	                         TRACKER_TYPE_INT_ARRAY_MAP,
	                         G_TYPE_INVALID);
}

void
tracker_writeback_shutdown (void)
{
	GList *l;

	if (proxy_resources) {
		g_object_unref (proxy_resources);
		proxy_resources = NULL;
	}

	for (l = writeback_callbacks; l; l = l->next) {
		WritebackCallback *cb = l->data;

		if (!cb) {
			continue;
		}

		g_slice_free (WritebackCallback, l->data);
	}

	g_list_free (writeback_callbacks);
}

guint
tracker_writeback_connect (TrackerWritebackCallback callback,
			   gpointer                 user_data)
{
	WritebackCallback *cb;

	g_return_val_if_fail (callback != NULL, 0);

	/* Connect the DBus signal if needed */
	if (!writeback_callbacks) {
		dbus_g_proxy_connect_signal (proxy_resources,
		                             "Writeback",
		                             G_CALLBACK (writeback_cb),
		                             NULL,
		                             NULL);
	}

	cb = g_slice_new0 (WritebackCallback);
	cb->id = ++writeback_callback_id;
	cb->func = callback;
	cb->data = user_data;

	writeback_callbacks = g_list_prepend (writeback_callbacks, cb);

	return cb->id;
}

void
tracker_writeback_disconnect (guint handle)
{
	GList *l;

	for (l = writeback_callbacks; l; l = l->next) {
		WritebackCallback *cb = l->data;

		if (!cb) {
			continue;
		}

		if (cb->id == handle) {
			g_slice_free (WritebackCallback, l->data);
			writeback_callbacks = g_list_remove (writeback_callbacks, l);
		}
	}

	/* Disconnect the DBus signal if not needed anymore */
	if (!writeback_callbacks) {
		dbus_g_proxy_disconnect_signal (proxy_resources,
		                                "Writeback",
		                                G_CALLBACK (writeback_cb),
		                                NULL);
	}
}
