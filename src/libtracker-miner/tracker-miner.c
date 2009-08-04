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

#include <libtracker/tracker.h>

#include "tracker-miner.h"
#include "tracker-miner-dbus.h"

#define TRACKER_MINER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER, TrackerMinerPrivate))

struct TrackerMinerPrivate {
	gchar *name;
	TrackerClient *client;
};

enum {
	PROP_0,
	PROP_NAME
};

enum {
	STARTED,
	STOPPED,
	PAUSED,
	RESUMED,
	TERMINATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static void miner_set_property (GObject      *object,
				guint         param_id,
				const GValue *value,
				GParamSpec   *pspec);
static void miner_get_property (GObject      *object,
				guint         param_id,
				GValue       *value,
				GParamSpec   *pspec);
static void miner_finalize     (GObject      *object);
static void miner_constructed  (GObject      *object);

G_DEFINE_ABSTRACT_TYPE (TrackerMiner, tracker_miner, G_TYPE_OBJECT)

static void
tracker_miner_class_init (TrackerMinerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = miner_set_property;
	object_class->get_property = miner_get_property;
	object_class->finalize     = miner_finalize;
	object_class->constructed  = miner_constructed;

	signals[STARTED] =
		g_signal_new ("started",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, started),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[STOPPED] =
		g_signal_new ("stopped",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, stopped),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[PAUSED] =
		g_signal_new ("paused",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, paused),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[RESUMED] =
		g_signal_new ("resumed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, resumed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[TERMINATED] =
		g_signal_new ("terminated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, terminated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Miner name",
							      "Miner name",
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (TrackerMinerPrivate));
}

static void
tracker_miner_init (TrackerMiner *miner)
{
	TrackerMinerPrivate *priv;

	miner->private = priv = TRACKER_MINER_GET_PRIVATE (miner);

	priv->client = tracker_connect (TRUE, -1);
}

static void
miner_set_property (GObject      *object,
		    guint         prop_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
	TrackerMiner *miner = TRACKER_MINER (object);

	switch (prop_id) {
	case PROP_NAME:
		g_free (miner->private->name);
		miner->private->name = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
miner_get_property (GObject    *object,
		    guint       prop_id,
		    GValue     *value,
		    GParamSpec *pspec)
{
	TrackerMiner *miner = TRACKER_MINER (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, miner->private->name);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
miner_finalize (GObject *object)
{
	TrackerMiner *miner = TRACKER_MINER (object);

	if (miner->private->client) {
		tracker_disconnect (miner->private->client);
	}

	G_OBJECT_CLASS (tracker_miner_parent_class)->finalize (object);
}

static gboolean
terminate_miner_cb (TrackerMiner *miner)
{
	g_signal_emit (miner, signals[TERMINATED], 0);
	return TRUE;
}

static void
miner_constructed (GObject *object)
{
	TrackerMiner *miner = TRACKER_MINER (object);

	if (!miner->private->name) {
		g_critical ("Miner should have been given a name, bailing out");
		g_assert_not_reached ();
	}

	if (!tracker_dbus_init (miner)) {
		g_critical ("Could not register object to DBus");
		g_idle_add ((GSourceFunc) terminate_miner_cb, miner);
	}
}

G_CONST_RETURN gchar *
tracker_miner_get_name (TrackerMiner *miner)
{
	g_return_val_if_fail (TRACKER_IS_MINER (miner), NULL);

	return miner->private->name;
}

void
tracker_miner_start (TrackerMiner *miner)
{
	g_return_if_fail (TRACKER_IS_MINER (miner));

	g_signal_emit (miner, signals[STARTED], 0);
}

/* DBus methods */
void
tracker_miner_pause (TrackerMiner           *miner,
		     DBusGMethodInvocation  *context,
		     GError                **error)
{
	g_return_if_fail (TRACKER_IS_MINER (miner));
	
}

void
tracker_miner_resume (TrackerMiner           *miner,
		      DBusGMethodInvocation  *context,
		      GError                **error)
{
	g_return_if_fail (TRACKER_IS_MINER (miner));
}
