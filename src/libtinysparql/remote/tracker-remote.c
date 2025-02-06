/*
 * Copyright (C) 2016 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include "tracker-remote.h"

#include <tracker-http.h>

#include "tracker-remote-namespaces.h"
#include "tracker-remote-statement.h"

struct _TrackerRemoteConnection
{
	TrackerSparqlConnection parent_instance;
};

typedef struct _TrackerRemoteConnectionPrivate TrackerRemoteConnectionPrivate;

struct _TrackerRemoteConnectionPrivate
{
	TrackerHttpClient *client;
	TrackerNamespaceManager *namespaces;
	gchar *base_uri;
};

enum {
	PROP_0,
	PROP_BASE_URI,
	N_PROPS
};

static GParamSpec *props[N_PROPS];

G_DEFINE_TYPE_WITH_PRIVATE (TrackerRemoteConnection, tracker_remote_connection,
			    TRACKER_TYPE_SPARQL_CONNECTION)

static void
tracker_remote_connection_set_property (GObject      *object,
					guint         prop_id,
					const GValue *value,
					GParamSpec   *pspec)
{
	TrackerRemoteConnection *remote =
		TRACKER_REMOTE_CONNECTION (object);
	TrackerRemoteConnectionPrivate *priv =
		tracker_remote_connection_get_instance_private (remote);

	switch (prop_id) {
	case PROP_BASE_URI:
		priv->base_uri = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_remote_connection_get_property (GObject    *object,
					guint       prop_id,
					GValue     *value,
					GParamSpec *pspec)
{
	TrackerRemoteConnection *remote =
		TRACKER_REMOTE_CONNECTION (object);
	TrackerRemoteConnectionPrivate *priv =
		tracker_remote_connection_get_instance_private (remote);

	switch (prop_id) {
	case PROP_BASE_URI:
		g_value_set_string (value, priv->base_uri);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_remote_connection_finalize (GObject *object)
{
	TrackerRemoteConnection *remote =
		TRACKER_REMOTE_CONNECTION (object);
	TrackerRemoteConnectionPrivate *priv =
		tracker_remote_connection_get_instance_private (remote);

	g_object_unref (priv->client);
	g_free (priv->base_uri);

	G_OBJECT_CLASS (tracker_remote_connection_parent_class)->finalize (object);
}

static TrackerSparqlCursor *
tracker_remote_connection_query (TrackerSparqlConnection  *connection,
				 const gchar              *sparql,
				 GCancellable             *cancellable,
				 GError                  **error)
{
	TrackerRemoteConnection *remote =
		TRACKER_REMOTE_CONNECTION (connection);
	TrackerRemoteConnectionPrivate *priv =
		tracker_remote_connection_get_instance_private (remote);
	TrackerSerializerFormat format;
	TrackerSparqlCursor *deserializer;
	GInputStream *stream;
	guint formats =
		(1 << TRACKER_SERIALIZER_FORMAT_JSON) |
		(1 << TRACKER_SERIALIZER_FORMAT_XML);

	stream = tracker_http_client_send_message (priv->client,
						   priv->base_uri,
						   sparql, formats,
						   cancellable,
						   &format,
						   error);
	if (!stream)
		return NULL;

	deserializer = tracker_deserializer_new (stream, NULL, format);
	g_object_unref (stream);

	return deserializer;
}

static void
query_message_cb (GObject      *source,
		  GAsyncResult *res,
		  gpointer      user_data)
{
	TrackerSerializerFormat format;
	TrackerSparqlCursor *deserializer;
	GInputStream *stream;
	GTask *task = user_data;
	GError *error = NULL;

	stream = tracker_http_client_send_message_finish (TRACKER_HTTP_CLIENT (source),
							  res,
							  &format,
							  &error);
	if (stream) {
		deserializer = tracker_deserializer_new (stream, NULL, format);
		g_object_unref (stream);

		g_task_return_pointer (task, deserializer, g_object_unref);
	} else {
		g_task_return_error (task, error);
	}

	g_object_unref (task);
}

static void
tracker_remote_connection_query_async (TrackerSparqlConnection *connection,
				       const gchar             *sparql,
				       GCancellable            *cancellable,
				       GAsyncReadyCallback      callback,
				       gpointer                 user_data)
{
	TrackerRemoteConnection *remote =
		TRACKER_REMOTE_CONNECTION (connection);
	TrackerRemoteConnectionPrivate *priv =
		tracker_remote_connection_get_instance_private (remote);
	GTask *task;
	guint flags =
		(1 << TRACKER_SERIALIZER_FORMAT_JSON) |
		(1 << TRACKER_SERIALIZER_FORMAT_XML);

	task = g_task_new (connection, cancellable, callback, user_data);
	tracker_http_client_send_message_async (priv->client,
						priv->base_uri,
						sparql, flags,
						cancellable,
						query_message_cb,
						task);
}

static TrackerSparqlCursor *
tracker_remote_connection_query_finish (TrackerSparqlConnection  *connection,
					GAsyncResult             *res,
					GError                  **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static TrackerSparqlStatement *
tracker_remote_connection_query_statement (TrackerSparqlConnection  *connection,
					   const gchar              *sparql,
					   GCancellable             *cancellable,
					   GError                  **error)
{
	return tracker_remote_statement_new (connection, sparql, error);
}

static void
tracker_remote_connection_close (TrackerSparqlConnection *connection)
{
}

static void
tracker_remote_connection_close_async (TrackerSparqlConnection *connection,
				       GCancellable            *cancellable,
				       GAsyncReadyCallback      callback,
				       gpointer                 user_data)
{
	GTask *task;

	task = g_task_new (connection, cancellable, callback, user_data);
	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

static gboolean
tracker_remote_connection_close_finish (TrackerSparqlConnection  *connection,
					GAsyncResult             *res,
					GError                  **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
serialize_message_cb (GObject      *source,
		      GAsyncResult *res,
		      gpointer      user_data)
{
	GInputStream *stream;
	GTask *task = user_data;
	GError *error = NULL;

	stream = tracker_http_client_send_message_finish (TRACKER_HTTP_CLIENT (source),
							  res,
							  NULL,
							  &error);
	if (stream)
		g_task_return_pointer (task, stream, g_object_unref);
	else
		g_task_return_error (task, error);

	g_object_unref (task);
}

static void
tracker_remote_connection_serialize_async (TrackerSparqlConnection  *connection,
					   TrackerSerializeFlags     flags,
					   TrackerRdfFormat          format,
					   const gchar              *query,
					   GCancellable             *cancellable,
					   GAsyncReadyCallback      callback,
					   gpointer                 user_data)
{
	TrackerRemoteConnection *remote =
		TRACKER_REMOTE_CONNECTION (connection);
	TrackerRemoteConnectionPrivate *priv =
		tracker_remote_connection_get_instance_private (remote);
	GTask *task;
	guint formats = 0;

	if (format == TRACKER_RDF_FORMAT_TURTLE)
		formats = 1 << TRACKER_SERIALIZER_FORMAT_TTL;
	else if (format == TRACKER_RDF_FORMAT_TRIG)
		formats = 1 << TRACKER_SERIALIZER_FORMAT_TRIG;
	else if (format == TRACKER_RDF_FORMAT_JSON_LD)
		formats = 1 << TRACKER_SERIALIZER_FORMAT_JSON_LD;

	task = g_task_new (connection, cancellable, callback, user_data);
	tracker_http_client_send_message_async (priv->client,
						priv->base_uri,
						query, formats,
						cancellable,
						serialize_message_cb,
						task);
}

static GInputStream *
tracker_remote_connection_serialize_finish (TrackerSparqlConnection  *connection,
					    GAsyncResult             *res,
					    GError                  **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

static TrackerNamespaceManager *
tracker_remote_connection_get_namespace_manager (TrackerSparqlConnection *connection)
{
	TrackerRemoteConnection *remote =
		TRACKER_REMOTE_CONNECTION (connection);
	TrackerRemoteConnectionPrivate *priv =
		tracker_remote_connection_get_instance_private (remote);

	if (!priv->namespaces) {
		priv->namespaces =
			tracker_remote_namespace_manager_new (connection);
	}

	return priv->namespaces;
}

static void
tracker_remote_connection_class_init (TrackerRemoteConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerSparqlConnectionClass *conn_class =
		TRACKER_SPARQL_CONNECTION_CLASS (klass);

	object_class->set_property = tracker_remote_connection_set_property;
	object_class->get_property = tracker_remote_connection_get_property;
	object_class->finalize = tracker_remote_connection_finalize;

	conn_class->query = tracker_remote_connection_query;
	conn_class->query_async = tracker_remote_connection_query_async;
	conn_class->query_finish = tracker_remote_connection_query_finish;
	conn_class->query_statement = tracker_remote_connection_query_statement;
	conn_class->close = tracker_remote_connection_close;
	conn_class->close_async = tracker_remote_connection_close_async;
	conn_class->close_finish = tracker_remote_connection_close_finish;
	conn_class->serialize_async = tracker_remote_connection_serialize_async;
	conn_class->serialize_finish = tracker_remote_connection_serialize_finish;
	conn_class->get_namespace_manager = tracker_remote_connection_get_namespace_manager;

	props[PROP_BASE_URI] =
		g_param_spec_string ("base-uri",
				     "Base URI",
				     "Base URI",
				     NULL,
				     G_PARAM_STATIC_STRINGS |
				     G_PARAM_CONSTRUCT_ONLY |
				     G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
tracker_remote_connection_init (TrackerRemoteConnection *remote)
{
	TrackerRemoteConnectionPrivate *priv =
		tracker_remote_connection_get_instance_private (remote);

	priv->client = tracker_http_client_new ();
}

TrackerRemoteConnection *
tracker_remote_connection_new (const gchar *base_uri)
{
	return g_object_new (TRACKER_TYPE_REMOTE_CONNECTION,
			     "base-uri", base_uri,
			     NULL);
}
