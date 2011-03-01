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
 */

#include "config.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

#include <libtracker-common/tracker-dbus.h>

#include "tracker-writeback-dispatcher.h"
#include "tracker-writeback-dbus.h"
#include "tracker-marshal.h"

#define TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_WRITEBACK_DISPATCHER, TrackerWritebackDispatcherPrivate))

#define TRACKER_SERVICE                 "org.freedesktop.Tracker1"
#define TRACKER_RESOURCES_OBJECT        "/org/freedesktop/Tracker1/Resources"
#define TRACKER_INTERFACE_RESOURCES     "org.freedesktop.Tracker1.Resources"

#define DBUS_MATCH_STR	  \
	"type='signal', " \
	"sender='" TRACKER_SERVICE "', " \
	"path='" TRACKER_RESOURCES_OBJECT "', " \
	"interface='" TRACKER_INTERFACE_RESOURCES "', " \
	"member='Writeback'"

typedef struct {
	GMainContext *context;
	DBusConnection *connection;
} TrackerWritebackDispatcherPrivate;

enum {
	PROP_0,
	PROP_MAIN_CONTEXT
};

enum {
	WRITEBACK,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void tracker_writeback_dispatcher_finalize            (GObject        *object);

static void tracker_writeback_dispatcher_get_property        (GObject        *object,
                                                              guint           param_id,
                                                              GValue         *value,
                                                              GParamSpec     *pspec);
static void tracker_writeback_dispatcher_set_property        (GObject        *object,
                                                              guint           param_id,
                                                              const GValue   *value,
                                                              GParamSpec     *pspec);
static void tracker_writeback_dispatcher_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (TrackerWritebackDispatcher, tracker_writeback_dispatcher, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                         tracker_writeback_dispatcher_initable_iface_init));

static void
handle_writeback_signal (TrackerWritebackDispatcher *dispatcher,
                         DBusMessage                *message)
{
	DBusMessageIter iter;
	gchar *signature;
	int arg_type;

	g_message ("Got writeback DBus signal");

	if (!dbus_message_iter_init (message, &iter)) {
		g_critical ("  Message had no arguments");
		return;
	}

	signature = dbus_message_iter_get_signature (&iter);

	if (g_strcmp0 (signature, "a{iai}") != 0) {
		g_critical ("  Unexpected message signature '%s'", signature);
		g_free (signature);
		return;
	}

	g_free (signature);

	while ((arg_type = dbus_message_iter_get_arg_type (&iter)) != DBUS_TYPE_INVALID) {
		DBusMessageIter arr;

		dbus_message_iter_recurse (&iter, &arr);

		while ((arg_type = dbus_message_iter_get_arg_type (&arr)) != DBUS_TYPE_INVALID) {
			DBusMessageIter dict, types_arr;
			gint subject;
			GArray *rdf_types;

			rdf_types = g_array_new (FALSE, FALSE, sizeof (gint));

			dbus_message_iter_recurse (&arr, &dict);

			dbus_message_iter_get_basic (&dict, &subject);

			dbus_message_iter_next (&dict);
			dbus_message_iter_recurse (&dict, &types_arr);

			while ((arg_type = dbus_message_iter_get_arg_type (&types_arr)) != DBUS_TYPE_INVALID) {
				gint type;

				dbus_message_iter_get_basic (&types_arr, &type);

				g_array_append_val (rdf_types, type);

				dbus_message_iter_next (&types_arr);
			}

			g_signal_emit (dispatcher, signals[WRITEBACK], 0, subject, rdf_types);
			g_array_free (rdf_types, TRUE);

			dbus_message_iter_next (&arr);
		}

		dbus_message_iter_next (&iter);
	}
}

static DBusHandlerResult
message_filter (DBusConnection *connection,
                DBusMessage    *message,
                gpointer        user_data)
{
	if (dbus_message_is_signal (message,
	                            TRACKER_INTERFACE_RESOURCES,
	                            "Writeback")) {
		TrackerWritebackDispatcher *dispatcher = user_data;

		handle_writeback_signal (dispatcher, message);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusConnection *
setup_dbus_connection (TrackerWritebackDispatcher  *dispatcher,
                       GMainContext                *context,
                       GError                     **n_error)
{
	DBusConnection *connection;
	DBusError error;
	gint result;

	dbus_error_init (&error);

	/* Create DBus connection */
	connection = dbus_bus_get_private (DBUS_BUS_SESSION, &error);

	if (dbus_error_is_set (&error)) {
		g_set_error_literal (n_error,
		                     TRACKER_DBUS_ERROR,
		                     TRACKER_DBUS_ERROR_ASSERTION_FAILED,
		                     error.message);
		dbus_error_free (&error);
		return NULL;
	}

	dbus_connection_set_exit_on_disconnect (connection, FALSE);

	/* Request writeback service name */
	g_message ("Registering D-Bus service '%s'...", TRACKER_WRITEBACK_DBUS_NAME);
	result = dbus_bus_request_name (connection,
	                                TRACKER_WRITEBACK_DBUS_NAME, 0,
	                                &error);

	if (dbus_error_is_set (&error)) {
		g_set_error_literal (n_error,
		                     TRACKER_DBUS_ERROR,
		                     TRACKER_DBUS_ERROR_ASSERTION_FAILED,
		                     error.message);
		dbus_error_free (&error);
		dbus_connection_close (connection);
		dbus_connection_unref (connection);

		return NULL;
	}

	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		g_set_error_literal (n_error,
		                     TRACKER_DBUS_ERROR,
		                     TRACKER_DBUS_ERROR_ASSERTION_FAILED,
		                     "D-Bus service name:'"TRACKER_WRITEBACK_DBUS_NAME"' is already taken, "
		                     "perhaps the application is already running?");

		dbus_connection_close (connection);
		dbus_connection_unref (connection);

		return NULL;
	}

	/* Add message filter function */
	if (!dbus_connection_add_filter (connection, message_filter, dispatcher, NULL)) {
		g_set_error_literal (n_error,
		                     TRACKER_DBUS_ERROR,
		                     TRACKER_DBUS_ERROR_ASSERTION_FAILED,
		                     "Could not add message filter");

		dbus_connection_close (connection);
		dbus_connection_unref (connection);

		return NULL;
	}

	/* Add match to receive writeback signals */
	dbus_bus_add_match (connection, DBUS_MATCH_STR, &error);

	if (dbus_error_is_set (&error)) {
		g_set_error_literal (n_error,
		                     TRACKER_DBUS_ERROR,
		                     TRACKER_DBUS_ERROR_ASSERTION_FAILED,
		                     error.message);
		dbus_error_free (&error);
		dbus_connection_close (connection);
		dbus_connection_unref (connection);

		return NULL;
	}

	/* Set up with the thread context */
	dbus_connection_setup_with_g_main (connection, context);

	return connection;
}

static gboolean
tracker_writeback_dispatcher_initable_init (GInitable     *initable,
                                            GCancellable  *cancellable,
                                            GError       **error)
{
	TrackerWritebackDispatcherPrivate *priv;
	TrackerWritebackDispatcher *dispatcher;
	DBusConnection *connection;
	GError *internal_error = NULL;

	dispatcher = TRACKER_WRITEBACK_DISPATCHER (initable);
	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (dispatcher);

	connection = setup_dbus_connection (dispatcher, priv->context,
	                                    &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	} else {
		priv->connection = connection;
	}

	return TRUE;
}

static void
tracker_writeback_dispatcher_initable_iface_init (GInitableIface *iface)
{
	iface->init = tracker_writeback_dispatcher_initable_init;
}

static void
tracker_writeback_dispatcher_class_init (TrackerWritebackDispatcherClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_writeback_dispatcher_finalize;
	object_class->get_property = tracker_writeback_dispatcher_get_property;
	object_class->set_property = tracker_writeback_dispatcher_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_MAIN_CONTEXT,
	                                 g_param_spec_pointer ("context",
	                                                       "Main context",
	                                                       "Main context to run the DBus service on",
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	signals[WRITEBACK] =
		g_signal_new ("writeback",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerWritebackDispatcherClass, writeback),
		              NULL, NULL,
		              tracker_marshal_VOID__INT_BOXED,
		              G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_ARRAY);

	g_type_class_add_private (object_class, sizeof (TrackerWritebackDispatcherPrivate));
}

static void
tracker_writeback_dispatcher_init (TrackerWritebackDispatcher *dispatcher)
{
}

static void
tracker_writeback_dispatcher_finalize (GObject *object)
{
	TrackerWritebackDispatcherPrivate *priv;
	DBusError error;

	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (object);
	dbus_error_init (&error);

	dbus_bus_remove_match (priv->connection, DBUS_MATCH_STR, &error);

	if (dbus_error_is_set (&error)) {
		g_critical ("Could not remove match rules, %s", error.message);
		dbus_error_free (&error);
	}

	dbus_connection_remove_filter (priv->connection, message_filter, object);
	dbus_connection_unref (priv->connection);

	G_OBJECT_CLASS (tracker_writeback_dispatcher_parent_class)->finalize (object);
}


static void
tracker_writeback_dispatcher_get_property (GObject    *object,
                                           guint       param_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
	TrackerWritebackDispatcherPrivate *priv;

	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_MAIN_CONTEXT:
		g_value_set_pointer (value, priv->context);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
tracker_writeback_dispatcher_set_property (GObject       *object,
                                           guint          param_id,
                                           const GValue  *value,
                                           GParamSpec    *pspec)
{
	TrackerWritebackDispatcherPrivate *priv;

	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_MAIN_CONTEXT:
		{
			GMainContext *context;

			context = g_value_get_pointer (value);

			if (context != priv->context) {
				if (priv->context) {
					g_main_context_unref (priv->context);
					priv->context = NULL;
				}

				if (context) {
					priv->context = g_main_context_ref (context);
				}
			}
		}

		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

TrackerWritebackDispatcher *
tracker_writeback_dispatcher_new (GMainContext  *context,
                                  GError       **error)
{
	GError *internal_error = NULL;
	TrackerWritebackDispatcher *ret;

	ret = g_initable_new (TRACKER_TYPE_WRITEBACK_DISPATCHER,
	                       NULL, &internal_error,
	                       "context", context,
	                       NULL);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return NULL;
	}

	return ret;
}
