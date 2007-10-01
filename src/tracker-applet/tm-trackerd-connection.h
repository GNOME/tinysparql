#ifndef __TM_TRACKERD_CONNECTION_H__
#define __TM_TRACKERD_CONNECTION_H__

#include <glib.h>
#include <glib-object.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>

#include "tracker-client.h"

#include "tm-debug.h"
#include "tm-utils.h"
#include "tm-common.h"

#define TYPE_TRACKERD_CONNECTION             (trackerd_connection_get_type())
#define TRACKERD_CONNECTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_TRACKERD_CONNECTION, TrackerDaemonConnection))
#define TRACKERD_CONNECTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_TRACKERD_CONNECTION, TrackerDaemonConnectionClass))
#define IS_TRACKERD_CONNECTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_TRACKERD_CONNECTION))
#define IS_TRACKERD_CONNECTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_TRACKERD_CONNECTION))
#define TRACKERD_CONNECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_TRACKERD_CONNECTION, TrackerDaemonConnectionClass))

typedef struct _TrackerDaemonConnection
{
   GObject parent;
} TrackerDaemonConnection;

typedef struct _TrackerDaemonConnectionClass
{
   GObjectClass parent_class;

   /* signals */
   void (*started)(TrackerDaemonConnection *connection);
   void (*stopped)(TrackerDaemonConnection *connection);

   /* methods */
   gboolean (*available)(TrackerDaemonConnection *connection);
   gchar * (*version)(TrackerDaemonConnection *connection);
   GPtrArray * (*statistics)(TrackerDaemonConnection *connection);

   void (*start)(TrackerDaemonConnection *connection);
   void (*stop)(TrackerDaemonConnection *connection);
} TrackerDaemonConnectionClass;

GType
trackerd_connection_get_type(void);

gboolean
trackerd_connection_available(TrackerDaemonConnection *connection);

gchar *
trackerd_connection_version(TrackerDaemonConnection *connection);

GPtrArray *
trackerd_connection_statistics(TrackerDaemonConnection *connection);

void
trackerd_connection_start(TrackerDaemonConnection *connection);

void
trackerd_connection_stop(TrackerDaemonConnection *connection);

#endif
