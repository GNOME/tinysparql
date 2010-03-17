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

#include "config.h"

#include <sys/mman.h>

#include <glib-object.h>

#include "tracker-password-provider.h"

#define TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE         (tracker_password_provider_keyfile_get_type())
#define TRACKER_PASSWORD_PROVIDER_KEYFILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE, TrackerPasswordProviderKeyfile))
#define TRACKER_PASSWORD_PROVIDER_KEYFILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE, TrackerPasswordProviderKeyfileClass))
#define TRACKER_IS_PASSWORD_PROVIDER_KEYFILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE))
#define TRACKER_IS_PASSWORD_PROVIDER_KEYFILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE))
#define TRACKER_PASSWORD_PROVIDER_KEYFILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE, TrackerPasswordProviderKeyfileClass))

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE, TrackerPasswordProviderKeyfilePrivate))

#define PASSWORD_PROVIDER_KEYFILE_NAME "KeyFile"

/* GKeyFile settings */
#define KEYFILE_FILENAME "passwords.cfg"
#define GROUP_GENERAL    "General"

typedef struct TrackerPasswordProviderKeyfile TrackerPasswordProviderKeyfile;
typedef struct TrackerPasswordProviderKeyfileClass TrackerPasswordProviderKeyfileClass;
typedef struct TrackerPasswordProviderKeyfilePrivate TrackerPasswordProviderKeyfilePrivate;

struct TrackerPasswordProviderKeyfile {
	GObject parent;
};

struct TrackerPasswordProviderKeyfileClass {
	GObjectClass parent_class;
};

struct TrackerPasswordProviderKeyfilePrivate {
	gchar    *name;
	GKeyFile *password_file;
};

enum {
	PROP_0,
	PROP_NAME
};

GType           tracker_password_provider_keyfile_get_type (void) G_GNUC_CONST;

static void     tracker_password_provider_iface_init       (TrackerPasswordProviderIface    *iface);
static void     password_provider_keyfile_constructed      (GObject                         *object);
static void     password_provider_keyfile_finalize         (GObject                         *object);
static void     password_provider_set_property             (GObject                         *object,
                                                            guint                            prop_id,
                                                            const GValue                    *value,
                                                            GParamSpec                      *pspec);
static void     password_provider_get_property             (GObject                         *object,
                                                            guint                            prop_id,
                                                            GValue                          *value,
                                                            GParamSpec                      *pspec);

void            password_provider_keyfile_store            (TrackerPasswordProvider         *provider,
                                                            const gchar                     *service,
                                                            const gchar                     *description,
                                                            const gchar                     *username,
                                                            const gchar                     *password,
                                                            GError                         **error);
gchar*          password_provider_keyfile_get              (TrackerPasswordProvider         *provider,
                                                            const gchar                     *service,
                                                            gchar                          **username,
                                                            GError                         **error);
void            password_provider_keyfile_forget           (TrackerPasswordProvider         *provider,
                                                            const gchar                     *service,
                                                            GError                         **error);

static void     load_password_file                         (TrackerPasswordProviderKeyfile  *kf,
                                                            GError                         **error);
static gboolean save_password_file                         (TrackerPasswordProviderKeyfile  *kf,
                                                            GError                         **error);

G_DEFINE_TYPE_WITH_CODE (TrackerPasswordProviderKeyfile,
                         tracker_password_provider_keyfile,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_PASSWORD_PROVIDER,
                                                tracker_password_provider_iface_init))

static void
tracker_password_provider_keyfile_class_init (TrackerPasswordProviderKeyfileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = password_provider_keyfile_finalize;
	object_class->constructed  = password_provider_keyfile_constructed;
	object_class->set_property = password_provider_set_property;
	object_class->get_property = password_provider_get_property;

	g_object_class_override_property (object_class,
	                                  PROP_NAME,
	                                  "name");

	g_type_class_add_private (object_class, sizeof (TrackerPasswordProviderKeyfilePrivate));
}

static void
tracker_password_provider_keyfile_init (TrackerPasswordProviderKeyfile *provider)
{
}

static void
tracker_password_provider_iface_init (TrackerPasswordProviderIface *iface)
{
	iface->store_password = password_provider_keyfile_store;
	iface->get_password = password_provider_keyfile_get;
	iface->forget_password = password_provider_keyfile_forget;
}

static void
password_provider_keyfile_constructed (GObject *object)
{
	TrackerPasswordProviderKeyfile *kf = TRACKER_PASSWORD_PROVIDER_KEYFILE (object);
	TrackerPasswordProviderKeyfilePrivate *priv = GET_PRIV (object);
	GError *error = NULL;

	priv->password_file = g_key_file_new ();

	load_password_file (kf, &error);

	if (error) {
		g_critical ("Cannot load password file: %s", error->message);
		g_error_free (error);
	}
}

static void
password_provider_keyfile_finalize (GObject *object)
{
	G_OBJECT_CLASS (tracker_password_provider_keyfile_parent_class)->finalize (object);
}

static void
password_provider_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	TrackerPasswordProviderKeyfilePrivate *priv = GET_PRIV (object);

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
	TrackerPasswordProviderKeyfilePrivate *priv = GET_PRIV (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	};
}

void
password_provider_keyfile_store (TrackerPasswordProvider  *provider,
                                 const gchar              *service,
                                 const gchar              *description,
                                 const gchar              *username,
                                 const gchar              *password,
                                 GError                  **error)
{
	TrackerPasswordProviderKeyfile *kf = TRACKER_PASSWORD_PROVIDER_KEYFILE (provider);
	TrackerPasswordProviderKeyfilePrivate *priv = GET_PRIV (provider);
	GError *local_error = NULL;

	g_key_file_set_string (priv->password_file,
	                       service,
	                       "description",
	                       description);
	g_key_file_set_string (priv->password_file,
	                       service,
	                       "username",
	                       username);
	g_key_file_set_string (priv->password_file,
	                       service,
	                       "password",
	                       password);

	if (!save_password_file (kf, &local_error)) {
		g_propagate_error (error, local_error);
	}
}

gchar*
password_provider_keyfile_get (TrackerPasswordProvider  *provider,
                               const gchar              *service,
                               gchar                   **username,
                               GError                  **error)
{
	TrackerPasswordProviderKeyfilePrivate *priv = GET_PRIV (provider);
	gchar *password = NULL;
	GError *error_password = NULL;
	GError *error_username = NULL;

	password = g_key_file_get_string (priv->password_file,
	                                  service,
	                                  "password",
	                                  &error_password);

	mlock (password, sizeof (password));

	if (username) {
		*username = g_key_file_get_string (priv->password_file,
		                                   service,
		                                   "username",
		                                   &error_username);
	}

	if (error_password || error_username) {
		g_set_error_literal (error,
		                     TRACKER_PASSWORD_PROVIDER_ERROR,
		                     TRACKER_PASSWORD_PROVIDER_ERROR_NOTFOUND,
		                     "Password not found");
	}

	if (error_password)
		g_error_free (error_password);
	if (error_username)
		g_error_free (error_username);

	return password;
}

void
password_provider_keyfile_forget (TrackerPasswordProvider  *provider,
                                  const gchar              *service,
                                  GError                  **error)
{
	TrackerPasswordProviderKeyfilePrivate *priv = GET_PRIV (provider);

	GError *e = NULL;

	if (!g_key_file_remove_group (priv->password_file, service, &e)) {
		g_warning ("Cannot remove group '%s' from password file: %s",
		           service,
		           e->message);
		g_error_free (e);

		g_set_error_literal (error,
		                     TRACKER_PASSWORD_PROVIDER_ERROR,
		                     TRACKER_PASSWORD_PROVIDER_ERROR_NOTFOUND,
		                     "Service not found");
	}
}

const TrackerPasswordProvider*
tracker_password_provider_get (void)
{
	static TrackerPasswordProvider *instance = NULL;
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock (&mutex);
	if (instance == NULL) {
		instance = g_object_new (TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE,
		                         "name", PASSWORD_PROVIDER_KEYFILE_NAME,
		                         NULL);
	}
	g_static_mutex_unlock (&mutex);

	g_assert (instance != NULL);

	return instance;
}

/* Copied from tracker-config-file.c */
static gchar *
config_dir_ensure_exists_and_return (GError **error)
{
	gchar *directory;

	directory = g_build_filename (g_get_user_config_dir (),
	                              "tracker",
	                              NULL);

	if (!g_file_test (directory, G_FILE_TEST_EXISTS)) {
		g_print ("Creating config directory:'%s'\n", directory);

		if (g_mkdir_with_parents (directory, 0700) == -1) {
			if (error) {
				*error = g_error_new (TRACKER_PASSWORD_PROVIDER_ERROR,
				                      TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
				                      "Impossible to create directory: '%s'",
				                      directory);
			}
			g_free (directory);
			return NULL;
		}
	}

	return directory;
}

static void
load_password_file (TrackerPasswordProviderKeyfile  *kf,
                    GError                         **error)
{
	TrackerPasswordProviderKeyfilePrivate *priv = GET_PRIV (kf);
	gchar *filename;
	gchar *directory;
	GError *local_error = NULL;

	directory = config_dir_ensure_exists_and_return (&local_error);
	if (!directory) {
		g_propagate_error (error, local_error);
		return;
	}

	filename = g_build_filename (directory, KEYFILE_FILENAME, NULL);
	g_free (directory);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		return;
	}

	g_key_file_load_from_file (priv->password_file,
	                           filename,
	                           G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
	                           &local_error);

	if (local_error) {
		g_propagate_error (error, local_error);
	}
}

static gboolean
save_password_file (TrackerPasswordProviderKeyfile  *kf,
                    GError                         **error)
{
	TrackerPasswordProviderKeyfilePrivate *priv = GET_PRIV (kf);
	gchar *filename;
	gchar *directory;
	gchar *data;
	gsize size;
	GError *local_error = NULL;

	directory = config_dir_ensure_exists_and_return (&local_error);
	if (!directory) {
		g_propagate_error (error, local_error);
		return FALSE;
	}

	filename = g_build_filename (directory, KEYFILE_FILENAME, NULL);
	g_free (directory);

	data = g_key_file_to_data (priv->password_file, &size, NULL);

	g_file_set_contents (filename, data, size, &local_error);
	g_free (data);
	g_free (filename);

	if (local_error) {
		g_propagate_error (error, local_error);
		return FALSE;
	}

	return TRUE;
}
