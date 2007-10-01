#include "tm-trackerd-connection.h"
#include "tm-trackerd-connection-private.h"

static guint signals[LAST_SIGNAL] = { 0, };

static void
trackerd_connection_class_init(TrackerDaemonConnectionClass *klass)
{
   g_type_class_add_private(klass, sizeof(TrackerDaemonConnectionPrivate));

   klass->available = _available;
   klass->version = _version;
   klass->statistics = _statistics;

   klass->start = _start_daemon;
   klass->stop = _stop_daemon;

   signals[STARTED] = g_signal_new("started",
                                   G_TYPE_FROM_CLASS(klass),
                                   G_SIGNAL_RUN_LAST,
                                   G_STRUCT_OFFSET(TrackerDaemonConnectionClass, started),
                                   NULL,
                                   NULL,
                                   g_cclosure_marshal_VOID__VOID,
                                   G_TYPE_NONE,
                                   0);

   signals[STOPPED] = g_signal_new("stopped",
                                   G_TYPE_FROM_CLASS(klass),
                                   G_SIGNAL_RUN_LAST,
                                   G_STRUCT_OFFSET(TrackerDaemonConnectionClass, stopped),
                                   NULL,
                                   NULL,
                                   g_cclosure_marshal_VOID__VOID,
                                   G_TYPE_NONE,
                                   0);
}

static void
trackerd_connection_init(GTypeInstance *instance, gpointer g_class)
{
   GError *error = NULL;
   TrackerDaemonConnection *self = TRACKERD_CONNECTION(instance);
   TrackerDaemonConnectionPrivate *priv = TRACKERD_CONNECTION_GET_PRIVATE(self);

   priv->connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);

   if (error) {
      critical_error("Unable to connect to the Session Bus due to the "
                     "following error:\n\n%s\n\nPlease ensure that DBUS "
                     "is running, and then start %s.", error->message, PROGRAM);
      g_error_free(error);
      return;
   } else if (priv->connection == NULL) {
      critical_error("Unable to connect to the Session Bus.  Please "
                     "ensure that DBus is running, and then start %s", PROGRAM);
      return;
   }

   priv->dbus_proxy = dbus_g_proxy_new_for_name(priv->connection,
                                                DBUS_SERVICE_DBUS,
                                                DBUS_PATH_DBUS,
                                                DBUS_INTERFACE_DBUS);

   dbus_g_proxy_add_signal(priv->dbus_proxy,
                           "NameOwnerChanged",
                           G_TYPE_STRING,
                           G_TYPE_STRING,
                           G_TYPE_STRING,
                           G_TYPE_INVALID);

   dbus_g_proxy_connect_signal(priv->dbus_proxy,
                               "NameOwnerChanged",
                               G_CALLBACK(name_owner_changed),
                               self,
                               NULL);
}

gboolean
trackerd_connection_available(TrackerDaemonConnection *connection)
{
   return TRACKERD_CONNECTION_GET_CLASS(connection)->available(connection);
}

gchar *
trackerd_connection_version(TrackerDaemonConnection *connection)
{
   return TRACKERD_CONNECTION_GET_CLASS(connection)->version(connection);
}

GPtrArray *
trackerd_connection_statistics(TrackerDaemonConnection *connection)
{
   return TRACKERD_CONNECTION_GET_CLASS(connection)->statistics(connection);
}

void
trackerd_connection_start(TrackerDaemonConnection *connection)
{
   return TRACKERD_CONNECTION_GET_CLASS(connection)->start(connection);
}

void
trackerd_connection_stop(TrackerDaemonConnection *connection)
{
   return TRACKERD_CONNECTION_GET_CLASS(connection)->stop(connection);
}

static gboolean
_available(TrackerDaemonConnection *connection)
{
   TrackerDaemonConnection *self = TRACKERD_CONNECTION(connection);
   TrackerDaemonConnectionPrivate *priv = TRACKERD_CONNECTION_GET_PRIVATE(self);

   GError *error = NULL;
   gboolean value = FALSE;

   org_freedesktop_DBus_name_has_owner(priv->dbus_proxy,
                                       DBUS_SERVICE_TRACKER,
                                       &value,
                                       &error);

   if (error) {
      g_warning("failed: org_freedesktop_DBus_name_has_owner: %s\n",
                error->message);
      g_error_free(error);
      return FALSE;
   }

   return value;
}

static gchar *
_version(TrackerDaemonConnection *connection)
{
   TrackerDaemonConnection *self = TRACKERD_CONNECTION(connection);
   TrackerDaemonConnectionPrivate *priv = TRACKERD_CONNECTION_GET_PRIVATE(self);

   guint version = 0;
   GError *error = NULL;

   if (!priv->tracker_proxy)
      connect(connection);

   org_freedesktop_Tracker_get_version(priv->tracker_proxy, &version, &error);

   if (error) {
      g_warning("failed: org_freedesktop_Tracker_get_version: %s\n",
                error->message);
      g_error_free(error);
      return NULL;
   }

   return version_int_to_string(version);
}

static GPtrArray *
_statistics(TrackerDaemonConnection *connection)
{
   TrackerDaemonConnection *self = TRACKERD_CONNECTION(connection);
   TrackerDaemonConnectionPrivate *priv = TRACKERD_CONNECTION_GET_PRIVATE(self);

   GError *error = NULL;
   GPtrArray *array = NULL;

   org_freedesktop_Tracker_get_stats(priv->tracker_proxy, &array, &error);

   if (error) {
      g_warning("failed: org_freedesktop_Tracker_get_stats: %s\n",
                error->message);
      g_error_free(error);
      return NULL;
   }

   return array;
}

static void
_start_daemon(TrackerDaemonConnection *connection)
{
   TrackerDaemonConnection *self = TRACKERD_CONNECTION(connection);
   TrackerDaemonConnectionPrivate *priv = TRACKERD_CONNECTION_GET_PRIVATE(self);

   guint version = 0;
   GError *error = NULL;

   if (_available(connection))
      return;

   priv->tracker_proxy = dbus_g_proxy_new_for_name(priv->connection,
                                                   DBUS_SERVICE_TRACKER,
                                                   DBUS_PATH_TRACKER,
                                                   DBUS_INTERFACE_TRACKER);

   /* This will actually launch the trackerd service if unavailable */
   org_freedesktop_Tracker_get_version(priv->tracker_proxy, &version, &error);

   if (error) {
      g_warning("failed: org_freedesktop_Tracker_get_version: %s\n",
                error->message);
      g_error_free(error);
   }
}

static void
_stop_daemon(TrackerDaemonConnection *connection)
{
   TrackerDaemonConnection *self = TRACKERD_CONNECTION(connection);
   TrackerDaemonConnectionPrivate *priv = TRACKERD_CONNECTION_GET_PRIVATE(self);

   g_warning("_stop_daemon is not implemented\n");
}

static void
connect(TrackerDaemonConnection *connection)
{
   TrackerDaemonConnection *self = TRACKERD_CONNECTION(connection);
   TrackerDaemonConnectionPrivate *priv = TRACKERD_CONNECTION_GET_PRIVATE(connection);

   if (!_available(connection))
      return;

   priv->tracker_proxy = dbus_g_proxy_new_for_name(priv->connection,
                                                   DBUS_SERVICE_TRACKER,
                                                   DBUS_PATH_TRACKER,
                                                   DBUS_INTERFACE_TRACKER);
}

static void
name_owner_changed(DBusGProxy *proxy, const gchar *name, const gchar *prev_owner, const gchar *new_owner, gpointer data)
{
   if (!g_str_equal(name, DBUS_SERVICE_TRACKER))
      return;

   if (g_str_equal(prev_owner, ""))
      g_signal_emit_by_name(data, "started");
   else if (g_str_equal(new_owner, ""))
      g_signal_emit_by_name(data, "stopped");
}

GType
trackerd_connection_get_type(void)
{
   static GType type = 0;

   if (type == 0) {
      static const GTypeInfo info = {
         sizeof(TrackerDaemonConnectionClass),
         NULL,                                           /* base_init */
         NULL,                                           /* base_finalize */
         (GClassInitFunc)trackerd_connection_class_init, /* class_init */
         NULL,                                           /* class_finalize */
         NULL,                                           /* class_data */
         sizeof(TrackerDaemonConnection),
         0,                                              /* n_preallocs */
         trackerd_connection_init                        /* instance_init */
      };

      type = g_type_register_static(G_TYPE_OBJECT, "TrackerDaemonConnectionType", &info, 0);
   }

   return type;
}
