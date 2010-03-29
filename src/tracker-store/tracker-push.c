/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <string.h>
#include <time.h>

#include <gmodule.h>

#include <dbus/dbus-glib-lowlevel.h>

#include <libtracker-common/tracker-utils.h>

#include "tracker-push.h"
#include "tracker-push-registrar.h"

typedef struct {
	DBusGConnection *connection;
	DBusGProxy *dbus_proxy;
	GList *modules;
} PushSupportPrivate;

typedef struct {
	TrackerPushRegistrar* (*init) (void);
	void (*shutdown) (TrackerPushRegistrar *registrar);
	TrackerPushRegistrar *registrar;
	GModule *module;
} PushModule;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;


static void
load_modules (PushSupportPrivate *private)
{
	GDir *dir = g_dir_open (PUSH_MODULES_DIR, 0, NULL);
	const gchar *name;

	if (!dir) {
		return;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		if (g_str_has_suffix (name, G_MODULE_SUFFIX)) {
			gchar *path = g_build_filename (PUSH_MODULES_DIR, name, NULL);
			PushModule *p_module = g_slice_new (PushModule);

			p_module->module = g_module_open (path, G_MODULE_BIND_LOCAL);

			if (!g_module_symbol (p_module->module, "tracker_push_module_shutdown",
			                      (gpointer *) &p_module->shutdown) ||
			    !g_module_symbol (p_module->module, "tracker_push_module_init",
			                      (gpointer *) &p_module->init)) {

				g_warning ("Could not load module symbols for '%s': %s",
				           path, g_module_error ());

				g_module_close (p_module->module);
				g_slice_free (PushModule, p_module);

			} else {
				g_module_make_resident (p_module->module);

				p_module->registrar = p_module->init ();

				private->modules = g_list_prepend (private->modules,
				                                   p_module);
			}

			g_free (path);
		}
	}

	g_dir_close (dir);
}

static void
name_owner_changed_cb (DBusGProxy *proxy,
                       gchar *name,
                       gchar *old_owner,
                       gchar *new_owner,
                       gpointer user_data)
{
	GList *copy;
	PushSupportPrivate *private;
	gboolean found = FALSE;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* If we receive a NameOwnerChanged about the manager's service */

	copy = private->modules;

	for (; copy && !found; copy = g_list_next (copy)) {
		PushModule *p_module = copy->data;
		TrackerPushRegistrar *registrar = p_module->registrar;
		const gchar *service = tracker_push_registrar_get_service (registrar);

		if (g_strcmp0 (name, service) == 0) {

			if (tracker_is_empty_string (new_owner) && !tracker_is_empty_string (old_owner)) {
				tracker_push_registrar_disable (registrar);
			}

			if (tracker_is_empty_string (old_owner) && !tracker_is_empty_string (new_owner)) {
				GError *error  = NULL;

				tracker_push_registrar_enable (registrar,
				                               private->connection,
				                               private->dbus_proxy,
				                               &error);

				if (error) {
					g_debug ("%s\n", error->message);
					g_error_free (error);
				}
			}

			found = TRUE;
		}
	}

	if (!found) {
		copy = private->modules;

		/* If the manager's service is found, start the registrar */

		for (; copy; copy = g_list_next (copy)) {
			PushModule *p_module = copy->data;
			TrackerPushRegistrar *registrar = p_module->registrar;
			const gchar *service = tracker_push_registrar_get_service (registrar);

			if (g_strcmp0 (name, service) == 0) {
				GError *error  = NULL;

				tracker_push_registrar_enable (registrar,
				                               private->connection,
				                               private->dbus_proxy,
				                               &error);

				if (error) {
					g_debug ("%s\n", error->message);
					g_error_free (error);
				}

				break;
			}
		}
	}
}

static void
list_names_reply_cb (DBusGProxy     *proxy,
                     DBusGProxyCall *call,
                     gpointer        user_data)
{
	PushSupportPrivate *private;
	GError *error = NULL;
	GStrv names = NULL;
	guint i = 0;
	gboolean found = FALSE;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	dbus_g_proxy_end_call (proxy, call, &error,
	                       G_TYPE_STRV, &names,
	                       G_TYPE_INVALID);

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		if (names)
			g_strfreev (names);
		return;
	}

	while (names[i] != NULL && !found) {
		GList *copy = private->modules;

		/* If the manager's service is found, start the registrar */

		for (; copy; copy = g_list_next (copy)) {
			PushModule *p_module = copy->data;
			TrackerPushRegistrar *registrar = p_module->registrar;
			const gchar *service = tracker_push_registrar_get_service (registrar);

			if (g_strcmp0 (names[i], service) == 0) {
				GError *lerror  = NULL;

				tracker_push_registrar_enable (registrar,
				                               private->connection,
				                               private->dbus_proxy,
				                               &lerror);

				if (lerror) {
					g_debug ("%s\n", lerror->message);
					g_error_free (lerror);
				}

				found = TRUE;
				break;
			}
		}
		i++;
	}

	g_strfreev (names);
}

static void
free_private (PushSupportPrivate *private)
{
	if (private->connection)
		dbus_g_connection_unref (private->connection);
	g_free (private);
}

void
tracker_push_init (void)
{
	DBusGConnection *connection;
	GError *error = NULL;
	PushSupportPrivate *private;

	private = g_new0 (PushSupportPrivate, 1);

	g_static_private_set (&private_key,
	                      private,
	                      (GDestroyNotify) free_private);

	load_modules (private);

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!error) {
		GList *copy;
		DBusError dbus_error;

		private->connection = dbus_g_connection_ref (connection);

		private->dbus_proxy = dbus_g_proxy_new_for_name (private->connection,
		                                                 DBUS_SERVICE_DBUS,
		                                                 DBUS_PATH_DBUS,
		                                                 DBUS_INTERFACE_DBUS);

		/* We listen for NameOwnerChanged to know when the manager's service
		 * comes up and to know when it goes down */

		dbus_g_proxy_add_signal (private->dbus_proxy, "NameOwnerChanged",
		                         G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
		                         G_TYPE_INVALID);

		dbus_g_proxy_connect_signal (private->dbus_proxy, "NameOwnerChanged",
		                             G_CALLBACK (name_owner_changed_cb),
		                             NULL, NULL);

		dbus_error_init (&dbus_error);

		copy = private->modules;

		for (; copy; copy = g_list_next (copy)) {

			PushModule *p_module = copy->data;
			TrackerPushRegistrar *registrar = p_module->registrar;
			const gchar *service = tracker_push_registrar_get_service (registrar);

			gchar *dbus_string = g_strdup_printf ("type='signal',"
			                                      "sender='" DBUS_SERVICE_DBUS
			                                      "',interface='" DBUS_INTERFACE_DBUS
			                                      "',path='" DBUS_PATH_DBUS
			                                      "',member='NameOwnerChanged',"
			                                      "arg0='%s'", service);

			dbus_bus_add_match ((DBusConnection *) dbus_g_connection_get_connection (private->connection),
			                    dbus_string, &dbus_error);

			if (dbus_error_is_set (&dbus_error)) {
				g_warning ("%s for rule=%s\n", dbus_error.message, dbus_string);
				g_free (dbus_string);
				dbus_error_free (&dbus_error);
				break;
			}

			g_free (dbus_string);
		}

		/* If the manager service is up and running, then list_names_reply_cb
		 * will execute activate_registrar, as it'll appear in the results of
		 * the ListNames DBus function. If not then we will just wait for the
		 * NameOwnerChanged to emit that the manager's service has came up. */

		dbus_g_proxy_begin_call (private->dbus_proxy, "ListNames",
		                         list_names_reply_cb, NULL, NULL,
		                         G_TYPE_INVALID,
		                         G_TYPE_INVALID);

	} else {
		g_critical ("Could not setup D-Bus, %s\n", error->message);
		g_error_free (error);
	}
}

static void
unload_module (gpointer data, gpointer user_data)
{
	PushModule *p_module = data;
	p_module->shutdown (p_module->registrar);
	g_object_unref (p_module->registrar);
	g_module_close (p_module->module);
	g_slice_free (PushModule, p_module);
}


void
tracker_push_shutdown (void)
{
	PushSupportPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (private->dbus_proxy) {
		g_object_unref (private->dbus_proxy);
		private->dbus_proxy = NULL;
	}

	g_list_foreach (private->modules, unload_module, NULL);
	g_list_free (private->modules);
	private->modules = NULL;

	g_static_private_set (&private_key, NULL, NULL);
}
