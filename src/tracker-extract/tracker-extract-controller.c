/*
 * Copyright (C) 2014 - Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include "tracker-extract-controller.h"

#include "tracker-main.h"

enum {
	PROP_DECORATOR = 1,
};

struct TrackerExtractControllerPrivate {
	TrackerDecorator *decorator;
	TrackerConfig *config;
	GCancellable *cancellable;
	guint watch_id;
	guint progress_signal_id;
	gint pause_cookie;
};

G_DEFINE_TYPE (TrackerExtractController, tracker_extract_controller, G_TYPE_OBJECT)

static void
files_miner_idleness_changed (TrackerExtractController *self,
                              gboolean                  idle)
{
	if (idle && self->priv->pause_cookie != 0) {
		tracker_miner_resume (TRACKER_MINER (self->priv->decorator),
		                      self->priv->pause_cookie,
		                      NULL);
		self->priv->pause_cookie = 0;
	} else if (!idle && self->priv->pause_cookie == 0) {
		self->priv->pause_cookie =
			tracker_miner_pause (TRACKER_MINER (self->priv->decorator),
			                     "Wait for files miner",
			                     NULL);
	}
}

static void
files_miner_status_changed (TrackerExtractController *self,
                            const gchar              *status)
{
	files_miner_idleness_changed (self, g_str_equal (status, "Idle"));
}

static void
files_miner_get_status_cb (GObject      *source,
                           GAsyncResult *result,
                           gpointer      user_data)
{
	TrackerExtractController *self = user_data;
	GDBusConnection *conn = (GDBusConnection *) source;
	GVariant *reply;
	const gchar *status;
	GError *error = NULL;

	reply = g_dbus_connection_call_finish (conn, result, &error);
	if (!reply) {
		g_debug ("Failed to get tracker-miner-fs status: %s",
		         error->message);
		g_clear_error (&error);
	} else {
		g_variant_get (reply, "(&s)", &status);
		files_miner_status_changed (self, status);
		g_variant_unref (reply);
	}

	g_clear_object (&self->priv->cancellable);
	g_object_unref (self);
}

static void
appeared_cb (GDBusConnection *connection,
             const gchar     *name,
             const gchar     *name_owner,
             gpointer         user_data)
{
	TrackerExtractController *self = user_data;

	/* Get initial status */
	self->priv->cancellable = g_cancellable_new ();
	g_dbus_connection_call (connection,
	                        "org.freedesktop.Tracker1.Miner.Files",
	                        "/org/freedesktop/Tracker1/Miner/Files",
	                        "org.freedesktop.Tracker1.Miner",
	                        "GetStatus",
	                        NULL,
	                        G_VARIANT_TYPE_STRING,
	                        G_DBUS_CALL_FLAGS_NO_AUTO_START,
	                        -1,
	                        self->priv->cancellable,
	                        files_miner_get_status_cb,
	                        g_object_ref (self));
}

static void
vanished_cb (GDBusConnection *connection,
             const gchar     *name,
             gpointer         user_data)
{
	TrackerExtractController *self = user_data;

	/* tracker-miner-fs vanished, we don't have anything to wait for
	 * anymore. */
	files_miner_idleness_changed (self, TRUE);
}

static void
files_miner_progress_cb (GDBusConnection *connection,
                         const gchar     *sender_name,
                         const gchar     *object_path,
                         const gchar     *interface_name,
                         const gchar     *signal_name,
                         GVariant        *parameters,
                         gpointer         user_data)
{
	TrackerExtractController *self = user_data;
	const gchar *status;

	g_return_if_fail (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(sdi)")));

	/* If we didn't get the initial status yet, ignore Progress signals */
	if (self->priv->cancellable)
		return;

	g_variant_get (parameters, "(&sdi)", &status, NULL, NULL);
	files_miner_status_changed (self, status);
}

static void
disconnect_all (TrackerExtractController *self)
{
	GDBusConnection *conn;

	conn = tracker_miner_get_dbus_connection (TRACKER_MINER (self->priv->decorator));

	if (self->priv->watch_id != 0)
		g_bus_unwatch_name (self->priv->watch_id);
	self->priv->watch_id = 0;

	if (self->priv->progress_signal_id != 0)
		g_dbus_connection_signal_unsubscribe (conn,
		                                      self->priv->progress_signal_id);
	self->priv->progress_signal_id = 0;

	if (self->priv->cancellable)
		g_cancellable_cancel (self->priv->cancellable);
	g_clear_object (&self->priv->cancellable);
}

static void
update_wait_for_miner_fs (TrackerExtractController *self)
{
	GDBusConnection *conn;

	conn = tracker_miner_get_dbus_connection (TRACKER_MINER (self->priv->decorator));

	if (tracker_config_get_wait_for_miner_fs (self->priv->config)) {
		self->priv->progress_signal_id =
			g_dbus_connection_signal_subscribe (conn,
			                                    "org.freedesktop.Tracker1.Miner.Files",
			                                    "org.freedesktop.Tracker1.Miner",
			                                    "Progress",
			                                    "/org/freedesktop/Tracker1/Miner/Files",
			                                    NULL,
			                                    G_DBUS_SIGNAL_FLAGS_NONE,
			                                    files_miner_progress_cb,
			                                    self, NULL);

		/* appeared_cb is guaranteed to be called even if the service
		 * was already running, so we'll start the miner from there. */
		self->priv->watch_id = g_bus_watch_name_on_connection (conn,
		                                                       "org.freedesktop.Tracker1.Miner.Files",
		                                                       G_BUS_NAME_WATCHER_FLAGS_NONE,
		                                                       appeared_cb,
		                                                       vanished_cb,
		                                                       self, NULL);
	} else {
		disconnect_all (self);
		files_miner_idleness_changed (self, TRUE);
	}
}

static void
tracker_extract_controller_constructed (GObject *object)
{
	TrackerExtractController *self = (TrackerExtractController *) object;

	G_OBJECT_CLASS (tracker_extract_controller_parent_class)->constructed (object);

	g_assert (self->priv->decorator != NULL);

	self->priv->config = g_object_ref (tracker_main_get_config ());
	g_signal_connect_object (self->priv->config,
	                         "notify::wait-for-miner-fs",
	                         G_CALLBACK (update_wait_for_miner_fs),
	                         self, G_CONNECT_SWAPPED);
	update_wait_for_miner_fs (self);
}

static void
tracker_extract_controller_get_property (GObject    *object,
                                         guint       param_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
	TrackerExtractController *self = (TrackerExtractController *) object;

	switch (param_id) {
	case PROP_DECORATOR:
		g_value_set_object (value, self->priv->decorator);
		break;
	}
}

static void
tracker_extract_controller_set_property (GObject      *object,
                                        guint         param_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
	TrackerExtractController *self = (TrackerExtractController *) object;

	switch (param_id) {
	case PROP_DECORATOR:
		g_assert (self->priv->decorator == NULL);
		self->priv->decorator = g_value_dup_object (value);
		break;
	}
}

static void
tracker_extract_controller_dispose (GObject *object)
{
	TrackerExtractController *self = (TrackerExtractController *) object;

	disconnect_all (self);
	g_clear_object (&self->priv->decorator);
	g_clear_object (&self->priv->config);

	G_OBJECT_CLASS (tracker_extract_controller_parent_class)->dispose (object);
}

static void
tracker_extract_controller_class_init (TrackerExtractControllerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructed = tracker_extract_controller_constructed;
	object_class->dispose = tracker_extract_controller_dispose;
	object_class->get_property = tracker_extract_controller_get_property;
	object_class->set_property = tracker_extract_controller_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_DECORATOR,
	                                 g_param_spec_object ("decorator",
	                                                      "Decorator",
	                                                      "Decorator",
	                                                      TRACKER_TYPE_DECORATOR,
	                                                      G_PARAM_STATIC_STRINGS |
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class,
	                          sizeof (TrackerExtractControllerPrivate));
}

static void
tracker_extract_controller_init (TrackerExtractController *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
	                                          TRACKER_TYPE_EXTRACT_CONTROLLER,
	                                          TrackerExtractControllerPrivate);
}

TrackerExtractController *
tracker_extract_controller_new (TrackerDecorator *decorator)
{
	g_return_val_if_fail (TRACKER_IS_DECORATOR (decorator), NULL);

	return g_object_new (TRACKER_TYPE_EXTRACT_CONTROLLER,
	                     "decorator", decorator,
	                     NULL);
}
