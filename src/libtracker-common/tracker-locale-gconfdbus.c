/*
 * Copyright (C) 2010 Nokia <ivan.frade@nokia.com>
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

#include <locale.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "tracker-locale-gconfdbus.h"

/* This helps with testing, change all the names in gconf-dbus and then you
 * can run it in parallel with an upstream gconf-d of the GNOME platform */
#define GCONF_DBUS_NAME "GConf"

#define GCONF_DBUS_SERVICE                    "org.gnome." GCONF_DBUS_NAME
#define GCONF_DBUS_SERVER_INTERFACE           "org.gnome." GCONF_DBUS_NAME ".Server"
#define GCONF_DBUS_DATABASE_INTERFACE         "org.gnome." GCONF_DBUS_NAME ".Database"
#define GCONF_DBUS_SERVER_OBJECT              "/org/gnome/" GCONF_DBUS_NAME "/Server"
#define GCONF_DBUS_CLIENT_OBJECT              "/org/gnome/" GCONF_DBUS_NAME "/Client"
#define GCONF_DBUS_CLIENT_INTERFACE           "org.gnome." GCONF_DBUS_NAME ".Client"

/* Base dir for all gconf locale values */
#define MEEGOTOUCH_LOCALE_DIR                 "/meegotouch/i18n"

#define TRACKER_DISABLE_MEEGOTOUCH_LOCALE_ENV "TRACKER_DISABLE_MEEGOTOUCH_LOCALE"

static gchar*gconf_dbus_default_db = NULL;
static GDBusConnection *connection = NULL;
static gboolean service_running = FALSE;
static guint watch_name_id = 0;
static guint registration_id = 0;
static GStaticMutex subscribers_mutex = G_STATIC_MUTEX_INIT;
GDBusNodeInfo *introspection_data = NULL;
static gboolean non_maemo_mode = FALSE;

/* gconf keys for tracker locales, as defined in:
 * http://apidocs.meego.com/1.0/mtf/i18n.html
 */
static const gchar *gconf_locales[TRACKER_LOCALE_LAST] = {
	MEEGOTOUCH_LOCALE_DIR "/language",
	MEEGOTOUCH_LOCALE_DIR "/lc_time",
	MEEGOTOUCH_LOCALE_DIR "/lc_collate",
	MEEGOTOUCH_LOCALE_DIR "/lc_numeric",
	MEEGOTOUCH_LOCALE_DIR "/lc_monetary"
};

/* Structure to hold the notification data of each subscriber */
typedef struct {
	TrackerLocaleID id;
	TrackerLocaleNotifyFunc func;
	gpointer user_data;
	GFreeFunc destroy_notify;
} TrackerLocaleNotification;

/* List of subscribers which want to get notified of locale changes */
static GSList *subscribers;

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='" GCONF_DBUS_CLIENT_INTERFACE "'>"
  "    <method name='Notify'>"
  "      <arg type='s' name='database' direction='in' />"
  "      <arg type='s' name='namespace_section' direction='in' />"
  "      <arg type='(s(is)bsbb)' name='value' direction='in' />"
  "    </method>"
  "  </interface>"
  "</node>";

static gboolean
add_notify (void)
{
	GVariant *reply;
	GError *error = NULL;

	reply = g_dbus_connection_call_sync (connection,
	                                     GCONF_DBUS_SERVICE,
	                                     gconf_dbus_default_db,
	                                     GCONF_DBUS_DATABASE_INTERFACE,
	                                     "AddNotify",
	                                     g_variant_new ("(s)", MEEGOTOUCH_LOCALE_DIR),
	                                     NULL,
	                                     G_DBUS_CALL_FLAGS_NONE,
	                                     -1,
	                                     NULL,
	                                     &error);

	if (error) {
		g_critical ("%s", error->message);
		g_clear_error (&error);
		return FALSE;
	}

	g_variant_unref (reply);

	return TRUE;
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
	/* Takes place in mainloop */

	if (g_strcmp0 (method_name, "Notify") == 0) {
		const gchar *key = NULL, *value = NULL;
		const gchar *schema = NULL, *database = NULL;
		const gchar *namespace_name = NULL;
		gboolean is_set, is_default, is_writable;
		gint type, i;
		GSList *li;

		if (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(ss(s(is)bsbb))"))) {
			g_variant_get (parameters, "(&s&s(&s(i&s)b&sbb))",
			               &database, &namespace_name,
			               &key, &type, &value,
			               &is_set, &schema,
			               &is_default, &is_writable,
			               NULL);

			/* Find the proper locale to change */
			for (i = 0; i < TRACKER_LOCALE_LAST; i++) {
				if (strcmp (gconf_locales[i], key) == 0) {
					break;
				}
			}

			/* Oh, not found? */
			if (i == TRACKER_LOCALE_LAST) {
				g_debug ("Skipping change on gconf key '%s' as not really needed", key);
				return;
			}

			/* Ensure a proper value was set */
			if (value == NULL || value[0] == '\0') {
				g_warning ("Locale value for '%s' cannot be NULL, not changing %s",
				           gconf_locales[i],
				           tracker_locale_get_name (i));
				return;
			}

			/* This always runs from mainloop, so no need for a lock other than
			 * subscribers, which might be added and removed by threads (and are
			 * here executed by function pointer on the mainloop) */

			tracker_locale_set (i, value);

			g_static_mutex_lock (&subscribers_mutex);

			for (li = subscribers; li; li = g_slist_next (li)) {
				TrackerLocaleNotification *data = li->data;

				if (i == data->id) {
					g_debug ("Notifying locale '%s' change to subscriber '%p'",
					         tracker_locale_get_name(i),
					         data);
					data->func (i, data->user_data);
				}
			}

			g_static_mutex_unlock (&subscribers_mutex);
		}
	}
}

static GVariant *
handle_get_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data)
{
	return NULL;
}

static gboolean
handle_set_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data)
{
	return TRUE;
}



static gchar *
get_value_from_config (const gchar *key_in)
{
	const gchar *locale = setlocale (LC_CTYPE, NULL);
	const gchar *key, *value, *schema;
	gchar *val = NULL;
	gboolean is_set, is_default, is_writable;
	gint type;
	GError *error = NULL;
	GVariant *reply;

	reply = g_dbus_connection_call_sync (connection,
	                                     GCONF_DBUS_SERVICE,
	                                     gconf_dbus_default_db,
	                                     GCONF_DBUS_DATABASE_INTERFACE,
	                                     "LookupExtended",
	                                     g_variant_new ("(ssb)", key_in, locale, TRUE),
	                                     NULL,
	                                     G_DBUS_CALL_FLAGS_NONE,
	                                     -1,
	                                     NULL,
	                                     &error);

	if (error) {
		g_variant_unref (reply);
		g_critical ("%s", error->message);
		g_clear_error (&error);

		return NULL;
	}

	if (g_variant_is_of_type (reply, G_VARIANT_TYPE ("((s(is)bsbb))"))) {
		g_variant_get (reply, "((&s(i&s)b&sbb))",
		               &key, &type, &value,
		               &is_set, &schema,
		               &is_default, &is_writable,
		               NULL);

		val = g_strdup (value);
	}

	g_variant_unref (reply);

	return val;
}


static void
on_gconfd_dbus_appeared (GDBusConnection *connection,
                         const gchar     *name,
                         const gchar     *name_owner,
                         gpointer         user_data)
{
	guint i;

	service_running = TRUE;
	add_notify ();

	/* And (re)initialize all */
	for (i = 0; i < TRACKER_LOCALE_LAST; i++) {
		gchar *str;

		str = get_value_from_config (gconf_locales[i]);
		if (str) {
			tracker_locale_set (i, str);
			g_free (str);
		}
	}
}

static void
on_gconfd_dbus_disappeared  (GDBusConnection *connection,
                             const gchar     *name,
                             gpointer         user_data)
{
	service_running = FALSE;
}

void
tracker_locale_gconfdbus_init (void)
{
	if (!g_getenv (TRACKER_DISABLE_MEEGOTOUCH_LOCALE_ENV) && !non_maemo_mode) {
		GError *error = NULL;
		GVariant *reply;
		GDBusInterfaceVTable interface_vtable = {
			handle_method_call,
			handle_get_property,
			handle_set_property
		};

		g_message ("Retrieving locale from GConf is ENABLED");

		connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

		if (error) {
			g_critical ("%s", error->message);
			g_clear_error (&error);
			return;
		}

		service_running = TRUE;

		reply = g_dbus_connection_call_sync (connection,
		                                     GCONF_DBUS_SERVICE,
		                                     GCONF_DBUS_SERVER_OBJECT,
		                                     GCONF_DBUS_SERVER_INTERFACE,
		                                     "GetDefaultDatabase",
		                                     NULL,
		                                     NULL,
		                                     G_DBUS_CALL_FLAGS_NONE,
		                                     -1,
		                                     NULL,
		                                     &error);


		if (error) {
			if (error->code == 19) {
				g_message ("GetDefaultDatabase doesn't exist on %s, this GConf "
				           "doesn't look like a gconf-dbus.\n"
				           "Continuing in non-maemo mode",
				           GCONF_DBUS_SERVER_OBJECT);
				g_object_unref (connection);
				connection = NULL;
				non_maemo_mode = TRUE;
				return;
			} else {
				g_critical ("%s", error->message);
				g_clear_error (&error);
				return;
			}
		}

		g_variant_get (reply, "(s)", &gconf_dbus_default_db, NULL);

		g_variant_unref (reply);

		introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, &error);

		if (error) {
			g_critical ("%s", error->message);
			g_clear_error (&error);
			return;
		}

		registration_id =
			g_dbus_connection_register_object (connection,
			                                   GCONF_DBUS_CLIENT_OBJECT,
			                                   introspection_data->interfaces[0],
			                                   &interface_vtable,
			                                   NULL,
			                                   NULL,
			                                   &error);

		if (error) {
			g_critical ("%s", error->message);
			g_clear_error (&error);
			return;
		}

		watch_name_id = g_bus_watch_name_on_connection (connection,
		                                                GCONF_DBUS_SERVICE,
		                                                G_BUS_NAME_WATCHER_FLAGS_NONE,
		                                                on_gconfd_dbus_appeared,
		                                                on_gconfd_dbus_disappeared,
		                                                NULL, NULL);
	}
}

void
tracker_locale_gconfdbus_shutdown (void)
{
	if (gconf_dbus_default_db != NULL && connection != NULL) {
		GVariant *reply;
		GError *error = NULL;

		reply = g_dbus_connection_call_sync (connection,
		                                     GCONF_DBUS_SERVICE,
		                                     gconf_dbus_default_db,
		                                     GCONF_DBUS_DATABASE_INTERFACE,
		                                     "RemoveNotify",
		                                     g_variant_new ("(s)", MEEGOTOUCH_LOCALE_DIR),
		                                     NULL,
		                                     G_DBUS_CALL_FLAGS_NONE,
		                                     -1,
		                                     NULL,
		                                     &error);

		if (error) {
			g_warning ("%s", error->message);
			g_clear_error (&error);
		} else {
			g_variant_unref (reply);
		}
	}

	if (watch_name_id != 0) {
		g_bus_unwatch_name (watch_name_id);
		watch_name_id = 0;
	}

	if (registration_id != 0) {
		g_dbus_connection_unregister_object (connection, registration_id);
		registration_id = 0;
	}

	g_free (gconf_dbus_default_db);
	gconf_dbus_default_db = NULL;

	if (introspection_data) {
		g_dbus_node_info_unref (introspection_data);
		introspection_data = NULL;
	}

	if (connection) {
		g_object_unref (connection);
		connection = NULL;
	}
}

gpointer
tracker_locale_gconfdbus_notify_add (TrackerLocaleID         id,
                                     TrackerLocaleNotifyFunc func,
                                     gpointer                user_data,
                                     GFreeFunc               destroy_notify)
{
	TrackerLocaleNotification *data;

	/* Can be called from a thread */

	g_assert (func != NULL);

	data = g_slice_new (TrackerLocaleNotification);
	data->id = id;
	data->func = func;
	data->user_data = user_data;
	data->destroy_notify = destroy_notify;

	g_static_mutex_lock (&subscribers_mutex);
	subscribers = g_slist_prepend (subscribers, data);
	g_static_mutex_unlock (&subscribers_mutex);

	return data;
}

static gboolean
destroy_locale_notify (gpointer data_p)
{
	/* Always on mainloop */

	TrackerLocaleNotification *data = data_p;

	/* Call the provided destroy_notify if any. */
	if (data->destroy_notify) {
		data->destroy_notify (data->user_data);
	}

	/* And fully dispose the notification data */
	g_slice_free (TrackerLocaleNotification, data);

	return FALSE;
}

void
tracker_locale_gconfdbus_notify_remove (gpointer notification_id)
{
	GSList *li;

	/* Can be called from a thread */

	g_static_mutex_lock (&subscribers_mutex);

	li = g_slist_find (subscribers, notification_id);
	if (li) {
		TrackerLocaleNotification *data = li->data;

		/* Remove item from list of subscribers */
		subscribers = g_slist_delete_link (subscribers, li);

		g_idle_add (destroy_locale_notify, data);
	}

	g_static_mutex_unlock (&subscribers_mutex);
}
