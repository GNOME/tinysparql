/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-miner/tracker-miner-dbus.h>

#include "tracker-writeback-dispatcher.h"

#define TRACKER_WRITEBACK_SERVICE   "org.freedesktop.Tracker1.Writeback"
#define TRACKER_WRITEBACK_PATH      "/org/freedesktop/Tracker1/Writeback"
#define TRACKER_WRITEBACK_INTERFACE "org.freedesktop.Tracker1.Writeback"

typedef struct {
	GFile *file;
	TrackerMinerFS *fs;
	GPtrArray *results;
	GStrv rdf_types;
	TrackerWritebackDispatcher *self; /* weak */
	GCancellable *cancellable;
	guint cancel_id;
	guint retry_timeout;
	guint retries;
} WritebackFileData;

typedef struct {
	TrackerMinerFiles *files_miner;
	GDBusConnection *d_connection;
	TrackerSparqlConnection *connection;
	gulong signal_id;
} TrackerWritebackDispatcherPrivate;

enum {
	PROP_0,
	PROP_FILES_MINER
};

#define TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_WRITEBACK_DISPATCHER, TrackerWritebackDispatcherPrivate))

static void     writeback_dispatcher_set_property    (GObject              *object,
                                                      guint                 param_id,
                                                      const GValue         *value,
                                                      GParamSpec           *pspec);
static void     writeback_dispatcher_get_property    (GObject              *object,
                                                      guint                 param_id,
                                                      GValue               *value,
                                                      GParamSpec           *pspec);
static void     writeback_dispatcher_finalize        (GObject              *object);
static gboolean writeback_dispatcher_initable_init   (GInitable            *initable,
                                                      GCancellable         *cancellable,
                                                      GError              **error);
static gboolean writeback_dispatcher_writeback_file  (TrackerMinerFS       *fs,
                                                      GFile                *file,
                                                      GStrv                 rdf_types,
                                                      GPtrArray            *results,
                                                      GCancellable         *cancellable,
                                                      gpointer              user_data);
static void     self_weak_notify                     (gpointer              data,
                                                      GObject              *where_the_object_was);

static void
writeback_dispatcher_initable_iface_init (GInitableIface *iface)
{
	iface->init = writeback_dispatcher_initable_init;
}

G_DEFINE_TYPE_WITH_CODE (TrackerWritebackDispatcher, tracker_writeback_dispatcher, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                writeback_dispatcher_initable_iface_init));

static void
tracker_writeback_dispatcher_class_init (TrackerWritebackDispatcherClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = writeback_dispatcher_finalize;
	object_class->set_property = writeback_dispatcher_set_property;
	object_class->get_property = writeback_dispatcher_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_FILES_MINER,
	                                 g_param_spec_object ("files_miner",
	                                                      "files_miner",
	                                                      "The FS Miner",
	                                                      TRACKER_TYPE_MINER_FILES,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (klass, sizeof (TrackerWritebackDispatcherPrivate));
}

static void
writeback_dispatcher_set_property (GObject      *object,
                                   guint         param_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	TrackerWritebackDispatcherPrivate *priv;

	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_FILES_MINER:
		priv->files_miner = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}


static void
writeback_dispatcher_get_property (GObject    *object,
                                   guint       param_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
	TrackerWritebackDispatcherPrivate *priv;

	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_FILES_MINER:
		g_value_set_object (value, priv->files_miner);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
writeback_dispatcher_finalize (GObject *object)
{
	TrackerWritebackDispatcherPrivate *priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (object);

	if (priv->signal_id != 0 && g_signal_handler_is_connected (object, priv->signal_id)) {
		g_signal_handler_disconnect (object, priv->signal_id);
	}

	if (priv->connection) {
		g_object_unref (priv->connection);
	}

	if (priv->d_connection) {
		g_object_unref (priv->d_connection);
	}

	if (priv->files_miner) {
		g_object_unref (priv->files_miner);
	}
}

static void
tracker_writeback_dispatcher_init (TrackerWritebackDispatcher *object)
{
}

static gboolean
writeback_dispatcher_initable_init (GInitable    *initable,
                                    GCancellable *cancellable,
                                    GError       **error)
{
	TrackerWritebackDispatcherPrivate *priv;
	GError *internal_error = NULL;

	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (initable);

	priv->connection = tracker_sparql_connection_get (NULL, &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	priv->d_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	priv->signal_id = g_signal_connect_object (priv->files_miner,
	                                           "writeback-file",
	                                           G_CALLBACK (writeback_dispatcher_writeback_file),
	                                           initable,
	                                           G_CONNECT_AFTER);

	return TRUE;
}

TrackerWritebackDispatcher *
tracker_writeback_dispatcher_new (TrackerMinerFiles  *miner_files,
                                  GError            **error)
{
	GObject *miner;
	GError *internal_error = NULL;

	miner =  g_initable_new (TRACKER_TYPE_WRITEBACK_DISPATCHER,
	                         NULL,
	                         &internal_error,
	                         "files-miner", miner_files,
	                         NULL);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return NULL;
	}

	return (TrackerWritebackDispatcher *) miner;
}

static void
writeback_file_data_free (WritebackFileData *data)
{
	if (data->self) {
		g_object_weak_unref (G_OBJECT (data->self), self_weak_notify, data);
	}

	g_object_unref (data->fs);
	g_object_unref (data->file);
	g_strfreev (data->rdf_types);
	g_ptr_array_unref (data->results);
	g_cancellable_disconnect (data->cancellable, data->cancel_id);
	g_object_unref (data->cancellable);
	g_free (data);
}

static void
self_weak_notify (gpointer data, GObject *where_the_object_was)
{
	WritebackFileData *udata = data;

	/* Shut down while retrying writeback */
	g_debug ("Shutdown while retrying WRITEBACK after unmount, not retrying anymore");

	if (udata->retry_timeout != 0) {
		g_source_remove (udata->retry_timeout);
	}

	udata->self = NULL;
	writeback_file_data_free (udata);
}

static gboolean
retry_idle (gpointer user_data)
{
	WritebackFileData *data = user_data;

	tracker_miner_fs_writeback_file (data->fs,
	                                 data->file,
	                                 data->rdf_types,
	                                 data->results);

	writeback_file_data_free (data);

	return FALSE;
}

static void
writeback_file_finished  (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
	WritebackFileData *data = user_data;
	GError *error = NULL;

	g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
	                               res, &error);

	if (error && error->code == G_DBUS_ERROR_NO_REPLY && data->retries < 5) {
		/* This happens in case of exit() of the tracker-writeback binary, which
		 * happens on unmount of the FS event, for example */

		g_debug ("Retry WRITEBACK after unmount");
		tracker_miner_fs_writeback_notify (data->fs, data->file, NULL);

		data->retry_timeout = g_timeout_add_seconds (5, retry_idle, data);
		data->retries++;

	} else {
		tracker_miner_fs_writeback_notify (data->fs, data->file, error);
		writeback_file_data_free (data);
	}
}

static void
writeback_cancel_remote_operation (GCancellable      *cancellable,
                                   WritebackFileData *data)
{
	TrackerWritebackDispatcherPrivate *priv;
	GDBusMessage *message;
        gchar *uris[2];

	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (data->self);

	uris[0] = g_file_get_uri (data->file);
	uris[1] = NULL;

	message = g_dbus_message_new_method_call (TRACKER_WRITEBACK_SERVICE,
	                                          TRACKER_WRITEBACK_PATH,
	                                          TRACKER_WRITEBACK_INTERFACE,
	                                          "CancelTasks");

	g_dbus_message_set_body (message, g_variant_new ("(^as)", uris));
	g_dbus_connection_send_message (priv->d_connection, message,
	                                G_DBUS_SEND_MESSAGE_FLAGS_NONE,
	                                NULL, NULL);
	g_free (uris[0]);
}

static gboolean
writeback_dispatcher_writeback_file (TrackerMinerFS *fs,
                                     GFile          *file,
                                     GStrv           rdf_types,
                                     GPtrArray      *results,
                                     GCancellable   *cancellable,
                                     gpointer        user_data)
{
	TrackerWritebackDispatcher *self = user_data;
	TrackerWritebackDispatcherPrivate *priv;
	gchar *uri;
	guint i;
	GVariantBuilder builder;
	WritebackFileData *data = g_new (WritebackFileData, 1);

	priv = TRACKER_WRITEBACK_DISPATCHER_GET_PRIVATE (self);

	uri = g_file_get_uri (file);
	g_debug ("Writeback: %s", uri);

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("(sasaas)"));

	g_variant_builder_add (&builder, "s", uri);

	g_variant_builder_open (&builder, G_VARIANT_TYPE ("as"));
	for (i = 0; rdf_types[i] != NULL; i++) {
		g_variant_builder_add (&builder, "s", rdf_types[i]);
	}
	g_variant_builder_close (&builder);

	g_variant_builder_open (&builder, G_VARIANT_TYPE ("aas"));

	for (i = 0; i< results->len; i++) {
		GStrv row = g_ptr_array_index (results, i);
		guint y;

		g_variant_builder_open (&builder, G_VARIANT_TYPE ("as"));
		for (y = 0; row[y] != NULL; y++) {
			g_variant_builder_add (&builder, "s", row[y]);
		}

		g_variant_builder_close (&builder);
	}

	g_variant_builder_close (&builder);

	data->retries = 0;
	data->retry_timeout = 0;
	data->self = self;
	g_object_weak_ref (G_OBJECT (data->self), self_weak_notify, data);
	data->fs = g_object_ref (fs);
	data->file = g_object_ref (file);
	data->results = g_ptr_array_ref (results);
	data->rdf_types = g_strdupv (rdf_types);
	data->cancellable = g_object_ref (cancellable);
	data->cancel_id = g_cancellable_connect (data->cancellable,
	                                         G_CALLBACK (writeback_cancel_remote_operation),
	                                         data, NULL);

	g_dbus_connection_call (priv->d_connection,
	                        TRACKER_WRITEBACK_SERVICE,
	                        TRACKER_WRITEBACK_PATH,
	                        TRACKER_WRITEBACK_INTERFACE,
	                        "PerformWriteback",
	                        g_variant_builder_end (&builder),
	                        NULL,
	                        G_DBUS_CALL_FLAGS_NONE,
	                        -1,
	                        cancellable,
	                        (GAsyncReadyCallback) writeback_file_finished,
	                        data);

	g_free (uri);

	return TRUE;
}
