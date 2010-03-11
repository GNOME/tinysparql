#include <glib-object.h>
#include <gobject/gvaluecollector.h>
#include <dbus/dbus-glib.h>
#include "empty-gobject.h"
#include "thumbnailer-mock.h"

static GList *calls = NULL;

void 
dbus_mock_call_log_reset () 
{
        if (calls) {
                g_list_foreach (calls, (GFunc)g_free, NULL);
                g_list_free (calls);
                calls = NULL;
        }
}

GList *
dbus_mock_call_log_get ()
{
        return calls;
}

static void
dbus_mock_call_log_append (const gchar *function_name)
{
        calls = g_list_append (calls, g_strdup (function_name));
}



/*
 * DBus overrides
 */

DBusGConnection * 	
dbus_g_bus_get (DBusBusType type, GError **error) 
{
        return (DBusGConnection *) empty_object_new ();
}

DBusGProxy *
dbus_g_proxy_new_for_name (DBusGConnection *connection,
                           const gchar *service,
                           const gchar *path,
                           const gchar *interface )
{
        return (DBusGProxy *) empty_object_new ();
}

gboolean
dbus_g_proxy_call (DBusGProxy *proxy,
                   const gchar *function_name,
                   GError  **error,
                   GType first_arg_type, ...)
{
        va_list args;
        GType arg_type;
        const gchar *supported_mimes[] = { "mock/one", "mock/two", NULL};
        GError *local_error = NULL;

        g_assert (g_strcmp0 (function_name, "GetSupported") == 0);
        
        /* 
	   G_TYPE_INVALID,
	   G_TYPE_STRV, &uri_schemes,
	   G_TYPE_STRV, &mime_types,
	   G_TYPE_INVALID);

           Set the mock values in the second parameter :)
        */

        va_start (args, first_arg_type);
        arg_type = va_arg (args, GType);

        int counter = 1;
        while (arg_type != G_TYPE_INVALID) {
                
                if (arg_type == G_TYPE_STRV && counter == 2) {
                        GValue value = { 0, };
                        g_value_init (&value, arg_type);
                        g_value_set_boxed (&value, supported_mimes);
                        G_VALUE_LCOPY (&value, 
                                       args, 0, 
                                       &local_error);
                        g_value_unset (&value);
                } else {
                        gpointer *out_param;
                        out_param = va_arg (args, gpointer *);
                }
                arg_type = va_arg (args, GType);
                counter += 1;
        }

        va_end (args);

        *error == NULL;
        return TRUE;
}


void              
dbus_g_proxy_call_no_reply (DBusGProxy        *proxy,
                            const char        *method,
                            GType              first_arg_type,
                            ...)
{
        dbus_mock_call_log_append (method);
}


