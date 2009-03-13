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

#include <glib-object.h>
#include <dbus/dbus-glib-bindings.h>

#include <libtracker-data/tracker-data-update.h>
#include <libtracker-data/tracker-data-manager.h>

#include <trackerd/tracker-push-registrar.h>

#define __TRACKER_KMAIL_REGISTRAR_C__

#include "tracker-kmail-registrar.h"
#include "tracker-kmail-registrar-glue.h"

#define TRACKER_KMAIL_REGISTRAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_KMAIL_REGISTRAR, TrackerKMailRegistrarPrivate))

#define TRACKER_TYPE_KMAIL_PUSH_REGISTRAR    (tracker_kmail_push_registrar_get_type ())
#define TRACKER_KMAIL_PUSH_REGISTRAR(module) (G_TYPE_CHECK_INSTANCE_CAST ((module), TRACKER_TYPE_KMAIL_PUSH_REGISTRAR, TrackerKMailPushRegistrar))

typedef struct TrackerKMailPushRegistrar TrackerKMailPushRegistrar;
typedef struct TrackerKMailPushRegistrarClass TrackerKMailPushRegistrarClass;

struct TrackerKMailPushRegistrar {
	TrackerPushRegistrar parent_instance;
};

struct TrackerKMailPushRegistrarClass {
	TrackerPushRegistrarClass parent_class;
};


typedef struct {
	DBusGProxy *idx_proxy;
	DBusGConnection *connection;
} TrackerKMailRegistrarPrivate;

enum {
	PROP_0,
	PROP_CONNECTION
};

static GType tracker_kmail_push_registrar_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (TrackerKMailRegistrar, tracker_kmail_registrar, G_TYPE_OBJECT)
G_DEFINE_TYPE (TrackerKMailPushRegistrar, tracker_kmail_push_registrar, TRACKER_TYPE_PUSH_REGISTRAR);

/* This runs in-process of trackerd. It simply proxies everything to the indexer
 * who wont always be running. Which is why this is needed (trackerd is always
 * running, so it's more suitable to respond to KMail's requests). */

static void
tracker_kmail_registrar_finalize (GObject *object)
{
	TrackerKMailRegistrarPrivate *priv = TRACKER_KMAIL_REGISTRAR_GET_PRIVATE (object);

	if (priv->idx_proxy)
		g_object_unref (priv->idx_proxy);

	G_OBJECT_CLASS (tracker_kmail_registrar_parent_class)->finalize (object);
}

static void 
tracker_kmail_registrar_set_connection (TrackerKMailRegistrar *object, 
					DBusGConnection *connection)
{
	TrackerKMailRegistrarPrivate *priv = TRACKER_KMAIL_REGISTRAR_GET_PRIVATE (object);

	priv->connection = connection; /* weak */

	priv->idx_proxy = dbus_g_proxy_new_for_name (priv->connection, 
						     "org.freedesktop.Tracker.Indexer",
						     TRACKER_KMAIL_INDEXER_PATH,
						     TRACKER_KMAIL_REGISTRAR_INTERFACE);
}

static void
tracker_kmail_registrar_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_CONNECTION:
		tracker_kmail_registrar_set_connection (TRACKER_KMAIL_REGISTRAR (object),
							    g_value_get_pointer (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_kmail_registrar_get_property (GObject    *object,
					  guint       prop_id,
					  GValue     *value,
					  GParamSpec *pspec)
{
	TrackerKMailRegistrarPrivate *priv = TRACKER_KMAIL_REGISTRAR_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_pointer (value, priv->connection);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_kmail_registrar_class_init (TrackerKMailRegistrarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_kmail_registrar_finalize;
	object_class->set_property = tracker_kmail_registrar_set_property;
	object_class->get_property = tracker_kmail_registrar_get_property;

	g_object_class_install_property (object_class,
					 PROP_CONNECTION,
					 g_param_spec_pointer ("connection",
							       "DBus connection",
							       "DBus connection",
							       G_PARAM_READWRITE |
							       G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerKMailRegistrarPrivate));
}

static void
tracker_kmail_registrar_init (TrackerKMailRegistrar *object)
{
}

void
tracker_kmail_registrar_set (TrackerKMailRegistrar *object, 
				 const gchar *subject, 
				 const GStrv predicates,
				 const GStrv values,
				 const guint modseq,
				 DBusGMethodInvocation *context,
				 GError *derror)
{
	TrackerKMailRegistrarPrivate *priv = TRACKER_KMAIL_REGISTRAR_GET_PRIVATE (object);

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
tracker_kmail_registrar_set_many (TrackerKMailRegistrar *object, 
				      const GStrv subjects, 
				      const GPtrArray *predicates,
				      const GPtrArray *values,
				      const guint modseq,
				      DBusGMethodInvocation *context,
				      GError *derror)
{
	TrackerKMailRegistrarPrivate *priv = TRACKER_KMAIL_REGISTRAR_GET_PRIVATE (object);
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
tracker_kmail_registrar_unset_many (TrackerKMailRegistrar *object, 
				    const GStrv subjects, 
				    const guint modseq,
				    DBusGMethodInvocation *context,
				    GError *derror)
{
	TrackerKMailRegistrarPrivate *priv = TRACKER_KMAIL_REGISTRAR_GET_PRIVATE (object);

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
tracker_kmail_registrar_unset (TrackerKMailRegistrar *object, 
			       const gchar *subject, 
			       const guint modseq,
			       DBusGMethodInvocation *context,
			       GError *derror)
{
	TrackerKMailRegistrarPrivate *priv = TRACKER_KMAIL_REGISTRAR_GET_PRIVATE (object);

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
tracker_kmail_registrar_cleanup (TrackerKMailRegistrar *object, 
				 const guint modseq,
				 DBusGMethodInvocation *context,
				 GError *derror)
{
	TrackerKMailRegistrarPrivate *priv = TRACKER_KMAIL_REGISTRAR_GET_PRIVATE (object);

	dbus_g_proxy_call_no_reply (priv->idx_proxy,
				    "Cleanup",
				    G_TYPE_UINT, modseq,
				    G_TYPE_INVALID, 
				    G_TYPE_INVALID);

	dbus_g_method_return (context);
}


static void
on_manager_destroy (DBusGProxy *proxy, gpointer user_data)
{
	return;
}

static void
tracker_kmail_push_registrar_enable (TrackerPushRegistrar *registrar, 
				     DBusGConnection      *connection,
				     DBusGProxy           *dbus_proxy, 
				     GError              **error)
{
	GError *nerror = NULL;
	guint result;
	DBusGProxy *manager_proxy;
	GObject *object;

	tracker_push_registrar_set_object (registrar, NULL);
	tracker_push_registrar_set_manager (registrar, NULL);

	manager_proxy = dbus_g_proxy_new_for_name (connection,
						   TRACKER_KMAIL_MANAGER_SERVICE,
						   TRACKER_KMAIL_MANAGER_PATH,
						   TRACKER_KMAIL_MANAGER_INTERFACE);

	/* Creation of the registrar */
	if (!org_freedesktop_DBus_request_name (dbus_proxy, 
						TRACKER_KMAIL_REGISTRAR_SERVICE,
						DBUS_NAME_FLAG_DO_NOT_QUEUE,
						&result, &nerror)) {

		g_critical ("Could not setup DBus, %s in use\n", 
			    TRACKER_KMAIL_REGISTRAR_SERVICE);

		if (nerror) {
			g_propagate_error (error, nerror);
			return;
		}
	}

	if (nerror) {
		g_propagate_error (error, nerror);
		return;
	}

	object = g_object_new (TRACKER_TYPE_KMAIL_REGISTRAR, 
			       "connection", connection, NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_tracker_kmail_registrar_object_info);

	dbus_g_connection_register_g_object (connection, 
					     TRACKER_KMAIL_REGISTRAR_PATH, 
					     object);

	/* Registration of the registrar to the manager */
	dbus_g_proxy_call_no_reply (manager_proxy, "Register",
				    G_TYPE_OBJECT, object, 
				    G_TYPE_UINT, (guint) tracker_data_manager_get_db_option_int ("KMailLastModseq"),
				    G_TYPE_INVALID,
				    G_TYPE_INVALID);

	/* If while we had a proxy for the manager the manager shut itself down,
	 * then we'll get rid of our registrar too, in on_manager_destroy */

	g_signal_connect (manager_proxy, "destroy",
			  G_CALLBACK (on_manager_destroy), registrar);

	tracker_push_registrar_set_object (registrar, object);
	tracker_push_registrar_set_manager (registrar, manager_proxy);

	g_object_unref (object); /* sink own */
	g_object_unref (manager_proxy);  /* sink own */
}

static void
tracker_kmail_push_registrar_disable (TrackerPushRegistrar *registrar)
{
	tracker_push_registrar_set_object (registrar, NULL);
	tracker_push_registrar_set_manager (registrar, NULL);
}

static void
tracker_kmail_push_registrar_class_init (TrackerKMailPushRegistrarClass *klass)
{
	TrackerPushRegistrarClass *p_class = TRACKER_PUSH_REGISTRAR_CLASS (klass);

	p_class->enable = tracker_kmail_push_registrar_enable;
	p_class->disable = tracker_kmail_push_registrar_disable;
}

static void
tracker_kmail_push_registrar_init (TrackerKMailPushRegistrar *registrar)
{
	return;
}

TrackerPushRegistrar *
tracker_push_module_init (void)
{
	GObject *object;

	object = g_object_new (TRACKER_TYPE_KMAIL_PUSH_REGISTRAR, NULL);

	tracker_push_registrar_set_service (TRACKER_PUSH_REGISTRAR (object),
					    TRACKER_KMAIL_MANAGER_SERVICE);

	return TRACKER_PUSH_REGISTRAR (object);
}

void
tracker_push_module_shutdown (TrackerPushRegistrar *registrar)
{
	tracker_kmail_push_registrar_disable (registrar);
}
