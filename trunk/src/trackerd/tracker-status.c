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

#include <sys/statvfs.h>

#include "tracker-status.h"
#include "tracker-dbus.h"
#include "tracker-daemon.h"
#include "tracker-main.h"
#include "tracker-indexer-client.h"

#define DISK_SPACE_CHECK_FREQUENCY 10

#define THROTTLE_DEFAULT	    0
#define THROTTLE_DEFAULT_ON_BATTERY 5

typedef struct {
	TrackerStatus  status;
	TrackerStatus  status_before_paused;
	gpointer       type_class;

	guint          disk_space_check_id;

	TrackerConfig *config;
	TrackerHal    *hal;

	DBusGProxy    *indexer_proxy;

	gboolean       is_readonly;
	gboolean       is_ready;
	gboolean       is_running;
	gboolean       is_first_time_index;
	gboolean       is_paused_manually;
	gboolean       is_paused_for_batt;
	gboolean       is_paused_for_io;
	gboolean       is_paused_for_space;
	gboolean       is_paused_for_dbus;
	gboolean       is_paused_for_unknown;
	gboolean       in_merge;
} TrackerStatusPrivate;

static void indexer_continued_cb  (DBusGProxy  *proxy,
				   gpointer     user_data);
static void indexer_paused_cb     (DBusGProxy  *proxy,
				   const gchar *reason,
				   gpointer     user_data);
static void indexer_continue      (gboolean     should_block);
static void indexer_pause         (gboolean     should_block);
static void battery_in_use_cb     (GObject     *gobject,
				   GParamSpec  *arg1,
				   gpointer     user_data);
static void battery_percentage_cb (GObject     *object,
				   GParamSpec  *pspec,
				   gpointer     user_data);

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void
private_free (gpointer data)
{
	TrackerStatusPrivate *private;

	private = data;

	if (private->disk_space_check_id) {
		g_source_remove (private->disk_space_check_id);
	}

	g_object_unref (private->config);

#ifdef HAVE_HAL
	g_signal_handlers_disconnect_by_func (private->hal,
					      battery_in_use_cb,
					      NULL);
	g_signal_handlers_disconnect_by_func (private->hal,
					      battery_percentage_cb,
					      NULL);

	g_object_unref (private->hal);
#endif

	if (private->type_class) {
		g_type_class_unref (private->type_class);
	}

	dbus_g_proxy_disconnect_signal (private->indexer_proxy, "Continued",
					G_CALLBACK (indexer_continued_cb),
					NULL);
	dbus_g_proxy_disconnect_signal (private->indexer_proxy, "Paused",
					G_CALLBACK (indexer_paused_cb),
					NULL);
	dbus_g_proxy_disconnect_signal (private->indexer_proxy, "Finished",
					G_CALLBACK (indexer_continued_cb),
					NULL);

	g_object_unref (private->indexer_proxy);

	g_free (private);
}

/*
 * Handle Indexer pausing/continuation
 */

static void
indexer_recheck (gboolean should_inform_indexer,
		 gboolean should_block,
		 gboolean should_signal_small_changes)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* Are we paused in any way? */
	if (private->is_paused_manually ||
	    private->is_paused_for_batt || 
	    private->is_paused_for_io ||
	    private->is_paused_for_space ||
	    private->is_paused_for_dbus ||
	    private->is_paused_for_unknown) {
		/* We are paused, but our status is NOT paused? */
		if (private->status != TRACKER_STATUS_PAUSED) {
			if (G_LIKELY (should_inform_indexer)) {
				/* We set state after confirmation*/
				indexer_pause (should_block);
			} else {
				tracker_status_set_and_signal (TRACKER_STATUS_PAUSED);
			}
			
			return;
		}
	} else {
		/* We are not paused, but our status is paused */
		if (private->status == TRACKER_STATUS_PAUSED) {
			if (G_LIKELY (should_inform_indexer)) {
				/* We set state after confirmation*/
				indexer_continue (should_block);
			} else {
				tracker_status_set_and_signal (private->status_before_paused);
			}

			return;
		}
	}

	/* Simply signal because in this case, state hasn't changed
	 * but one of the reasons for being paused has changed. 
	 */
	if (should_signal_small_changes) {
		tracker_status_signal ();
	}
}

static void
indexer_paused_cb (DBusGProxy  *proxy,
		   const gchar *reason,
		   gpointer     user_data)
{
	g_message ("The indexer has paused");
}

static void
indexer_continued_cb (DBusGProxy *proxy,
		      gpointer	  user_data)
{
	g_message ("The indexer has continued");
}

static void
indexer_continue_cb (DBusGProxy *proxy,
		     GError     *error,
		     gpointer    user_data)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (G_UNLIKELY (error)) {
		g_message ("Could not continue the indexer, %s",
			   error->message);
		return;
	}

	tracker_status_set_and_signal (private->status_before_paused);
}

static void
indexer_continue (gboolean should_block)
{
	TrackerStatusPrivate *private;

	/* NOTE: We don't need private, but we do this to check we
	 * are initialised before calling continue.
	 */
	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (G_LIKELY (!should_block)) {
		org_freedesktop_Tracker_Indexer_continue_async (private->indexer_proxy,
								indexer_continue_cb,
								NULL);
	} else {
		org_freedesktop_Tracker_Indexer_continue (private->indexer_proxy, 
							  NULL);
		tracker_status_set_and_signal (private->status_before_paused);
	}
}

static void
indexer_pause_cb (DBusGProxy *proxy,
		  GError     *error,
		  gpointer    user_data)
{
	if (G_UNLIKELY (error)) {
		g_message ("Could not pause the indexer, %s",
			   error->message);
		return;
	}
	
	tracker_status_set_and_signal (TRACKER_STATUS_PAUSED);
}

static void
indexer_pause (gboolean should_block)
{
	TrackerStatusPrivate *private;

	/* NOTE: We don't need private, but we do this to check we
	 * are initialised before calling continue.
	 */
	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (G_LIKELY (!should_block)) {
		org_freedesktop_Tracker_Indexer_pause_async (private->indexer_proxy,
							     indexer_pause_cb,
							     NULL);
	} else {
		org_freedesktop_Tracker_Indexer_pause (private->indexer_proxy, 
						       NULL);
		tracker_status_set_and_signal (TRACKER_STATUS_PAUSED);
	}
}


static gboolean
disk_space_check (void)
{
	TrackerStatusPrivate *private;
	struct statvfs        st;
	gint                  limit;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	limit = tracker_config_get_low_disk_space_limit (private->config);

	if (limit < 1) {
		return FALSE;
	}

	if (statvfs (tracker_get_data_dir (), &st) == -1) {
		g_warning ("Could not statvfs() '%s'", tracker_get_data_dir ());
		return FALSE;
	}

	if (((long long) st.f_bavail * 100 / st.f_blocks) <= limit) {
		g_message ("Disk space is low");
		return TRUE;
	}

	return FALSE;
}

static gboolean
disk_space_check_cb (gpointer user_data)
{
	if (disk_space_check ()) {
		tracker_status_set_is_paused_for_space (TRUE);
	} else {
		tracker_status_set_is_paused_for_space (FALSE);
	}

	return TRUE;
}

static void
disk_space_check_start (void)
{
	TrackerStatusPrivate *private;
	gint limit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (private->disk_space_check_id != 0) {
		return;
	}

	limit = tracker_config_get_low_disk_space_limit (private->config);

	if (limit != -1) {
		g_message ("Setting disk space check for every %d seconds",
			   DISK_SPACE_CHECK_FREQUENCY);
		private->disk_space_check_id = 
			g_timeout_add_seconds (DISK_SPACE_CHECK_FREQUENCY,
					       disk_space_check_cb,
					       NULL);

		/* Call the function now too to make sure we have an
		 * initial value too!
		 */
		disk_space_check_cb (NULL);
	} else {
		g_message ("Not setting disk space, configuration is set to -1 (disabled)");
	}
}

#ifdef HAVE_HAL

static void
set_up_throttle (void)
{
	TrackerStatusPrivate *private;
	gint                  throttle;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* If on a laptop battery and the throttling is default (i.e.
	 * 0), then set the throttle to be higher so we don't kill
	 * the laptop battery.
	 */
	throttle = tracker_config_get_throttle (private->config);

	if (tracker_hal_get_battery_in_use (private->hal)) {
		g_message ("We are running on battery");

		if (throttle == THROTTLE_DEFAULT) {
			tracker_config_set_throttle (private->config,
						     THROTTLE_DEFAULT_ON_BATTERY);
			g_message ("Setting throttle from %d to %d",
				   throttle,
				   THROTTLE_DEFAULT_ON_BATTERY);
		} else {
			g_message ("Not setting throttle, it is currently set to %d",
				   throttle);
		}
	} else {
		g_message ("We are not running on battery");

		if (throttle == THROTTLE_DEFAULT_ON_BATTERY) {
			tracker_config_set_throttle (private->config,
						     THROTTLE_DEFAULT);
			g_message ("Setting throttle from %d to %d",
				   throttle,
				   THROTTLE_DEFAULT);
		} else {
			g_message ("Not setting throttle, it is currently set to %d",
				   throttle);
		}
	}
}

static void
battery_in_use_cb (GObject *gobject,
		   GParamSpec *arg1,
		   gpointer user_data)
{
	set_up_throttle ();
}

static void
battery_percentage_cb (GObject    *object,
		       GParamSpec *pspec,
		       gpointer    user_data)
{
	TrackerStatusPrivate *private;
	gdouble               percentage;
	gboolean              battery_in_use;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	percentage = tracker_hal_get_battery_percentage (private->hal);
	battery_in_use = tracker_hal_get_battery_in_use (private->hal);

	/* FIXME: This could be a configuration option */
	if (battery_in_use) {
		g_message ("Battery percentage is now %d%%",
			   (gint) percentage * 100);
		
		if (percentage <= 0.05) {
			/* Running on low batteries, stop indexing for now */
			tracker_status_set_is_paused_for_batt (TRUE);
		} else {
			tracker_status_set_is_paused_for_batt (FALSE);
		}
	} else {
		tracker_status_set_is_paused_for_batt (FALSE);
	}

	set_up_throttle ();
}

#endif /* HAVE_HAL */

gboolean
tracker_status_init (TrackerConfig *config,
		     TrackerHal    *hal)
{
	GType		      type;
	DBusGProxy           *proxy;
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
	private->status_before_paused = private->status;

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

#ifdef HAVE_HAL 
	private->hal = g_object_ref (hal);

	g_message ("Setting battery percentage checking");
	g_signal_connect (private->hal, "notify::battery-percentage",
			  G_CALLBACK (battery_percentage_cb),
			  NULL);
#endif

	private->is_readonly = FALSE;
	private->is_ready = FALSE;
	private->is_running = FALSE;
	private->is_first_time_index = FALSE;
	private->is_paused_manually = FALSE;
	private->is_paused_for_batt = FALSE;
	private->is_paused_for_io = FALSE;
	private->is_paused_for_space = FALSE;
	private->is_paused_for_dbus = FALSE;
	private->is_paused_for_unknown = FALSE;
	private->in_merge = FALSE;

	g_static_private_set (&private_key,
			      private,
			      private_free);

	/* Initialize the DBus indexer listening functions */
	proxy = tracker_dbus_indexer_get_proxy ();
	private->indexer_proxy = g_object_ref (proxy);

	dbus_g_proxy_connect_signal (proxy, "Paused",
				     G_CALLBACK (indexer_paused_cb),
				     NULL,
				     NULL);
	dbus_g_proxy_connect_signal (proxy, "Continued",
				     G_CALLBACK (indexer_continued_cb),
				     NULL,
				     NULL);
	
	/* Start monitoring system for low disk space, low battery
	 * power, etc. 
	 */
	disk_space_check_start ();

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
			{ TRACKER_STATUS_PAUSED,
			  "TRACKER_STATUS_PAUSED",
			  "Paused" },
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

	g_message ("State change from '%s' --> '%s'",
		   tracker_status_to_string (private->status),
		   tracker_status_to_string (new_status));

	/* Don't set previous status to the same as we are now,
	 * otherwise we could end up setting PAUSED and our old state
	 * to return to is also PAUSED.
	 */
	if (private->status_before_paused != new_status) {
		private->status_before_paused = private->status;
	}

	private->status = new_status;
}

void
tracker_status_signal (void)
{
	TrackerStatusPrivate *private;
	GObject		     *object;
	gboolean              pause_for_io;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	object = tracker_dbus_get_object (TRACKER_TYPE_DAEMON);

	/* There are times on startup whe we haven't initialized the
	 * DBus objects yet so signalling status is not practical.
	 */
	if (!object) {
		return;
	}

	/* NOTE: We have to combine pausing for IO, pausing for
	 * space and pausing for unknown reasons here because we can't
	 * change API yet.
	 */
	pause_for_io = 
		private->is_paused_for_io || 
		private->is_paused_for_space || 
		private->is_paused_for_dbus ||
		private->is_paused_for_unknown;
	
	g_signal_emit_by_name (object,
			       "index-state-change",
			       tracker_status_to_string (private->status),
			       private->is_first_time_index,
			       private->in_merge,
			       private->is_paused_manually,
			       private->is_paused_for_batt,
			       pause_for_io,
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
tracker_status_get_is_ready (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	return private->is_ready;
}

void
tracker_status_set_is_ready (gboolean value)
{
	TrackerStatusPrivate *private;
	gboolean	      emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	emit = private->is_ready != value;

	if (!emit) {
		return;
	}

	/* Set value */
	private->is_ready = value;

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
	gboolean              emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* Set value */
	emit = private->is_paused_manually != value;
	private->is_paused_manually = value;

	/* Set indexer state and our state to paused or not */ 
	indexer_recheck (TRUE, FALSE, emit);
}

gboolean
tracker_status_get_is_paused_for_batt (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	return private->is_paused_for_batt;
}

void
tracker_status_set_is_paused_for_batt (gboolean value)
{
	TrackerStatusPrivate *private;
	gboolean              emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* Set value */
	emit = private->is_paused_for_batt != value;
	private->is_paused_for_batt = value;

	/* Set indexer state and our state to paused or not */ 
	indexer_recheck (TRUE, FALSE, emit);
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

	/* Set value */
	emit = private->is_paused_for_io != value;
	private->is_paused_for_io = value;

	/* Set indexer state and our state to paused or not */ 
	indexer_recheck (TRUE, FALSE, emit);
}

gboolean
tracker_status_get_is_paused_for_space (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	return private->is_paused_for_space;
}

void
tracker_status_set_is_paused_for_space (gboolean value)
{
	TrackerStatusPrivate *private;
	gboolean              emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* Set value */
	emit = private->is_paused_for_space != value;
	private->is_paused_for_space = value;

	/* Set indexer state and our state to paused or not */ 
	indexer_recheck (TRUE, FALSE, emit);
}

gboolean
tracker_status_get_is_paused_for_dbus (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	return private->is_paused_for_space;
}

void
tracker_status_set_is_paused_for_dbus (gboolean value)
{
	TrackerStatusPrivate *private;
	gboolean              emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* Set value */
	emit = private->is_paused_for_dbus != value;
	private->is_paused_for_dbus = value;

	/* Set indexer state and our state to paused or not */ 
	indexer_recheck (TRUE, TRUE, emit);
}

