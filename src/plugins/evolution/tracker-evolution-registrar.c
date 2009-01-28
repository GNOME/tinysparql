/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
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

#include <libtracker-data/tracker-data-update.h>
#define __TRACKER_EVOLUTION_REGISTRAR_C__
#include "tracker-evolution-registrar.h"
#include "tracker-evolution-registrar-glue.h"

const DBusGMethodInfo *registrar_methods = dbus_glib_tracker_evolution_registrar_methods;

#define TRACKER_EVOLUTION_REGISTRAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_EVOLUTION_REGISTRAR, TrackerEvolutionRegistrarPrivate))

G_DEFINE_TYPE (TrackerEvolutionRegistrar, tracker_evolution_registrar, G_TYPE_OBJECT)

/* This runs in-process of trackerd. It simply proxies everything to the indexer
 * who wont always be running. Which is why this is needed (trackerd is always
 * running, so it's more suitable to respond to Evolution's requests). */

typedef struct {
	DBusGProxy *idx_proxy;
	DBusGConnection *connection;
} TrackerEvolutionRegistrarPrivate;

enum {
	PROP_0,
	PROP_CONNECTION
};

static void
tracker_evolution_registrar_finalize (GObject *object)
{
	TrackerEvolutionRegistrarPrivate *priv = TRACKER_EVOLUTION_REGISTRAR_GET_PRIVATE (object);

	if (priv->idx_proxy)
		g_object_unref (priv->idx_proxy);

	G_OBJECT_CLASS (tracker_evolution_registrar_parent_class)->finalize (object);
}

static void 
tracker_evolution_registrar_set_connection (TrackerEvolutionRegistrar *object, 
					    DBusGConnection *connection)
{
	TrackerEvolutionRegistrarPrivate *priv = TRACKER_EVOLUTION_REGISTRAR_GET_PRIVATE (object);

	priv->connection = connection; /* weak */

	priv->idx_proxy = dbus_g_proxy_new_for_name (priv->connection, 
						     "org.freedesktop.Tracker.Indexer",
						     TRACKER_EVOLUTION_INDEXER_PATH,
						     TRACKER_EVOLUTION_REGISTRAR_INTERFACE);
}

static void
tracker_evolution_registrar_set_property (GObject      *object,
					  guint         prop_id,
					  const GValue *value,
					  GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_CONNECTION:
		tracker_evolution_registrar_set_connection (TRACKER_EVOLUTION_REGISTRAR (object),
							    g_value_get_pointer (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_evolution_registrar_get_property (GObject    *object,
					  guint       prop_id,
					  GValue     *value,
					  GParamSpec *pspec)
{
	TrackerEvolutionRegistrarPrivate *priv = TRACKER_EVOLUTION_REGISTRAR_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_pointer (value, priv->connection);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_evolution_registrar_class_init (TrackerEvolutionRegistrarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_evolution_registrar_finalize;
	object_class->set_property = tracker_evolution_registrar_set_property;
	object_class->get_property = tracker_evolution_registrar_get_property;

	g_object_class_install_property (object_class,
					 PROP_CONNECTION,
					 g_param_spec_pointer ("connection",
							       "DBus connection",
							       "DBus connection",
							       G_PARAM_READWRITE |
							       G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerEvolutionRegistrarPrivate));
}

static void
tracker_evolution_registrar_init (TrackerEvolutionRegistrar *object)
{
}

void
tracker_evolution_registrar_set (TrackerEvolutionRegistrar *object, 
				 const gchar *subject, 
				 const GStrv predicates,
				 const GStrv values,
				 const guint modseq,
				 DBusGMethodInvocation *context,
				 GError *derror)
{
	TrackerEvolutionRegistrarPrivate *priv = TRACKER_EVOLUTION_REGISTRAR_GET_PRIVATE (object);

	dbus_async_return_if_fail (subject != NULL, context);

	if (predicates && values) {

		dbus_async_return_if_fail (g_strv_length (predicates) == 
					   g_strv_length (values), context);

		dbus_g_proxy_call_no_reply (priv->idx_proxy,
					    "Set",
					    G_TYPE_STRING, subject,
					    G_TYPE_STRV, predicates,
					    G_TYPE_STRV, values,
					    G_TYPE_UINT, modseq,
					    G_TYPE_INVALID, 
					    G_TYPE_INVALID);
	}

	dbus_g_method_return (context);
}

void
tracker_evolution_registrar_set_many (TrackerEvolutionRegistrar *object, 
				      const GStrv subjects, 
				      const GPtrArray *predicates,
				      const GPtrArray *values,
				      const guint modseq,
				      DBusGMethodInvocation *context,
				      GError *derror)
{
	TrackerEvolutionRegistrarPrivate *priv = TRACKER_EVOLUTION_REGISTRAR_GET_PRIVATE (object);
	guint len;

	dbus_async_return_if_fail (subjects != NULL, context);
	dbus_async_return_if_fail (predicates != NULL, context);
	dbus_async_return_if_fail (values != NULL, context);

	len = g_strv_length (subjects);

	dbus_async_return_if_fail (len == predicates->len, context);
	dbus_async_return_if_fail (len == values->len, context);

	dbus_g_proxy_call_no_reply (priv->idx_proxy,
				    "SetMany",
				    G_TYPE_STRV, subjects,
				    TRACKER_TYPE_G_STRV_ARRAY, predicates,
				    TRACKER_TYPE_G_STRV_ARRAY, values,
				    G_TYPE_UINT, modseq,
				    G_TYPE_INVALID, 
				    G_TYPE_INVALID);

	dbus_g_method_return (context);
}

void
tracker_evolution_registrar_unset_many (TrackerEvolutionRegistrar *object, 
					const GStrv subjects, 
					const guint modseq,
					DBusGMethodInvocation *context,
					GError *derror)
{
	TrackerEvolutionRegistrarPrivate *priv = TRACKER_EVOLUTION_REGISTRAR_GET_PRIVATE (object);

	dbus_async_return_if_fail (subjects != NULL, context);

	dbus_g_proxy_call_no_reply (priv->idx_proxy,
				    "UnsetMany",
				    G_TYPE_STRV, subjects,
				    G_TYPE_UINT, modseq,
				    G_TYPE_INVALID, 
				    G_TYPE_INVALID);

	dbus_g_method_return (context);
}

void
tracker_evolution_registrar_unset (TrackerEvolutionRegistrar *object, 
				   const gchar *subject, 
				   const guint modseq,
				   DBusGMethodInvocation *context,
				   GError *derror)
{
	TrackerEvolutionRegistrarPrivate *priv = TRACKER_EVOLUTION_REGISTRAR_GET_PRIVATE (object);

	dbus_async_return_if_fail (subject != NULL, context);

	dbus_g_proxy_call_no_reply (priv->idx_proxy,
				    "Unset",
				    G_TYPE_STRING, subject,
				    G_TYPE_UINT, modseq,
				    G_TYPE_INVALID, 
				    G_TYPE_INVALID);

	dbus_g_method_return (context);
}

void
tracker_evolution_registrar_cleanup (TrackerEvolutionRegistrar *object, 
				     const guint modseq,
				     DBusGMethodInvocation *context,
				     GError *derror)
{
	TrackerEvolutionRegistrarPrivate *priv = TRACKER_EVOLUTION_REGISTRAR_GET_PRIVATE (object);

	dbus_g_proxy_call_no_reply (priv->idx_proxy,
				    "Cleanup",
				    G_TYPE_UINT, modseq,
				    G_TYPE_INVALID, 
				    G_TYPE_INVALID);

	dbus_g_method_return (context);
}
