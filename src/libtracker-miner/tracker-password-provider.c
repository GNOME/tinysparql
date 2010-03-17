/*
 * Copyright (C) 2009, Adrien Bustany (abustany@gnome.org)
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

#include <string.h>
#include <sys/mman.h>

#include "tracker-password-provider.h"

static void
tracker_password_provider_init (gpointer object_class)
{
	static gboolean is_initialized = FALSE;

	if (!is_initialized) {
		g_object_interface_install_property (object_class,
		                                     g_param_spec_string ("name",
		                                                          "Password provider name",
		                                                          "Password provider name",
		                                                          NULL,
		                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
		is_initialized = TRUE;
	}
}

/* That would be better done with G_DECLARE_INTERFACE, but it's GLib 2.24 */
GType
tracker_password_provider_get_type (void)
{
	static GType iface_type = 0;

	if (iface_type == 0) {
		static const GTypeInfo info = {
			sizeof (TrackerPasswordProviderIface),
			tracker_password_provider_init,
			NULL
		};

		iface_type = g_type_register_static (G_TYPE_INTERFACE,
		                                     "TrackerPasswordProvider",
		                                     &info,
		                                     0);
	}

	return iface_type;
}

GQuark
tracker_password_provider_error_quark (void)
{
	return g_quark_from_static_string (TRACKER_PASSWORD_PROVIDER_ERROR_DOMAIN);
}

gchar*
tracker_password_provider_get_name (TrackerPasswordProvider *provider)
{
	g_assert (TRACKER_IS_PASSWORD_PROVIDER (provider));

	gchar *name;
	g_object_get (provider, "name", &name, NULL);

	return name;
}

gboolean
tracker_password_provider_store_password (TrackerPasswordProvider  *provider,
                                          const gchar              *service,
                                          const gchar              *description,
                                          const gchar              *username,
                                          const gchar              *password,
                                          GError                  **error)
{
	g_assert (TRACKER_IS_PASSWORD_PROVIDER (provider));

	return TRACKER_PASSWORD_PROVIDER_GET_INTERFACE (provider)->store_password (provider,
	                                                                    service,
	                                                                    description,
	                                                                    username,
	                                                                    password,
	                                                                    error);
}

gchar*
tracker_password_provider_get_password (TrackerPasswordProvider   *provider,
                                        const gchar               *service,
                                        gchar                    **username,
                                        GError                   **error)
{
	g_assert (TRACKER_IS_PASSWORD_PROVIDER (provider));

	gchar *password = TRACKER_PASSWORD_PROVIDER_GET_INTERFACE (provider)->get_password (provider,
	                                                                                    service,
	                                                                                    username,
	                                                                                    error);
	return password;
}

void
tracker_password_provider_forget_password (TrackerPasswordProvider   *provider,
                                           const gchar               *service,
                                           GError                   **error)
{
	g_assert (TRACKER_IS_PASSWORD_PROVIDER (provider));

	TRACKER_PASSWORD_PROVIDER_GET_INTERFACE (provider)->forget_password (provider,
	                                                                     service,
	                                                                     error);
}

gchar*
tracker_password_provider_strdup_mlock (const gchar *source)
{
	gchar *dest;
	dest = malloc (1 + strlen (source));
	dest = memset (dest, 0, 1 + strlen (source));
	mlock (dest, sizeof (dest));
	memcpy (dest, source, strlen (source));

	return dest;
}
