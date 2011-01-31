/*
 * Copyright (C) 2009, Adrien Bustany <abustany@gnome.org>
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
 */

#include "config.h"

#include <libtracker-common/tracker-dbus.h>

#include "tracker-miner-web.h"
#include "tracker-miner-dbus.h"

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.freedesktop.Tracker1.Miner.Web'>"
  "    <method name='Authenticate' />"
  "    <method name='GetAssociationData'>"
  "      <arg name='result' type='a{ss}' direction='out' />"
  "    </method>"
  "    <method name='Associate'>"
  "      <arg name='data' type='a{ss}' direction='in' />"
  "    </method>"
  "    <method name='Dissociate' />"
  "    <property name='Associated' type='b' access='read' />"
  "  </interface>"
  "</node>";

/**
 * SECTION:tracker-miner-web
 * @short_description: Abstract base class for miners using web services
 * @include: libtracker-miner/tracker-miner.h
 *
 * #TrackerMinerWeb is an abstract base class for miners retrieving data
 * from web services. It's a very thin layer above #TrackerMiner, only
 * adding virtual methods needed to handle association with the remote
 * service.
 **/

#define TRACKER_MINER_WEB_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_WEB, TrackerMinerWebPrivate))

struct TrackerMinerWebPrivate {
	gboolean associated;
	GDBusNodeInfo *introspection_data;
	guint registration_id;
};

enum {
	PROP_0,
	PROP_ASSOCIATED
};

static void       miner_web_set_property        (GObject                *object,
                                                 guint                   param_id,
                                                 const GValue           *value,
                                                 GParamSpec             *pspec);
static void       miner_web_get_property        (GObject                *object,
                                                 guint                   param_id,
                                                 GValue                 *value,
                                                 GParamSpec             *pspec);
static void       miner_web_initable_iface_init (GInitableIface         *iface);
static gboolean   miner_web_initable_init       (GInitable              *initable,
                                                 GCancellable           *cancellable,
                                                 GError                **error);
static void       miner_web_finalize            (GObject                *object);
static void       handle_method_call            (GDBusConnection        *connection,
                                                 const gchar            *sender,
                                                 const gchar            *object_path,
                                                 const gchar            *interface_name,
                                                 const gchar            *method_name,
                                                 GVariant               *parameters,
                                                 GDBusMethodInvocation  *invocation,
                                                 gpointer                user_data);
static GVariant  *handle_get_property           (GDBusConnection        *connection,
                                                 const gchar            *sender,
                                                 const gchar            *object_path,
                                                 const gchar            *interface_name,
                                                 const gchar            *property_name,
                                                 GError                **error,
                                                 gpointer                user_data);
static gboolean   handle_set_property           (GDBusConnection        *connection,
                                                 const gchar            *sender,
                                                 const gchar            *object_path,
                                                 const gchar            *interface_name,
                                                 const gchar            *property_name,
                                                 GVariant               *value,
                                                 GError                **error,
                                                 gpointer                user_data);

static GInitableIface* miner_web_initable_parent_iface;

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (TrackerMinerWeb, tracker_miner_web, TRACKER_TYPE_MINER,
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         miner_web_initable_iface_init));

static void
tracker_miner_web_class_init (TrackerMinerWebClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = miner_web_finalize;
	object_class->set_property = miner_web_set_property;
	object_class->get_property = miner_web_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_ASSOCIATED,
	                                 g_param_spec_boolean ("associated",
	                                                       "Associated",
	                                                       "Tells if the miner is associated with the remote service",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerMinerWebPrivate));
}

static void
tracker_miner_web_init (TrackerMinerWeb *miner)
{
	miner->private = TRACKER_MINER_WEB_GET_PRIVATE (miner);
}

static void
miner_web_initable_iface_init (GInitableIface *iface)
{
	miner_web_initable_parent_iface = g_type_interface_peek_parent (iface);
	iface->init = miner_web_initable_init;
}

static gboolean
miner_web_initable_init (GInitable     *initable,
                         GCancellable  *cancellable,
                         GError       **error)
{
	TrackerMiner *miner;
	TrackerMinerWeb *mw;
	GError *inner_error = NULL;
	GDBusInterfaceVTable interface_vtable = {
		handle_method_call,
		handle_get_property,
		handle_set_property
	};

	miner = TRACKER_MINER (initable);
	mw = TRACKER_MINER_WEB (initable);

	/* Chain up parent's initable callback before calling child's one */
	if (!miner_web_initable_parent_iface->init (initable, cancellable, error)) {
		return FALSE;
	}

	/* Setup web-interface introspection data */
	mw->private->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

	g_message ("Registering Web interface in D-Bus object...");
	g_message ("  Path:'%s'", tracker_miner_get_dbus_full_path (miner));
	g_message ("  Object Type:'%s'", G_OBJECT_TYPE_NAME (initable));

	mw->private->registration_id =
		g_dbus_connection_register_object (tracker_miner_get_dbus_connection (miner),
		                                   tracker_miner_get_dbus_full_path (miner),
		                                   mw->private->introspection_data->interfaces[0],
		                                   &interface_vtable,
		                                   mw,
		                                   NULL,
		                                   &inner_error);
	if (inner_error) {
		g_propagate_error (error, inner_error);
		g_prefix_error (error,
		                "Could not register the D-Bus object %s. ",
		                tracker_miner_get_dbus_full_path (miner));
		return FALSE;
	}

	/* No need to RequestName again as already done by the parent TrackerMiner object */

	return TRUE;
}

static void
miner_web_set_property (GObject      *object,
                        guint         param_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
	TrackerMinerWebPrivate *priv;

	priv = TRACKER_MINER_WEB_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_ASSOCIATED:
		priv->associated = g_value_get_boolean (value);
		g_object_notify (object, "associated");
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
miner_web_get_property (GObject    *object,
                        guint       param_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
	TrackerMinerWebPrivate *priv;

	priv = TRACKER_MINER_WEB_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_ASSOCIATED:
		g_value_set_boolean (value, priv->associated);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
miner_web_finalize (GObject *object)
{
	TrackerMinerWebPrivate *priv;

	priv = TRACKER_MINER_WEB_GET_PRIVATE (object);

	if (priv->registration_id != 0) {
		g_dbus_connection_unregister_object (tracker_miner_get_dbus_connection (TRACKER_MINER (object)),
		                                     priv->registration_id);
	}

	if (priv->introspection_data) {
		g_dbus_node_info_unref (priv->introspection_data);
	}

	G_OBJECT_CLASS (tracker_miner_web_parent_class)->finalize (object);
}

static void
handle_method_call_authenticate (TrackerMinerWeb       *miner,
                                 GDBusMethodInvocation *invocation,
                                 GVariant              *parameters)
{
	GError *local_error = NULL;
	TrackerDBusRequest *request;

	request = tracker_g_dbus_request_begin (invocation, "%s()", __PRETTY_FUNCTION__);

	TRACKER_MINER_WEB_GET_CLASS (miner)->authenticate (miner, &local_error);

	if (local_error != NULL) {
		tracker_dbus_request_end (request, local_error);

		g_dbus_method_invocation_return_gerror (invocation, local_error);

		g_error_free (local_error);
	} else {
		tracker_dbus_request_end (request, NULL);
		g_dbus_method_invocation_return_value (invocation, NULL);
	}
}

static GVariant*
variant_from_hashtable (GHashTable *table)
{
	GVariantBuilder builder;
	GHashTableIter iter;
	gpointer key;
	gpointer value;

	g_hash_table_iter_init (&iter, table);
	g_variant_builder_init (&builder, G_VARIANT_TYPE_DICTIONARY);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		g_variant_builder_add (&builder, "{?*}",
		                       g_variant_new_string ((const gchar*) key),
		                       g_variant_new_string ((const gchar *) value));
	}

	return g_variant_ref_sink (g_variant_builder_end (&builder));
}

static GHashTable*
hashtable_from_variant (GVariant *variant)
{
	GHashTable* table;
	GVariantIter iter;
	GVariant* variant1;
	GVariant* variant2;

	table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	g_variant_iter_init (&iter, variant);

	while (g_variant_iter_loop (&iter, "{?*}", &variant1, &variant2)) {
		g_hash_table_insert (table,
		                     g_variant_dup_string (variant1, NULL),
		                     g_variant_dup_string (variant2, NULL));
	}

	return table;
}

static void
handle_method_call_get_association_data (TrackerMinerWeb       *miner,
                                         GDBusMethodInvocation *invocation,
                                         GVariant              *parameters)
{
	GHashTable *association_data;
	GError *local_error = NULL;
	TrackerDBusRequest *request;

	g_return_if_fail (TRACKER_IS_MINER_WEB (miner));

	request = tracker_g_dbus_request_begin (invocation, "%s()", __PRETTY_FUNCTION__);

	association_data = TRACKER_MINER_WEB_GET_CLASS (miner)->get_association_data (miner, &local_error);

	if (local_error != NULL) {
		tracker_dbus_request_end (request, local_error);

		g_dbus_method_invocation_return_gerror (invocation, local_error);

		g_error_free (local_error);
	} else {
		tracker_dbus_request_end (request, NULL);
		g_dbus_method_invocation_return_value (invocation, variant_from_hashtable (association_data));

		/* This was commented out before GDBus port too
		 * g_hash_table_unref (association_data); */
	}
}

static void
handle_method_call_associate (TrackerMinerWeb       *miner,
                              GDBusMethodInvocation *invocation,
                              GVariant              *parameters)
{
	GHashTable *association_data;
	GError *local_error = NULL;
	TrackerDBusRequest *request;

	association_data = hashtable_from_variant (parameters);

	request = tracker_g_dbus_request_begin (invocation, "%s()", __PRETTY_FUNCTION__);

	TRACKER_MINER_WEB_GET_CLASS (miner)->associate (miner, association_data, &local_error);

	g_hash_table_unref (association_data);

	if (local_error != NULL) {
		tracker_dbus_request_end (request, local_error);

		g_dbus_method_invocation_return_gerror (invocation, local_error);

		g_error_free (local_error);
	} else {
		tracker_dbus_request_end (request, NULL);
		g_dbus_method_invocation_return_value (invocation, NULL);
	}
}

static void
handle_method_call_dissociate (TrackerMinerWeb       *miner,
                               GDBusMethodInvocation *invocation,
                               GVariant              *parameters)
{
	GError *local_error = NULL;
	TrackerDBusRequest *request;

	request = tracker_g_dbus_request_begin (invocation, "%s()", __PRETTY_FUNCTION__);

	TRACKER_MINER_WEB_GET_CLASS (miner)->dissociate (miner, &local_error);

	if (local_error != NULL) {
		tracker_dbus_request_end (request, local_error);

		g_dbus_method_invocation_return_gerror (invocation, local_error);

		g_error_free (local_error);
	} else {
		tracker_dbus_request_end (request, NULL);
		g_dbus_method_invocation_return_value (invocation, NULL);
	}
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
	TrackerMinerWeb *miner = user_data;

	tracker_gdbus_async_return_if_fail (miner != NULL, invocation);
	tracker_gdbus_async_return_if_fail (TRACKER_IS_MINER_WEB (miner), invocation);

	if (g_strcmp0 (method_name, "Authenticate") == 0) {
		handle_method_call_authenticate (miner, invocation, parameters);
	} else
	if (g_strcmp0 (method_name, "GetAssociationData") == 0) {
		handle_method_call_get_association_data (miner, invocation, parameters);
	} else
	if (g_strcmp0 (method_name, "Associate") == 0) {
		handle_method_call_associate (miner, invocation, parameters);
	} else
	if (g_strcmp0 (method_name, "Dissociate") == 0) {
		handle_method_call_dissociate (miner, invocation, parameters);
	} else {
		g_assert_not_reached ();
	}
}

static GVariant *
handle_get_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data)
{
	GVariant *ret = NULL;
	TrackerMinerWeb *miner = user_data;
	TrackerMinerWebPrivate *priv;

	priv = TRACKER_MINER_WEB_GET_PRIVATE (miner);

	if (g_strcmp0 (property_name, "Associated") == 0) {
		ret = g_variant_new ("(b)", priv->associated);
	} else {
		g_assert_not_reached ();
	}

	return ret;
}

static gboolean
handle_set_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data)
{
	g_assert_not_reached ();
	return TRUE;
}

/**
 * tracker_miner_web_error_quark:
 *
 * Returns: the #GQuark used to identify miner web errors in GError
 * structures.
 **/
GQuark
tracker_miner_web_error_quark (void)
{
	return g_quark_from_static_string (TRACKER_MINER_WEB_ERROR_DOMAIN);
}

/**
 * tracker_miner_web_authenticate:
 * @miner: a #TrackerMinerWeb
 * @error: return location for errors
 *
 * Asks @miner to authenticate with a remote service. On failure
 * @error will be set.
 **/
void
tracker_miner_web_authenticate (TrackerMinerWeb  *miner,
                                GError          **error)
{
	g_return_if_fail (TRACKER_IS_MINER_WEB (miner));

	TRACKER_MINER_WEB_GET_CLASS (miner)->authenticate (miner, error);
}

/**
 * tracker_miner_web_get_association_data:
 * @miner: a #TrackerMinerWeb
 * @error: return location for errors
 *
 * Asks @miner to retrieve association_data for. The data returned in
 * the %GHashTable depends on the @miner implementation and the type
 * of authentication. See <classname>TrackerMinerWebClass</classname>
 * for more information.
 *
 * Returns: a %GHashTable with the data. On failure @error will be set
 * and %NULL will be returned.
 **/
GHashTable *
tracker_miner_web_get_association_data (TrackerMinerWeb  *miner,
                                        GError          **error)
{
	g_return_val_if_fail (TRACKER_IS_MINER_WEB (miner), NULL);

	return TRACKER_MINER_WEB_GET_CLASS (miner)->get_association_data (miner, error);
}

/**
 * tracker_miner_web_associate:
 * @miner: a #TrackerMinerWeb
 * @association_data: a %GHashTable with the data to use for
 * associating with a remote service.
 * @error: return location for errors
 *
 * Asks @miner to associate with a remote service using
 * @association_data. To know what data to put into @association_data,
 * see <classname>TrackerMinerWebClass</classname> for more
 * information.
 *
 * On failure @error will be set.
 **/
void
tracker_miner_web_associate (TrackerMinerWeb  *miner,
                             GHashTable       *association_data,
                             GError          **error)
{
	g_return_if_fail (TRACKER_IS_MINER_WEB (miner));
	g_return_if_fail (association_data != NULL);

	TRACKER_MINER_WEB_GET_CLASS (miner)->associate (miner, association_data, error);
}

/**
 * tracker_miner_web_dissociate:
 * @miner: a #TrackerMinerWeb
 * @error: return location for errors
 *
 * Asks @miner to dissociate from a remote service. At this point, the
 * miner should stop storing any credentials or sensitive information
 * which could be used to authenticate with the remote service.
 *
 * On failure @error will be set.
 **/
void
tracker_miner_web_dissociate (TrackerMinerWeb   *miner,
                              GError           **error)
{
	g_return_if_fail (TRACKER_IS_MINER_WEB (miner));

	TRACKER_MINER_WEB_GET_CLASS (miner)->dissociate (miner, error);
}
