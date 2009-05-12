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




