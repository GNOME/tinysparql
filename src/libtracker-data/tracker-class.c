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

#define GET_PRIV(obj) (((TrackerClass*) obj)->priv)
#define TRACKER_CLASS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_CLASS, TrackerClassPrivate))

struct _TrackerClassPrivate {
	gchar *uri;
	gchar *name;
	gint count;
	gint id;
	gboolean is_new;
	gboolean db_schema_changed;
	gboolean notify;

	GArray *super_classes;
	GArray *domain_indexes;
	GArray *last_domain_indexes;

	struct {
		struct {
			GArray *sub_pred_ids;
			GArray *obj_graph_ids;
		} pending;
		struct {
			GArray *sub_pred_ids;
			GArray *obj_graph_ids;
		} ready;
	} deletes;
	struct {
		struct {
			GArray *sub_pred_ids;
			GArray *obj_graph_ids;
		} pending;
		struct {
			GArray *sub_pred_ids;
			GArray *obj_graph_ids;
		} ready;
	} inserts;
};

static void class_finalize     (GObject      *object);

G_DEFINE_TYPE (TrackerClass, tracker_class, G_TYPE_OBJECT);

static void
tracker_class_class_init (TrackerClassClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = class_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerClassPrivate));
}

static void
tracker_class_init (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	priv = TRACKER_CLASS_GET_PRIVATE (service);

	priv->id = 0;
	priv->super_classes = g_array_new (TRUE, TRUE, sizeof (TrackerClass *));
	priv->domain_indexes = g_array_new (TRUE, TRUE, sizeof (TrackerProperty *));
	priv->last_domain_indexes = NULL;

	priv->deletes.pending.sub_pred_ids = g_array_new (FALSE, FALSE, sizeof (gint64));
	priv->deletes.pending.obj_graph_ids = g_array_new (FALSE, FALSE, sizeof (gint64));
	priv->deletes.ready.sub_pred_ids = g_array_new (FALSE, FALSE, sizeof (gint64));
	priv->deletes.ready.obj_graph_ids = g_array_new (FALSE, FALSE, sizeof (gint64));

	priv->inserts.pending.sub_pred_ids = g_array_new (FALSE, FALSE, sizeof (gint64));
	priv->inserts.pending.obj_graph_ids = g_array_new (FALSE, FALSE, sizeof (gint64));
	priv->inserts.ready.sub_pred_ids = g_array_new (FALSE, FALSE, sizeof (gint64));
	priv->inserts.ready.obj_graph_ids = g_array_new (FALSE, FALSE, sizeof (gint64));

	/* Make GET_PRIV working */
	service->priv = priv;
}

static void
class_finalize (GObject *object)
{
	TrackerClassPrivate *priv;

	priv = GET_PRIV (object);

	g_free (priv->uri);
	g_free (priv->name);

	g_array_free (priv->super_classes, TRUE);
	g_array_free (priv->domain_indexes, TRUE);

	g_array_free (priv->deletes.pending.sub_pred_ids, TRUE);
	g_array_free (priv->deletes.pending.obj_graph_ids, TRUE);
	g_array_free (priv->deletes.ready.sub_pred_ids, TRUE);
	g_array_free (priv->deletes.ready.obj_graph_ids, TRUE);

	g_array_free (priv->inserts.pending.sub_pred_ids, TRUE);
	g_array_free (priv->inserts.pending.obj_graph_ids, TRUE);
	g_array_free (priv->inserts.ready.sub_pred_ids, TRUE);
	g_array_free (priv->inserts.ready.obj_graph_ids, TRUE);

	if (priv->last_domain_indexes)
		g_array_free (priv->last_domain_indexes, TRUE);


	(G_OBJECT_CLASS (tracker_class_parent_class)->finalize) (object);
}

TrackerClass *
tracker_class_new (void)
{
	TrackerClass *service;

	service = g_object_new (TRACKER_TYPE_CLASS, NULL);

	return service;
}

const gchar *
tracker_class_get_uri (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = GET_PRIV (service);

	return priv->uri;
}

const gchar *
tracker_class_get_name (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = GET_PRIV (service);

	return priv->name;
}

gint
tracker_class_get_count (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), 0);

	priv = GET_PRIV (service);

	return priv->count;
}

gint
tracker_class_get_id (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), 0);

	priv = GET_PRIV (service);

	return priv->id;
}

TrackerClass **
tracker_class_get_super_classes (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = GET_PRIV (service);

	return (TrackerClass **) priv->super_classes->data;
}

TrackerProperty **
tracker_class_get_domain_indexes (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = GET_PRIV (service);

	return (TrackerProperty **) priv->domain_indexes->data;
}


TrackerProperty **
tracker_class_get_last_domain_indexes (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), NULL);

	priv = GET_PRIV (service);

	return (TrackerProperty **) (priv->last_domain_indexes ? priv->last_domain_indexes->data : NULL);
}

gboolean
tracker_class_get_is_new (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = GET_PRIV (service);

	return priv->is_new;
}

gboolean
tracker_class_get_notify (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = GET_PRIV (service);

	return priv->notify;
}

gboolean
tracker_class_get_db_schema_changed (TrackerClass *service)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (service), FALSE);

	priv = GET_PRIV (service);

	return priv->db_schema_changed;
}

void
tracker_class_set_uri (TrackerClass *service,
                       const gchar  *value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

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
			namespace = tracker_ontologies_get_namespace_by_uri (namespace_uri);
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
tracker_class_set_count (TrackerClass *service,
                         gint          value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->count = value;
}


void
tracker_class_set_id (TrackerClass *service,
                      gint          value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->id = value;
}

void
tracker_class_add_super_class (TrackerClass *service,
                               TrackerClass *value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));
	g_return_if_fail (TRACKER_IS_CLASS (value));

	priv = GET_PRIV (service);

	g_array_append_val (priv->super_classes, value);
}

void
tracker_class_add_domain_index (TrackerClass *service,
                                TrackerProperty *value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));
	g_return_if_fail (TRACKER_IS_PROPERTY (value));

	priv = GET_PRIV (service);

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

	priv = GET_PRIV (service);

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

	priv = GET_PRIV (service);
	priv->last_domain_indexes = priv->domain_indexes;
	priv->domain_indexes = g_array_new (TRUE, TRUE, sizeof (TrackerProperty *));
}

void
tracker_class_set_is_new (TrackerClass *service,
                          gboolean      value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->is_new = value;
}


void
tracker_class_set_notify (TrackerClass *service,
                          gboolean      value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->notify = value;
}

void
tracker_class_set_db_schema_changed (TrackerClass *service,
                                     gboolean      value)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (service));

	priv = GET_PRIV (service);

	priv->db_schema_changed = value;
}

gboolean
tracker_class_has_insert_events (TrackerClass *class)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (class), FALSE);

	priv = GET_PRIV (class);

	return (priv->inserts.ready.sub_pred_ids->len > 0);
}

gboolean
tracker_class_has_delete_events (TrackerClass *class)
{
	TrackerClassPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_CLASS (class), FALSE);

	priv = GET_PRIV (class);

	return (priv->deletes.ready.sub_pred_ids->len > 0);
}

void
tracker_class_foreach_insert_event (TrackerClass        *class,
                                    TrackerEventsForeach foreach,
                                    gpointer             user_data)
{
	guint i;
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (class));
	g_return_if_fail (foreach != NULL);

	priv = GET_PRIV (class);

	for (i = 0; i < priv->inserts.ready.sub_pred_ids->len; i++) {
		gint graph_id, subject_id, pred_id, object_id;
		gint64 sub_pred_id;
		gint64 obj_graph_id;

		sub_pred_id = g_array_index (priv->inserts.ready.sub_pred_ids, gint64, i);
		obj_graph_id = g_array_index (priv->inserts.ready.obj_graph_ids, gint64, i);

		pred_id = sub_pred_id & 0xffffffff;
		subject_id = sub_pred_id >> 32;
		graph_id = obj_graph_id & 0xffffffff;
		object_id = obj_graph_id >> 32;

		foreach (graph_id, subject_id, pred_id, object_id, user_data);
	}
}

void
tracker_class_foreach_delete_event (TrackerClass        *class,
                                    TrackerEventsForeach foreach,
                                    gpointer             user_data)
{
	guint i;
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (class));
	g_return_if_fail (foreach != NULL);

	priv = GET_PRIV (class);

	for (i = 0; i < priv->deletes.ready.sub_pred_ids->len; i++) {
		gint graph_id, subject_id, pred_id, object_id;
		gint64 sub_pred_id;
		gint64 obj_graph_id;

		sub_pred_id = g_array_index (priv->deletes.ready.sub_pred_ids, gint64, i);
		obj_graph_id = g_array_index (priv->deletes.ready.obj_graph_ids, gint64, i);

		pred_id = sub_pred_id & 0xffffffff;
		subject_id = sub_pred_id >> 32;
		graph_id = obj_graph_id & 0xffffffff;
		object_id = obj_graph_id >> 32;

		foreach (graph_id, subject_id, pred_id, object_id, user_data);
	}
}

void
tracker_class_reset_ready_events (TrackerClass *class)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (class));

	priv = GET_PRIV (class);

	/* Reset */
	g_array_set_size (priv->deletes.ready.sub_pred_ids, 0);
	g_array_set_size (priv->deletes.ready.obj_graph_ids, 0);

	g_array_set_size (priv->inserts.ready.sub_pred_ids, 0);
	g_array_set_size (priv->inserts.ready.obj_graph_ids, 0);

}

void
tracker_class_reset_pending_events (TrackerClass *class)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (class));

	priv = GET_PRIV (class);

	/* Reset */
	g_array_set_size (priv->deletes.pending.sub_pred_ids, 0);
	g_array_set_size (priv->deletes.pending.obj_graph_ids, 0);

	g_array_set_size (priv->inserts.pending.sub_pred_ids, 0);
	g_array_set_size (priv->inserts.pending.obj_graph_ids, 0);

}

void
tracker_class_transact_events (TrackerClass *class)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (class));
	priv = GET_PRIV (class);

	/* Move */
	g_array_insert_vals (priv->deletes.ready.obj_graph_ids,
	                     priv->deletes.ready.obj_graph_ids->len,
	                     priv->deletes.pending.obj_graph_ids->data,
	                     priv->deletes.pending.obj_graph_ids->len);

	g_array_insert_vals (priv->deletes.ready.sub_pred_ids,
	                     priv->deletes.ready.sub_pred_ids->len,
	                     priv->deletes.pending.sub_pred_ids->data,
	                     priv->deletes.pending.sub_pred_ids->len);

	/* Reset */
	g_array_set_size (priv->deletes.pending.sub_pred_ids, 0);
	g_array_set_size (priv->deletes.pending.obj_graph_ids, 0);


	/* Move */
	g_array_insert_vals (priv->inserts.ready.obj_graph_ids,
	                     priv->inserts.ready.obj_graph_ids->len,
	                     priv->inserts.pending.obj_graph_ids->data,
	                     priv->inserts.pending.obj_graph_ids->len);

	g_array_insert_vals (priv->inserts.ready.sub_pred_ids,
	                     priv->inserts.ready.sub_pred_ids->len,
	                     priv->inserts.pending.sub_pred_ids->data,
	                     priv->inserts.pending.sub_pred_ids->len);

	/* Reset */
	g_array_set_size (priv->inserts.pending.sub_pred_ids, 0);
	g_array_set_size (priv->inserts.pending.obj_graph_ids, 0);

}

static void
insert_vals_into_arrays (GArray *sub_pred_ids,
                         GArray *obj_graph_ids,
                         gint    graph_id,
                         gint    subject_id,
                         gint    pred_id,
                         gint    object_id)
{
	guint i, j, k;
	gint64 tmp;
	gint64 sub_pred_id;
	gint64 obj_graph_id;

	sub_pred_id = (gint64) subject_id;
	sub_pred_id = sub_pred_id << 32 | pred_id;
	obj_graph_id = (gint64) object_id;
	obj_graph_id = obj_graph_id << 32 | graph_id;

	i = 0;
	if (sub_pred_ids->len == 0 || g_array_index (sub_pred_ids, gint64, i) > sub_pred_id) {
		g_array_prepend_val (sub_pred_ids, sub_pred_id);
		g_array_prepend_val (obj_graph_ids, obj_graph_id);
		return;
	}

	j = sub_pred_ids->len - 1;
	if (g_array_index (sub_pred_ids, gint64, j) <= sub_pred_id) {
		g_array_append_val (sub_pred_ids, sub_pred_id);
		g_array_append_val (obj_graph_ids, obj_graph_id);
		return;
	}

	while (j - i > 1) {
		k = (i + j) / 2;
		tmp = g_array_index (sub_pred_ids, gint64, k);
		if (tmp == sub_pred_id) {
			j = k + 1;
			break;
		} else if (tmp > sub_pred_id)
			j = k;
		else
			i = k;
	}

	g_array_insert_val (sub_pred_ids, j, sub_pred_id);
	g_array_insert_val (obj_graph_ids, j, obj_graph_id);
}

void
tracker_class_add_insert_event (TrackerClass *class,
                                gint          graph_id,
                                gint          subject_id,
                                gint          pred_id,
                                gint          object_id)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (class));
	priv = GET_PRIV (class);

	insert_vals_into_arrays (priv->inserts.pending.sub_pred_ids,
	                         priv->inserts.pending.obj_graph_ids,
	                         graph_id,
	                         subject_id,
	                         pred_id,
	                         object_id);
}

void
tracker_class_add_delete_event (TrackerClass *class,
                                gint          graph_id,
                                gint          subject_id,
                                gint          pred_id,
                                gint          object_id)
{
	TrackerClassPrivate *priv;

	g_return_if_fail (TRACKER_IS_CLASS (class));
	priv = GET_PRIV (class);

	insert_vals_into_arrays (priv->deletes.pending.sub_pred_ids,
	                         priv->deletes.pending.obj_graph_ids,
	                         graph_id,
	                         subject_id,
	                         pred_id,
	                         object_id);
}
