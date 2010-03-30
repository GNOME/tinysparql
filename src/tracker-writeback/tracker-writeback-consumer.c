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
#include <libtracker-client/tracker.h>

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
	gchar *subject;
	GStrv rdf_types;
} QueryData;

typedef struct {
	GHashTable *modules;
	TrackerClient *client;
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

	priv = TRACKER_WRITEBACK_CONSUMER_GET_PRIVATE (consumer);

	priv->client = tracker_client_new (TRACKER_CLIENT_ENABLE_WARNINGS, G_MAXINT);
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

	if (priv->client) {
		g_object_unref (priv->client);
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
sparql_query_cb (GPtrArray *result,
                 GError    *error,
                 gpointer   user_data)
{
	TrackerWritebackConsumerPrivate *priv;
	TrackerWritebackConsumer *consumer;
	QueryData *data;

	consumer = TRACKER_WRITEBACK_CONSUMER (user_data);
	priv = TRACKER_WRITEBACK_CONSUMER_GET_PRIVATE (consumer);
	data = g_queue_pop_head (priv->process_queue);

	if (!error && result && result->len > 0) {
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

				g_message ("  Updating metadata for subject:'%s' using module:'%s'",
				           data->subject,
				           module->name);

				writeback = tracker_writeback_module_create (module);
				tracker_writeback_update_metadata (writeback, result, priv->client);
				g_object_unref (writeback);
			}
		}
	} else {
		g_message ("  No files qualify for updates");
	}

	g_free (data->subject);
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
	gchar *query;

	consumer = TRACKER_WRITEBACK_CONSUMER (user_data);
	priv = TRACKER_WRITEBACK_CONSUMER_GET_PRIVATE (consumer);
	data = g_queue_peek_head (priv->process_queue);

	if (!data) {
		/* No more data left, back to idle state */
		priv->state = STATE_IDLE;
		priv->idle_id = 0;
		return FALSE;
	}

	query = g_strdup_printf ("SELECT ?url '%s' ?predicate ?object {"
	                         "  <%s> ?predicate ?object ; "
	                         "       nie:url ?url ."
	                         "  ?predicate tracker:writeback true "
	                         "}",
	                         data->subject, data->subject);

	tracker_resources_sparql_query_async (priv->client,
	                                      query,
	                                      sparql_query_cb,
	                                      consumer);

	g_free (query);

	/* Keep "processing" state */
	priv->idle_id = 0;
	return FALSE;
}

void
tracker_writeback_consumer_add_subject (TrackerWritebackConsumer *consumer,
                                        const gchar              *subject,
                                        const GStrv               rdf_types)
{
	TrackerWritebackConsumerPrivate *priv;
	QueryData *data;

	g_return_if_fail (TRACKER_IS_WRITEBACK_CONSUMER (consumer));
	g_return_if_fail (subject != NULL);
	g_return_if_fail (rdf_types != NULL);

	priv = TRACKER_WRITEBACK_CONSUMER_GET_PRIVATE (consumer);

	data = g_slice_new (QueryData);
	data->subject = g_strdup (subject);
	data->rdf_types = g_strdupv (rdf_types);

	g_queue_push_tail (priv->process_queue, data);

	if (priv->state != STATE_PROCESSING && priv->idle_id == 0) {
		priv->state = STATE_PROCESSING;
		priv->idle_id = g_idle_add (process_queue_cb, consumer);
	}
}
