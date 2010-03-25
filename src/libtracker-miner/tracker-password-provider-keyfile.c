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
#include <gio/gio.h>

#include "tracker-password-provider.h"

#define TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE         (tracker_password_provider_keyfile_get_type())
#define TRACKER_PASSWORD_PROVIDER_KEYFILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE, TrackerPasswordProviderKeyfile))
#define TRACKER_PASSWORD_PROVIDER_KEYFILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE, TrackerPasswordProviderKeyfileClass))
#define TRACKER_IS_PASSWORD_PROVIDER_KEYFILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE))
#define TRACKER_IS_PASSWORD_PROVIDER_KEYFILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE))
#define TRACKER_PASSWORD_PROVIDER_KEYFILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE, TrackerPasswordProviderKeyfileClass))

#define TRACKER_PASSWORD_PROVIDER_KEYFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE, TrackerPasswordProviderKeyfilePrivate))

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

GType           tracker_password_provider_keyfile_get_type (void) G_GNUC_CONST;

static void     tracker_password_provider_iface_init       (TrackerPasswordProviderIface    *iface);
static void     password_provider_keyfile_constructed      (GObject                         *object);
static void     password_provider_set_property             (GObject                         *object,
                                                            guint                            prop_id,
                                                            const GValue                    *value,
                                                            GParamSpec                      *pspec);
static void     password_provider_get_property             (GObject                         *object,
                                                            guint                            prop_id,
                                                            GValue                          *value,
                                                            GParamSpec                      *pspec);

static gboolean password_provider_keyfile_store            (TrackerPasswordProvider         *provider,
                                                            const gchar                     *service,
                                                            const gchar                     *description,
                                                            const gchar                     *username,
                                                            const gchar                     *password,
                                                            GError                         **error);
static gchar *  password_provider_keyfile_get              (TrackerPasswordProvider         *provider,
                                                            const gchar                     *service,
                                                            gchar                          **username,
                                                            GError                         **error);
static gboolean password_provider_keyfile_forget           (TrackerPasswordProvider         *provider,
                                                            const gchar                     *service,
                                                            GError                         **error);
static void     load_password_file                         (TrackerPasswordProviderKeyfile  *kf,
                                                            GError                         **error);
static gboolean save_password_file                         (TrackerPasswordProviderKeyfile  *kf,
                                                            GError                         **error);

enum {
	PROP_0,
	PROP_NAME
};

G_DEFINE_TYPE_WITH_CODE (TrackerPasswordProviderKeyfile,
                         tracker_password_provider_keyfile,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TRACKER_TYPE_PASSWORD_PROVIDER,
                                                tracker_password_provider_iface_init))

static void
tracker_password_provider_keyfile_class_init (TrackerPasswordProviderKeyfileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructed  = password_provider_keyfile_constructed;
	object_class->set_property = password_provider_set_property;
	object_class->get_property = password_provider_get_property;

	g_object_class_override_property (object_class, PROP_NAME, "name");

	g_type_class_add_private (object_class, sizeof (TrackerPasswordProviderKeyfilePrivate));
}

static void
tracker_password_provider_keyfile_init (TrackerPasswordProviderKeyfile *provider)
{
}

static void
tracker_password_provider_iface_init (TrackerPasswordProviderIface *iface)
{
	iface->store_password  = password_provider_keyfile_store;
	iface->get_password    = password_provider_keyfile_get;
	iface->forget_password = password_provider_keyfile_forget;
}

static void
password_provider_keyfile_constructed (GObject *object)
{
	TrackerPasswordProviderKeyfile *kf;
	TrackerPasswordProviderKeyfilePrivate *priv;
	GError *error = NULL;

	kf = TRACKER_PASSWORD_PROVIDER_KEYFILE (object);
	priv = TRACKER_PASSWORD_PROVIDER_KEYFILE_GET_PRIVATE (kf);

	priv->password_file = g_key_file_new ();

	load_password_file (kf, &error);

	if (error) {
		g_critical ("Could not load GKeyFile password file, %s",
		            error->message);
		g_error_free (error);
	}
}

static void
password_provider_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	TrackerPasswordProviderKeyfilePrivate *priv;

	priv = TRACKER_PASSWORD_PROVIDER_KEYFILE_GET_PRIVATE (object);

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
	TrackerPasswordProviderKeyfilePrivate *priv;

	priv = TRACKER_PASSWORD_PROVIDER_KEYFILE_GET_PRIVATE (object);

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
password_provider_keyfile_store (TrackerPasswordProvider  *provider,
                                 const gchar              *service,
                                 const gchar              *description,
                                 const gchar              *username,
                                 const gchar              *password,
                                 GError                  **error)
{
	TrackerPasswordProviderKeyfile *kf;
	TrackerPasswordProviderKeyfilePrivate *priv;

	kf = TRACKER_PASSWORD_PROVIDER_KEYFILE (provider);
	priv = TRACKER_PASSWORD_PROVIDER_KEYFILE_GET_PRIVATE (kf);

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

	return save_password_file (kf, error);
}

static gchar *
password_provider_keyfile_get (TrackerPasswordProvider  *provider,
                               const gchar              *service,
                               gchar                   **username,
                               GError                  **error)
{
	TrackerPasswordProviderKeyfilePrivate *priv;
	gchar *password;
	GError *local_error = NULL;

	priv = TRACKER_PASSWORD_PROVIDER_KEYFILE_GET_PRIVATE (provider);

	password = g_key_file_get_string (priv->password_file,
	                                  service,
	                                  "password",
	                                  &local_error);

	if (local_error) {
		g_set_error_literal (error,
		                     TRACKER_PASSWORD_PROVIDER_ERROR,
		                     TRACKER_PASSWORD_PROVIDER_ERROR_NOTFOUND,
		                     "Could not get GKeyFile password, it was not found");
		g_error_free (local_error);
		g_free (password);

		return NULL;
	}

	if (username) {
		*username = g_key_file_get_string (priv->password_file,
		                                   service,
		                                   "username",
		                                   &local_error);

		if (local_error) {
			g_set_error_literal (error,
			                     TRACKER_PASSWORD_PROVIDER_ERROR,
			                     TRACKER_PASSWORD_PROVIDER_ERROR_NOTFOUND,
			                     "Could not get GKeyFile password, it was not found");
			g_error_free (local_error);
			g_free (password);
			return NULL;
		}
	}

	mlock (password, sizeof (password));

	return password;
}

static gboolean
password_provider_keyfile_forget (TrackerPasswordProvider  *provider,
                                  const gchar              *service,
                                  GError                  **error)
{
	TrackerPasswordProviderKeyfilePrivate *priv;
	GError *local_error = NULL;

	priv = TRACKER_PASSWORD_PROVIDER_KEYFILE_GET_PRIVATE (provider);

	if (!g_key_file_remove_group (priv->password_file, service, &local_error)) {
		g_warning ("Could not remove GKeyFile group '%s' from password file, %s",
		           service,
		           local_error->message);
		g_error_free (local_error);

		g_set_error_literal (error,
		                     TRACKER_PASSWORD_PROVIDER_ERROR,
		                     TRACKER_PASSWORD_PROVIDER_ERROR_NOTFOUND,
		                     "Could not find service for GKeyFile password");

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

	if (instance == NULL) {
		instance = g_object_new (TRACKER_TYPE_PASSWORD_PROVIDER_KEYFILE,
		                         "name", PASSWORD_PROVIDER_KEYFILE_NAME,
		                         NULL);
	}

	g_static_mutex_unlock (&mutex);

	return instance;
}

/* Copied from tracker-config-file.c */
static gchar *
config_dir_ensure_exists_and_return (GError **error)
{
	gchar *directory;

	directory = g_build_filename (g_get_user_config_dir (), "tracker", NULL);

	if (!g_file_test (directory, G_FILE_TEST_EXISTS)) {
		if (g_mkdir_with_parents (directory, 0700) == -1) {
			if (error) {
				*error = g_error_new (TRACKER_PASSWORD_PROVIDER_ERROR,
				                      TRACKER_PASSWORD_PROVIDER_ERROR_SERVICE,
				                      "Could not create directory '%s'",
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
	TrackerPasswordProviderKeyfilePrivate *priv;
	gchar *filename;
	gchar *directory;

	directory = config_dir_ensure_exists_and_return (error);
	if (!directory) {
		return;
	}

	filename = g_build_filename (directory, KEYFILE_FILENAME, NULL);
	g_free (directory);

	priv = TRACKER_PASSWORD_PROVIDER_KEYFILE_GET_PRIVATE (kf);

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		g_key_file_load_from_file (priv->password_file,
		                           filename,
		                           G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
		                           error);
	}

	g_free (filename);
}

static gboolean
save_password_file (TrackerPasswordProviderKeyfile  *kf,
                    GError                         **error)
{
	TrackerPasswordProviderKeyfilePrivate *priv;
	gchar *filename;
	gchar *directory;
	gchar *data;
	gsize size;

	directory = config_dir_ensure_exists_and_return (error);
	if (!directory) {
		return FALSE;
	}

	filename = g_build_filename (directory, KEYFILE_FILENAME, NULL);
	g_free (directory);

	priv = TRACKER_PASSWORD_PROVIDER_KEYFILE_GET_PRIVATE (kf);

	data = g_key_file_to_data (priv->password_file, &size, NULL);

	g_file_set_contents (filename, data, size, error);
	g_free (data);
	g_free (filename);

	return *error == NULL ? TRUE : FALSE;
}
