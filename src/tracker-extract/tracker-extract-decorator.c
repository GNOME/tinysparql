/*
 * Copyright (C) 2014 Carlos Garnacho <carlosg@gnome.org>
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

#include "config.h"
#include "tracker-extract-decorator.h"
#include <libtracker-extract/tracker-extract.h>
#include "tracker-extract.h"

enum {
	PROP_EXTRACTOR = 1
};

#define TRACKER_EXTRACT_DATA_SOURCE "tracker:extractor-data-source"
#define TRACKER_EXTRACT_DECORATOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_EXTRACT_DECORATOR, TrackerExtractDecoratorPrivate))

typedef struct _TrackerExtractDecoratorPrivate TrackerExtractDecoratorPrivate;
typedef struct _ExtractData ExtractData;

struct _ExtractData {
	TrackerDecorator *decorator;
	TrackerDecoratorInfo *decorator_info;
};

struct _TrackerExtractDecoratorPrivate {
	TrackerExtract *extractor;
	GTimer *timer;
};

static void decorator_get_next_file (TrackerDecorator *decorator);


G_DEFINE_TYPE (TrackerExtractDecorator, tracker_extract_decorator,
               TRACKER_TYPE_DECORATOR_FS)

static void
tracker_extract_decorator_get_property (GObject    *object,
                                        guint       param_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (object)->priv;

	switch (param_id) {
	case PROP_EXTRACTOR:
		g_value_set_object (value, priv->extractor);
		break;
	}
}

static void
tracker_extract_decorator_set_property (GObject      *object,
                                        guint         param_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (object)->priv;

	switch (param_id) {
	case PROP_EXTRACTOR:
		priv->extractor = g_value_dup_object (value);
		break;
	}
}

static void
tracker_extract_decorator_finalize (GObject *object)
{
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (object)->priv;

	if (priv->extractor)
		g_object_unref (priv->extractor);

	if (priv->timer)
		g_timer_destroy (priv->timer);

	G_OBJECT_CLASS (tracker_extract_decorator_parent_class)->finalize (object);
}

static void
decorator_save_info (TrackerSparqlBuilder    *sparql,
                     TrackerExtractDecorator *decorator,
                     TrackerDecoratorInfo    *decorator_info,
                     TrackerExtractInfo      *info)
{
	const gchar *urn, *result, *where;
	TrackerSparqlBuilder *builder;
	gchar *str;

	urn = tracker_decorator_info_get_urn (decorator_info);

	tracker_sparql_builder_insert_open (sparql, NULL);
	tracker_sparql_builder_graph_open (sparql, TRACKER_MINER_FS_GRAPH_URN);

	/* Set tracker-extract data source */
	tracker_sparql_builder_subject_iri (sparql, urn);
	tracker_sparql_builder_predicate (sparql, "nie:dataSource");
	tracker_sparql_builder_object_iri (sparql,
	                                   tracker_decorator_get_data_source (TRACKER_DECORATOR (decorator)));

	/* Add extracted metadata */
	str = g_strdup_printf ("<%s>", urn);
	tracker_sparql_builder_append (sparql, str);
	g_free (str);

	builder = tracker_extract_info_get_metadata_builder (info);
	result = tracker_sparql_builder_get_result (builder);
	tracker_sparql_builder_append (sparql, result);

	/* Close graph and insert statement, insert where clause */
	tracker_sparql_builder_graph_close (sparql);
        tracker_sparql_builder_insert_close (sparql);

	where = tracker_extract_info_get_where_clause (info);

	if (where && *where) {
		tracker_sparql_builder_where_open (sparql);
		tracker_sparql_builder_append (sparql, where);
		tracker_sparql_builder_where_close (sparql);
	}

	/* Prepend/append pre/postupdate chunks */
	builder = tracker_extract_info_get_preupdate_builder (info);
	result = tracker_sparql_builder_get_result (builder);

	if (result && *result)
		tracker_sparql_builder_prepend (sparql, result);

	builder = tracker_extract_info_get_postupdate_builder (info);
	result = tracker_sparql_builder_get_result (builder);

	if (result && *result)
		tracker_sparql_builder_append (sparql, result);
}

static void
get_metadata_cb (TrackerExtract *extract,
                 GAsyncResult   *result,
                 ExtractData    *data)
{
	TrackerExtractInfo *info;
	GTask *task;

	task = tracker_decorator_info_get_task (data->decorator_info);
	info = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));

	if (!info) {
		GError *error = NULL;

		g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), &error);
		g_task_return_error (task, error);
	} else {
		decorator_save_info (g_task_get_task_data (task),
		                     TRACKER_EXTRACT_DECORATOR (data->decorator),
		                     data->decorator_info, info);
		g_task_return_boolean (task, TRUE);
	}

	decorator_get_next_file (data->decorator);
	g_free (data);
}

static void
decorator_next_item_cb (TrackerDecorator *decorator,
                        GAsyncResult     *result,
                        gpointer          user_data)
{
	TrackerExtractDecoratorPrivate *priv;
	TrackerDecoratorInfo *info;
	GError *error = NULL;
	ExtractData *data;
	GTask *task;

	priv = TRACKER_EXTRACT_DECORATOR (decorator)->priv;
	info = tracker_decorator_next_finish (decorator, result, &error);

	if (!info) {
		if (tracker_decorator_get_n_items (decorator) != 0)
			g_warning ("Next item could not be retrieved: %s\n", error->message);
		g_error_free (error);
		return;
	}

	data = g_new0 (ExtractData, 1);
	data->decorator = decorator;
	data->decorator_info = info;
	task = tracker_decorator_info_get_task (info);

	g_debug ("Extracting metadata for: %s\n", tracker_decorator_info_get_url (info));

	tracker_extract_file (priv->extractor,
	                      tracker_decorator_info_get_url (info),
	                      tracker_decorator_info_get_mimetype (info),
	                      TRACKER_MINER_FS_GRAPH_URN,
	                      g_task_get_cancellable (task),
	                      (GAsyncReadyCallback) get_metadata_cb, data);
}

static void
decorator_get_next_file (TrackerDecorator *decorator)
{
	tracker_decorator_next (decorator, NULL,
	                        (GAsyncReadyCallback) decorator_next_item_cb,
	                        NULL);
}

static void
tracker_extract_decorator_paused (TrackerMiner *miner)
{
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (miner)->priv;
	g_debug ("Decorator paused\n");
	g_timer_stop (priv->timer);
}

static void
tracker_extract_decorator_resumed (TrackerMiner *miner)
{
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (miner)->priv;
	g_debug ("Resuming processing of %d items\n",
	         tracker_decorator_get_n_items (TRACKER_DECORATOR (miner)));
	g_timer_continue (priv->timer);
	decorator_get_next_file (TRACKER_DECORATOR (miner));
}

static void
tracker_extract_decorator_items_available (TrackerDecorator *decorator)
{
	TrackerExtractDecoratorPrivate *priv;

	priv = TRACKER_EXTRACT_DECORATOR (decorator)->priv;
	g_debug ("Starting processing of %d items\n",
	         tracker_decorator_get_n_items (decorator));
	priv->timer = g_timer_new ();
	decorator_get_next_file (decorator);
}

static void
tracker_extract_decorator_finished (TrackerDecorator *decorator)
{
	TrackerExtractDecoratorPrivate *priv;
	gchar *time_str;

	priv = TRACKER_EXTRACT_DECORATOR (decorator)->priv;
	time_str = tracker_seconds_to_string ((gint) g_timer_elapsed (priv->timer, NULL), TRUE);
	g_debug ("Extraction finished in %s", time_str);
	g_timer_destroy (priv->timer);
	priv->timer = NULL;
	g_free (time_str);
}

static void
tracker_extract_decorator_class_init (TrackerExtractDecoratorClass *klass)
{
	TrackerDecoratorClass *decorator_class = TRACKER_DECORATOR_CLASS (klass);
	TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_extract_decorator_finalize;
	object_class->get_property = tracker_extract_decorator_get_property;
	object_class->set_property = tracker_extract_decorator_set_property;

	miner_class->paused = tracker_extract_decorator_paused;
	miner_class->resumed = tracker_extract_decorator_resumed;

	decorator_class->items_available = tracker_extract_decorator_items_available;
	decorator_class->finished = tracker_extract_decorator_finished;

	g_object_class_install_property (object_class,
	                                 PROP_EXTRACTOR,
	                                 g_param_spec_object ("extractor",
	                                                      "Extractor",
	                                                      "Extractor",
	                                                      TRACKER_TYPE_EXTRACT,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class,
	                          sizeof (TrackerExtractDecoratorPrivate));
}

static void
tracker_extract_decorator_init (TrackerExtractDecorator *decorator)
{
	decorator->priv = TRACKER_EXTRACT_DECORATOR_GET_PRIVATE (decorator);
}

TrackerDecorator *
tracker_extract_decorator_new (TrackerExtract  *extract,
                               GCancellable    *cancellable,
                               GError         **error)
{
	/* Preferably classes with tracker:notify true, if an
	 * extractor module handles new ones, it must be added
	 * here.
	 */
	gchar *classes[] = {
		"nfo:Document",
		"nfo:Audio",
		"nfo:Image",
		"nfo:Video",
		"nfo:FilesystemImage",
		NULL
	};

	return g_initable_new (TRACKER_TYPE_EXTRACT_DECORATOR,
	                       cancellable, error,
	                       "name", "Extract",
	                       "data-source", TRACKER_EXTRACT_DATA_SOURCE,
	                       "class-names", classes,
	                       "extractor", extract,
	                       NULL);
}
