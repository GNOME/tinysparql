/*
 * Copyright (C) 2009, Adrien Bustany <abustany@gnome.org>
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

/* That would be better done with G_DECLARE_INTERFACE, but it's GLib 2.24. */
/**
 * tracker_password_provider_get_type:
 *
 * Returns: a #GType representing a %TrackerPasswordProvider.
 **/
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

/**
 * tracker_password_provider_error_quark:
 *
 * Returns: the #GQuark used to identify password provider errors in
 * GError structures.
 **/
GQuark
tracker_password_provider_error_quark (void)
{
	return g_quark_from_static_string (TRACKER_PASSWORD_PROVIDER_ERROR_DOMAIN);
}

/**
 * tracker_password_provider_get_name:
 * @provider: a TrackerPasswordProvider
 *
 * At the moment there are only two providers, "GNOME Keyring" and
 * "GKeyFile". Either of these is what will be returned unless new
 * providers are written.
 *
 * Returns: a newly allocated string representing the #Object:name
 * which must be freed with g_free().
 **/
gchar *
tracker_password_provider_get_name (TrackerPasswordProvider *provider)
{
	gchar *name;

	g_return_val_if_fail (TRACKER_IS_PASSWORD_PROVIDER (provider), NULL);

	g_object_get (provider, "name", &name, NULL);

	return name;
}

/**
 * tracker_password_provider_store_password:
 * @provider: a TrackerPasswordProvider
 * @service: the name of the remote service associated with @username
 * and @password
 * @description: the description for @service
 * @username: the username to store
 * @password: the password to store
 * @error: return location for errors
 *
 * This function calls the password provider's "store_password"
 * implementation with @service, @description, @username and @password.
 *
 * Returns: %TRUE if the password was saved, otherwise %FALSE is
 * returned and @error will be set.
 **/
gboolean
tracker_password_provider_store_password (TrackerPasswordProvider  *provider,
                                          const gchar              *service,
                                          const gchar              *description,
                                          const gchar              *username,
                                          const gchar              *password,
                                          GError                  **error)
{
	TrackerPasswordProviderIface *iface;

	g_return_val_if_fail (TRACKER_IS_PASSWORD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (service != NULL, FALSE);
	g_return_val_if_fail (description != NULL, FALSE);
	g_return_val_if_fail (password != NULL, FALSE);

	iface = TRACKER_PASSWORD_PROVIDER_GET_INTERFACE (provider);

	if (!iface || !iface->store_password) {
		return FALSE;
	}

	return iface->store_password (provider, service, description, username, password, error);
}

/**
 * tracker_password_provider_get_password:
 * @provider: a TrackerPasswordProvider
 * @service: the name of the remote service associated with @username
 * @username: the username associated with the password we are returning
 * @error: return location for errors
 *
 * This function calls the password provider's "get_password"
 * implementation with @service and @username.
 *
 * Returns: a newly allocated string representing a password which
 * must be freed with g_free(), otherwise %NULL is returned and @error
 * will be set.
 **/
gchar *
tracker_password_provider_get_password (TrackerPasswordProvider  *provider,
                                        const gchar              *service,
                                        gchar                   **username,
                                        GError                  **error)
{
	TrackerPasswordProviderIface *iface;

	g_return_val_if_fail (TRACKER_IS_PASSWORD_PROVIDER (provider), NULL);
	g_return_val_if_fail (service != NULL, NULL);

	iface = TRACKER_PASSWORD_PROVIDER_GET_INTERFACE (provider);

	if (!iface || !iface->get_password) {
		return NULL;
	}

	return iface->get_password (provider, service, username, error);
}

/**
 * tracker_password_provider_forget_password:
 * @provider: a TrackerPasswordProvider
 * @service: the name of the remote service associated with @username
 * @error: return location for errors
 *
 * This function calls the password provider's "forget_password"
 * implementation with @service.
 *
 * On failure @error will be set.
 **/
void
tracker_password_provider_forget_password (TrackerPasswordProvider  *provider,
                                           const gchar              *service,
                                           GError                  **error)
{
	TrackerPasswordProviderIface *iface;

	g_return_if_fail (TRACKER_IS_PASSWORD_PROVIDER (provider));
	g_return_if_fail (service != NULL);

	iface = TRACKER_PASSWORD_PROVIDER_GET_INTERFACE (provider);

	if (!iface || !iface->forget_password) {
		return;
	}

	iface->forget_password (provider, service, error);
}

/**
 * tracker_password_provider_lock_password:
 * @password: a string pointer
 *
 * This function calls mlock() to secure a memory region newly
 * allocated and @password is copied using memcpy() into the new
 * address.
 * 
 * Password can not be %NULL or an empty string ("").
 *
 * Returns: a newly allocated string which <emphasis>MUST</emphasis>
 * be freed with tracker_password_provider_unlock_password(). On
 * failure %NULL is returned.
 **/
gchar *
tracker_password_provider_lock_password (const gchar *password)
{
	gchar *dest;
	int retval;

	g_return_val_if_fail (password != NULL, NULL);
	g_return_val_if_fail (password[0] != '\0', NULL);

	dest = g_malloc0 (strlen (password) + 1);
	retval = mlock (dest, sizeof (dest));

	if (retval != 0) {
		g_free (dest);
		return NULL;
	}

	memcpy (dest, password, strlen (password));

	return dest;
}

/**
 * tracker_password_provider_unlock_password:
 * @password: a string pointer
 *
 * This function calls munlock() on @password which should be a
 * secured memory region. The @password is zeroed first with bzero()
 * and once unlocked it is freed with g_free(). 
 *
 * The @password can not be %NULL or an empty string (""). In
 * addition, @password <emphasis>MUST</emphasis> be a string created
 * with tracker_password_provider_lock_password().
 *
 * Returns: %TRUE if munlock() succeeded, otherwise %FALSE is returned.
 **/
gboolean
tracker_password_provider_unlock_password (gchar *password)
{
	int retval;

	g_return_val_if_fail (password != NULL, FALSE);
	g_return_val_if_fail (password[0] != '\0', FALSE);
	
	bzero (password, strlen (password));
	retval = munlock (password, sizeof (password));
	g_free (password);

	if (retval != 0) {
		/* FIXME: Handle errors? */
		return FALSE;
	}

	return TRUE;
}
