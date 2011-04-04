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

void
tracker_locale_gconfdbus_init (void)
{
	if (!g_getenv (TRACKER_DISABLE_MEEGOTOUCH_LOCALE_ENV)) {
		guint i;
		GError *error = NULL;
		GVariant *reply;

		g_message ("Retrieving locale from GConf is ENABLED");

		connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

		if (error) {
			g_critical ("%s", error->message);
			g_clear_error (&error);
			return;
		}

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
			g_critical ("%s", error->message);
			g_clear_error (&error);
			return;
		}

		g_variant_get (reply, "(s)", &gconf_dbus_default_db, NULL);

		g_variant_unref (reply);

		/* And initialize all */
		for (i = 0; i < TRACKER_LOCALE_LAST; i++) {
			gchar *str;

			str = get_value_from_config (gconf_locales[i]);
			if (str) {
				tracker_locale_set (i, str);
				g_free (str);
			}
		}
	}
}

void
tracker_locale_gconfdbus_shutdown (void)
{
	g_free (gconf_dbus_default_db);
	gconf_dbus_default_db = NULL;

	if (connection) {
		g_object_unref (connection);
		connection = NULL;
	}
}

