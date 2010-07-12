/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <glib-object.h>
#include <gobject/gvaluecollector.h>
#include <dbus/dbus-glib.h>
#include "empty-gobject.h"
#include "miners-mock.h"
#include "tracker-miner-mock.h"

#include <string.h>

GHashTable *miners = NULL;

void
miners_mock_init ()
{
	TrackerMinerMock *miner;

	miners = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	miner = tracker_miner_mock_new (MOCK_MINER_1);
	tracker_miner_mock_set_paused (miner, FALSE);
	g_hash_table_insert (miners, g_strdup (MOCK_MINER_1), miner);

	miner = tracker_miner_mock_new (MOCK_MINER_2);
	tracker_miner_mock_set_paused (miner, TRUE);
	g_hash_table_insert (miners, g_strdup (MOCK_MINER_2), miner);
}

/*
 * DBus overrides
 */
DBusGConnection *
dbus_g_bus_get (DBusBusType type, GError **error)
{
	return (DBusGConnection *) empty_object_new ();
}

DBusGProxy *
dbus_g_proxy_new_for_name (DBusGConnection *connection,
                           const gchar *service,
                           const gchar *path,
                           const gchar *interface )
{
	TrackerMinerMock *miner;

	miner = (TrackerMinerMock *)g_hash_table_lookup (miners, service);
	if (!miner) {
		return (DBusGProxy *) empty_object_new ();
	}
	return (DBusGProxy *) miner;
}

void
dbus_g_proxy_add_signal (DBusGProxy *proxy, const char *signal_name, GType first_type,...)
{
}

void
dbus_g_proxy_connect_signal (DBusGProxy *proxy,
                             const char *signal_name,
                             GCallback handler,
                             void *data,
                             GClosureNotify free_data_func)
{
	TrackerMinerMock *miner = (TrackerMinerMock *)proxy;

	if (g_strcmp0 (signal_name, "NameOwnerChanged") == 0) {
		return;
	}

	g_signal_connect (miner, g_utf8_strdown (signal_name, -1), handler, data);

}

/*
 * Two mock miners available but only 1 running
 */
gboolean
dbus_g_proxy_call (DBusGProxy *proxy,
                   const gchar *function_name,
                   GError  **error,
                   GType first_arg_type, ...)
{
	va_list args;
	GType   arg_type;
	const gchar *running_services[] = { "org.gnome.Tomboy",
	                                    "org.gnome.GConf",
	                                    MOCK_MINER_1,
	                                    "org.gnome.SessionManager",
	                                    NULL};

	va_start (args, first_arg_type);

	if (g_strcmp0 (function_name, "ListNames") == 0) {
		/*
		 *  G_TYPE_INVALID,
		 *  G_TYPE_STRV, &result,
		 *  G_TYPE_INVALID
		 */
		GValue value = { 0, };
		gchar *local_error = NULL;

		arg_type = va_arg (args, GType);

		g_assert (arg_type == G_TYPE_STRV);
		g_value_init (&value, arg_type);
		g_value_set_boxed (&value, running_services);
		G_VALUE_LCOPY (&value,
		               args, 0,
		               &local_error);
		g_free (local_error);
		g_value_unset (&value);

	} else if (g_strcmp0 (function_name, "NameHasOwner") == 0) {
		/*
		 * G_TYPE_STRING, miner,
		 * G_TYPE_INVALID,
		 * G_TYPE_BOOLEAN, &active,
		 *  G_TYPE_INVALID)) {
		 */
		GValue value = { 0, };
		gchar *local_error = NULL;
		const gchar *miner_name;
		TrackerMinerMock *miner;
		gboolean     active;

		g_value_init (&value, G_TYPE_STRING);
		G_VALUE_COLLECT (&value, args, 0, &local_error);
		g_free (local_error);
		miner_name = g_value_get_string (&value);

		miner = (TrackerMinerMock *)g_hash_table_lookup (miners, miner_name);
		active = !tracker_miner_mock_get_paused (miner);
		g_value_unset (&value);

		arg_type = va_arg (args, GType);
		g_assert (arg_type == G_TYPE_INVALID);

		arg_type = va_arg (args, GType);
		g_assert (arg_type == G_TYPE_BOOLEAN);
		g_value_init (&value, arg_type);
		g_value_set_boolean (&value, active);
		G_VALUE_LCOPY (&value,
		               args, 0,
		               &local_error);
		g_free (local_error);
		g_value_unset (&value);

	} else if (g_strcmp0 (function_name, "GetPauseDetails") == 0) {
		/*
		 *  G_TYPE_INVALID,
		 *  G_TYPE_STRV, &apps,
		 *  G_TYPE_STRV, &reasons,
		 *  G_TYPE_INVALID
		 */
		GValue value = { 0, };
		gchar *local_error = NULL;
		gint   amount;
		gchar **apps, **reasons;
		TrackerMinerMock *miner = (TrackerMinerMock *)proxy;

		arg_type = va_arg (args, GType);
		g_assert (arg_type == G_TYPE_STRV);
		g_value_init (&value, arg_type);
		apps = tracker_miner_mock_get_apps (miner, &amount);
		if (apps == NULL || amount == 0) {
			apps = g_new0 (gchar *, 1);
		}
		g_value_set_boxed (&value, apps);
		G_VALUE_LCOPY (&value,
		               args, 0,
		               &local_error);
		g_free (local_error);
		g_value_unset (&value);

		arg_type = va_arg (args, GType);
		g_assert (arg_type == G_TYPE_STRV);
		g_value_init (&value, arg_type);
		reasons = tracker_miner_mock_get_reasons (miner, &amount);
		if (reasons == NULL || amount == 0) {
			reasons = g_new0 (gchar *, 1);
		}
		g_value_set_boxed (&value, reasons);
		G_VALUE_LCOPY (&value,
		               args, 0,
		               &local_error);
		g_free (local_error);
		g_value_unset (&value);

	} else if (g_strcmp0 (function_name, "Pause") == 0) {
		/*
		 *  G_TYPE_STRING, &app,
		 *  G_TYPE_STRING, &reason,
		 *  G_TYPE_INVALID,
		 *  G_TYPE_INT, &cookie,
		 *  G_TYPE_INVALID
		 */
		GValue value_app = { 0, };
		gchar *local_error = NULL;
		GValue value_reason = {0, };
		const gchar *app;
		const gchar *reason;
		TrackerMinerMock *miner = (TrackerMinerMock *)proxy;

		g_value_init (&value_app, G_TYPE_STRING);
		G_VALUE_COLLECT (&value_app, args, 0, &local_error);
		g_free (local_error);
		app = g_value_get_string (&value_app);

		arg_type = va_arg (args, GType);
		g_value_init (&value_reason, G_TYPE_STRING);
		G_VALUE_COLLECT (&value_reason, args, 0, &local_error);
		g_free (local_error);
		reason = g_value_get_string (&value_reason);

		tracker_miner_mock_pause (miner, app, reason);

	} else if (g_strcmp0 (function_name, "Resume") == 0) {
		/*
		 * G_TYPE_INT, &cookie
		 * G_TYPE_INVALID
		 */
		TrackerMinerMock *miner = (TrackerMinerMock *)proxy;
		tracker_miner_mock_resume (miner);

	} else if (g_strcmp0 (function_name, "IgnoreNextUpdate") == 0) {
		/* Well, ok... */
	} else if (g_strcmp0 (function_name, "GetProgress") == 0) {
		/* Whatever */
	} else if (g_strcmp0 (function_name, "GetStatus") == 0) {
		/* Whatever */
	} else {
		g_critical ("dbus_g_proxy_call '%s' unsupported", function_name);
	}

	va_end (args);
	return TRUE;
}


void
dbus_g_proxy_call_no_reply (DBusGProxy        *proxy,
                            const char        *method,
                            GType              first_arg_type,
                            ...)
{
}


void
dbus_g_connection_unref (DBusGConnection *conn)
{
	/* It is an EmptyGObject */
	g_object_unref (conn);
}
