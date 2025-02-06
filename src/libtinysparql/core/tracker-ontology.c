/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "tracker-ontology.h"

typedef struct _TrackerOntologyPrivate TrackerOntologyPrivate;

struct _TrackerOntologyPrivate {
	gchar *uri;
	gboolean is_new;
	TrackerOntologies *ontologies;
};

static void ontology_finalize     (GObject      *object);

G_DEFINE_TYPE_WITH_PRIVATE (TrackerOntology, tracker_ontology, G_TYPE_OBJECT)

static void
tracker_ontology_class_init (TrackerOntologyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ontology_finalize;
}

static void
tracker_ontology_init (TrackerOntology *service)
{
}

static void
ontology_finalize (GObject *object)
{
	TrackerOntologyPrivate *priv;

	priv = tracker_ontology_get_instance_private (TRACKER_ONTOLOGY (object));

	g_free (priv->uri);

	(G_OBJECT_CLASS (tracker_ontology_parent_class)->finalize) (object);
}

TrackerOntology *
tracker_ontology_new (void)
{
	TrackerOntology *ontology;

	ontology = g_object_new (TRACKER_TYPE_ONTOLOGY, NULL);

	return ontology;
}

const gchar *
tracker_ontology_get_uri (TrackerOntology *ontology)
{
	TrackerOntologyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_ONTOLOGY (ontology), NULL);

	priv = tracker_ontology_get_instance_private (ontology);

	return priv->uri;
}


void
tracker_ontology_set_uri (TrackerOntology *ontology,
                          const gchar    *value)
{
	TrackerOntologyPrivate *priv;

	g_return_if_fail (TRACKER_IS_ONTOLOGY (ontology));

	priv = tracker_ontology_get_instance_private (ontology);

	g_free (priv->uri);

	if (value) {
		priv->uri = g_strdup (value);
	} else {
		priv->uri = NULL;
	}
}

void
tracker_ontology_set_ontologies (TrackerOntology   *ontology,
                                 TrackerOntologies *ontologies)
{
	TrackerOntologyPrivate *priv;

	g_return_if_fail (TRACKER_IS_ONTOLOGY (ontology));
	g_return_if_fail (ontologies != NULL);
	priv = tracker_ontology_get_instance_private (ontology);

	priv->ontologies = ontologies;
}
