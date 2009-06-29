/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia

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
 */

#include "config.h"

#include <libtracker-common/tracker-dbus.h>

#include "tracker-miner.h"

#define TRACKER_MINER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER, TrackerMinerPrivate))

struct TrackerMinerPrivate {
	TrackerIndexer *indexer;
};

enum {
	PROP_0,
	PROP_RUNNING,
};

enum {
	PAUSED,
	CONTINUED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (TrackerMiner, tracker_miner, G_TYPE_OBJECT)

static void
tracker_miner_init (TrackerMiner *miner)
{
}

static void
tracker_miner_finalize (GObject *object)
{
	TrackerMinerPrivate *priv;

	priv = TRACKER_MINER_GET_PRIVATE (object);

	if (priv->indexer) {
		g_object_unref (priv->indexer);
	}

	G_OBJECT_CLASS (tracker_miner_parent_class)->finalize (object);
}

static void
tracker_miner_get_property (GObject	 *object,
			      guint	  prop_id,
			      GValue	 *value,
			      GParamSpec *pspec)
{
	switch (prop_id) {
	case PROP_RUNNING:
 		g_value_set_boolean (value, TRUE);
				     /* tracker_miner_get_running (TRACKER_MINER (object))); */
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_miner_class_init (TrackerMinerClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = tracker_miner_finalize;
	object_class->get_property = tracker_miner_get_property;

	signals[PAUSED] =
		g_signal_new ("paused",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, paused),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      0);
	signals[CONTINUED] =
		g_signal_new ("continued",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, continued),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_object_class_install_property (object_class,
					 PROP_RUNNING,
					 g_param_spec_boolean ("running",
							       "Running",
							       "Whether the miner is running",
							       TRUE,
							       G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof (TrackerMinerPrivate));
}

TrackerMiner *
tracker_miner_new (TrackerIndexer *indexer)
{
	TrackerMiner *miner;
	TrackerMinerPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_INDEXER (indexer), NULL);

	miner = g_object_new (TRACKER_TYPE_MINER, NULL);

	priv = TRACKER_MINER_GET_PRIVATE (miner);

	priv->indexer = g_object_ref (indexer);

	return miner;
}

void
tracker_miner_pause (TrackerMiner	    *miner,
		     DBusGMethodInvocation  *context,
		     GError		   **error)
{
	TrackerMinerPrivate *priv;
	guint request_id;

	priv = TRACKER_MINER_GET_PRIVATE (miner);

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (TRACKER_IS_MINER (miner), context);

	tracker_dbus_request_new (request_id, "%s", __FUNCTION__);

	tracker_indexer_pause (priv->indexer);

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}

void
tracker_miner_continue (TrackerMiner	       *miner,
			DBusGMethodInvocation  *context,
			GError		      **error)
{
	TrackerMinerPrivate *priv;
	guint request_id;

	priv = TRACKER_MINER_GET_PRIVATE (miner);

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (TRACKER_IS_MINER (miner), context);

	tracker_dbus_request_new (request_id, "%s", __FUNCTION__);

	tracker_indexer_continue (priv->indexer);

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}
