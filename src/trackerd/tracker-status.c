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
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <unistd.h>

#include <libtracker-db/tracker-db-manager.h>

#include "tracker-status.h"
#include "tracker-dbus.h"
#include "tracker-daemon.h"
#include "tracker-main.h"
#include "tracker-indexer-client.h"

#define DISK_SPACE_CHECK_FREQUENCY 10

#define THROTTLE_DEFAULT	    0
#define THROTTLE_DEFAULT_ON_BATTERY 5

#define PROCESS_PRIORITY_FOR_BUSY   19

typedef struct {
	TrackerStatus  status;
	TrackerStatus  status_before_paused;
	gpointer       status_type_class;

	TrackerMode    mode;
	gpointer       mode_type_class;

	guint          disk_space_check_id;

	gint           cpu_priority;

	TrackerConfig *config;
	TrackerHal    *hal;


	DBusGProxy    *indexer_proxy;

	gboolean       is_readonly;
	gboolean       is_ready;
	gboolean       is_running;
	gboolean       is_first_time_index;
	gboolean       is_initial_check;
	gboolean       is_paused_manually;
	gboolean       is_paused_for_batt;
	gboolean       is_paused_for_io;
	gboolean       is_paused_for_space;
	gboolean       is_paused_for_dbus;
	gboolean       is_paused_for_unknown;
	gboolean       in_merge;
} TrackerStatusPrivate;

static void indexer_continued_cb    (DBusGProxy  *proxy,
				     gpointer     user_data);
static void indexer_paused_cb       (DBusGProxy  *proxy,
				     const gchar *reason,
				     gpointer     user_data);
static void indexer_continue        (gboolean     should_block);
static void indexer_pause           (gboolean     should_block);
static void low_disk_space_limit_cb (GObject     *gobject,
				     GParamSpec  *arg1,
				     gpointer     user_data);
static void battery_in_use_cb       (GObject     *gobject,
				     GParamSpec  *arg1,
				     gpointer     user_data);
static void battery_percentage_cb   (GObject     *object,
				     GParamSpec  *pspec,
				     gpointer     user_data);
static void disk_space_check_stop   (void);

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

static void
private_free (gpointer data)
{
	TrackerStatusPrivate *private;

	private = data;

	if (private->disk_space_check_id) {
		g_source_remove (private->disk_space_check_id);
	}

	g_signal_handlers_disconnect_by_func (private->config,
					      low_disk_space_limit_cb,
					      NULL);

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

	if (private->status_type_class) {
		g_type_class_unref (private->status_type_class);
	}

	if (private->mode_type_class) {
		g_type_class_unref (private->mode_type_class);
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
		g_message ("WARNING: Available disk space is below configured "
			   "threshold for acceptable working (%d%%)",
			   limit);
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
		g_message ("Starting disk space check for every %d seconds",
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

static void
disk_space_check_stop (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (private->disk_space_check_id) {
		g_message ("Stopping disk space check");
		g_source_remove (private->disk_space_check_id);
		private->disk_space_check_id = 0;
	}
}

static void
mode_check (void)
{
	TrackerDBManagerFlags flags = 0;
	TrackerStatusPrivate *private;
	TrackerMode           new_mode;
	const gchar          *new_mode_str;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	new_mode = private->mode;

	if (private->is_paused_for_batt ||
	    private->is_paused_for_space) {
		new_mode = TRACKER_MODE_SAFE;
	} else {
		new_mode = TRACKER_MODE_FAST;
	}

	if (new_mode == private->mode) {
		return;
	}

	new_mode_str = tracker_mode_to_string (new_mode);

	g_message ("Mode change from '%s' --> '%s'",
		   tracker_mode_to_string (private->mode),
		   new_mode_str);


	private->mode = new_mode;

	/* Tell the indexer to switch profile */
	if (!tracker_dbus_indexer_set_profile (new_mode)) {
		return;
	}

	if (tracker_config_get_low_memory_mode (private->config)) {
		flags |= TRACKER_DB_MANAGER_LOW_MEMORY_MODE;
	}

	/* Now reinitialize DBs ourselves */
	tracker_db_manager_shutdown ();

	if (!tracker_db_manager_init (flags, NULL, TRUE, new_mode_str)) {
		g_critical ("Could not restart DB Manager, trying again with defaults");

		if (!tracker_db_manager_init (flags, NULL, TRUE, NULL)) {
			g_critical ("Not even defaults worked, bailing out");
			g_assert_not_reached ();
		}
	}
}

#ifdef HAVE_HAL

static void
set_up_throttle (gboolean debugging)
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
		if (debugging) {
			g_message ("We are running on battery");
		}

		if (throttle == THROTTLE_DEFAULT) {
			tracker_config_set_throttle (private->config,
						     THROTTLE_DEFAULT_ON_BATTERY);

			if (debugging) {
				g_message ("Setting throttle from %d to %d",
					   throttle,
					   THROTTLE_DEFAULT_ON_BATTERY);
			}
		} else {
			if (debugging) {
				g_message ("Not setting throttle, it is currently set to %d",
					   throttle);
			}
		}
	} else {
		if (debugging) {
			g_message ("We are not running on battery");
		}

		if (throttle == THROTTLE_DEFAULT_ON_BATTERY) {
			tracker_config_set_throttle (private->config,
						     THROTTLE_DEFAULT);

			if (debugging) {
				g_message ("Setting throttle from %d to %d",
					   throttle,
					   THROTTLE_DEFAULT);
			}
		} else {
			if (debugging) {
				g_message ("Not setting throttle, it is currently set to %d",
					   throttle);
			}
		}
	}
}

static void
low_disk_space_limit_cb (GObject    *gobject,
			 GParamSpec *arg1,
			 gpointer    user_data)
{
	disk_space_check_cb (NULL);
}

static void
battery_in_use_cb (GObject    *gobject,
		   GParamSpec *arg1,
		   gpointer    user_data)
{
	set_up_throttle (TRUE);
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

	g_message ("Battery percentage is now %.0f%%",
		   percentage * 100);

	/* FIXME: This could be a configuration option */
	if (battery_in_use) {
		if (percentage <= 0.05) {
			/* Running on low batteries, stop indexing for now */
			tracker_status_set_is_paused_for_batt (TRUE);
		} else {
			tracker_status_set_is_paused_for_batt (FALSE);
		}
	} else {
		tracker_status_set_is_paused_for_batt (FALSE);
	}

	set_up_throttle (FALSE);
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

	private->mode = TRACKER_MODE_SAFE;

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
	private->status_type_class = g_type_class_ref (type);

	type = tracker_mode_get_type ();
	private->mode_type_class = g_type_class_ref (type);

	private->config = g_object_ref (config);

	g_signal_connect (private->config, "notify::low-disk-space-limit",
			  G_CALLBACK (low_disk_space_limit_cb),
			  NULL);


#ifdef HAVE_HAL 
	private->hal = g_object_ref (hal);

	g_message ("Setting battery percentage checking");
	g_signal_connect (private->hal, "notify::battery-percentage",
			  G_CALLBACK (battery_percentage_cb),
			  NULL);
	g_signal_connect (private->hal, "notify::battery-in-use",
			  G_CALLBACK (battery_in_use_cb),
			  NULL);
#endif

	private->is_readonly = FALSE;
	private->is_ready = FALSE;
	private->is_running = FALSE;
	private->is_first_time_index = FALSE;
	private->is_initial_check = FALSE;
	private->is_paused_manually = FALSE;
	private->is_paused_for_batt = FALSE;
	private->is_paused_for_io = FALSE;
	private->is_paused_for_space = FALSE;
	private->is_paused_for_dbus = FALSE;
	private->is_paused_for_unknown = FALSE;
	private->in_merge = FALSE;

	/* Get process priority on start up:
	 * We use nice() when crawling so we don't steal all
	 * the processor time, for all other states, we return the
	 * nice() value to what it was.
	 *
	 * NOTE: We set errno first because -1 is a valid priority and
	 * we need to check it isn't an error.
	 */
	errno = 0;
	private->cpu_priority = getpriority (PRIO_PROCESS, 0);

	if ((private->cpu_priority < 0) && errno) {
		const gchar *str = g_strerror (errno);

		g_message ("Couldn't get nice value, %s", 
			   str ? str : "no error given");

		/* Default to 0 */
		private->cpu_priority = 0;
	}

	g_message ("Current process priority is set to %d, this will be used for all non-crawling states",
		   private->cpu_priority);

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
	
	/* Do initial disk space check, we don't start the timeout
	 * which checks the disk space every 10 seconds here because
	 * we might not have enough to begin with. So we do one
	 * initial check to set the state correctly. 
	 */
	disk_space_check_cb (NULL);

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

gboolean
tracker_status_is_initialized (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);

	return (private != NULL);
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

static void
status_set_priority (TrackerStatus old_status,
		     TrackerStatus new_status,
		     gint          special_priority,
		     gint          default_priority)
{
	/* Note, we can't use -1 here, so we use a value
	 * outside the nice values from -20->19 
	 */
	gint new_priority = 100;
	
	/* Handle priority changes */
	if (new_status == TRACKER_STATUS_PENDING) {
		new_priority = special_priority;
	} else if (old_status == TRACKER_STATUS_PENDING) {
		new_priority = default_priority;
	}
	
	if (new_priority != 100) {
		g_message ("Setting process priority to %d (%s)", 
			   new_priority,
			   new_priority == default_priority ? "default" : "special");
		
		if (nice (new_priority) == -1) {
			const gchar *str = g_strerror (errno);
			
			g_message ("Couldn't set nice value to %d, %s",
				   new_priority,
				   str ? str : "no error given");
		}
	}
}

void
tracker_status_set (TrackerStatus new_status)
{
	TrackerStatusPrivate *private;
	gboolean              should_be_paused;
	gboolean              invalid_new_state;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	should_be_paused =  
		private->is_paused_manually ||
		private->is_paused_for_batt || 
		private->is_paused_for_io ||
		private->is_paused_for_space ||
		private->is_paused_for_dbus ||
		private->is_paused_for_unknown;

	invalid_new_state = 
		should_be_paused && 
		new_status != TRACKER_STATUS_PAUSED;

	g_message ("State change from '%s' --> '%s' %s",
		   tracker_status_to_string (private->status),
		   tracker_status_to_string (new_status),
		   invalid_new_state ? "attempted with pause conditions, doing nothing" : "");

	/* Don't set previous status to the same as we are now,
	 * otherwise we could end up setting PAUSED and our old state
	 * to return to is also PAUSED.
	 *  
	 * We really should start using a proper state machine here,
	 * but I am avoiding it for now, -mr.
	 */
	if (private->status_before_paused != new_status && 
	    private->status != TRACKER_STATUS_PAUSED) {
		/* ALWAYS only set this after checking against the
		 * previous value 
		 */
		private->status_before_paused = private->status;
	}

	/* State machine */
	if (private->status != new_status) {
		/* The reason we have this check, is that it is
		 * possible that we are trying to set our state to
		 * IDLE after finishing crawling but actually, we
		 * should be in a PAUSED state due to another flag,
		 * such as low disk space. So we force PAUSED state.
		 * However, the interesting thing here is, we must
		 * remember to set the OLD state so we go back to the
		 * state as set by the caller. If we don't we end up
		 * going back to PENDING/WATCHING instead of IDLE when
		 * we come out of being PAUSED.
		 *
		 * FIXME: Should we ONLY cater for IDLE here? -mr
		 */
		if (invalid_new_state) {
			g_message ("Attempt to set state to '%s' with pause conditions, doing nothing",
				   tracker_status_to_string (new_status));

			if (private->status != TRACKER_STATUS_PAUSED) {
				tracker_status_set (TRACKER_STATUS_PAUSED);
			} 

			/* Set last state so we know what to return
			 * to when we come out of PAUSED 
			 */
			private->status_before_paused = new_status;

			return;
		}

		/* Make sure we are using the correct priority for
		 * the new state.
		 */
		status_set_priority (new_status, 
				     private->status,
				     TRACKER_STATUS_PENDING,
				     private->cpu_priority);
		
		/* Handle disk space moitor changes
		 * 
		 * So these are the conditions when we need checks
		 * running:
		 *
		 * 1. Are we low on space?
		 * 3. State is INDEXING or OPTIMIZING (use of disk)
		 */
		if (private->is_paused_for_space ||
		    new_status == TRACKER_STATUS_INDEXING || 
		    new_status == TRACKER_STATUS_OPTIMIZING) {
			disk_space_check_start ();
		} else {
			disk_space_check_stop ();
		}
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
tracker_status_get_is_initial_check (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, FALSE);

	return private->is_initial_check;
}

void
tracker_status_set_is_initial_check (gboolean value)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* Set value */
	private->is_initial_check = value;

	/* We don't need to signal this */
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
	mode_check ();
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
	mode_check ();
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
	mode_check ();
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
	mode_check ();
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
	mode_check ();
}

/*
 * Modes
 */

GType
tracker_mode_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GEnumValue values[] = {
			{ TRACKER_MODE_SAFE,
			  "TRACKER_MODE_SAFE",
			  "Safe" },
			{ TRACKER_MODE_FAST,
			  "TRACKER_MODE_FAST",
			  "Fast" },
			{ 0, NULL, NULL }
		};

		type = g_enum_register_static ("TrackerMode", values);
	}

	return type;
}

const gchar *
tracker_mode_to_string (TrackerMode mode)
{
	GType	    type;
	GEnumClass *enum_class;
	GEnumValue *enum_value;

	type = tracker_mode_get_type ();
	enum_class = G_ENUM_CLASS (g_type_class_peek (type));
	enum_value = g_enum_get_value (enum_class, mode);

	if (!enum_value) {
		enum_value = g_enum_get_value (enum_class, TRACKER_MODE_SAFE);
	}

	return enum_value->value_nick;
}

TrackerMode
tracker_mode_get (void)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_val_if_fail (private != NULL, TRACKER_MODE_SAFE);

	return private->mode;
}
