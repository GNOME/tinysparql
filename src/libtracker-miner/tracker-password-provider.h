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

#ifndef __LIBTRACKER_MINER_PASSWORD_PROVIDER_H__
#define __LIBTRACKER_MINER_PASSWORD_PROVIDER_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_PASSWORD_PROVIDER             (tracker_password_provider_get_type())
#define TRACKER_PASSWORD_PROVIDER(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_PASSWORD_PROVIDER, TrackerPasswordProvider))
#define TRACKER_IS_PASSWORD_PROVIDER(o)            (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_PASSWORD_PROVIDER))
#define TRACKER_PASSWORD_PROVIDER_GET_INTERFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o),  TRACKER_TYPE_PASSWORD_PROVIDER, TrackerPasswordProviderIface))

#define TRACKER_PASSWORD_PROVIDER_ERROR_DOMAIN  "TrackerPasswordProvider"
#define TRACKER_PASSWORD_PROVIDER_ERROR         tracker_password_provider_error_quark()

typedef struct TrackerPasswordProvider TrackerPasswordProvider;
typedef struct TrackerPasswordProviderIface TrackerPasswordProviderIface;

/**
 * TrackerPasswordProviderError:
 * @TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE: An internal error
 * occurred which meant the operation failed.
 * @TRACKER_PASSWORD_PROVIDER_ERROR_NOTFOUND: No password provider was
 * found to store/retrieve the remote service's authentication
 * credentials
 *
 * The following errors are possible during any of the performed
 * actions with a password provider.
 *
 * Since: 0.8
 **/
typedef enum {
	TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
	TRACKER_PASSWORD_PROVIDER_ERROR_NOTFOUND
} TrackerPasswordProviderError;

/**
 * TrackerPasswordProviderIface
 * @parent_iface: parent object interface
 * @store_password: save the service, username and password
 * @get_password: get a password for a given service
 * @forget_password: forget any password associated with a given
 * service
 *
 * Since: 0.8.
 **/
struct TrackerPasswordProviderIface {
	GTypeInterface parent_iface;

	gboolean (* store_password)  (TrackerPasswordProvider  *provider,
	                              const gchar              *service,
	                              const gchar              *description,
	                              const gchar              *username,
	                              const gchar              *password,
	                              GError                  **error);
	gchar *  (* get_password)    (TrackerPasswordProvider  *provider,
	                              const gchar              *service,
	                              gchar                   **username,
	                              GError                  **error);
	gboolean (* forget_password) (TrackerPasswordProvider  *provider,
	                              const gchar              *service,
	                              GError                  **error);
};

GType    tracker_password_provider_get_type         (void) G_GNUC_CONST;
GQuark   tracker_password_provider_error_quark      (void);

gchar *  tracker_password_provider_get_name         (TrackerPasswordProvider  *provider);
gboolean tracker_password_provider_store_password   (TrackerPasswordProvider  *provider,
                                                     const gchar              *service,
                                                     const gchar              *description,
                                                     const gchar              *username,
                                                     const gchar              *password,
                                                     GError                  **error);
gchar *  tracker_password_provider_get_password     (TrackerPasswordProvider  *provider,
                                                     const gchar              *service,
                                                     gchar                   **username,
                                                     GError                  **error);
void     tracker_password_provider_forget_password  (TrackerPasswordProvider  *provider,
                                                     const gchar              *service,
                                                     GError                  **error);
gchar *  tracker_password_provider_lock_password    (const gchar              *password);
gboolean tracker_password_provider_unlock_password  (gchar                    *password);

/**
 * tracker_password_provider_get:
 *
 * This function <emphasis>MUST</emphasis> be defined by the
 * implementation of TrackerPasswordProvider.
 *
 * For example, tracker-password-provider-gnome.c should include this
 * function for a GNOME Keyring implementation.
 *
 * Only one implementation can exist at once.
 *
 * Returns: a %TrackerPasswordProvider.
 **/
TrackerPasswordProvider *
         tracker_password_provider_get             (void);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_PASSWORD_PROVIDER_H__ */
