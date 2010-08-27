/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <stdlib.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-writeback-consumer.h"
#include "tracker-writeback-dbus.h"
#include "tracker-writeback-glue.h"
#include "tracker-writeback-module.h"
#include "tracker-marshal.h"

#define TRACKER_WRITEBACK_CONSUMER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_WRITEBACK_CONSUMER, TrackerWritebackConsumerPrivate))

#define TRACKER_SERVICE             "org.freedesktop.Tracker1"
#define TRACKER_RESOURCES_OBJECT    "/org/freedesktop/Tracker1/Resources"
#define TRACKER_INTERFACE_RESOURCES "org.freedesktop.Tracker1.Resources"

typedef struct {
	gint    subject;
	GStrv   rdf_types;
	GArray *rdf_types_int;
} QueryData;

typedef struct {
	GHashTable *modules;
	TrackerSparqlConnection *connection;
	TrackerMinerManager *manager;
	GQueue *process_queue;
	guint idle_id;
	guint state;
} TrackerWritebackConsumerPrivate;

enum {
	STATE_IDLE,
	STATE_PROCESSING
};

static void tracker_writeback_consumer_finalize    (GObject       *object);
static void tracker_writeback_consumer_constructed (GObject       *object);
static gboolean process_queue_cb                   (gpointer       user_data);


G_DEFINE_TYPE (TrackerWritebackConsumer, tracker_writeback_consumer, G_TYPE_OBJECT)

static void
tracker_writeback_consumer_class_init (TrackerWritebackConsumerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_writeback_consumer_finalize;
	object_class->constructed = tracker_writeback_consumer_constructed;

	g_type_class_add_private (object_class, sizeof (TrackerWritebackConsumerPrivate));
}

static void
tracker_writeback_consumer_init (TrackerWritebackConsumer *consumer)
{
	TrackerWritebackConsumerPrivate *priv;
	GError *error = NULL;

	priv = TRACKER_WRITEBACK_CONSUMER_GET_PRIVATE (consumer);

	priv->connection = tracker_sparql_connection_get (&error);

	if (!priv->connection) {
		g_printerr ("%s: %s\n",
		            _("Could not establish a connection to Tracker"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
	}

	priv->modules = g_hash_table_new_full (g_str_hash,
	                                       g_str_equal,
	                                       (GDestroyNotify) g_free,
	                                       NULL);
	priv->process_queue = g_queue_new ();

	priv->manager = tracker_writeback_get_miner_manager ();
	priv->state = STATE_IDLE;
}

static void
tracker_writeback_consumer_finalize (GObject *object)
{
	TrackerWritebackConsumerPrivate *priv;

	priv = TRACKER_WRITEBACK_CONSUMER_GET_PRIVATE (object);

	if (priv->connection) {
		g_object_unref (priv->connection);
	}

	g_object_unref (priv->manager);

	G_OBJECT_CLASS (tracker_writeback_consumer_parent_class)->finalize (object);
}

static void
tracker_writeback_consumer_constructed (GObject *object)
{
	TrackerWritebackConsumerPrivate *priv;
	GList *modules;

	priv = TRACKER_WRITEBACK_CONSUMER_GET_PRIVATE (object);
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
}

TrackerWritebackConsumer *
tracker_writeback_consumer_new (void)
{
	return g_object_new (TRACKER_TYPE_WRITEBACK_CONSUMER, NULL);
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

static void
sparql_query_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	TrackerWritebackConsumerPrivate *priv;
	TrackerWritebackConsumer *consumer;
	QueryData *data;
	GError *error = NULL;
	TrackerSparqlCursor *cursor;

	consumer = TRACKER_WRITEBACK_CONSUMER (user_data);
	priv = TRACKER_WRITEBACK_CONSUMER_GET_PRIVATE (consumer);
	data = g_queue_pop_head (priv->process_queue);

	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (object), result, &error);

	if (!error) {
		GPtrArray *results = g_ptr_array_new ();
		guint cols = tracker_sparql_cursor_get_n_columns (cursor);

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			GStrv row = g_new0 (gchar*, cols);
			guint i;
			for (i = 0; i < cols; i++)
				row[i] = g_strdup (tracker_sparql_cursor_get_string (cursor, i, NULL));
			g_ptr_array_add (results, row);
		}

		if (results->len > 0) {
			GHashTableIter iter;
			gpointer key, value;
			GStrv rdf_types;

			rdf_types = data->rdf_types;

			g_hash_table_iter_init (&iter, priv->modules);

			while (g_hash_table_iter_next (&iter, &key, &value)) {
				TrackerWritebackModule *module;
				const gchar * const *module_types;

				module = value;
				module_types = tracker_writeback_module_get_rdf_types (module);

				if (sparql_rdf_types_match (module_types, (const gchar * const *) rdf_types)) {
					TrackerWriteback *writeback;

					g_message ("  Updating metadata for subject:'%d' using module:'%s'",
					           data->subject,
					           module->name);

					writeback = tracker_writeback_module_create (module);
					tracker_writeback_update_metadata (writeback, results, priv->connection);
					g_object_unref (writeback);
				}
			}
			g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		} else {
			g_message ("  No files qualify for updates");
		}
		g_ptr_array_free (results, TRUE);
		g_object_unref (cursor);
	} else {
		g_message ("  No files qualify for updates (%s)", error->message);
		g_error_free (error);
	}

	g_strfreev (data->rdf_types);
	g_slice_free (QueryData, data);

	priv->idle_id = g_idle_add (process_queue_cb, consumer);
}

static void
rdf_types_to_uris_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
	TrackerWritebackConsumerPrivate *priv;
	TrackerWritebackConsumer *consumer;
	QueryData *data;
	GError *error = NULL;
	TrackerSparqlCursor *cursor;
	TrackerSparqlConnection *connection;

	consumer = TRACKER_WRITEBACK_CONSUMER (user_data);
	priv = TRACKER_WRITEBACK_CONSUMER_GET_PRIVATE (consumer);
	connection = TRACKER_SPARQL_CONNECTION (object);

	data = g_queue_peek_head (priv->process_queue);

	cursor = tracker_sparql_connection_query_finish (connection, result, &error);

	if (!error) {
		gchar *query;
		const gchar *subject;
		GArray *rdf_types;
		guint i;

		rdf_types = g_array_new (TRUE, TRUE, sizeof (gchar *));

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			gchar *uri = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
			g_array_append_val (rdf_types, uri);
		}

		data->rdf_types = g_strdupv ((gchar **) rdf_types->data);

		for (i = 0; i < rdf_types->len; i++)
			g_free (g_array_index (rdf_types, gchar*, i));
		g_array_free (rdf_types, TRUE);

		g_object_unref (cursor);

		query = g_strdup_printf ("SELECT tracker:uri (%d) {}", data->subject);
		cursor = tracker_sparql_connection_query (connection, query, NULL, NULL);
		g_free (query);

		if (cursor && tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			subject = tracker_sparql_cursor_get_string (cursor, 0, NULL);
			if (!subject) {
				g_object_unref (cursor);
				goto trouble;
			}
		} else {
			if (cursor)
				g_object_unref (cursor);
			goto trouble;
		}

		query = g_strdup_printf ("SELECT ?url '%s' ?predicate ?object {"
		                         "  <%s> ?predicate ?object ; "
		                         "       nie:url ?url ."
		                         "  ?predicate tracker:writeback true "
		                         "}",
		                         subject, subject);

		tracker_sparql_connection_query_async (priv->connection,
		                                       query,
		                                       NULL,
		                                       sparql_query_cb,
		                                       consumer);

		g_free (query);
		g_object_unref (cursor);

	} else {
		goto trouble;
	}

	g_array_free (data->rdf_types_int, TRUE);

	return;

trouble:
		if (error) {
			g_message ("  No files qualify for updates (%s)", error->message);
			g_error_free (error);
		}
		g_array_free (data->rdf_types_int, TRUE);
		data = g_queue_pop_head (priv->process_queue);
		g_strfreev (data->rdf_types);
		g_slice_free (QueryData, data);

		priv->idle_id = g_idle_add (process_queue_cb, consumer);

}

static gboolean
process_queue_cb (gpointer user_data)
{
	TrackerWritebackConsumerPrivate *priv;
	TrackerWritebackConsumer *consumer;
	QueryData *data;
	GString *query;
	guint i;

	consumer = TRACKER_WRITEBACK_CONSUMER (user_data);
	priv = TRACKER_WRITEBACK_CONSUMER_GET_PRIVATE (consumer);
	data = g_queue_peek_head (priv->process_queue);

	if (!data) {
		/* No more data left, back to idle state */
		priv->state = STATE_IDLE;
		priv->idle_id = 0;
		return FALSE;
	}

	query = g_string_new ("SELECT ?resource { ?resource a rdfs:Class . "
	                      "FILTER (tracker:id (?resource) IN (");

	for (i = 0; i < data->rdf_types_int->len; i++) {
		gint id = g_array_index (data->rdf_types_int, gint, i);
		if (i != 0) {
			g_string_append_printf (query, ", %d", id);
		} else {
			g_string_append_printf (query, "%d", id);
		}
	}

	g_string_append (query, ")) }");

	tracker_sparql_connection_query_async (priv->connection,
	                                       query->str,
	                                       NULL,
	                                       rdf_types_to_uris_cb,
	                                       consumer);

	g_string_free (query, TRUE);

	/* Keep "processing" state */
	priv->idle_id = 0;
	return FALSE;
}

void
tracker_writeback_consumer_add_subject (TrackerWritebackConsumer *consumer,
                                        gint                      subject,
                                        GArray                   *rdf_types)
{
	TrackerWritebackConsumerPrivate *priv;
	QueryData *data;
	guint i;

	g_return_if_fail (TRACKER_IS_WRITEBACK_CONSUMER (consumer));
	g_return_if_fail (rdf_types != NULL);

	priv = TRACKER_WRITEBACK_CONSUMER_GET_PRIVATE (consumer);

	data = g_slice_new (QueryData);
	data->subject = subject;

	data->rdf_types_int = g_array_sized_new (FALSE, FALSE, sizeof (gint), rdf_types->len);

	for (i = 0; i < rdf_types->len; i++) {
		gint id = g_array_index (rdf_types, gint, i);
		g_array_append_val (data->rdf_types_int, id);
	}

	g_queue_push_tail (priv->process_queue, data);

	if (priv->state != STATE_PROCESSING && priv->idle_id == 0) {
		priv->state = STATE_PROCESSING;
		priv->idle_id = g_idle_add (process_queue_cb, consumer);
	}
}
