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
 * one attribute (an integer), generated with vala.
 */

#include "mock-dbus-gproxy.h"




enum  {
	MOCK_DBUS_GPROXY_DUMMY_PROPERTY
};
static gpointer mock_dbus_gproxy_parent_class = NULL;
static void mock_dbus_gproxy_finalize (GObject* obj);



MockDBusGProxy* mock_dbus_gproxy_construct (GType object_type) {
	MockDBusGProxy * self;
	self = g_object_newv (object_type, 0, NULL);
	return self;
}


MockDBusGProxy* mock_dbus_gproxy_new (void) {
	return mock_dbus_gproxy_construct (TYPE_MOCK_DBUS_GPROXY);
}


static void mock_dbus_gproxy_class_init (MockDBusGProxyClass * klass) {
	mock_dbus_gproxy_parent_class = g_type_class_peek_parent (klass);
	G_OBJECT_CLASS (klass)->finalize = mock_dbus_gproxy_finalize;
}


static void mock_dbus_gproxy_instance_init (MockDBusGProxy * self) {
	self->id = 1;
}


static void mock_dbus_gproxy_finalize (GObject* obj) {
	MockDBusGProxy * self;
	self = MOCK_DBUS_GPROXY (obj);
	G_OBJECT_CLASS (mock_dbus_gproxy_parent_class)->finalize (obj);
}


GType mock_dbus_gproxy_get_type (void) {
	static GType mock_dbus_gproxy_type_id = 0;
	if (mock_dbus_gproxy_type_id == 0) {
		static const GTypeInfo g_define_type_info = { sizeof (MockDBusGProxyClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) mock_dbus_gproxy_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (MockDBusGProxy), 0, (GInstanceInitFunc) mock_dbus_gproxy_instance_init, NULL };
		mock_dbus_gproxy_type_id = g_type_register_static (G_TYPE_OBJECT, "MockDBusGProxy", &g_define_type_info, 0);
	}
	return mock_dbus_gproxy_type_id;
}




