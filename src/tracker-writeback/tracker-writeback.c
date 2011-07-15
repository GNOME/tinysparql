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

#include "tracker-writeback.h"
#include "tracker-writeback-module.h"

#include <libtracker-common/tracker-common.h>
#include <libtracker-miner/tracker-miner.h>
#include <libtracker-sparql/tracker-sparql.h>

#include <gio/gio.h>

#ifdef STAYALIVE_ENABLE_TRACE
#warning Stayalive traces enabled
#endif /* STAYALIVE_ENABLE_TRACE */

#define THREAD_ENABLE_TRACE

#ifdef THREAD_ENABLE_TRACE
#warning Controller thread traces enabled
#endif /* THREAD_ENABLE_TRACE */

typedef struct {
	GMainContext *context;
	GMainLoop *main_loop;

	TrackerStorage *storage;

	GDBusConnection *d_connection;
	GDBusNodeInfo *introspection_data;
	guint registration_id;
	guint bus_name_id;

	GList *ongoing_tasks;

	guint shutdown_timeout;
	GSource *shutdown_source;

	GCond *initialization_cond;
	GMutex *initialization_mutex;
	GError *initialization_error;

	guint initialized : 1;

	GHashTable *modules;
	TrackerSparqlConnection *connection;
} TrackerControllerPrivate;

typedef struct {
	TrackerController *controller;
	GCancellable *cancellable;
	GDBusMethodInvocation *invocation;
	TrackerDBusRequest *request;
	gchar *subject;
	GPtrArray *results;
	TrackerSparqlConnection *connection;
	TrackerWriteback *writeback;
} WritebackData;

#define TRACKER_WRITEBACK_SERVICE   "org.freedesktop.Tracker1.Writeback"
#define TRACKER_WRITEBACK_PATH      "/org/freedesktop/Tracker1/Writeback"
#define TRACKER_WRITEBACK_INTERFACE "org.freedesktop.Tracker1.Writeback"

static const gchar *introspection_xml =
	"<node>"
	"  <interface name='org.freedesktop.Tracker1.Writeback'>"
	"    <method name='GetPid'>"
	"      <arg type='i' name='value' direction='out' />"
	"    </method>"
	"    <method name='PerformWriteback'>"
	"      <arg type='s' name='subject' direction='in' />"
	"      <arg type='as' name='rdf_types' direction='in' />"
	"      <arg type='aas' name='results' direction='in' />"
	"    </method>"
	"    <method name='CancelTasks'>"
	"      <arg type='as' name='subjects' direction='in' />"
	"    </method>"
	"  </interface>"
	"</node>";

enum {
	PROP_0,
	PROP_SHUTDOWN_TIMEOUT,
};

static void     tracker_controller_initable_iface_init  (GInitableIface     *iface);
static gboolean tracker_controller_dbus_start           (TrackerController  *controller,
                                                         GError            **error);
static void     tracker_controller_dbus_stop            (TrackerController  *controller);
static gboolean tracker_controller_start                (TrackerController  *controller,
                                                         GError            **error);

G_DEFINE_TYPE_WITH_CODE (TrackerController, tracker_controller, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                tracker_controller_initable_iface_init));

static gboolean
tracker_controller_initable_init (GInitable     *initable,
                                  GCancellable  *cancellable,
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

	if (priv->shutdown_source) {
		g_source_destroy (priv->shutdown_source);
		priv->shutdown_source = NULL;
	}

	tracker_controller_dbus_stop (controller);

	g_object_unref (priv->storage);
	g_hash_table_unref (priv->modules);

	g_main_loop_unref (priv->main_loop);
	g_main_context_unref (priv->context);

	g_cond_free (priv->initialization_cond);
	g_mutex_free (priv->initialization_mutex);

	G_OBJECT_CLASS (tracker_controller_parent_class)->finalize (object);
}

static void
tracker_controller_get_property (GObject    *object,
                                 guint       param_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
	TrackerControllerPrivate *priv = TRACKER_CONTROLLER (object)->priv;

	switch (param_id) {
	case PROP_SHUTDOWN_TIMEOUT:
		g_value_set_uint (value, priv->shutdown_timeout);
		break;
	}
}

static void
tracker_controller_set_property (GObject      *object,
                                 guint         param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
	TrackerControllerPrivate *priv = TRACKER_CONTROLLER (object)->priv;

	switch (param_id) {
	case PROP_SHUTDOWN_TIMEOUT:
		priv->shutdown_timeout = g_value_get_uint (value);
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

	g_type_class_add_private (object_class, sizeof (TrackerControllerPrivate));
}

static WritebackData *
writeback_data_new (TrackerController       *controller,
                    TrackerWriteback        *writeback,
                    TrackerSparqlConnection *connection,
                    const gchar             *subject,
                    GPtrArray               *results,
                    GDBusMethodInvocation   *invocation,
                    TrackerDBusRequest      *request)
{
	WritebackData *data;

	data = g_slice_new (WritebackData);
	data->cancellable = g_cancellable_new ();
	data->controller = g_object_ref (controller);
	data->subject = g_strdup (subject);
	data->results = g_ptr_array_ref (results);
	data->invocation = invocation;
	data->connection = g_object_ref (connection);
	data->writeback = g_object_ref (writeback);
	data->request = request;

	return data;
}

static void
writeback_data_free (WritebackData *data)
{
	/* We rely on data->invocation being freed through
	 * the g_dbus_method_invocation_return_* methods
	 */
	g_free (data->subject);
	g_object_unref (data->connection);
	g_object_unref (data->writeback);
	g_ptr_array_unref (data->results);
	g_object_unref (data->cancellable);
	g_slice_free (WritebackData, data);
}

static void
cancel_tasks (TrackerController *controller,
              const gchar       *subject,
              GFile             *file)
{
	TrackerControllerPrivate *priv;
	GList *elem;

	priv = controller->priv;

	for (elem = priv->ongoing_tasks; elem; elem = elem->next) {
		WritebackData *data = elem->data;

		if (g_strcmp0 (subject, data->subject) == 0) {
			g_message ("Cancelling not yet processed task ('%s')",
			           data->subject);
			g_cancellable_cancel (data->cancellable);
		}

		if (file) {
			guint i;
			for (i = 0; i < data->results->len; i++) {
				GStrv row = g_ptr_array_index (data->results, i);
				if (row[0] != NULL) {
					GFile *task_file;
					task_file = g_file_new_for_uri (row[0]);
					if (g_file_equal (task_file, file) ||
					    g_file_has_prefix (task_file, file)) {
						/* Mount path contains some file being processed */
						g_message ("Cancelling task ('%s')", row[0]);
						g_cancellable_cancel (data->cancellable);
					}
					g_object_unref (task_file);
				}
			}
		}
	}
}

static void
mount_point_removed_cb (TrackerStorage *storage,
                        const gchar    *uuid,
                        const gchar    *mount_point,
                        gpointer        user_data)
{
	GFile *mount_file;

	mount_file = g_file_new_for_path (mount_point);
	cancel_tasks (TRACKER_CONTROLLER (user_data), NULL, mount_file);
	g_object_unref (mount_file);
}

static gboolean
reset_shutdown_timeout_cb (gpointer user_data)
{
	TrackerControllerPrivate *priv;

#ifdef STAYALIVE_ENABLE_TRACE
	g_debug ("Stayalive --- time has expired");
#endif /* STAYALIVE_ENABLE_TRACE */

	g_message ("Shutting down due to no activity");

	priv = TRACKER_CONTROLLER (user_data)->priv;
	g_main_loop_quit (priv->main_loop);

	return FALSE;
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

#ifdef STAYALIVE_ENABLE_TRACE
	g_debug ("Stayalive --- (Re)setting timeout");
#endif /* STAYALIVE_ENABLE_TRACE */

	if (priv->shutdown_source) {
		g_source_destroy (priv->shutdown_source);
		priv->shutdown_source = NULL;
	}

	source = g_timeout_source_new_seconds (priv->shutdown_timeout);
	g_source_set_callback (source,
	                       reset_shutdown_timeout_cb,
	                       controller, NULL);

	g_source_attach (source, priv->context);
	priv->shutdown_source = source;
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
	g_signal_connect (priv->storage, "mount-point-removed",
	                  G_CALLBACK (mount_point_removed_cb), controller);

	priv->initialization_cond = g_cond_new ();
	priv->initialization_mutex = g_mutex_new ();
}

static void
handle_method_call_get_pid (TrackerController     *controller,
                            GDBusMethodInvocation *invocation,
                            GVariant              *parameters)
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

static gboolean
perform_writeback_cb (gpointer user_data)
{
	TrackerControllerPrivate *priv;
	WritebackData *data;

	data = user_data;
	priv = data->controller->priv;
	priv->ongoing_tasks = g_list_remove (priv->ongoing_tasks, data);
	g_dbus_method_invocation_return_value (data->invocation, NULL);
	tracker_dbus_request_end (data->request, NULL);
	writeback_data_free (data);

	return FALSE;
}

static gboolean
sparql_rdf_types_match (const gchar * const *module_types,
                        const gchar * const *rdf_types)
{
	guint n;

	for (n = 0; rdf_types[n] != NULL; n++) {
		guint i;

		for (i = 0; module_types[i] != NULL; i++) {
			if (g_strcmp0 (module_types[i], rdf_types[n]) == 0) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

static gboolean
io_writeback_job (GIOSchedulerJob *job,
                  GCancellable    *cancellable,
                  gpointer         user_data)
{
	WritebackData *data = user_data;

	tracker_writeback_update_metadata (data->writeback,
	                                   data->results,
	                                   data->connection,
	                                   data->cancellable);

	g_idle_add (perform_writeback_cb, data);

	return FALSE;
}

static void
handle_method_call_perform_writeback (TrackerController     *controller,
                                      GDBusMethodInvocation *invocation,
                                      GVariant              *parameters)
{
	TrackerControllerPrivate *priv;
	TrackerDBusRequest *request;
	const gchar *subject;
	GPtrArray *results = NULL;
	GHashTableIter iter;
	gpointer key, value;
	GVariantIter *iter1, *iter2, *iter3;
	GArray *rdf_types_array;
	GStrv rdf_types;
	gchar *rdf_type = NULL;

	priv = controller->priv;

	results = g_ptr_array_new_with_free_func ((GDestroyNotify) g_strfreev);
	g_variant_get (parameters, "(&sasaas)", &subject, &iter1, &iter2);

	rdf_types_array = g_array_new (TRUE, TRUE, sizeof (gchar *));
	while (g_variant_iter_loop (iter1, "&s", &rdf_type)) {
		g_array_append_val (rdf_types_array, rdf_type);
	}

	rdf_types = (GStrv) rdf_types_array->data;
	g_array_free (rdf_types_array, FALSE);

	while (g_variant_iter_loop (iter2, "as", &iter3)) {
		GArray *row_array = g_array_new (TRUE, TRUE, sizeof (gchar *));
		gchar *cell = NULL;

		while (g_variant_iter_loop (iter3, "&s", &cell)) {
			g_array_append_val (row_array, cell);
		}

		g_ptr_array_add (results, row_array->data);
		g_array_free (row_array, FALSE);
	}

	g_variant_iter_free (iter1);
	g_variant_iter_free (iter2);

	reset_shutdown_timeout (controller);
	request = tracker_dbus_request_begin (NULL, "%s (%s)", __FUNCTION__, subject);

	g_hash_table_iter_init (&iter, priv->modules);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		TrackerWritebackModule *module;
		const gchar * const *module_types;

		module = value;
		module_types = tracker_writeback_module_get_rdf_types (module);

		if (sparql_rdf_types_match (module_types, (const gchar * const *) rdf_types)) {
			WritebackData *data;
			TrackerWriteback *writeback;

			g_message ("  Updating metadata for subject:'%s' using module:'%s'",
			           subject,
			           module->name);

			writeback = tracker_writeback_module_create (module);
			data = writeback_data_new (controller,
			                           writeback,
			                           priv->connection,
			                           subject,
			                           results,
			                           invocation,
			                           request);

			g_io_scheduler_push_job (io_writeback_job, data, NULL, 0,
			                         data->cancellable);

			g_object_unref (writeback);
		}
	}

	g_free (rdf_types);
}

static void
handle_method_call_cancel_tasks (TrackerController     *controller,
                                 GDBusMethodInvocation *invocation,
                                 GVariant              *parameters)
{
	TrackerDBusRequest *request;
	gchar **subjects;
	gint i;

#ifdef THREAD_ENABLE_TRACE
	g_debug ("Thread:%p (Controller) --> Got Tasks cancellation request",
		 g_thread_self ());
#endif /* THREAD_ENABLE_TRACE */


	g_variant_get (parameters, "(^as)", &subjects);

	request = tracker_dbus_request_begin (NULL, "%s (%s, ...)", __FUNCTION__, subjects[0]);

	for (i = 0; subjects[i] != NULL; i++) {
		cancel_tasks (controller, subjects[i], NULL);
	}

	g_strfreev (subjects);
	tracker_dbus_request_end (request, NULL);
	g_dbus_method_invocation_return_value (invocation, NULL);
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
	TrackerController *controller = user_data;

	if (g_strcmp0 (method_name, "GetPid") == 0) {
		handle_method_call_get_pid (controller, invocation, parameters);
	} else if (g_strcmp0 (method_name, "PerformWriteback") == 0) {
		handle_method_call_perform_writeback (controller, invocation, parameters);
	} else if (g_strcmp0 (method_name, "CancelTasks") == 0) {
		handle_method_call_cancel_tasks (controller, invocation, parameters);
	} else {
		g_warning ("Unknown method '%s' called", method_name);
	}
}

static void
controller_notify_main_thread (TrackerController *controller,
                               GError            *error)
{
	TrackerControllerPrivate *priv;

	priv = controller->priv;

	g_mutex_lock (priv->initialization_mutex);

	priv->initialized = TRUE;
	priv->initialization_error = error;

	/* Notify about the initialization */
	g_cond_signal (priv->initialization_cond);

	g_mutex_unlock (priv->initialization_mutex);
}

static void
bus_name_acquired_cb (GDBusConnection *connection,
                      const gchar     *name,
                      gpointer         user_data)
{
	controller_notify_main_thread (TRACKER_CONTROLLER (user_data), NULL);
}

static void
bus_name_vanished_cb (GDBusConnection *connection,
                      const gchar     *name,
                      gpointer         user_data)
{
	TrackerController *controller;
	TrackerControllerPrivate *priv;

	controller = user_data;
	priv = controller->priv;

	if (!priv->initialized) {
		GError *error;

		error = g_error_new_literal (TRACKER_DBUS_ERROR, 0,
		                             "Could not acquire bus name, "
		                             "perhaps it's already taken?");
		controller_notify_main_thread (controller, error);
	} else {
		/* We're already in control of the program
		 * lifetime, so just quit the mainloop
		 */
		g_main_loop_quit (priv->main_loop);
	}
}

static gboolean
tracker_controller_dbus_start (TrackerController   *controller,
                               GError             **error)
{
	TrackerControllerPrivate *priv;
	GError *err = NULL;
	GDBusInterfaceVTable interface_vtable = {
		handle_method_call,
		NULL, NULL
	};

	priv = controller->priv;

	priv->connection = tracker_sparql_connection_get (NULL, &err);

	if (!priv->connection) {
		g_propagate_error (error, err);
		return FALSE;
	}

	priv->d_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &err);

	if (!priv->d_connection) {
		g_propagate_error (error, err);
		return FALSE;
	}

	priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, &err);
	if (!priv->introspection_data) {
		g_propagate_error (error, err);
		return FALSE;
	}

	g_message ("Registering D-Bus object...");
	g_message ("  Path:'" TRACKER_WRITEBACK_PATH "'");
	g_message ("  Object Type:'%s'", G_OBJECT_TYPE_NAME (controller));

	priv->bus_name_id =
		g_bus_own_name_on_connection (priv->d_connection,
		                              TRACKER_WRITEBACK_SERVICE,
		                              G_BUS_NAME_OWNER_FLAGS_NONE,
		                              bus_name_acquired_cb,
		                              bus_name_vanished_cb,
		                              controller, NULL);

	priv->registration_id =
		g_dbus_connection_register_object (priv->d_connection,
		                                   TRACKER_WRITEBACK_PATH,
		                                   priv->introspection_data->interfaces[0],
		                                   &interface_vtable,
		                                   controller,
		                                   NULL,
		                                   &err);

	if (err) {
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
		g_dbus_connection_unregister_object (priv->d_connection,
		                                     priv->registration_id);
	}

	if (priv->bus_name_id != 0) {
		g_bus_unown_name (priv->bus_name_id);
	}

	if (priv->introspection_data) {
		g_dbus_node_info_unref (priv->introspection_data);
	}

	if (priv->d_connection) {
		g_object_unref (priv->d_connection);
	}

	if (priv->connection) {
		g_object_unref (priv->connection);
	}
}

TrackerController *
tracker_controller_new (guint             shutdown_timeout,
                        GError          **error)
{
	return g_initable_new (TRACKER_TYPE_CONTROLLER,
	                       NULL, error,
	                       "shutdown-timeout", shutdown_timeout,
	                       NULL);
}

static gpointer
tracker_controller_thread_func (gpointer user_data)
{
	TrackerController *controller;
	TrackerControllerPrivate *priv;
	GError *error = NULL;

#ifdef THREAD_ENABLE_TRACE
	g_debug ("Thread:%p (Controller) --- Created, dispatching...",
	         g_thread_self ());
#endif /* THREAD_ENABLE_TRACE */

	controller = user_data;
	priv = controller->priv;
	g_main_context_push_thread_default (priv->context);

	reset_shutdown_timeout (controller);

	if (!tracker_controller_dbus_start (controller, &error)) {
		/* Error has been filled in, so we return
		 * in this thread. The main thread will be
		 * notified about the error and exit.
		 */
		controller_notify_main_thread (controller, error);
		return NULL;
	}

	g_main_loop_run (priv->main_loop);

#ifdef THREAD_ENABLE_TRACE
	g_debug ("Thread:%p (Controller) --- Shutting down...",
	         g_thread_self ());
#endif /* THREAD_ENABLE_TRACE */

	g_object_unref (controller);

	/* This is where we exit, be it
	 * either through umount events on monitored
	 * files' volumes or the timeout being reached
	 */
	exit (0);
	return NULL;
}

static gboolean
tracker_controller_start (TrackerController  *controller,
                          GError            **error)
{
	TrackerControllerPrivate *priv;
	GList *modules;

	priv = controller->priv;

	priv->modules = g_hash_table_new_full (g_str_hash,
	                                       g_str_equal,
	                                       (GDestroyNotify) g_free,
	                                       NULL);

	modules = tracker_writeback_modules_list ();

	while (modules) {
		TrackerWritebackModule *module;
		const gchar *path;

		path = modules->data;
		module = tracker_writeback_module_get (path);

		if (module) {
			g_hash_table_insert (priv->modules, g_strdup (path), module);
		}

		modules = modules->next;
	}

	if (!g_thread_create (tracker_controller_thread_func,
	                      controller, FALSE, error)) {
		return FALSE;
	}


#ifdef THREAD_ENABLE_TRACE
	g_debug ("Thread:%p (Controller) --- Waiting for controller thread to initialize...",
	         g_thread_self ());
#endif /* THREAD_ENABLE_TRACE */

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

#ifdef THREAD_ENABLE_TRACE
	g_debug ("Thread:%p (Controller) --- Initialized",
	         g_thread_self ());
#endif /* THREAD_ENABLE_TRACE */

	return TRUE;
}
