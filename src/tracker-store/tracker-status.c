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

#include <string.h>

#include <libtracker-common/tracker-dbus.h>

#include "tracker-dbus.h"
#include "tracker-status.h"
#include "tracker-marshal.h"

#define TRACKER_STATUS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_STATUS, TrackerStatusPrivate))

#define PROGRESS_TIMEOUT_S 5

typedef struct {
	gdouble progress;
	gchar *status;
	guint timer_id;
	TrackerStatus *object;
} TrackerStatusPrivate;

enum {
	PROGRESS,
	LAST_SIGNAL
};

static void tracker_status_finalize (GObject *object);

G_DEFINE_TYPE(TrackerStatus, tracker_status, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = {0};


TrackerStatus *
tracker_status_new (void)
{
	return g_object_new (TRACKER_TYPE_STATUS, NULL);
}

static void
tracker_status_class_init (TrackerStatusClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_status_finalize;

	/**
	 * TrackerStatus::progress:
	 * @notifier: the TrackerStatus
	 * @status: store status
	 * @progress: a #gdouble indicating store progress, from 0 to 1.
	 *
	 * the ::progress signal will be emitted by TrackerStatus
	 * to indicate progress about the store process. @status will
	 * contain a translated string with the current status and @progress
	 * will indicate how much has been processed so far.
	 **/
	signals[PROGRESS] =
		g_signal_new ("progress",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerStatusClass, progress),
		              NULL, NULL,
		              tracker_marshal_VOID__STRING_DOUBLE,
		              G_TYPE_NONE, 2,
		              G_TYPE_STRING,
		              G_TYPE_DOUBLE);

	g_type_class_add_private (object_class, sizeof (TrackerStatusPrivate));
}


static void
tracker_status_finalize (GObject *object)
{
	TrackerStatusPrivate *priv = TRACKER_STATUS_GET_PRIVATE (object);
	if (priv->timer_id != 0)
		g_source_remove (priv->timer_id);
	g_free (priv->status);
}


static void
tracker_status_init (TrackerStatus *object)
{
	TrackerStatusPrivate *priv = TRACKER_STATUS_GET_PRIVATE (object);

	priv->object = object;
	priv->timer_id = 0;
}

static gboolean
busy_notification_timeout (gpointer user_data)
{
	TrackerStatusPrivate *priv = user_data;

	g_signal_emit (priv->object, signals[PROGRESS], 0,
	               priv->status,
	               priv->progress);

	return FALSE;
}

static void
busy_notification_destroy (gpointer user_data)
{
	TrackerStatusPrivate *priv = user_data;

	priv->timer_id = 0;
}


static void
tracker_status_callback (const gchar *status,
                         gdouble progress,
                         gpointer user_data)
{
	static gboolean first_time = TRUE;
	TrackerStatusPrivate *priv = user_data;
	priv->progress = progress;

	if (g_strcmp0 (status, priv->status) != 0) {
		g_free (priv->status);
		priv->status = g_strdup (status);
	}

	if (priv->timer_id == 0) {
		if (first_time) {
			priv->timer_id = g_idle_add_full (G_PRIORITY_DEFAULT,
			                                  busy_notification_timeout,
			                                  priv,
			                                  busy_notification_destroy);
			first_time = FALSE;
		} else {
			priv->timer_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT, PROGRESS_TIMEOUT_S,
			                                             busy_notification_timeout,
			                                             priv,
			                                             busy_notification_destroy);
		}
	}

	while (g_main_context_iteration (NULL, FALSE))
		;
}

TrackerBusyCallback
tracker_status_get_callback (TrackerStatus *object, gpointer *user_data)
{
	TrackerStatusPrivate *priv = TRACKER_STATUS_GET_PRIVATE (object);
	*user_data = priv;
	return tracker_status_callback;
}

void 
tracker_status_get_progress  (TrackerStatus    *object,
                              DBusGMethodInvocation  *context,
                              GError                **error)
{
	TrackerStatusPrivate *priv = TRACKER_STATUS_GET_PRIVATE (object);
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();
	dbus_g_method_return (context, priv->progress);
	tracker_dbus_request_success (request_id, context);

	return;
}


void 
tracker_status_get_status  (TrackerStatus    *object,
                            DBusGMethodInvocation  *context,
                            GError                **error)
{
	TrackerStatusPrivate *priv = TRACKER_STATUS_GET_PRIVATE (object);
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();
	dbus_g_method_return (context, priv->status);
	tracker_dbus_request_success (request_id, context);

	return;
}
