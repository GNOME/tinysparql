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

static GDBusConnection *connection;
static GDBusProxy *proxy_resources;
static GList *writeback_callbacks;
static guint writeback_callback_id;
static guint signal_id;

static void
writeback_cb (GDBusProxy       *proxy,
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


static GHashTable*
hashtable_from_variant (GVariant *variant)
{
	GHashTable* table;
	GVariantIter iter;
	GVariant* variant1;
	GVariant* variant2;

	table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	g_variant_iter_init (&iter, variant);

	while (g_variant_iter_loop (&iter, "{?*}", &variant1, &variant2)) {
		g_hash_table_insert (table,
		                     g_variant_dup_string (variant1, NULL),
		                     g_variant_dup_string (variant2, NULL));
	}

	return table;
}


static void
on_signal (GDBusProxy *proxy,
           gchar      *sender_name,
           gchar      *signal_name,
           GVariant   *parameters,
           gpointer    user_data)
{
	if (g_strcmp0 (signal_name, "Writeback") == 0) {
		GHashTable *table;

		table = hashtable_from_variant (parameters);
		writeback_cb (proxy, table, user_data);
		g_hash_table_unref (table);
	} else {
		g_assert_not_reached ();
	}
}

void
tracker_writeback_init (void)
{
	GError *error = NULL;

	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (!connection || error) {
		g_warning ("Could not connect to D-Bus session bus, %s\n",
			   error ? error->message : "no error given");
		g_clear_error (&error);
		return;
	}

	proxy_resources =
		g_dbus_proxy_new_sync (connection,
		                       G_DBUS_PROXY_FLAGS_NONE,
		                       NULL,
		                       TRACKER_DBUS_SERVICE,
		                       TRACKER_DBUS_OBJECT_RESOURCES,
		                       TRACKER_DBUS_INTERFACE_RESOURCES,
		                       NULL, NULL);
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
		signal_id = g_signal_connect (proxy_resources,
		                              "g-signal",
		                              G_CALLBACK (on_signal),
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
		g_signal_handler_disconnect (proxy_resources, signal_id);
	}

	g_object_unref (connection);
}
