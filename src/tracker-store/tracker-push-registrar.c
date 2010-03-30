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

#include "tracker-push-registrar.h"

#define TRACKER_PUSH_REGISTRAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_PUSH_REGISTRAR, TrackerPushRegistrarPrivate))

typedef struct TrackerPushRegistrarPrivate TrackerPushRegistrarPrivate;

struct TrackerPushRegistrarPrivate {
	DBusGProxy *manager_proxy;
	GObject *object;
	const gchar *service;
};

enum {
	PROP_0,
	PROP_MANAGER,
	PROP_OBJECT,
	PROP_SERVICE
};

static void   tracker_push_registrar_finalize     (GObject      *object);
static void   tracker_push_registrar_constructed  (GObject      *object);
static void   tracker_push_registrar_set_property (GObject      *object,
                                                   guint         prop_id,
                                                   const GValue *value,
                                                   GParamSpec   *pspec);
static void   tracker_push_registrar_get_property (GObject      *object,
                                                   guint         prop_id,
                                                   GValue       *value,
                                                   GParamSpec   *pspec);

G_DEFINE_TYPE (TrackerPushRegistrar, tracker_push_registrar, G_TYPE_OBJECT)

static void
tracker_push_registrar_class_init (TrackerPushRegistrarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_push_registrar_finalize;

	object_class->constructed = tracker_push_registrar_constructed;
	object_class->set_property = tracker_push_registrar_set_property;
	object_class->get_property = tracker_push_registrar_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_MANAGER,
	                                 g_param_spec_object ("manager",
	                                                      "Manager",
	                                                      "Manager ",
	                                                      G_TYPE_OBJECT,
	                                                      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_OBJECT,
	                                 g_param_spec_object ("object",
	                                                      "Object",
	                                                      "Object ",
	                                                      G_TYPE_OBJECT,
	                                                      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_MANAGER,
	                                 g_param_spec_string ("service",
	                                                      "Service",
	                                                      "Service ",
	                                                      NULL,
	                                                      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (TrackerPushRegistrarPrivate));
}

static void
tracker_push_registrar_init (TrackerPushRegistrar *registrar)
{
}

static void
tracker_push_registrar_finalize (GObject *object)
{
	TrackerPushRegistrarPrivate *priv;

	priv = TRACKER_PUSH_REGISTRAR_GET_PRIVATE (object);

	if (priv->object)
		g_object_unref (priv->object);

	if (priv->manager_proxy)
		g_object_unref (priv->manager_proxy);

	G_OBJECT_CLASS (tracker_push_registrar_parent_class)->finalize (object);
}

static void
tracker_push_registrar_constructed (GObject *object)
{
	if (G_OBJECT_CLASS (tracker_push_registrar_parent_class)->constructed) {
		G_OBJECT_CLASS (tracker_push_registrar_parent_class)->constructed (object);
	}
}

static void
tracker_push_registrar_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_MANAGER:
		tracker_push_registrar_set_manager (TRACKER_PUSH_REGISTRAR (object),
		                                    g_value_get_object (value));
		break;
	case PROP_OBJECT:
		tracker_push_registrar_set_object (TRACKER_PUSH_REGISTRAR (object),
		                                   g_value_get_object (value));
		break;
	case PROP_SERVICE:
		tracker_push_registrar_set_service (TRACKER_PUSH_REGISTRAR (object),
		                                    g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_push_registrar_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
	TrackerPushRegistrarPrivate *priv;

	priv = TRACKER_PUSH_REGISTRAR_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_MANAGER:
		g_value_set_object (value, priv->manager_proxy);
		break;
	case PROP_OBJECT:
		g_value_set_object (value, priv->object);
		break;
	case PROP_SERVICE:
		g_value_set_static_string (value, priv->service);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

/**
 * tracker_push_registrar_get_service:
 * @registrar: A #TrackerPushRegistrar
 *
 * Returns the DBus service that @registrar consumes.
 *
 * Returns: The DBus service name.
 **/
G_CONST_RETURN gchar *
tracker_push_registrar_get_service (TrackerPushRegistrar *registrar)
{
	TrackerPushRegistrarPrivate *priv;

	priv = TRACKER_PUSH_REGISTRAR_GET_PRIVATE (registrar);

	return priv->service;
}

/**
 * tracker_push_registrar_get_object:
 * @registrar: A #TrackerPushRegistrar
 *
 * Returns the DBus object that @registrar provides
 *
 * Returns: The DBus object.
 **/
GObject *
tracker_push_registrar_get_object (TrackerPushRegistrar *registrar)
{
	TrackerPushRegistrarPrivate *priv;

	priv = TRACKER_PUSH_REGISTRAR_GET_PRIVATE (registrar);

	return priv->object;
}

/**
 * tracker_push_registrar_get_manager:
 * @registrar: A #TrackerPushRegistrar
 *
 * Returns the DBus proxy to the DBus object that @registrar consumes
 *
 * Returns: The DBus proxy.
 **/
DBusGProxy *
tracker_push_registrar_get_manager (TrackerPushRegistrar *registrar)
{
	TrackerPushRegistrarPrivate *priv;

	priv = TRACKER_PUSH_REGISTRAR_GET_PRIVATE (registrar);

	return priv->manager_proxy;
}

/**
 * tracker_push_registrar_set_service:
 * @registrar: A #TrackerPushRegistrar
 * @service: a DBus service string
 *
 * Set the DBus service string that @registrar will consumes
 *
 **/
void
tracker_push_registrar_set_service (TrackerPushRegistrar *registrar,
                                    const gchar *service)
{
	TrackerPushRegistrarPrivate *priv;

	priv = TRACKER_PUSH_REGISTRAR_GET_PRIVATE (registrar);

	priv->service = service;

	g_object_notify (G_OBJECT (registrar), "service");
}

/**
 * tracker_push_registrar_set_object:
 * @registrar: A #TrackerPushRegistrar
 * @object: a DBus object
 *
 * Set the DBus object created
 **/
void
tracker_push_registrar_set_object (TrackerPushRegistrar *registrar,
                                   GObject              *object)
{
	TrackerPushRegistrarPrivate *priv;

	priv = TRACKER_PUSH_REGISTRAR_GET_PRIVATE (registrar);

	if (object) {
		g_object_ref (object);
	}

	if (priv->object) {
		g_object_unref (priv->object);
	}

	priv->object = object;

	g_object_notify (G_OBJECT (registrar), "object");
}

/**
 * tracker_push_registrar_set_manager:
 * @registrar: A #TrackerPushRegistrar
 * @manager: a DBus proxy
 *
 * Set the DBus proxy to the manager DBus object being consumed
 **/
void
tracker_push_registrar_set_manager (TrackerPushRegistrar *registrar,
                                    DBusGProxy           *manager)
{
	TrackerPushRegistrarPrivate *priv;

	priv = TRACKER_PUSH_REGISTRAR_GET_PRIVATE (registrar);

	if (manager) {
		g_object_ref (manager);
	}

	if (priv->manager_proxy) {
		g_object_unref (priv->manager_proxy);
	}

	priv->manager_proxy = manager;

	g_object_notify (G_OBJECT (registrar), "manager");

}

/**
 * tracker_push_registrar_enable:
 * @registrar: A #TrackerPushRegistrar
 *
 * Enables the feature
 *
 **/
void
tracker_push_registrar_enable (TrackerPushRegistrar *registrar,
                               DBusGConnection *connection,
                               DBusGProxy *dbus_proxy,
                               GError **error)
{
	if (TRACKER_PUSH_REGISTRAR_GET_CLASS (registrar)->enable) {
		TRACKER_PUSH_REGISTRAR_GET_CLASS (registrar)->enable (registrar,
		                                                      connection,
		                                                      dbus_proxy,
		                                                      error);
	}
}

/**
 * tracker_push_registrar_disable:
 * @registrar: A #TrackerPushRegistrar
 *
 * Disables the feature
 *
 **/
void
tracker_push_registrar_disable (TrackerPushRegistrar *registrar)
{
	if (TRACKER_PUSH_REGISTRAR_GET_CLASS (registrar)->disable)
		TRACKER_PUSH_REGISTRAR_GET_CLASS (registrar)->disable (registrar);
}
