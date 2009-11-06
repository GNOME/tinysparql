/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_PASSWORD_PROVIDER_GNOME, TrackerPasswordProviderGnomePrivate))

#define PASSWORD_PROVIDER_GNOME_NAME "Gnome keyring"

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
	gchar    *name;
};

const GnomeKeyringPasswordSchema password_schema = {
	GNOME_KEYRING_ITEM_GENERIC_SECRET, {
		{ "service", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
		{ "username", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
		{ NULL, 0 }
	}
};

enum {
	PROP_0,
	PROP_NAME
};

GType           tracker_password_provider_gnome_get_type (void) G_GNUC_CONST;

static void     tracker_password_provider_iface_init (TrackerPasswordProviderIface  *iface);
static void     password_provider_gnome_finalize     (GObject                       *object);
static void     password_provider_set_property       (GObject                       *object,
                                                      guint                          prop_id,
                                                      const GValue                  *value,
                                                      GParamSpec                    *pspec);
static void     password_provider_get_property       (GObject                       *object,
                                                      guint                          prop_id,
                                                      GValue                        *value,
                                                      GParamSpec                    *pspec);

static void     password_provider_gnome_store        (TrackerPasswordProvider       *provider,
                                                      const gchar                   *service,
                                                      const gchar                   *description,
                                                      const gchar                   *username,
                                                      const gchar                   *password,
                                                      GError                       **error);
static gchar*   password_provider_gnome_get          (TrackerPasswordProvider       *provider,
                                                      const gchar                   *service,
                                                      gchar                        **username,
                                                      GError                       **error);
static void     password_provider_gnome_forget       (TrackerPasswordProvider       *provider,
                                                      const gchar                   *service,
                                                      GError                       **error);

G_DEFINE_TYPE_WITH_CODE (TrackerPasswordProviderGnome,
                         tracker_password_provider_gnome,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_PASSWORD_PROVIDER,
                                                tracker_password_provider_iface_init))

static void
tracker_password_provider_gnome_class_init (TrackerPasswordProviderGnomeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = password_provider_gnome_finalize;
	object_class->set_property = password_provider_set_property;
	object_class->get_property = password_provider_get_property;

	g_object_class_override_property (object_class,
	                                  PROP_NAME,
	                                  "name");
	g_type_class_add_private (object_class,
	                          sizeof (TrackerPasswordProviderGnomePrivate));
}

static void
tracker_password_provider_gnome_init (TrackerPasswordProviderGnome *provider)
{
}

static void
tracker_password_provider_iface_init (TrackerPasswordProviderIface *iface)
{
	iface->store_password = password_provider_gnome_store;
	iface->get_password = password_provider_gnome_get;
	iface->forget_password = password_provider_gnome_forget;
}

static void
password_provider_gnome_finalize (GObject *object)
{
	G_OBJECT_CLASS (tracker_password_provider_gnome_parent_class)->finalize (object);
}

static void
password_provider_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	TrackerPasswordProviderGnomePrivate *priv = GET_PRIV (object);

	switch (prop_id) {
	case PROP_NAME:
		g_free (priv->name);
		priv->name = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	};
}

static void
password_provider_get_property (GObject      *object,
                                guint         prop_id,
                                GValue       *value,
                                GParamSpec   *pspec)
{
	TrackerPasswordProviderGnomePrivate *priv = GET_PRIV (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	};
}

static void
password_provider_gnome_store (TrackerPasswordProvider  *provider,
                               const gchar              *service,
                               const gchar              *description,
                               const gchar              *username,
                               const gchar              *password,
                               GError                  **error)
{
	GnomeKeyringResult r = gnome_keyring_store_password_sync (&password_schema,
	                                                          NULL,
	                                                          description,
	                                                          password,
	                                                          "service", service,
	                                                          "username", username,
	                                                          NULL);
	if (r != GNOME_KEYRING_RESULT_OK) {
		g_set_error (error,
		             TRACKER_PASSWORD_PROVIDER_ERROR,
		             TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
		             "Cannot store password: %s",
		             gnome_keyring_result_to_message (r));
	}
}

static gchar*
password_provider_gnome_get (TrackerPasswordProvider  *provider,
                             const gchar              *service,
                             gchar                   **username,
                             GError                  **error)
{
	gchar *password;
	GnomeKeyringAttributeList *search_attributes;
	GList *found_items = NULL;
	GnomeKeyringFound *found;
	GnomeKeyringResult r;
	gint i;

	search_attributes = gnome_keyring_attribute_list_new ();
	gnome_keyring_attribute_list_append_string (search_attributes,
	                                            "service", service);

	r = gnome_keyring_find_items_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
	                                   search_attributes,
	                                   &found_items);

	gnome_keyring_attribute_list_free (search_attributes);

	if (r != GNOME_KEYRING_RESULT_OK) {
		if (r == GNOME_KEYRING_RESULT_NO_MATCH) {
			g_set_error_literal (error,
			                     TRACKER_PASSWORD_PROVIDER_ERROR,
			                     TRACKER_PASSWORD_PROVIDER_ERROR_NOTFOUND,
			                     "Password not found");
		} else {
			g_set_error (error,
			             TRACKER_PASSWORD_PROVIDER_ERROR,
			             TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
			             "Keyring error: %s",
			             gnome_keyring_result_to_message (r));
		}

		gnome_keyring_found_list_free (found_items);
		return NULL;
	}

	found = (GnomeKeyringFound *)(found_items->data);

	/* Walk through all attributes and select the ones we're interested in */
	for (i = 0 ; i < found->attributes->len ; ++i) {
		GnomeKeyringAttribute *attr = &gnome_keyring_attribute_list_index (found->attributes, i);
		if (username && !g_strcmp0 (attr->name, "username")) {
			*username = g_strdup (attr->value.string);
		}
	}

	password = tracker_password_provider_strdup_mlock (found->secret);

	gnome_keyring_found_list_free (found_items);

	return password;
}

static void
password_provider_gnome_forget (TrackerPasswordProvider  *provider,
                                const gchar              *service,
                                GError                  **error)
{
	GnomeKeyringResult r = gnome_keyring_delete_password_sync (&password_schema,
	                                                           "service", service,
	                                                           NULL);

	if (r != GNOME_KEYRING_RESULT_OK) {
		g_set_error (error,
		             TRACKER_PASSWORD_PROVIDER_ERROR,
		             TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
		             "Cannot delete password: %s",
		             gnome_keyring_result_to_message (r));
	}
}

const TrackerPasswordProvider*
tracker_password_provider_get (void)
{
	static TrackerPasswordProvider *instance = NULL;
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock (&mutex);
	if (instance == NULL) {
		instance = g_object_new (TRACKER_TYPE_PASSWORD_PROVIDER_GNOME,
		                         "name", PASSWORD_PROVIDER_GNOME_NAME,
		                         NULL);
	}
	g_static_mutex_unlock (&mutex);

	g_assert (instance != NULL);

	return instance;
}

