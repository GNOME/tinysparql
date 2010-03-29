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

#include <glib-object.h>

#include <gnome-keyring.h>

#include "tracker-password-provider.h"

#define TRACKER_TYPE_PASSWORD_PROVIDER_GNOME         (tracker_password_provider_gnome_get_type())
#define TRACKER_PASSWORD_PROVIDER_GNOME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_PASSWORD_PROVIDER_GNOME, TrackerPasswordProviderGnome))
#define TRACKER_PASSWORD_PROVIDER_GNOME_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_PASSWORD_PROVIDER_GNOME, TrackerPasswordProviderGnomeClass))
#define TRACKER_IS_PASSWORD_PROVIDER_GNOME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_PASSWORD_PROVIDER_GNOME))
#define TRACKER_IS_PASSWORD_PROVIDER_GNOME_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_PASSWORD_PROVIDER_GNOME))
#define TRACKER_PASSWORD_PROVIDER_GNOME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_PASSWORD_PROVIDER_GNOME, TrackerPasswordProviderGnomeClass))

#define TRACKER_PASSWORD_PROVIDER_GNOME_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_PASSWORD_PROVIDER_GNOME, TrackerPasswordProviderGnomePrivate))

#define PASSWORD_PROVIDER_GNOME_NAME "GNOME Keyring"

typedef struct TrackerPasswordProviderGnome TrackerPasswordProviderGnome;
typedef struct TrackerPasswordProviderGnomeClass TrackerPasswordProviderGnomeClass;
typedef struct TrackerPasswordProviderGnomePrivate TrackerPasswordProviderGnomePrivate;

struct TrackerPasswordProviderGnome {
	GObject parent;
};

struct TrackerPasswordProviderGnomeClass {
	GObjectClass parent_class;
};

struct TrackerPasswordProviderGnomePrivate {
	gchar *name;
};

GType           tracker_password_provider_gnome_get_type (void) G_GNUC_CONST;
static void     tracker_password_provider_iface_init     (TrackerPasswordProviderIface  *iface);
static void     password_provider_set_property           (GObject                       *object,
                                                          guint                          prop_id,
                                                          const GValue                  *value,
                                                          GParamSpec                    *pspec);
static void     password_provider_get_property           (GObject                       *object,
                                                          guint                          prop_id,
                                                          GValue                        *value,
                                                          GParamSpec                    *pspec);
static gboolean password_provider_gnome_store            (TrackerPasswordProvider       *provider,
                                                          const gchar                   *service,
                                                          const gchar                   *description,
                                                          const gchar                   *username,
                                                          const gchar                   *password,
                                                          GError                       **error);
static gchar*   password_provider_gnome_get              (TrackerPasswordProvider       *provider,
                                                          const gchar                   *service,
                                                          gchar                        **username,
                                                          GError                       **error);
static gboolean password_provider_gnome_forget           (TrackerPasswordProvider       *provider,
                                                          const gchar                   *service,
                                                          GError                       **error);

const GnomeKeyringPasswordSchema password_schema = {
	GNOME_KEYRING_ITEM_GENERIC_SECRET,
	{ { "service",  GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
	  { "username", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
	  { NULL, 0 }
	}
};

enum {
	PROP_0,
	PROP_NAME
};

G_DEFINE_TYPE_WITH_CODE (TrackerPasswordProviderGnome,
                         tracker_password_provider_gnome,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_PASSWORD_PROVIDER,
                                                tracker_password_provider_iface_init))

static void
tracker_password_provider_gnome_class_init (TrackerPasswordProviderGnomeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = password_provider_set_property;
	object_class->get_property = password_provider_get_property;

	g_object_class_override_property (object_class, PROP_NAME, "name");

	g_type_class_add_private (object_class, sizeof (TrackerPasswordProviderGnomePrivate));
}

static void
tracker_password_provider_gnome_init (TrackerPasswordProviderGnome *provider)
{
}

static void
tracker_password_provider_iface_init (TrackerPasswordProviderIface *iface)
{
	iface->store_password  = password_provider_gnome_store;
	iface->get_password    = password_provider_gnome_get;
	iface->forget_password = password_provider_gnome_forget;
}

static void
password_provider_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	TrackerPasswordProviderGnomePrivate *priv;

	priv = TRACKER_PASSWORD_PROVIDER_GNOME_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_NAME:
		g_free (priv->name);
		priv->name = g_value_dup_string (value);
		g_object_notify (object, "name");
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	};
}

static void
password_provider_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	TrackerPasswordProviderGnomePrivate *priv;

	priv = TRACKER_PASSWORD_PROVIDER_GNOME_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	};
}

static gboolean
password_provider_gnome_store (TrackerPasswordProvider  *provider,
                               const gchar              *service,
                               const gchar              *description,
                               const gchar              *username,
                               const gchar              *password,
                               GError                  **error)
{
	GnomeKeyringResult result;

	result = gnome_keyring_store_password_sync (&password_schema,
	                                            NULL,
	                                            description,
	                                            password,
	                                            "service", service,
	                                            "username", username,
	                                            NULL);

	if (result != GNOME_KEYRING_RESULT_OK) {
		g_set_error (error,
		             TRACKER_PASSWORD_PROVIDER_ERROR,
		             TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
		             "Could not store GNOME keyring password, %s",
		             gnome_keyring_result_to_message (result));

		return FALSE;
	}

	return TRUE;
}

static gchar *
password_provider_gnome_get (TrackerPasswordProvider  *provider,
                             const gchar              *service,
                             gchar                   **username,
                             GError                  **error)
{
	GnomeKeyringAttributeList *search_attributes;
	GnomeKeyringFound *found;
	GnomeKeyringResult result;
	GList *found_items = NULL;
	gchar *password;
	gint i;

	search_attributes = gnome_keyring_attribute_list_new ();
	gnome_keyring_attribute_list_append_string (search_attributes,
	                                            "service",
	                                            service);

	result = gnome_keyring_find_items_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
	                                        search_attributes,
	                                        &found_items);

	gnome_keyring_attribute_list_free (search_attributes);

	if (result != GNOME_KEYRING_RESULT_OK) {
		if (result == GNOME_KEYRING_RESULT_NO_MATCH) {
			g_set_error_literal (error,
			                     TRACKER_PASSWORD_PROVIDER_ERROR,
			                     TRACKER_PASSWORD_PROVIDER_ERROR_NOTFOUND,
			                     "Could not find GNOME keyring password");
		} else {
			g_set_error (error,
			             TRACKER_PASSWORD_PROVIDER_ERROR,
			             TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
			             "Could not fetch GNOME keyring password, %s",
			             gnome_keyring_result_to_message (result));
		}

		gnome_keyring_found_list_free (found_items);

		return NULL;
	}

	found = found_items->data;

	/* Find username if we asked for it */
	if (username) {
		/* Make sure it is always set */
		*username = NULL;

		for (i = 0; i < found->attributes->len; ++i) {
			GnomeKeyringAttribute *attr;

			attr = &gnome_keyring_attribute_list_index (found->attributes, i);

			if (g_ascii_strcasecmp (attr->name, "username") == 0) {
				*username = g_strdup (attr->value.string);
			}
		}
	}

	password = tracker_password_provider_lock_password (found->secret);

	gnome_keyring_found_list_free (found_items);

	return password;
}

static gboolean
password_provider_gnome_forget (TrackerPasswordProvider  *provider,
                                const gchar              *service,
                                GError                  **error)
{
	GnomeKeyringResult result;

	result = gnome_keyring_delete_password_sync (&password_schema,
	                                             "service", service,
	                                             NULL);

	if (result != GNOME_KEYRING_RESULT_OK) {
		g_set_error (error,
		             TRACKER_PASSWORD_PROVIDER_ERROR,
		             TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
		             "Coult not delete GNOME keyring password, %s",
		             gnome_keyring_result_to_message (result));
		return FALSE;
	}

	return TRUE;
}

TrackerPasswordProvider *
tracker_password_provider_get (void)
{
	static TrackerPasswordProvider *instance = NULL;
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock (&mutex);

	if (!instance) {
		instance = g_object_new (TRACKER_TYPE_PASSWORD_PROVIDER_GNOME,
		                         "name", PASSWORD_PROVIDER_GNOME_NAME,
		                         NULL);
	}

	g_static_mutex_unlock (&mutex);

	return instance;
}

