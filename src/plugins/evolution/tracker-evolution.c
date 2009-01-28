/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
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

#include <libtracker-data/tracker-data-manager.h>

#include "tracker-evolution.h"
#include "tracker-evolution-registrar.h"

typedef struct {
	TrackerConfig *config;
	DBusGConnection *connection;
	gboolean is_enabled;
	DBusGProxy *dbus_proxy;
	DBusGProxy *manager_proxy;
	GObject *object;
	gboolean deactivating;
} EvolutionSupportPrivate;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static guint
get_stored_last_modseq (void)
{
	return (guint) tracker_data_manager_get_db_option_int ("EvolutionLastModseq");
}

static void
deactivate_registrar (void)
{
	EvolutionSupportPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (private->object) {
		g_object_unref (private->object);
		private->object = NULL;
	}

	if (private->manager_proxy && !private->deactivating)
		g_object_unref (private->manager_proxy);

	private->manager_proxy = NULL;
}

static void
deactivate_dbus_client (void)
{
	EvolutionSupportPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	deactivate_registrar ();

	if (private->dbus_proxy) {
		g_object_unref (private->dbus_proxy);
		private->dbus_proxy = NULL;
	}
}

static void
on_manager_destroy (DBusGProxy *proxy, gpointer user_data)
{
	EvolutionSupportPrivate *private;
	gboolean old_setting;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	old_setting = private->deactivating;

	private->deactivating = TRUE;
	deactivate_registrar ();
	private->deactivating = old_setting;
}


static void
activate_registrar (void)
{
	EvolutionSupportPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (private->object)
		return;

	private->manager_proxy = 
		dbus_g_proxy_new_for_name (private->connection,
					   TRACKER_EVOLUTION_MANAGER_SERVICE,
					   TRACKER_EVOLUTION_MANAGER_PATH,
					   TRACKER_EVOLUTION_MANAGER_INTERFACE);

	/* If while we had a proxy for the manager the manager shut itself down,
	 * then we'll get rid of our registrar too, in on_manager_destroy */

	g_signal_connect (private->manager_proxy, "destroy",
			  G_CALLBACK (on_manager_destroy), NULL);

	if (private->manager_proxy) {
		GError *error = NULL;
		guint result;

		/* Creation of the registrar */
		if (!org_freedesktop_DBus_request_name (private->dbus_proxy, 
							   TRACKER_EVOLUTION_REGISTRAR_SERVICE,
							   DBUS_NAME_FLAG_DO_NOT_QUEUE,
							   &result, &error)) {
			g_critical ("Could not setup DBus, %s in use\n", TRACKER_EVOLUTION_REGISTRAR_SERVICE);
			goto error_handler;
		}

		if (error)
			goto error_handler;

		private->object = g_object_new (TRACKER_TYPE_EVOLUTION_REGISTRAR, 
						"connection", private->connection, 
						NULL);

		dbus_g_object_type_install_info (G_OBJECT_TYPE (private->object), 
						 &registrar_methods);
		dbus_g_connection_register_g_object (private->connection, 
						     TRACKER_EVOLUTION_REGISTRAR_PATH, 
						     private->object);

		/* Registration of the registrar to the manager */
		dbus_g_proxy_call_no_reply (private->manager_proxy, "Register",
					    G_TYPE_OBJECT, private->object, /* TRACKER_EVOLUTION_REGISTRAR_PATH, */
					    G_TYPE_UINT, get_stored_last_modseq (),
					    G_TYPE_INVALID,
					    G_TYPE_INVALID);

		error_handler:

		if (error) {
			g_critical ("Could not setup DBus, %s\n", error->message);
			g_error_free (error);
		}
	}
}

static void
name_owner_changed_cb (DBusGProxy *proxy, 
		       gchar *name, 
		       gchar *old_owner, 
		       gchar *new_owner, 
		       gpointer user_data)
{

	/* If we receive a NameOwnerChanged about the manager's service */

	if (g_strcmp0 (name, TRACKER_EVOLUTION_MANAGER_SERVICE) == 0) {
		if (tracker_is_empty_string (new_owner) && !tracker_is_empty_string (old_owner))
			deactivate_registrar ();
		if (tracker_is_empty_string (old_owner) && !tracker_is_empty_string (new_owner))
			activate_registrar ();
	}
}

static void
list_names_reply_cb (DBusGProxy     *proxy,
		     DBusGProxyCall *call,
		     gpointer        user_data)
{
	GError *error = NULL;
	GStrv names = NULL;
	guint i = 0;

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

	while (names[i] != NULL) {
		/* If the manager's service is found, start the registrar */
		if (g_strcmp0 (names[i], TRACKER_EVOLUTION_MANAGER_SERVICE) == 0) {
			activate_registrar ();
			break;
		}
		i++;
	}

	g_strfreev (names); 
}

static void
activate_dbus_client (void)
{
	EvolutionSupportPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

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

	/* If the manager service is up and running, then list_names_reply_cb
	 * will execute activate_registrar, as it'll appear in the results of
	 * the ListNames DBus function. If not then we will just wait for the
	 * NameOwnerChanged to emit that the manager's service has came up. */

	dbus_g_proxy_begin_call (private->dbus_proxy, "ListNames",
				 list_names_reply_cb, NULL, NULL,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
}

static gboolean 
is_enabled (TrackerConfig *config)
{
	const gchar *module_name = "Evolution";

	/* If none of the disabled modules include the Evolution module,
	 * we assume we are enabled in the configuration. 
	 */
	if (!tracker_module_config_get_enabled (module_name)) {
		return FALSE;
	}

	if (g_slist_find_custom (tracker_config_get_disabled_modules (config),
				 module_name,
				 (GCompareFunc) strcmp)) {
		return FALSE;
	}

	return TRUE;
}

static void 
disabled_notify (GObject    *pspec,
		 GParamSpec *gobject,
		 gpointer    user_data)
{
	EvolutionSupportPrivate *private;
	gboolean new_value;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	new_value = is_enabled (private->config);

	if (private->is_enabled != new_value) {
		if (private->is_enabled) {
			/* If we were enabled, disable */
			deactivate_dbus_client ();
		} else {
			/* If we were disabled, enable */
			activate_dbus_client ();
		}
		private->is_enabled = new_value;
	}

	g_message ("Evolution support service %s", 
		   private->is_enabled ? "enabled" : "disabled");
}

static void
free_private (EvolutionSupportPrivate *private)
{
	dbus_g_connection_unref (private->connection);
	g_object_unref (private->config);
	g_free (private);
}

void
tracker_evolution_init (TrackerConfig *config)
{
	DBusGConnection *connection;
	GError *error = NULL;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!error) {
		EvolutionSupportPrivate *private;

		private = g_new0 (EvolutionSupportPrivate, 1);

		private->deactivating = FALSE;
		private->config = g_object_ref (config);
		private->is_enabled = is_enabled (config);
		private->connection = dbus_g_connection_ref (connection);

		g_static_private_set (&private_key,
				      private,
				      (GDestroyNotify) free_private);

		/* Hook configuration changes */
		g_signal_connect (private->config, "notify::disabled-modules",
				  G_CALLBACK (disabled_notify), 
				  NULL);

		/* If in configuration we are enabled now */
		if (private->is_enabled)
			activate_dbus_client ();
	} else {
		g_critical ("Could not setup DBus, %s\n", error->message);
		g_error_free (error);
	}
}

void
tracker_evolution_shutdown ()
{
	EvolutionSupportPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (private->is_enabled)
		deactivate_dbus_client ();

	g_static_private_set (&private_key, NULL, NULL);
}
