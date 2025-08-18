/*
 * Copyright (C) 2025, Red Hat Inc.
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

#include "tracker-deserializer-merger.h"

typedef struct _TrackerDeserializerMergerPrivate TrackerDeserializerMergerPrivate;
struct _TrackerDeserializerMergerPrivate {
	TrackerDeserializerRdf parent_instance;
	GList *deserializers;
	GList *current_deserializer;
};

G_DEFINE_TYPE_WITH_PRIVATE (TrackerDeserializerMerger,
                            tracker_deserializer_merger,
                            TRACKER_TYPE_DESERIALIZER_RDF)

static void
tracker_deserializer_merger_finalize (GObject *object)
{
	TrackerDeserializerMerger *deserializer =
		TRACKER_DESERIALIZER_MERGER (object);
	TrackerDeserializerMergerPrivate *priv =
		tracker_deserializer_merger_get_instance_private (deserializer);

	g_list_free_full (priv->deserializers, g_object_unref);
	priv->current_deserializer = NULL;

	G_OBJECT_CLASS (tracker_deserializer_merger_parent_class)->finalize (object);
}

static TrackerSparqlValueType
tracker_deserializer_merger_get_value_type (TrackerSparqlCursor *cursor,
                                            gint                 column)
{
	TrackerDeserializerMerger *deserializer =
		TRACKER_DESERIALIZER_MERGER (cursor);
	TrackerDeserializerMergerPrivate *priv =
		tracker_deserializer_merger_get_instance_private (deserializer);

	if (!priv->current_deserializer)
		return TRACKER_SPARQL_VALUE_TYPE_UNBOUND;

	return tracker_sparql_cursor_get_value_type (priv->current_deserializer->data,
	                                             column);
}

static gboolean
tracker_deserializer_merger_next (TrackerSparqlCursor  *cursor,
                                  GCancellable         *cancellable,
                                  GError              **error)
{
	TrackerDeserializerMerger *deserializer =
		TRACKER_DESERIALIZER_MERGER (cursor);
	TrackerDeserializerMergerPrivate *priv =
		tracker_deserializer_merger_get_instance_private (deserializer);
	gboolean has_next = FALSE;

	if (!priv->current_deserializer)
		priv->current_deserializer = g_list_last (priv->deserializers);

	while (!has_next && priv->current_deserializer) {
		has_next = tracker_sparql_cursor_next (priv->current_deserializer->data,
						       cancellable, error);
		if (!has_next) {
			GList *to_remove;

			to_remove = priv->current_deserializer;
			priv->current_deserializer = priv->current_deserializer->prev;
			priv->deserializers = g_list_remove_link (priv->deserializers, to_remove);
			g_object_unref (to_remove->data);
			g_list_free1 (to_remove);
		}
	}

	return has_next;
}

static const gchar *
tracker_deserializer_merger_get_string (TrackerSparqlCursor  *cursor,
                                        gint                  column,
                                        const gchar         **langtag,
                                        glong                *length)
{
	TrackerDeserializerMerger *deserializer =
		TRACKER_DESERIALIZER_MERGER (cursor);
	TrackerDeserializerMergerPrivate *priv =
		tracker_deserializer_merger_get_instance_private (deserializer);

	if (!priv->current_deserializer) {
		if (langtag)
			*langtag = NULL;
		if (length)
			*length = 0;

		return NULL;
	}

	if (langtag) {
		return tracker_sparql_cursor_get_langstring (priv->current_deserializer->data,
		                                             column, langtag, length);
	} else {
		return tracker_sparql_cursor_get_string (priv->current_deserializer->data,
		                                         column, length);
	}
}

void
tracker_deserializer_merger_close (TrackerSparqlCursor* cursor)
{
	TrackerDeserializerMerger *deserializer =
		TRACKER_DESERIALIZER_MERGER (cursor);
	TrackerDeserializerMergerPrivate *priv =
		tracker_deserializer_merger_get_instance_private (deserializer);

	tracker_sparql_cursor_close (priv->current_deserializer->data);
}

static gboolean
tracker_deserializer_merger_get_parser_location (TrackerDeserializer  *deserializer,
                                                 const char          **name,
                                                 goffset              *line_no,
                                                 goffset              *column_no)
{
	TrackerDeserializerMerger *deserializer_merger =
		TRACKER_DESERIALIZER_MERGER (deserializer);
	TrackerDeserializerMergerPrivate *priv =
		tracker_deserializer_merger_get_instance_private (deserializer_merger);

	if (!priv->current_deserializer)
		return FALSE;

	return tracker_deserializer_get_parser_location (priv->current_deserializer->data,
	                                                 name, line_no, column_no);
}

static void
tracker_deserializer_merger_class_init (TrackerDeserializerMergerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerSparqlCursorClass *cursor_class = TRACKER_SPARQL_CURSOR_CLASS (klass);
	TrackerDeserializerClass *deserializer_class = TRACKER_DESERIALIZER_CLASS (klass);

	object_class->finalize = tracker_deserializer_merger_finalize;

	cursor_class->get_value_type = tracker_deserializer_merger_get_value_type;
	cursor_class->get_string = tracker_deserializer_merger_get_string;
	cursor_class->next = tracker_deserializer_merger_next;
	cursor_class->close = tracker_deserializer_merger_close;

	deserializer_class->get_parser_location = tracker_deserializer_merger_get_parser_location;
}

static void
tracker_deserializer_merger_init (TrackerDeserializerMerger *deserializer)
{
}

TrackerSparqlCursor *
tracker_deserializer_merger_new (void)
{
	return g_object_new (TRACKER_TYPE_DESERIALIZER_MERGER, NULL);
}

void
tracker_deserializer_merger_add_child (TrackerDeserializerMerger *deserializer,
                                       TrackerDeserializer       *child)
{
	TrackerDeserializerMergerPrivate *priv =
		tracker_deserializer_merger_get_instance_private (deserializer);

	priv->deserializers = g_list_prepend (priv->deserializers, g_object_ref (child));
}
