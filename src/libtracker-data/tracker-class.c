/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "tracker-class.h"
#include "tracker-namespace.h"
#include "tracker-ontologies.h"

typedef struct _TrackerClassPrivate TrackerClassPrivate;

struct _TrackerClassPrivate {
	gchar *uri;
	gchar *name;
	TrackerRowid id;
	guint is_new : 1;
	guint db_schema_changed : 1;
	guint notify : 1;
	guint use_gvdb : 1;

	gchar *ontology_path;
	goffset definition_line_no;
	goffset definition_column_no;

	GMutex mutex;

	GArray *super_classes;
	GArray *domain_indexes;
	GArray *last_domain_indexes;
	GArray *last_super_classes;

	TrackerOntologies *ontologies;
};

static void class_finalize     (GObject      *object);

G_DEFINE_TYPE_WITH_PRIVATE (TrackerClass, tracker_class, G_TYPE_OBJECT)

static void
tracker_class_class_init (TrackerClassClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = class_finalize;
}

static void
tracker_class_init (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	priv = tracker_class_get_instance_private (service);

	priv->id = 0;
	priv->super_classes = g_array_new (TRUE, TRUE, sizeof (TrackerClass *));
	priv->domain_indexes = g_array_new (TRUE, TRUE, sizeof (TrackerProperty *));
	priv->last_domain_indexes = NULL;
	priv->last_super_classes = NULL;
	g_mutex_init (&priv->mutex);
}

static void
class_finalize (GObject *object)
{
	TrackerClassPrivate *priv;

	priv = tracker_class_get_instance_private (TRACKER_CLASS (object));

	g_free (priv->uri);
	g_free (priv->name);

	g_array_free (priv->super_classes, TRUE);
	g_array_free (priv->domain_indexes, TRUE);

	if (priv->ontology_path) {
		g_free (priv->ontology_path);
	}

	if (priv->last_domain_indexes) {
		g_array_free (priv->last_domain_indexes, TRUE);
	}

	if (priv->last_super_classes) {
		g_array_free (priv->last_super_classes, TRUE);
	}

	(G_OBJECT_CLASS (tracker_class_parent_class)->finalize) (object);
}

TrackerClass *
tracker_class_new (gboolean use_gvdb)
{
	TrackerClass *service;
	TrackerClassPrivate *priv;

	service = g_object_new (TRACKER_TYPE_CLASS, NULL);

	if (use_gvdb) {
		priv = tracker_class_get_instance_private (service);
		priv->use_gvdb = !!use_gvdb;
	}

	return service;
}

static void
tracker_class_maybe_sync_from_gvdb (TrackerClass *service)
{
	TrackerClassPrivate *priv;
	TrackerClass *super_class;
	GVariant *variant;

	priv = tracker_class_get_instance_private (service);

	if (!priv->use_gvdb)
		return;

	g_mutex_lock (&priv->mutex);

	/* In case the lock was contended, make the second lose */
	if (!priv->use_gvdb)
		goto out;

	tracker_class_reset_super_classes (service);

	variant = tracker_ontologies_get_class_value_gvdb (priv->ontologies, priv->uri, "super-classes");
	if (variant) {
		GVariantIter iter;
		const gchar *uri;

		g_variant_iter_init (&iter, variant);
		while (g_variant_iter_loop (&iter, "&s", &uri)) {
			super_class = tracker_ontologies_get_class_by_uri (priv->ontologies, uri);

			tracker_class_add_super_class (service, super_class);
		}

		g_variant_unref (variant);
	}

	priv->use_gvdb = FALSE;
out:
	g_mutex_unlock (&priv->mutex);
}

const gchar *
tracker_class_get_uri (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = tracker_class_get_instance_private (service);

	return priv->uri;
}

const gchar *
tracker_class_get_name (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = tracker_class_get_instance_private (service);

	return priv->name;
}

TrackerRowid
tracker_class_get_id (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), 0);

	priv = tracker_class_get_instance_private (service);

	return priv->id;
}

TrackerClass **
tracker_class_get_super_classes (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = tracker_class_get_instance_private (service);

	tracker_class_maybe_sync_from_gvdb (service);

	return (TrackerClass **) priv->super_classes->data;
}

TrackerProperty **
tracker_class_get_domain_indexes (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = tracker_class_get_instance_private (service);

	return (TrackerProperty **) priv->domain_indexes->data;
}


TrackerProperty **
tracker_class_get_last_domain_indexes (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = tracker_class_get_instance_private (service);

	return (TrackerProperty **) (priv->last_domain_indexes ? priv->last_domain_indexes->data : NULL);
}

TrackerClass **
tracker_class_get_last_super_classes (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = tracker_class_get_instance_private (service);

	return (TrackerClass **) (priv->last_super_classes ? priv->last_super_classes->data : NULL);
}

gboolean
tracker_class_get_is_new (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = tracker_class_get_instance_private (service);

	return priv->is_new;
}

gboolean
tracker_class_get_notify (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = tracker_class_get_instance_private (service);

	return priv->notify;
}

gboolean
tracker_class_get_db_schema_changed (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = tracker_class_get_instance_private (service);

	return priv->db_schema_changed;
}

const gchar *
tracker_class_get_ontology_path (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = tracker_class_get_instance_private (service);

	return priv->ontology_path;
}

goffset
tracker_class_get_definition_line_no (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = tracker_class_get_instance_private (service);

	return priv->definition_line_no;
}

goffset
tracker_class_get_definition_column_no (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = tracker_class_get_instance_private (service);

	return priv->definition_column_no;
}

void
tracker_class_set_uri (TrackerClass *service,
                       const gchar  *value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = tracker_class_get_instance_private (service);

	g_free (priv->uri);
	g_free (priv->name);
	priv->uri = NULL;
	priv->name = NULL;

	if (value) {
		gchar *namespace_uri, *hash;
		TrackerNamespace *namespace;

		priv->uri = g_strdup (value);

		hash = strrchr (priv->uri, '#');
		if (hash == NULL) {
			/* support ontologies whose namespace uri does not end in a hash, e.g. dc */
			hash = strrchr (priv->uri, '/');
		}
		if (hash == NULL) {
			g_critical ("Unknown namespace of class %s", priv->uri);
		} else {
			namespace_uri = g_strndup (priv->uri, hash - priv->uri + 1);
			namespace = tracker_ontologies_get_namespace_by_uri (priv->ontologies, namespace_uri);
			if (namespace == NULL) {
				g_critical ("Unknown namespace %s of class %s", namespace_uri, priv->uri);
			} else {
				priv->name = g_strdup_printf ("%s:%s", tracker_namespace_get_prefix (namespace), hash + 1);
			}
			g_free (namespace_uri);
		}
	}
}

void
tracker_class_set_id (TrackerClass *service,
                      TrackerRowid  value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = tracker_class_get_instance_private (service);

	priv->id = value;
}

void
tracker_class_add_super_class (TrackerClass *service,
                               TrackerClass *value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));
	g_return_if_fail (TRACKER_IS_CLASS (value));

	priv = tracker_class_get_instance_private (service);

	g_array_append_val (priv->super_classes, value);
}

void
tracker_class_reset_super_classes (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = tracker_class_get_instance_private (service);

	if (priv->last_super_classes) {
		g_array_free (priv->last_super_classes, TRUE);
	}

	priv->last_super_classes = priv->super_classes;
	priv->super_classes = g_array_new (TRUE, TRUE, sizeof (TrackerClass *));
}

void
tracker_class_add_domain_index (TrackerClass *service,
                                TrackerProperty *value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));
	g_return_if_fail (TRACKER_IS_PROPERTY (value));

	priv = tracker_class_get_instance_private (service);

	g_array_append_val (priv->domain_indexes, value);
}

void
tracker_class_del_domain_index (TrackerClass    *service,
                                TrackerProperty *value)
{
	TrackerClassPrivate *priv;
	gint i = 0, found = -1;
	TrackerProperty **properties;

	g_return_if_fail (TRACKER_IS_CLASS (service));
	g_return_if_fail (TRACKER_IS_PROPERTY (value));

	priv = tracker_class_get_instance_private (service);

	properties = (TrackerProperty **) priv->domain_indexes->data;
	while (*properties) {
		if (*properties == value) {
			found = i;
			break;
		}
		i++;
		properties++;
	}

	if (found != -1) {
		g_array_remove_index (priv->domain_indexes, found);
	}
}

void
tracker_class_reset_domain_indexes (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = tracker_class_get_instance_private (service);
	priv->last_domain_indexes = priv->domain_indexes;
	priv->domain_indexes = g_array_new (TRUE, TRUE, sizeof (TrackerProperty *));
}

void
tracker_class_set_is_new (TrackerClass *service,
                          gboolean      value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = tracker_class_get_instance_private (service);

	priv->is_new = !!value;
}


void
tracker_class_set_notify (TrackerClass *service,
                          gboolean      value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = tracker_class_get_instance_private (service);

	priv->notify = !!value;
}

void
tracker_class_set_db_schema_changed (TrackerClass *service,
                                     gboolean      value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = tracker_class_get_instance_private (service);

	priv->db_schema_changed = !!value;
}

void
tracker_class_set_ontologies (TrackerClass      *class,
                              TrackerOntologies *ontologies)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (class));
	g_return_if_fail (ontologies != NULL);

	priv = tracker_class_get_instance_private (class);
	priv->ontologies = ontologies;
}

void
tracker_class_set_ontology_path (TrackerClass *service,
                                 const gchar  *value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = tracker_class_get_instance_private (service);

	if (priv->ontology_path)
		g_free (priv->ontology_path);

	priv->ontology_path = g_strdup (value);
}

void
tracker_class_set_definition_line_no (TrackerClass *service,
                                      goffset       value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = tracker_class_get_instance_private (service);

	priv->definition_line_no = value;
}

void
tracker_class_set_definition_column_no (TrackerClass *service,
                                        goffset       value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = tracker_class_get_instance_private (service);

	priv->definition_column_no = value;
}
