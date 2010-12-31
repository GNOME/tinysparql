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
	GDBusConnection *d_connection;
	GDBusNodeInfo *introspection_data;
	guint registration_id;
	guint own_id;
	gchar *full_name;
	gchar *full_path;
};

enum {
	PROP_0,
	PROP_ASSOCIATED
};

static void miner_web_set_property (GObject      *object,
                                    guint         param_id,
                                    const GValue *value,
                                    GParamSpec   *pspec);
static void miner_web_get_property (GObject      *object,
                                    guint         param_id,
                                    GValue       *value,
                                    GParamSpec   *pspec);
static void miner_web_constructed  (GObject      *object);
static void miner_web_finalize     (GObject      *object);

G_DEFINE_ABSTRACT_TYPE (TrackerMinerWeb, tracker_miner_web, TRACKER_TYPE_MINER)

static void
tracker_miner_web_class_init (TrackerMinerWebClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = miner_web_finalize;
	object_class->set_property = miner_web_set_property;
	object_class->get_property = miner_web_get_property;
	object_class->constructed  = miner_web_constructed;

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

	if (priv->own_id != 0) {
		g_bus_unown_name (priv->own_id);
	}

	if (priv->registration_id != 0) {
		g_dbus_connection_unregister_object (priv->d_connection,
		                                     priv->registration_id);
	}

	if (priv->introspection_data) {
		g_dbus_node_info_unref (priv->introspection_data);
	}

	if (priv->d_connection) {
		g_object_unref (priv->d_connection);
	}

	g_free (priv->full_name);
	g_free (priv->full_path);

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

static const GDBusInterfaceVTable interface_vtable = {
	handle_method_call,
	handle_get_property,
	handle_set_property
};

static void
miner_web_constructed (GObject *miner)
{
	TrackerMinerWebPrivate *priv;
	gchar *name, *full_path, *full_name;
	GError *error = NULL;

	priv = TRACKER_MINER_WEB_GET_PRIVATE (miner);

	priv->d_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (!priv->d_connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            error ? error->message : "no error given.");
		g_clear_error (&error);
		return;
	}

	priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

	g_object_get (miner, "name", &name, NULL);

	if (!name) {
		g_critical ("Miner '%s' should have been given a name, bailing out",
		            G_OBJECT_TYPE_NAME (miner));
		g_assert_not_reached ();
	}

	full_name = g_strconcat (TRACKER_MINER_DBUS_NAME_PREFIX, name, NULL);

	priv->own_id = g_bus_own_name_on_connection (priv->d_connection,
	                                             full_name,
	                                             G_BUS_NAME_OWNER_FLAGS_NONE,
	                                             NULL, NULL, NULL, NULL);
	priv->full_name = full_name;

	/* Register the service name for the miner */
	full_path = g_strconcat (TRACKER_MINER_DBUS_PATH_PREFIX, name, NULL);

	g_message ("Registering D-Bus object...");
	g_message ("  Path:'%s'", full_path);
	g_message ("  Object Type:'%s'", G_OBJECT_TYPE_NAME (miner));

	priv->registration_id =
		g_dbus_connection_register_object (priv->d_connection,
	                                       full_path,
	                                       priv->introspection_data->interfaces[0],
	                                       &interface_vtable,
	                                       miner,
	                                       NULL,
	                                       &error);

	if (error) {
		g_critical ("Could not register the D-Bus object %s, %s",
		            full_path,
		            error ? error->message : "no error given.");
		g_clear_error (&error);
		return;
	}

	g_free (name);

	priv->full_path = full_path;

	G_OBJECT_CLASS (tracker_miner_web_parent_class)->constructed (miner);
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
