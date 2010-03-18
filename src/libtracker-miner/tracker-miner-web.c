/*
 * Copyright (C) 2009, Adrien Bustany (abustany@gnome.org)
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

#include "tracker-dbus.h"
#include "tracker-miner-web.h"
#include "tracker-miner-web-dbus.h"
#include "tracker-miner-web-glue.h"

/**
 * SECTION:tracker-miner-web
 * @short_description: Abstract base class for miners using web services
 * @include: libtracker-miner/tracker-miner-web.h
 *
 * #TrackerMinerWeb is an abstract base class for miners retrieving data
 * from web services. It's a very thin layer above #TrackerMiner, only
 * adding virtual methods needed to handle association with the remote
 * service.
 **/

#define TRACKER_MINER_WEB_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_WEB, TrackerMinerWebPrivate))

struct TrackerMinerWebPrivate {
	gboolean associated;
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

G_DEFINE_ABSTRACT_TYPE (TrackerMinerWeb, tracker_miner_web, TRACKER_TYPE_MINER)

static void
tracker_miner_web_class_init (TrackerMinerWebClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

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
miner_web_constructed (GObject *object)
{
	tracker_miner_dbus_init (TRACKER_MINER (object),
	                         &dbus_glib_tracker_miner_web_dbus_object_info);

	G_OBJECT_CLASS (tracker_miner_web_parent_class)->constructed (object);
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

/* DBus methods */
void
tracker_miner_web_dbus_authenticate (TrackerMinerWeb        *miner,
                                     DBusGMethodInvocation  *context,
                                     GError                **error)
{
	GError *local_error = NULL;

	g_return_if_fail (TRACKER_IS_MINER_WEB (miner));

	TRACKER_MINER_WEB_GET_CLASS (miner)->authenticate (miner, &local_error);

	if (local_error != NULL) {
		dbus_g_method_return_error (context, local_error);
		g_error_free (local_error);
	} else {
		dbus_g_method_return (context);
	}
}

void
tracker_miner_web_dbus_get_association_data (TrackerMinerWeb        *miner,
                                             DBusGMethodInvocation  *context,
                                             GError                **error)
{
	GHashTable *association_data;
	GError *local_error = NULL;

	g_return_if_fail (TRACKER_IS_MINER_WEB (miner));

	association_data = TRACKER_MINER_WEB_GET_CLASS (miner)->get_association_data (miner, &local_error);

	if (local_error != NULL) {
		dbus_g_method_return_error (context, local_error);
		g_error_free (local_error);
	} else {
		dbus_g_method_return (context, association_data);
		/* g_hash_table_unref (association_data); */
	}
}

void
tracker_miner_web_dbus_associate (TrackerMinerWeb        *miner,
                                  GHashTable             *association_data,
                                  DBusGMethodInvocation  *context,
                                  GError                **error)
{
	GError *local_error = NULL;

	g_return_if_fail (TRACKER_IS_MINER_WEB (miner));
	g_return_if_fail (association_data != NULL);

	TRACKER_MINER_WEB_GET_CLASS (miner)->associate (miner, association_data, &local_error);

	if (local_error != NULL) {
		dbus_g_method_return_error (context, local_error);
		g_error_free (local_error);
	} else {
		dbus_g_method_return (context);
	}
}

void
tracker_miner_web_dbus_dissociate (TrackerMinerWeb        *miner,
                                   DBusGMethodInvocation  *context,
                                   GError                **error)
{
	GError *local_error = NULL;

	g_return_if_fail (TRACKER_IS_MINER_WEB (miner));

	TRACKER_MINER_WEB_GET_CLASS (miner)->dissociate (miner, &local_error);

	if (local_error != NULL) {
		dbus_g_method_return_error (context, local_error);
		g_error_free (local_error);
	} else {
		dbus_g_method_return (context);
	}
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
