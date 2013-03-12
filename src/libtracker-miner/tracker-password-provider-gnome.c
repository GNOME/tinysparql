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

#include <libsecret/secret.h>

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
static const SecretSchema password_schema = {
	"org.gnome.Tracker.Miner",
	SECRET_SCHEMA_DONT_MATCH_NAME,
	{ { "service",  SECRET_SCHEMA_ATTRIBUTE_STRING },
	  { "username", SECRET_SCHEMA_ATTRIBUTE_STRING },
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
	GError *secret_error = NULL;

	secret_password_store_sync (&password_schema, NULL, description,
	                            password, NULL, &secret_error,
	                            "service", service,
	                            "username", username,
	                            NULL);

	if (secret_error != NULL) {
		g_set_error (error,
		             TRACKER_PASSWORD_PROVIDER_ERROR,
		             TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
		             "Could not store GNOME keyring password, %s",
		             secret_error->message);
		g_error_free (secret_error);
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
	GList *found_items = NULL;
	GHashTable *search_attributes;
	GHashTable *attributes;
	GError *secret_error = NULL;
	SecretValue *secret = NULL;
	SecretItem *found = NULL;
	gchar *password;

	search_attributes = secret_attributes_build (&password_schema,
	                                             "service", service,
	                                             NULL);

	found_items = secret_service_search_sync (NULL,
	                                          &password_schema,
	                                          search_attributes,
	                                          SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
	                                          NULL,
	                                          &secret_error);

	g_hash_table_unref (search_attributes);

	if (secret_error != NULL) {
		g_set_error (error,
		             TRACKER_PASSWORD_PROVIDER_ERROR,
		             TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
		             "Could not fetch GNOME keyring password, %s",
		             secret_error->message);
		g_error_free (secret_error);
		return NULL;
	}

	if (found_items != NULL) {
		found = found_items->data;
		secret = secret_item_get_secret (found);
	}

	if (secret == NULL) {
		g_set_error_literal (error,
		                     TRACKER_PASSWORD_PROVIDER_ERROR,
		                     TRACKER_PASSWORD_PROVIDER_ERROR_NOTFOUND,
		                     "Could not find GNOME keyring password");
		g_list_free_full (found_items, g_object_unref);
		return NULL;
	}

	/* Find username if we asked for it */
	if (username) {
		/* Make sure it is always set */
		attributes = secret_item_get_attributes (found);
		*username = g_strdup (g_hash_table_lookup (attributes, "username"));
		g_hash_table_unref (attributes);
	}

	password = tracker_password_provider_lock_password (secret_value_get (secret, NULL));
	secret_value_unref (secret);
	g_list_free_full (found_items, g_object_unref);

	return password;
}

static gboolean
password_provider_gnome_forget (TrackerPasswordProvider  *provider,
                                const gchar              *service,
                                GError                  **error)
{
	GError *secret_error = NULL;

	secret_password_clear_sync (&password_schema,
	                            NULL,
	                            &secret_error,
	                            "service", service,
	                            NULL);

	if (secret_error != NULL) {
		g_set_error (error,
		             TRACKER_PASSWORD_PROVIDER_ERROR,
		             TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
		             "Coult not delete GNOME keyring password, %s",
		             secret_error->message);
		g_error_free (secret_error);
		return FALSE;
	}

	return TRUE;
}

TrackerPasswordProvider *
tracker_password_provider_get (void)
{
	static TrackerPasswordProvider *instance = NULL;
	static GMutex mutex;

	g_mutex_lock (&mutex);

	if (!instance) {
		instance = g_object_new (TRACKER_TYPE_PASSWORD_PROVIDER_GNOME,
		                         "name", PASSWORD_PROVIDER_GNOME_NAME,
		                         NULL);
	}

	g_mutex_unlock (&mutex);

	return instance;
}
