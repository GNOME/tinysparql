/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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
 */

#include "config.h"

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <libtracker-miner/tracker-miner-dbus.h>

#include "tracker-writeback-listener.h"


typedef struct {
	TrackerMinerFiles *files_miner;
	GDBusConnection *connection;
} TrackerWritebackListenerPrivate;

enum {
	PROP_0,
	PROP_FILES_MINER
};

#define TRACKER_WRITEBACK_LISTENER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_WRITEBACK_LISTENER, TrackerWritebackListenerPrivate))

static void     writeback_set_property    (GObject              *object,
                                           guint                 param_id,
                                           const GValue         *value,
                                           GParamSpec           *pspec);
static void     writeback_get_property    (GObject              *object,
                                           guint                 param_id,
                                           GValue               *value,
                                           GParamSpec           *pspec);
static void     writeback_finalize        (GObject              *object);
static gboolean writeback_initable_init   (GInitable            *initable,
                                           GCancellable         *cancellable,
                                           GError              **error);

static void
writeback_initable_iface_init (GInitableIface *iface)
{
	iface->init = writeback_initable_init;
}

G_DEFINE_TYPE_WITH_CODE (TrackerWritebackListener, tracker_writeback_listener, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                writeback_initable_iface_init));

static void
tracker_writeback_listener_class_init (TrackerWritebackListenerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = writeback_finalize;
	object_class->set_property = writeback_set_property;
	object_class->get_property = writeback_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_FILES_MINER,
	                                 g_param_spec_object ("files_miner",
	                                                      "files_miner",
	                                                      "The FS Miner",
	                                                      TRACKER_TYPE_MINER_FILES,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (klass, sizeof (TrackerWritebackListenerPrivate));
}

static void
writeback_set_property (GObject      *object,
                    guint         param_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
	TrackerWritebackListenerPrivate *priv;

	priv = TRACKER_WRITEBACK_LISTENER_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_FILES_MINER:
		priv->files_miner = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}


static void
writeback_get_property (GObject    *object,
                    guint       param_id,
                    GValue     *value,
                    GParamSpec *pspec)
{
	TrackerWritebackListenerPrivate *priv;

	priv = TRACKER_WRITEBACK_LISTENER_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_FILES_MINER:
		g_value_set_object (value, priv->files_miner);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
writeback_finalize (GObject *object)
{
	TrackerWritebackListenerPrivate *priv = TRACKER_WRITEBACK_LISTENER_GET_PRIVATE (object);

	if (priv->connection) {
		g_object_unref (priv->connection);
	}

	g_object_unref (priv->files_miner);
}


static void
tracker_writeback_listener_init (TrackerWritebackListener *object)
{
}

static gboolean
writeback_initable_init (GInitable    *initable,
                         GCancellable *cancellable,
                         GError       **error)
{
	TrackerWritebackListenerPrivate *priv;
	GError *internal_error = NULL;

	priv = TRACKER_WRITEBACK_LISTENER_GET_PRIVATE (initable);

	priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &internal_error);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	return TRUE;
}

TrackerWritebackListener *
tracker_writeback_listener_new (TrackerMinerFiles  *miner_files,
                                GError            **error)
{
	GObject *miner;
	GError *internal_error = NULL;

	miner =  g_initable_new (TRACKER_TYPE_WRITEBACK_LISTENER,
	                         NULL,
	                         &internal_error,
	                         "files-miner", miner_files,
	                         NULL);

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return NULL;
	}

	return (TrackerWritebackListener *) miner;
}

	// tracker_miner_fs_writeback_file (TRACKER_MINER_FS (miner_files), file, ...);

