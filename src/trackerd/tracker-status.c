/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Mr Jamie McCracken (jamiemcc@gnome.org)
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
 */

#include "config.h"

#include "tracker-status.h"
#include "tracker-dbus.h"
#include "tracker-daemon.h"
#include "tracker-main.h"

typedef struct {
	TrackerStatus  status;
	gpointer       type_class;

	TrackerConfig *config;

	gboolean       is_running;
	gboolean       is_readonly;
	gboolean       is_first_time_index;
	gboolean       is_paused_manually;
	gboolean       is_paused_for_io;
	gboolean       in_merge;
} TrackerStatusPrivate;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void
private_free (gpointer data)
{
	TrackerStatusPrivate *private;

	private = data;

	if (private->config) {
		g_object_unref (private->config);
	}

	if (private->type_class) {
		g_type_class_unref (private->type_class);
	}

	g_free (private);
}

gboolean
tracker_status_init (TrackerConfig *config)
{
	GType		      type;
	TrackerStatusPrivate *private;

	g_return_val_if_fail (TRACKER_IS_CONFIG (config), FALSE);

	private = g_static_private_get (&private_key);
	if (private) {
		g_warning ("Already initialized (%s)",
			   __FUNCTION__);
		return FALSE;
	}

	private = g_new0 (TrackerStatusPrivate, 1);

	private->status = TRACKER_STATUS_INITIALIZING;

	/* Since we don't reference this enum anywhere, we do
	 * it here to make sure it exists when we call
	 * g_type_class_peek(). This wouldn't be necessary if
	 * it was a param in a GObject for example.
	 *
	 * This does mean that we are leaking by 1 reference
	 * here and should clean it up, but it doesn't grow so
	 * this is acceptable.
	 */
	type = tracker_status_get_type ();
	private->type_class = g_type_class_ref (type);

	private->config = g_object_ref (config);

	private->is_running = FALSE;
	private->is_readonly = FALSE;
	private->is_first_time_index = FALSE;
	private->is_paused_manually = FALSE;
	private->is_paused_for_io = FALSE;
	private->in_merge = FALSE;

	g_static_private_set (&private_key,
			      private,
			      private_free);

	return TRUE;
}

void
tracker_status_shutdown (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	if (!private) {
		g_warning ("Not initialized (%s)",
			   __FUNCTION__);
		return;
	}

	g_static_private_free (&private_key);
}

GType
tracker_status_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GEnumValue values[] = {
			{ TRACKER_STATUS_INITIALIZING,
			  "TRACKER_STATUS_INITIALIZING",
			  "Initializing" },
			{ TRACKER_STATUS_WATCHING,
			  "TRACKER_STATUS_WATCHING",
			  "Watching" },
			{ TRACKER_STATUS_INDEXING,
			  "TRACKER_STATUS_INDEXING",
			  "Indexing" },
			{ TRACKER_STATUS_PENDING,
			  "TRACKER_STATUS_PENDING",
			  "Pending" },
			{ TRACKER_STATUS_OPTIMIZING,
			  "TRACKER_STATUS_OPTIMIZING",
			  "Optimizing" },
			{ TRACKER_STATUS_IDLE,
			  "TRACKER_STATUS_IDLE",
			  "Idle" },
			{ TRACKER_STATUS_SHUTDOWN,
			  "TRACKER_STATUS_SHUTDOWN",
			  "Shutdown" },
			{ 0, NULL, NULL }
		};

		type = g_enum_register_static ("TrackerStatus", values);
	}

	return type;
}

const gchar *
tracker_status_to_string (TrackerStatus status)
{
	GType	    type;
	GEnumClass *enum_class;
	GEnumValue *enum_value;

	type = tracker_status_get_type ();
	enum_class = G_ENUM_CLASS (g_type_class_peek (type));
	enum_value = g_enum_get_value (enum_class, status);

	if (!enum_value) {
		enum_value = g_enum_get_value (enum_class, TRACKER_STATUS_IDLE);
	}

	return enum_value->value_nick;
}

TrackerStatus
tracker_status_get (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, TRACKER_STATUS_INITIALIZING);

	return private->status;
}

const gchar *
tracker_status_get_as_string (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, tracker_status_to_string (TRACKER_STATUS_INITIALIZING));

	return tracker_status_to_string (private->status);
}

void
tracker_status_set (TrackerStatus new_status)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	private->status = new_status;
}

void
tracker_status_signal (void)
{
	TrackerStatusPrivate *private;
	GObject		     *object;
	gboolean	      pause_on_battery;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	object = tracker_dbus_get_object (TRACKER_TYPE_DAEMON);

	/* There are times on startup whe we haven't initialized the
	 * DBus objects yet so signalling status is not practical.
	 */
	if (!object) {
		return;
	}

	if (private->is_first_time_index) {
		pause_on_battery =
			tracker_config_get_disable_indexing_on_battery_init (private->config);
	} else {
		pause_on_battery =
			tracker_config_get_disable_indexing_on_battery (private->config);
	}

	g_signal_emit_by_name (object,
			       "index-state-change",
			       tracker_status_to_string (private->status),
			       private->is_first_time_index,
			       private->in_merge,
			       private->is_paused_manually,
			       pause_on_battery,
			       private->is_paused_for_io,
			       !private->is_readonly);
}

void
tracker_status_set_and_signal (TrackerStatus new_status)
{
	TrackerStatusPrivate *private;
	gboolean	      emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	emit = private->status != new_status;

	if (!emit) {
		return;
	}

	g_message ("State change from '%s' --> '%s'",
		   tracker_status_to_string (private->status),
		   tracker_status_to_string (new_status));

	tracker_status_set (new_status);
	tracker_status_signal ();
}

gboolean
tracker_status_get_is_readonly (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	return private->is_readonly;
}

void
tracker_status_set_is_readonly (gboolean value)
{
	TrackerStatusPrivate *private;
	gboolean	      emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	emit = private->is_readonly != value;

	if (!emit) {
		return;
	}

	/* Set value */
	private->is_readonly = value;

	/* Signal the status change */
	tracker_status_signal ();
}

gboolean
tracker_status_get_is_running (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	return private->is_running;
}

void
tracker_status_set_is_running (gboolean value)
{
	TrackerStatusPrivate *private;
	gboolean	      emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	emit = private->is_running != value;

	if (!emit) {
		return;
	}

	/* Set value */
	private->is_running = value;

	/* Signal the status change */
	tracker_status_signal ();
}

gboolean
tracker_status_get_is_first_time_index (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	return private->is_first_time_index;
}

void
tracker_status_set_is_first_time_index (gboolean value)
{
	TrackerStatusPrivate *private;
	gboolean	      emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	emit = private->is_first_time_index != value;

	if (!emit) {
		return;
	}

	/* Set value */
	private->is_first_time_index = value;

	/* Signal the status change */
	tracker_status_signal ();
}

gboolean
tracker_status_get_in_merge (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	return private->in_merge;
}

void
tracker_status_set_in_merge (gboolean value)
{
	TrackerStatusPrivate *private;
	gboolean	      emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	emit = private->in_merge != value;

	if (!emit) {
		return;
	}

	/* Set value */
	private->in_merge = value;

	/* Signal the status change */
	tracker_status_signal ();
}

gboolean
tracker_status_get_is_paused_manually (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	return private->is_paused_manually;
}

void
tracker_status_set_is_paused_manually (gboolean value)
{
	TrackerStatusPrivate *private;
	gboolean	      emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	emit = private->is_paused_manually != value;

	if (!emit) {
		return;
	}

	/* Set value */
	private->is_paused_manually = value;

	/* Signal the status change */
	tracker_status_signal ();
}

gboolean
tracker_status_get_is_paused_for_io (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	return private->is_paused_for_io;
}

void
tracker_status_set_is_paused_for_io (gboolean value)
{
	TrackerStatusPrivate *private;
	gboolean	      emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	emit = private->is_paused_for_io != value;

	if (!emit) {
		return;
	}

	/* Set value */
	private->is_paused_for_io = value;

	/* Signal the status change */
	tracker_status_signal ();
}
