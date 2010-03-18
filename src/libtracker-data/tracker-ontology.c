/*
 * Copyright (C) 2009, Nokia
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

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_ONTOLOGY, TrackerOntologyPrivate))

typedef struct _TrackerOntologyPrivate TrackerOntologyPrivate;

struct _TrackerOntologyPrivate {
	gchar *uri;
	time_t last_modified;
	gboolean is_new;
};

static void ontology_finalize     (GObject      *object);

G_DEFINE_TYPE (TrackerOntology, tracker_ontology, G_TYPE_OBJECT);

static void
tracker_ontology_class_init (TrackerOntologyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = ontology_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerOntologyPrivate));
}

static void
tracker_ontology_init (TrackerOntology *service)
{
}

static void
ontology_finalize (GObject *object)
{
	TrackerOntologyPrivate *priv;

	priv = GET_PRIV (object);

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

time_t
tracker_ontology_get_last_modified (TrackerOntology *ontology)
{
	TrackerOntologyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_ONTOLOGY (ontology), 0);

	priv = GET_PRIV (ontology);

	return priv->last_modified;
}

gboolean
tracker_ontology_get_is_new (TrackerOntology *ontology)
{
	TrackerOntologyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_ONTOLOGY (ontology), FALSE);

	priv = GET_PRIV (ontology);

	return priv->is_new;
}


void
tracker_ontology_set_last_modified (TrackerOntology *ontology,
                                    time_t           value)
{
	TrackerOntologyPrivate *priv;

	g_return_if_fail (TRACKER_IS_ONTOLOGY (ontology));

	priv = GET_PRIV (ontology);

	priv->last_modified = value;
}


const gchar *
tracker_ontology_get_uri (TrackerOntology *ontology)
{
	TrackerOntologyPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_ONTOLOGY (ontology), NULL);

	priv = GET_PRIV (ontology);

	return priv->uri;
}


void
tracker_ontology_set_uri (TrackerOntology *ontology,
                          const gchar    *value)
{
	TrackerOntologyPrivate *priv;

	g_return_if_fail (TRACKER_IS_ONTOLOGY (ontology));

	priv = GET_PRIV (ontology);

	g_free (priv->uri);

	if (value) {
		priv->uri = g_strdup (value);
	} else {
		priv->uri = NULL;
	}
}

void
tracker_ontology_set_is_new (TrackerOntology *ontology,
                             gboolean         value)
{
	TrackerOntologyPrivate *priv;

	g_return_if_fail (TRACKER_IS_ONTOLOGY (ontology));

	priv = GET_PRIV (ontology);

	priv->is_new = value;
}
