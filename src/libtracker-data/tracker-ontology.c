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
static void ontology_get_property (GObject      *object,
                                   guint         param_id,
                                   GValue       *value,
                                   GParamSpec   *pspec);
static void ontology_set_property (GObject      *object,
                                   guint         param_id,
                                   const GValue *value,
                                   GParamSpec   *pspec);

enum {
	PROP_0,
	PROP_URI,
	PROP_LAST_MODIFIED,
	PROP_IS_NEW
};

G_DEFINE_TYPE (TrackerOntology, tracker_ontology, G_TYPE_OBJECT);

static void
tracker_ontology_class_init (TrackerOntologyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = ontology_finalize;
	object_class->get_property = ontology_get_property;
	object_class->set_property = ontology_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_URI,
	                                 g_param_spec_string ("uri",
	                                                      "uri",
	                                                      "URI",
	                                                      NULL,
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_LAST_MODIFIED,
	                                 g_param_spec_int64  ("last-modified",
	                                                      "last-modified",
	                                                      "Last modified",
	                                                      G_MININT64,
	                                                      G_MAXINT64,
	                                                      0,
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_IS_NEW,
	                                 g_param_spec_boolean ("is-new",
	                                                       "is-new",
	                                                       "Set to TRUE when a new class or property is to be added to the database ontology",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE));

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

static void
ontology_get_property (GObject    *object,
                       guint       param_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
	TrackerOntologyPrivate *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_URI:
		g_value_set_string (value, priv->uri);
		break;
	case PROP_LAST_MODIFIED:
		g_value_set_int64 (value, priv->last_modified);
		break;
	case PROP_IS_NEW:
		g_value_set_boolean (value, priv->is_new);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
ontology_set_property (GObject      *object,
                       guint         param_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
	TrackerOntologyPrivate *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_URI:
		tracker_ontology_set_uri (TRACKER_ONTOLOGY (object),
		                          g_value_get_string (value));
		break;
	case PROP_LAST_MODIFIED:
		tracker_ontology_set_last_modified (TRACKER_ONTOLOGY (object),
		                                    g_value_get_int64 (value));
		break;
	case PROP_IS_NEW:
		tracker_ontology_set_is_new (TRACKER_ONTOLOGY (object),
		                             g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
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

	g_object_notify (G_OBJECT (ontology), "last-modified");
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

	g_object_notify (G_OBJECT (ontology), "uri");
}

void
tracker_ontology_set_is_new (TrackerOntology *ontology,
                             gboolean         value)
{
	TrackerOntologyPrivate *priv;

	g_return_if_fail (TRACKER_IS_ONTOLOGY (ontology));

	priv = GET_PRIV (ontology);

	priv->is_new = value;
	g_object_notify (G_OBJECT (ontology), "is-new");
}
