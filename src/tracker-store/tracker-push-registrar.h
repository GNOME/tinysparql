/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#ifndef __TRACKER_STORE_PUSH_REGISTRAR_H__
#define __TRACKER_STORE_PUSH_REGISTRAR_H__

#include <glib-object.h>
#include <dbus/dbus-glib-bindings.h>

G_BEGIN_DECLS

#define TRACKER_TYPE_PUSH_REGISTRAR         (tracker_push_registrar_get_type())
#define TRACKER_PUSH_REGISTRAR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_PUSH_REGISTRAR, TrackerPushRegistrar))
#define TRACKER_PUSH_REGISTRAR_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_PUSH_REGISTRAR, TrackerPushRegistrarClass))
#define TRACKER_IS_PUSH_REGISTRAR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_PUSH_REGISTRAR))
#define TRACKER_IS_PUSH_REGISTRAR_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_PUSH_REGISTRAR))
#define TRACKER_PUSH_REGISTRAR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_PUSH_REGISTRAR, TrackerPushRegistrarClass))

typedef struct TrackerPushRegistrar TrackerPushRegistrar;
typedef struct TrackerPushRegistrarClass TrackerPushRegistrarClass;

struct TrackerPushRegistrar {
	GObject parent_instance;
};

struct TrackerPushRegistrarClass {
	GObjectClass parent_class;

	void (*enable) (TrackerPushRegistrar *registrar,
	                DBusGConnection      *connection,
	                DBusGProxy           *dbus_proxy,
	                GError              **error);

	void (*disable) (TrackerPushRegistrar *registrar);
};

GType                   tracker_push_registrar_get_type      (void) G_GNUC_CONST;

G_CONST_RETURN gchar *  tracker_push_registrar_get_service   (TrackerPushRegistrar *registrar);
GObject *               tracker_push_registrar_get_object    (TrackerPushRegistrar *registrar);
DBusGProxy *            tracker_push_registrar_get_manager   (TrackerPushRegistrar *registrar);

void                    tracker_push_registrar_set_service   (TrackerPushRegistrar *registrar,
                                                              const gchar *service);
void                    tracker_push_registrar_set_object    (TrackerPushRegistrar *registrar,
                                                              GObject              *object);
void                    tracker_push_registrar_set_manager   (TrackerPushRegistrar *registrar,
                                                              DBusGProxy           *manager);


void                    tracker_push_registrar_enable        (TrackerPushRegistrar *registrar,
                                                              DBusGConnection      *connection,
                                                              DBusGProxy           *dbus_proxy,
                                                              GError              **error);
void                    tracker_push_registrar_disable       (TrackerPushRegistrar *registrar);

TrackerPushRegistrar *  tracker_push_module_init             (void);
void                    tracker_push_module_shutdown         (TrackerPushRegistrar *registrar);

G_END_DECLS

#endif /* __TRACKER_STORE_PUSH_REGISTRAR_H__ */
