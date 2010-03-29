/*
 * Copyright (C) 2010, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/*
 * To simulate a DBusGProxy in the unit tests, we need a GObject (dbus
 * disconnection does a g_object_unref (). This is a GObject with only
 * one attribute (an integer), generated with the following vala code:
 * (command-line: valac -C mock-dbus-gproxy.vala)
 *
 *    using GLib;
 *
 *    public class MockDBusGProxy : GLib.Object {
 *      public int id = 1;
 *    }
 *
 *
 * Modify this code as long as the final result is still a GObject.
 */
#ifndef __MOCK_DBUS_GPROXY_H__
#define __MOCK_DBUS_GPROXY_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS


#define TYPE_MOCK_DBUS_GPROXY (mock_dbus_gproxy_get_type ())
#define MOCK_DBUS_GPROXY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_MOCK_DBUS_GPROXY, MockDBusGProxy))
#define MOCK_DBUS_GPROXY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_MOCK_DBUS_GPROXY, MockDBusGProxyClass))
#define IS_MOCK_DBUS_GPROXY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_MOCK_DBUS_GPROXY))
#define IS_MOCK_DBUS_GPROXY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_MOCK_DBUS_GPROXY))
#define MOCK_DBUS_GPROXY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_MOCK_DBUS_GPROXY, MockDBusGProxyClass))

typedef struct _MockDBusGProxy MockDBusGProxy;
typedef struct _MockDBusGProxyClass MockDBusGProxyClass;
typedef struct _MockDBusGProxyPrivate MockDBusGProxyPrivate;

struct _MockDBusGProxy {
	GObject parent_instance;
	MockDBusGProxyPrivate * priv;
	gint id;
};

struct _MockDBusGProxyClass {
	GObjectClass parent_class;
};


MockDBusGProxy* mock_dbus_gproxy_construct (GType object_type);
MockDBusGProxy* mock_dbus_gproxy_new (void);
GType mock_dbus_gproxy_get_type (void);


G_END_DECLS

#endif
