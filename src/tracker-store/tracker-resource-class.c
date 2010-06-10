/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <string.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-ontologies.h>

#include <libtracker-db/tracker-db-dbus.h>

#include <libtracker-data/tracker-data-update.h>

#include "tracker-dbus.h"
#include "tracker-events.h"
#include "tracker-resource-class.h"
#include "tracker-marshal.h"

#define TRACKER_RESOURCE_CLASS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_RESOURCE_CLASS, TrackerResourceClassPrivate))

typedef struct {
	gchar *rdf_class;
	gchar *dbus_path;
	GPtrArray *adds, *ups, *dels;
	GStringChunk *changed_strings;
	DBusConnection *connection;
} TrackerResourceClassPrivate;

typedef struct {
	gchar *uri;
	TrackerProperty *predicate;
} ChangedItem;

enum {
	SUBJECTS_ADDED,
	SUBJECTS_REMOVED,
	SUBJECTS_CHANGED,
	LAST_SIGNAL
};

static void tracker_resource_class_finalize (GObject *object);

G_DEFINE_TYPE(TrackerResourceClass, tracker_resource_class, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = {0};

static void
tracker_resource_class_class_init (TrackerResourceClassClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_resource_class_finalize;

	/* Not used by the code, but left here for the D-Bus introspection to work
	 * right (for our beloved d-feet users) */

	signals[SUBJECTS_ADDED] =
		g_signal_new ("subjects-added",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              tracker_marshal_VOID__BOXED,
		              G_TYPE_NONE,
		              1,
		              G_TYPE_STRV);

	signals[SUBJECTS_REMOVED] =
		g_signal_new ("subjects-removed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              tracker_marshal_VOID__BOXED,
		              G_TYPE_NONE,
		              1,
		              G_TYPE_STRV);

	signals[SUBJECTS_CHANGED] =
		g_signal_new ("subjects-changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              tracker_marshal_VOID__BOXED_BOXED,
		              G_TYPE_NONE,
		              2,
		              G_TYPE_STRV,
		              G_TYPE_STRV);

	g_type_class_add_private (object_class, sizeof (TrackerResourceClassPrivate));
}




static void
tracker_resource_class_init (TrackerResourceClass *object)
{
}

static void
emit_strings (TrackerResourceClass *object,
              const gchar *signal_name,
              GPtrArray *array)
{
	TrackerResourceClassPrivate *priv;

	priv = TRACKER_RESOURCE_CLASS_GET_PRIVATE (object);

	if (array->len > 0) {
		DBusMessageIter iter;
		DBusMessageIter strv_iter;
		DBusMessage *message;
		guint i;

		message = dbus_message_new_signal (priv->dbus_path,
		                                   TRACKER_RESOURCES_CLASS_INTERFACE,
		                                   signal_name);

		dbus_message_iter_init_append (message, &iter);

		dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY, 
		                                  DBUS_TYPE_STRING_AS_STRING, &strv_iter);

		for (i = 0; i < array->len; i++) {
			dbus_message_iter_append_basic (&strv_iter, DBUS_TYPE_STRING, &array->pdata [i]);
		}

		dbus_message_iter_close_container (&iter, &strv_iter);

		dbus_connection_send (priv->connection, message, NULL);

		dbus_message_unref (message);
	}
}

static void
emit_changed_strings (TrackerResourceClass *object,
                      GPtrArray *array)
{
	TrackerResourceClassPrivate *priv;

	priv = TRACKER_RESOURCE_CLASS_GET_PRIVATE (object);

	if (array->len > 0) {
		DBusMessageIter iter;
		DBusMessageIter strv1_iter;
		DBusMessageIter strv2_iter;
		DBusMessage *message;
		guint i;

		message = dbus_message_new_signal (priv->dbus_path,
		                                   TRACKER_RESOURCES_CLASS_INTERFACE,
		                                   "SubjectsChanged");

		dbus_message_iter_init_append (message, &iter);

		dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY, 
		                                  DBUS_TYPE_STRING_AS_STRING, &strv1_iter);

		for (i = 0; i < array->len; i++) {
			ChangedItem *item = array->pdata [i];
			dbus_message_iter_append_basic (&strv1_iter, DBUS_TYPE_STRING, &item->uri);
		}

		dbus_message_iter_close_container (&iter, &strv1_iter);

		dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY, 
		                                  DBUS_TYPE_STRING_AS_STRING, &strv2_iter);

		for (i = 0; i < array->len; i++) {
			ChangedItem *item = array->pdata [i];
			const gchar *predicate = tracker_property_get_uri (item->predicate);
			dbus_message_iter_append_basic (&strv2_iter, DBUS_TYPE_STRING, &predicate);
		}

		dbus_message_iter_close_container (&iter, &strv2_iter);

		dbus_connection_send (priv->connection, message, NULL);

		dbus_message_unref (message);
	}
}

static void
free_changed_array (GPtrArray *array)
{
	guint i;
	for (i = 0; i < array->len; i++) {
		ChangedItem *item = array->pdata [i];
		g_object_unref (item->predicate);
		g_slice_free (ChangedItem, item);
	}
	g_ptr_array_free (array, TRUE);
}

void
tracker_resource_class_emit_events (TrackerResourceClass  *object)
{
	TrackerResourceClassPrivate *priv;

	priv = TRACKER_RESOURCE_CLASS_GET_PRIVATE (object);

	if (priv->adds) {
		emit_strings (object, "SubjectsAdded", priv->adds);
		g_ptr_array_free (priv->adds, TRUE);
		priv->adds = NULL;
	}

	if (priv->ups) {
		emit_changed_strings (object, priv->ups);
		free_changed_array (priv->ups);
		priv->ups = NULL;
	}

	if (priv->dels) {
		emit_strings (object, "SubjectsRemoved", priv->dels);
		g_ptr_array_free (priv->dels, TRUE);
		priv->dels = NULL;
	}

	if (priv->changed_strings) {
		g_string_chunk_free (priv->changed_strings);
		priv->changed_strings = NULL;
	}

}


static void
tracker_resource_class_finalize (GObject *object)
{
	TrackerResourceClassPrivate *priv;

	priv = TRACKER_RESOURCE_CLASS_GET_PRIVATE (object);

	/* Emit pending events */
	tracker_resource_class_emit_events ((TrackerResourceClass *) object);

	g_free (priv->rdf_class);
	g_free (priv->dbus_path);
	dbus_connection_unref (priv->connection);

	G_OBJECT_CLASS (tracker_resource_class_parent_class)->finalize (object);
}

TrackerResourceClass *
tracker_resource_class_new (const gchar     *rdf_class,
                            const gchar     *dbus_path,
                            DBusGConnection *connection)
{
	TrackerResourceClass         *object;
	TrackerResourceClassPrivate *priv;

	object = g_object_new (TRACKER_TYPE_RESOURCE_CLASS, NULL);

	priv = TRACKER_RESOURCE_CLASS_GET_PRIVATE (object);

	priv->rdf_class = g_strdup (rdf_class);
	priv->dbus_path = g_strdup (dbus_path);
	priv->connection = dbus_connection_ref (dbus_g_connection_get_connection (connection));

	return object;
}


const gchar *
tracker_resource_class_get_rdf_class (TrackerResourceClass  *object)
{
	TrackerResourceClassPrivate *priv;

	priv = TRACKER_RESOURCE_CLASS_GET_PRIVATE (object);

	return priv->rdf_class;
}

static gboolean
has_already (GPtrArray *array, const gchar *uri)
{
	guint i;

	if (!array) {
		return FALSE;
	}

	for (i = 0; i < array->len; i++) {
		if (g_strcmp0 (g_ptr_array_index (array, i), uri) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

void
tracker_resource_class_add_event (TrackerResourceClass  *object,
                                  const gchar           *uri,
                                  TrackerProperty       *predicate,
                                  TrackerDBusEventsType type)
{
	TrackerResourceClassPrivate *priv;

	priv = TRACKER_RESOURCE_CLASS_GET_PRIVATE (object);

	if (!priv->changed_strings) {
		/* Default size a bit longer than this sample uri */
		priv->changed_strings = g_string_chunk_new (strlen (uri) + 10);
	}

	switch (type) {
	case TRACKER_DBUS_EVENTS_TYPE_ADD:
		if (!has_already (priv->adds, uri)) {
			if (!priv->adds)
				priv->adds = g_ptr_array_new ();
			g_ptr_array_add (priv->adds, g_string_chunk_insert_const (priv->changed_strings, uri));
		}
		break;
	case TRACKER_DBUS_EVENTS_TYPE_UPDATE: {
			/* Duplicate checking slows down too much
			   if (!changed_has_already (priv->ups, uri, predicate)) { */
			ChangedItem *item;

			item = g_slice_new (ChangedItem);

			item->uri = g_string_chunk_insert_const (priv->changed_strings, uri);
			item->predicate = g_object_ref (predicate);

			if (!priv->ups)
				priv->ups = g_ptr_array_new ();
			g_ptr_array_add (priv->ups, item);
		}
		break;
	case TRACKER_DBUS_EVENTS_TYPE_DELETE:
		if (!has_already (priv->dels, uri)) {
			if (!priv->dels)
				priv->dels = g_ptr_array_new ();
			g_ptr_array_add (priv->dels, g_string_chunk_insert_const (priv->changed_strings, uri));
		}
		break;
	default:
		break;
	}
}
