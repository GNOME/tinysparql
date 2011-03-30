/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "tracker-controller.h"
#include "tracker-extract.h"

#include <gio/gunixoutputstream.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixfdlist.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-extract/tracker-extract.h>
#include <libtracker-miner/tracker-miner.h>
#include <gio/gio.h>

typedef struct TrackerControllerPrivate TrackerControllerPrivate;
typedef struct GetMetadataData GetMetadataData;

struct TrackerControllerPrivate {
	GMainContext *context;
	GMainLoop *main_loop;

	TrackerStorage *storage;
	TrackerExtract *extractor;

	GDBusConnection *connection;
	GDBusNodeInfo *introspection_data;
	guint registration_id;

	guint shutdown_timeout;
	guint shutdown_timeout_id;

	GCond *initialization_cond;
	GMutex *initialization_mutex;
	GError *initialization_error;

	guint initialized : 1;
};

struct GetMetadataData {
	TrackerController *controller;
	GCancellable *cancellable;
	GDBusMethodInvocation *invocation;
	TrackerDBusRequest *request;
	gchar *uri;
	gchar *mimetype;
	gint fd; /* Only for fast queries */
};

#define TRACKER_EXTRACT_SERVICE	       "org.freedesktop.Tracker1.Extract"
#define TRACKER_EXTRACT_PATH	       "/org/freedesktop/Tracker1/Extract"
#define TRACKER_EXTRACT_INTERFACE      "org.freedesktop.Tracker1.Extract"

#define MAX_EXTRACT_TIME 10

static const gchar *introspection_xml =
	"<node>"
	"  <interface name='org.freedesktop.Tracker1.Extract'>"
	"    <method name='GetPid'>"
	"      <arg type='i' name='value' direction='out' />"
	"    </method>"
	"    <method name='GetMetadata'>"
	"      <arg type='s' name='uri' direction='in' />"
	"      <arg type='s' name='mime' direction='in' />"
	"      <arg type='s' name='preupdate' direction='out' />"
	"      <arg type='s' name='embedded' direction='out' />"
	"      <arg type='s' name='where' direction='out' />"
	"    </method>"
	"    <method name='GetMetadataFast'>"
	"      <arg type='s' name='uri' direction='in' />"
	"      <arg type='s' name='mime' direction='in' />"
	"      <arg type='h' name='fd' direction='in' />"
	"    </method>"
	"  </interface>"
	"</node>";

enum {
	PROP_0,
	PROP_SHUTDOWN_TIMEOUT,
	PROP_EXTRACTOR
};

static void	tracker_controller_initable_iface_init (GInitableIface	   *iface);
static gboolean tracker_controller_dbus_start          (TrackerController  *controller,
                                                        GError	          **error);
static void	tracker_controller_dbus_stop           (TrackerController  *controller);


G_DEFINE_TYPE_WITH_CODE (TrackerController, tracker_controller, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                tracker_controller_initable_iface_init));

static gboolean
tracker_controller_initable_init (GInitable	*initable,
                                  GCancellable	*cancellable,
                                  GError       **error)
{
	return tracker_controller_start (TRACKER_CONTROLLER (initable), error);
}

static void
tracker_controller_initable_iface_init (GInitableIface *iface)
{
	iface->init = tracker_controller_initable_init;
}


static void
tracker_controller_finalize (GObject *object)
{
	TrackerControllerPrivate *priv;
	TrackerController *controller;

	controller = TRACKER_CONTROLLER (object);
	priv = controller->priv;

	if (priv->shutdown_timeout_id) {
		g_source_remove (priv->shutdown_timeout_id);
		priv->shutdown_timeout_id = 0;
	}

	tracker_controller_dbus_stop (controller);

	g_object_unref (priv->storage);

	g_main_loop_unref (priv->main_loop);
	g_main_context_unref (priv->context);

	g_cond_free (priv->initialization_cond);
	g_mutex_free (priv->initialization_mutex);

	G_OBJECT_CLASS (tracker_controller_parent_class)->finalize (object);
}

static void
tracker_controller_get_property (GObject    *object,
                                 guint	     param_id,
                                 GValue	    *value,
                                 GParamSpec *pspec)
{
	TrackerControllerPrivate *priv = TRACKER_CONTROLLER (object)->priv;

	switch (param_id) {
	case PROP_SHUTDOWN_TIMEOUT:
		g_value_set_uint (value, priv->shutdown_timeout);
		break;
	case PROP_EXTRACTOR:
		g_value_set_object (value, priv->extractor);
		break;
	}
}

static void
tracker_controller_set_property (GObject      *object,
                                 guint	       param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
	TrackerControllerPrivate *priv = TRACKER_CONTROLLER (object)->priv;

	switch (param_id) {
	case PROP_SHUTDOWN_TIMEOUT:
		priv->shutdown_timeout = g_value_get_uint (value);
		break;
	case PROP_EXTRACTOR:
		priv->extractor = g_value_get_object (value);
		break;
	}
}

static void
tracker_controller_class_init (TrackerControllerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_controller_finalize;
	object_class->get_property = tracker_controller_get_property;
	object_class->set_property = tracker_controller_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_SHUTDOWN_TIMEOUT,
	                                 g_param_spec_uint ("shutdown-timeout",
	                                                    "Shutdown timeout",
	                                                    "Shutdown timeout, 0 to disable",
	                                                    0, 1000, 0,
	                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
	                                 PROP_EXTRACTOR,
	                                 g_param_spec_object ("extractor",
	                                                      "Extractor",
	                                                      "Extractor",
	                                                      TRACKER_TYPE_EXTRACT,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (TrackerControllerPrivate));
}

static GetMetadataData *
metadata_data_new (TrackerController     *controller,
                   const gchar           *uri,
                   const gchar           *mime,
                   GDBusMethodInvocation *invocation,
                   TrackerDBusRequest    *request)
{
	GetMetadataData *data;

	data = g_slice_new (GetMetadataData);
	data->cancellable = g_cancellable_new ();
	data->controller = controller;
	data->uri = g_strdup (uri);
	data->mimetype = g_strdup (mime);
	data->invocation = invocation;
	data->request = request;

	return data;
}

static void
metadata_data_free (GetMetadataData *data)
{
	/* We rely on data->invocation being freed through
	 * the g_dbus_method_invocation_return_* methods
	 */
	g_free (data->uri);
	g_free (data->mimetype);
	g_object_unref (data->cancellable);
	g_slice_free (GetMetadataData, data);
}

static gboolean
reset_shutdown_timeout_cb (gpointer user_data)
{
	TrackerControllerPrivate *priv;

	g_message ("Extractor lifetime has expired");

	priv = TRACKER_CONTROLLER (user_data)->priv;
	g_main_loop_quit (priv->main_loop);

	return TRUE;
}

static void
reset_shutdown_timeout (TrackerController *controller)
{
	TrackerControllerPrivate *priv;
	GSource *source;

	priv = controller->priv;

	if (priv->shutdown_timeout == 0) {
		return;
	}

	g_message ("(Re)setting shutdown timeout");

	if (priv->shutdown_timeout_id != 0) {
		g_source_remove (priv->shutdown_timeout_id);
	}

	source = g_timeout_source_new_seconds (priv->shutdown_timeout);
	g_source_set_callback (source,
	                       reset_shutdown_timeout_cb,
	                       controller, NULL);

	priv->shutdown_timeout_id = g_source_attach (source, priv->context);
}

static void
tracker_controller_init (TrackerController *controller)
{
	TrackerControllerPrivate *priv;

	priv = controller->priv = G_TYPE_INSTANCE_GET_PRIVATE (controller,
	                                                       TRACKER_TYPE_CONTROLLER,
	                                                       TrackerControllerPrivate);

	priv->context = g_main_context_new ();
	priv->main_loop = g_main_loop_new (priv->context, FALSE);

	priv->storage = tracker_storage_new ();

	priv->initialization_cond = g_cond_new ();
	priv->initialization_mutex = g_mutex_new ();
}

static void
handle_method_call_get_pid (TrackerController	  *controller,
                            GDBusMethodInvocation *invocation,
                            GVariant		  *parameters)
{
	TrackerDBusRequest *request;
	pid_t value;

	request = tracker_g_dbus_request_begin (invocation,
	                                        "%s()",
	                                        __FUNCTION__);

	reset_shutdown_timeout (controller);
	value = getpid ();
	tracker_dbus_request_debug (request,
	                            "PID is %d",
	                            value);

	tracker_dbus_request_end (request, NULL);

	g_dbus_method_invocation_return_value (invocation,
	                                       g_variant_new ("(i)", (gint) value));
}

static void
get_metadata_cb (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
	GetMetadataData *data;
	TrackerExtractInfo *info;

	data = user_data;
	info = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));

	if (info) {
		if (tracker_sparql_builder_get_length (info->statements) > 0) {
			const gchar *preupdate_str = NULL;

			if (tracker_sparql_builder_get_length (info->preupdate) > 0) {
				preupdate_str = tracker_sparql_builder_get_result (info->preupdate);
			}

			g_dbus_method_invocation_return_value (data->invocation,
			                                       g_variant_new ("(sss)",
			                                                      preupdate_str ? preupdate_str : "",
			                                                      tracker_sparql_builder_get_result (info->statements),
			                                                      info->where ? info->where : ""));
		} else {
			g_dbus_method_invocation_return_value (data->invocation,
			                                       g_variant_new ("(sss)", "", "", ""));
		}

		tracker_dbus_request_end (data->request, NULL);
	} else {
		GError *error = NULL;

		g_message ("Controller thread (%p) got error back", g_thread_self ());
		g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), &error);
		tracker_dbus_request_end (data->request, error);
		g_dbus_method_invocation_return_gerror (data->invocation, error);
		g_error_free (error);
	}

	metadata_data_free (data);
}

static void
handle_method_call_get_metadata (TrackerController     *controller,
                                 GDBusMethodInvocation *invocation,
                                 GVariant              *parameters)
{
	TrackerControllerPrivate *priv;
	GetMetadataData *data;
	TrackerDBusRequest *request;
	const gchar *uri, *mime;

	priv = controller->priv;
	g_variant_get (parameters, "(&s&s)", &uri, &mime);

	reset_shutdown_timeout (controller);
	request = tracker_dbus_request_begin (NULL, "%s (%s, %s)", __FUNCTION__, uri, mime);

	data = metadata_data_new (controller, uri, mime, invocation, request);
	tracker_extract_file (priv->extractor, uri, mime, data->cancellable,
	                      get_metadata_cb, data);
}

static void
get_metadata_fast_cb (GObject      *object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
	GetMetadataData *data;
	TrackerExtractInfo *info;

	data = user_data;
	info = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));

	if (info) {
		GOutputStream *unix_output_stream;
		GOutputStream *buffered_output_stream;
		GDataOutputStream *data_output_stream;
		GError *error = NULL;

		g_message ("Controller thread (%p) got metadata back", g_thread_self ());

		unix_output_stream = g_unix_output_stream_new (data->fd, TRUE);
		buffered_output_stream = g_buffered_output_stream_new_sized (unix_output_stream,
		                                                             64 * 1024);
		data_output_stream = g_data_output_stream_new (buffered_output_stream);
		g_data_output_stream_set_byte_order (G_DATA_OUTPUT_STREAM (data_output_stream),
		                                     G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN);

		if (tracker_sparql_builder_get_length (info->statements) > 0) {
			const gchar *preupdate_str = NULL;

			if (tracker_sparql_builder_get_length (info->preupdate) > 0) {
				preupdate_str = tracker_sparql_builder_get_result (info->preupdate);
			}

			g_data_output_stream_put_string (data_output_stream,
			                                 preupdate_str ? preupdate_str : "",
			                                 NULL,
			                                 &error);

			if (!error) {
				g_data_output_stream_put_byte (data_output_stream,
				                               0,
				                               NULL,
				                               &error);
			}

			if (!error) {
				g_data_output_stream_put_string (data_output_stream,
				                                 tracker_sparql_builder_get_result (info->statements),
				                                 NULL,
				                                 &error);
			}

			if (!error) {
				g_data_output_stream_put_byte (data_output_stream,
				                               0,
				                               NULL,
				                               &error);
			}

			if (!error && info->where) {
				g_data_output_stream_put_string (data_output_stream,
				                                 info->where,
				                                 NULL,
				                                 &error);
			}

			if (!error) {
				g_data_output_stream_put_byte (data_output_stream,
				                               0,
				                               NULL,
				                               &error);
			}
		}

		g_object_unref (data_output_stream);
		g_object_unref (buffered_output_stream);
		g_object_unref (unix_output_stream);

		if (error) {
			tracker_dbus_request_end (data->request, error);
			g_dbus_method_invocation_return_gerror (data->invocation, error);
			g_error_free (error);
		} else {
			tracker_dbus_request_end (data->request, NULL);
			g_dbus_method_invocation_return_value (data->invocation, NULL);
		}
	} else {
		GError *error = NULL;

		g_message ("Controller thread (%p) got error back", g_thread_self ());
		g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), &error);
		tracker_dbus_request_end (data->request, error);
		g_dbus_method_invocation_return_gerror (data->invocation, error);
		g_error_free (error);

		close (data->fd);
	}

	metadata_data_free (data);
}

static void
handle_method_call_get_metadata_fast (TrackerController	    *controller,
                                      GDBusMethodInvocation *invocation,
                                      GVariant		    *parameters)
{
	GDBusConnection *connection;
	GDBusMessage *method_message;
	TrackerDBusRequest *request;

	connection = g_dbus_method_invocation_get_connection (invocation);
	method_message = g_dbus_method_invocation_get_message (invocation);

	reset_shutdown_timeout (controller);
	request = tracker_dbus_request_begin (NULL, "%s", __FUNCTION__);

	if (g_dbus_connection_get_capabilities (connection) & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING) {
		TrackerControllerPrivate *priv;
		GetMetadataData *data;
		const gchar *uri, *mime;
		gint index_fd, fd;
		GUnixFDList *fd_list;
		GError *error = NULL;

		priv = controller->priv;
		g_variant_get (parameters, "(&s&sh)", &uri, &mime, &index_fd);

		fd_list = g_dbus_message_get_unix_fd_list (method_message);

		if ((fd = g_unix_fd_list_get (fd_list, index_fd, &error)) != -1) {
			data = metadata_data_new (controller, uri, mime, invocation, request);
			data->fd = fd;

			tracker_extract_file (priv->extractor, uri, mime, data->cancellable,
			                      get_metadata_fast_cb, data);
		} else {
			tracker_dbus_request_end (request, error);
			g_dbus_method_invocation_return_dbus_error (invocation,
			                                            TRACKER_EXTRACT_SERVICE ".GetMetadataFastError",
			                                            "No FD list");
			g_error_free (error);
		}
	} else {
		tracker_dbus_request_end (request, NULL);
		g_dbus_method_invocation_return_dbus_error (invocation,
		                                            TRACKER_EXTRACT_SERVICE ".GetMetadataFastError",
		                                            "No FD passing capabilities");
	}
}

static void
handle_method_call (GDBusConnection	  *connection,
		    const gchar		  *sender,
		    const gchar		  *object_path,
		    const gchar		  *interface_name,
		    const gchar		  *method_name,
		    GVariant		  *parameters,
		    GDBusMethodInvocation *invocation,
		    gpointer		   user_data)
{
	TrackerController *controller = user_data;

	if (g_strcmp0 (method_name, "GetPid") == 0) {
		handle_method_call_get_pid (controller, invocation, parameters);
	} else if (g_strcmp0 (method_name, "GetMetadataFast") == 0) {
		handle_method_call_get_metadata_fast (controller, invocation, parameters);
	} else if (g_strcmp0 (method_name, "GetMetadata") == 0) {
		handle_method_call_get_metadata (controller, invocation, parameters);
	} else {
		g_warning ("Unknown method '%s' called", method_name);
	}
}

static gboolean
tracker_controller_dbus_start (TrackerController  *controller,
			       GError		 **error)
{
	TrackerControllerPrivate *priv;
	GError *err = NULL;
	GDBusInterfaceVTable interface_vtable = {
		handle_method_call,
		NULL, NULL
	};

	priv = controller->priv;
	priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &err);

	if (!priv->connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            err ? err->message : "no error given.");
		g_propagate_error (error, err);
		return FALSE;
	}

	priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, &err);
	if (!priv->introspection_data) {
		g_critical ("Could not create node info from introspection XML, %s",
		            err ? err->message : "no error given.");
		g_propagate_error (error, err);
		return FALSE;
	}

	g_message ("Registering D-Bus object...");
	g_message ("  Path:'" TRACKER_EXTRACT_PATH "'");
	g_message ("  Object Type:'%s'", G_OBJECT_TYPE_NAME (controller));

	g_bus_own_name_on_connection (priv->connection,
	                              TRACKER_EXTRACT_SERVICE,
	                              G_BUS_NAME_OWNER_FLAGS_NONE,
	                              NULL, NULL, NULL, NULL);

	priv->registration_id =
		g_dbus_connection_register_object (priv->connection,
		                                   TRACKER_EXTRACT_PATH,
		                                   priv->introspection_data->interfaces[0],
		                                   &interface_vtable,
		                                   controller,
		                                   NULL,
		                                   &err);

	if (err) {
		g_critical ("Could not register the D-Bus object "TRACKER_EXTRACT_PATH", %s",
		            err ? err->message : "no error given.");
		g_propagate_error (error, err);
		return FALSE;
	}

	return TRUE;
}

static void
tracker_controller_dbus_stop (TrackerController *controller)
{
	TrackerControllerPrivate *priv;

	priv = controller->priv;

	if (priv->registration_id != 0) {
		g_dbus_connection_unregister_object (priv->connection,
		                                     priv->registration_id);
	}

	if (priv->introspection_data) {
		g_dbus_node_info_unref (priv->introspection_data);
	}

	if (priv->connection) {
		g_object_unref (priv->connection);
	}
}

TrackerController *
tracker_controller_new (TrackerExtract  *extractor,
                        guint	         shutdown_timeout,
			GError         **error)
{
	return g_initable_new (TRACKER_TYPE_CONTROLLER,
	                       NULL, error,
	                       "extractor", extractor,
	                       "shutdown-timeout", shutdown_timeout,
	                       NULL);
}

static gpointer
tracker_controller_thread_func (gpointer user_data)
{
	TrackerController *controller;
	TrackerControllerPrivate *priv;
	gboolean success;

	g_message ("Controller thread '%p' created, dispatching...", g_thread_self ());

	controller = user_data;
	priv = controller->priv;
	g_main_context_push_thread_default (priv->context);

	reset_shutdown_timeout (controller);

	success = tracker_controller_dbus_start (controller, &priv->initialization_error);
	priv->initialized = TRUE;

	/* Notify about the initialization */
	g_mutex_lock (priv->initialization_mutex);
	g_cond_signal (priv->initialization_cond);
	g_mutex_unlock (priv->initialization_mutex);

	if (!success) {
		/* Return here, the main thread will
		 * be notified about the error and exit
		 */
		return NULL;
	}

	g_main_loop_run (priv->main_loop);

	g_message ("Shutting down...");

	g_object_unref (controller);

	/* This is where tracker-extract exits, be it
	 * either through umount events on monitored
	 * files' volumes or the timeout being reached
	 */
	exit (0);
	return NULL;
}

gboolean
tracker_controller_start (TrackerController  *controller,
                          GError            **error)
{
	TrackerControllerPrivate *priv;

	if (!g_thread_create (tracker_controller_thread_func,
	                      controller, FALSE, error)) {
		return FALSE;
	}

	priv = controller->priv;

	g_message ("Waiting for controller thread to initialize...");

	/* Wait for the controller thread to notify initialization */
	g_mutex_lock (priv->initialization_mutex);

	while (!priv->initialized) {
		g_cond_wait (priv->initialization_cond, priv->initialization_mutex);
	}

	g_mutex_unlock (priv->initialization_mutex);

	/* If there was any error resulting from initialization, propagate it */
	if (priv->initialization_error != NULL) {
		g_propagate_error (error, priv->initialization_error);
		return FALSE;
	}

	g_message ("Controller thread initialized");

	return TRUE;
}
