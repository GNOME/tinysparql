#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include "mock-dbus-gproxy.h"

DBusGConnection* 
dbus_g_bus_get (DBusBusType type, GError **error)
{
        //g_print ("Calling the right function %s\n", __func__);
        return (DBusGConnection *)g_strdup ("mock connection");
}

DBusGProxy*         
dbus_g_proxy_new_for_name (DBusGConnection *connection,
                           const char *name,
                           const char *path,
                           const char *interface)
{
        //g_print ("Calling the right function %s\n", __func__);
        return (DBusGProxy *)mock_dbus_gproxy_new ();
}

gboolean    
dbus_g_proxy_call (DBusGProxy *proxy,
                   const char *method,
                   GError **error,
                   GType first_arg_type,
                   ...)
{
        va_list args;

        //g_print ("Calling the right function %s\n", __func__);
        g_message ("DBUS-CALL: %s", method);

        va_start (args, first_arg_type);

        if (g_strcmp0 (method, "GetSupported") == 0) {
                GType *t;
                GStrv *mime_types;
                gchar *mimetypes[] = {"image/jpeg", "image/png", NULL};

                t = va_arg (args, GType*);
                mime_types = va_arg (args, GStrv*);
                
                *mime_types = g_strdupv (mimetypes);
        }

        va_end (args);
        return TRUE;       
}

void 	
dbus_g_proxy_call_no_reply (DBusGProxy *proxy, 
                            const char *method, 
                            GType first_arg_type,...)
{

        g_message ("DBUS-CALL: %s", method);
}

