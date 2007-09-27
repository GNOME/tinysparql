#ifndef __TM_TRACKERD_CONNECTION_PRIVATE_H__
#define __TM_TRACKERD_CONNECTION_PRIVATE_H__

#define TRACKERD_CONNECTION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TYPE_TRACKERD_CONNECTION, TrackerDaemonConnectionPrivate))

enum
{
   STARTED,
   STOPPED,
   LAST_SIGNAL,
};

typedef struct _TrackerDaemonConnectionPrivate
{
   DBusGProxy *dbus_proxy;
   DBusGProxy *tracker_proxy;
   DBusGConnection *connection;
} TrackerDaemonConnectionPrivate;

static void
trackerd_connection_class_init(TrackerDaemonConnectionClass *klass);

static void
trackerd_connection_init(GTypeInstance *instance, gpointer g_class);

static gboolean
_available(TrackerDaemonConnection *connection);

static gchar *
_version(TrackerDaemonConnection *connection);

static GPtrArray *
_statistics(TrackerDaemonConnection *connection);

/* NOTE: This is not named _start to prevent collision with libc's _start */
static void
_start_daemon(TrackerDaemonConnection *connection);

static void
_stop_daemon(TrackerDaemonConnection *connection);

static void
connect(TrackerDaemonConnection *connection);

static void
name_owner_changed(DBusGProxy *proxy, const gchar *name, const gchar *prev_owner, const gchar *new_owner, gpointer data);

#endif
