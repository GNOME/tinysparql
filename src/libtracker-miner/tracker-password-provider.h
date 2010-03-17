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

#ifndef __LIBTRACKER_MINER_PASSWORD_PROVIDER_H__
#define __LIBTRACKER_MINER_PASSWORD_PROVIDER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_PASSWORD_PROVIDER         (tracker_password_provider_get_type())
#define TRACKER_PASSWORD_PROVIDER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_PASSWORD_PROVIDER, TrackerPasswordProvider))
#define TRACKER_IS_PASSWORD_PROVIDER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_PASSWORD_PROVIDER))
#define TRACKER_PASSWORD_PROVIDER_GET_INTERFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o),  TRACKER_TYPE_PASSWORD_PROVIDER, TrackerPasswordProviderIface))

#define TRACKER_PASSWORD_PROVIDER_ERROR_DOMAIN  "TrackerPasswordProvider"
#define TRACKER_PASSWORD_PROVIDER_ERROR         tracker_password_provider_error_quark()

typedef struct TrackerPasswordProvider TrackerPasswordProvider;
typedef struct TrackerPasswordProviderIface TrackerPasswordProviderIface;

typedef enum {
	TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
	TRACKER_PASSWORD_PROVIDER_ERROR_NOTFOUND
} TrackerPasswordProviderError;

struct TrackerPasswordProviderIface
{
	GTypeInterface parent_iface;

	void     (* store_password)        (TrackerPasswordProvider  *provider,
	                                    const gchar              *service,
	                                    const gchar              *description,
	                                    const gchar              *username,
	                                    const gchar              *password,
	                                    GError                  **error);
	gchar*   (* get_password)          (TrackerPasswordProvider  *provider,
	                                    const gchar              *service,
	                                    gchar                   **username,
	                                    GError                  **error);
	void     (* forget_password)       (TrackerPasswordProvider  *provider,
	                                    const gchar              *service,
	                                    GError                  **error);
};

GType  tracker_password_provider_get_type        (void) G_GNUC_CONST;
GQuark tracker_password_provider_error_quark     (void);

gchar* tracker_password_provider_get_name        (TrackerPasswordProvider   *provider);

/* Must be defined by the selected implementation */
TrackerPasswordProvider*
       tracker_password_provider_get             (void);
void   tracker_password_provider_store_password  (TrackerPasswordProvider   *provider,
                                                  const gchar              *service,
                                                  const gchar              *description,
                                                  const gchar              *username,
                                                  const gchar              *password,
                                                  GError                  **error);

gchar* tracker_password_provider_get_password    (TrackerPasswordProvider   *provider,
                                                  const gchar              *service,
                                                  gchar                   **username,
                                                  GError                  **error);
void   tracker_password_provider_forget_password (TrackerPasswordProvider   *provider,
                                                  const gchar              *service,
                                                  GError                  **error);

gchar* tracker_password_provider_strdup_mlock    (const gchar              *source);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_PASSWORD_PROVIDER_H__ */
