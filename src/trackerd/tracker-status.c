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
#include "tracker-indexer-client.h"

typedef struct {
	TrackerStatus  status;
	TrackerStatus  status_before_paused;
	gpointer       type_class;

	TrackerConfig *config;

	DBusGProxy    *indexer_proxy;

	gboolean       is_readonly;
	gboolean       is_ready;
	gboolean       is_running;
	gboolean       is_first_time_index;
	gboolean       is_paused_manually;
	gboolean       is_paused_for_batt;
	gboolean       is_paused_for_io;
	gboolean       is_paused_for_space;
	gboolean       is_paused_for_unknown;
	gboolean       in_merge;
} TrackerStatusPrivate;

static void indexer_continued_cb (DBusGProxy  *proxy,
				  gpointer     user_data);
static void indexer_paused_cb    (DBusGProxy  *proxy,
				  const gchar *reason,
				  gpointer     user_data);
static void indexer_continue     (guint        seconds);
static void indexer_pause        (void);

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
indexer_recheck (gboolean should_inform_indexer)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* Are we paused in any way? */
	if (private->is_paused_manually ||
	    private->is_paused_for_batt || 
	    private->is_paused_for_io ||
	    private->is_paused_for_space) {
		/* We are paused, but our status is NOT paused? */
		if (private->status != TRACKER_STATUS_PAUSED) {
			private->status_before_paused = private->status;
			private->status = TRACKER_STATUS_PAUSED;

			if (G_LIKELY (should_inform_indexer)) {
				indexer_pause ();
			}
		}
	} else {
		/* We are not paused, but our status is paused */
		if (private->status == TRACKER_STATUS_PAUSED) {
			private->status = private->status_before_paused;

			if (G_LIKELY (should_inform_indexer)) {
				indexer_continue (0);
			}
		}
	}
}

static void
indexer_paused_cb (DBusGProxy  *proxy,
		   const gchar *reason,
		   gpointer     user_data)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* NOTE: This is when we are told by the indexer, so we don't
	 * know without checking with the status module if we sent
	 * this or not - we certainly should not inform the indexer
	 * again.
	 */
	g_message ("The indexer has paused (Reason: %s)", 
		   reason ? reason : "None");

	if (reason) {
		if (strcmp (reason, "Disk full") == 0) {
			private->is_paused_for_space = TRUE;
			indexer_recheck (FALSE);
			tracker_status_signal ();
		} else if (strcmp (reason, "Battery low") == 0) {
			private->is_paused_for_batt = TRUE;
		} else {
			private->is_paused_for_unknown = TRUE;
		}
	} else {
		private->is_paused_for_unknown = TRUE;
	}

	indexer_recheck (FALSE);
	tracker_status_signal ();
}

static void
indexer_continued_cb (DBusGProxy *proxy,
		      gpointer	  user_data)
{
	TrackerStatusPrivate *private;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	/* NOTE: This is when we are told by the indexer, so we don't
	 * know without checking with the status module if we sent
	 * this or not - we certainly should not inform the indexer
	 * again.
	 */
	
	g_message ("The indexer has continued");

	/* So now the indexer has told us it has continued, we make
	 * sure that none of the pause states are TRUE.
	 */
	private->is_paused_manually = FALSE;
	private->is_paused_for_batt = FALSE;
	private->is_paused_for_io = FALSE;
	private->is_paused_for_space = FALSE;
	private->is_paused_for_unknown = FALSE;

	/* Set state to what it was before the pause. */
	private->status = private->status_before_paused;

	/* We signal this to listening apps, but we don't call
	 * indexer_recheck() because we don't want to tell the indexer
	 * what it just told us :)
	 */
	tracker_status_set_and_signal (private->status);
}

static void
indexer_continue_cb (DBusGProxy *proxy,
		     GError     *error,
		     gpointer    user_data)
{
	if (error) {
		g_message ("Could not continue the indexer, %s",
			   error->message);

		/* Return state to paused */
		tracker_status_set_and_signal (TRACKER_STATUS_PAUSED);

		/* FIXME: Should we set some sort of boolean here for:
		 * [I couldn't resume because the indexer b0rked]? 
		 * 
		 * This is a potential deadlock, since we won't check
		 * again until we get another dbus request or
		 * something else sets this off. 
		 *
		 * -mr
		 */
	}
}

static void
indexer_continue (guint seconds)
{
	TrackerStatusPrivate *private;

	/* NOTE: We don't need private, but we do this to check we
	 * are initialised before calling continue.
	 */
	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	if (seconds < 1) {
		org_freedesktop_Tracker_Indexer_continue_async (private->indexer_proxy,
								indexer_continue_cb,
								NULL);
	} else {
		/* FIXME: Finish */
	}
}

static void
indexer_pause_cb (DBusGProxy *proxy,
		  GError     *error,
		  gpointer    user_data)
{
	if (error) {
		TrackerStatusPrivate *private;
		
		private = g_static_private_get (&private_key);
		g_return_if_fail (private != NULL);

		g_message ("Could not pause the indexer, %s",
			   error->message);

		/* Return state to before paused */
		tracker_status_set_and_signal (private->status_before_paused);

		/* FIXME: Should we set some sort of boolean here for:
		 * [I couldn't resume because the indexer b0rked]? 
		 * 
		 * This is a potential deadlock, since we won't check
		 * again until we get another dbus request or
		 * something else sets this off. 
		 *
		 * -mr
		 */
	}
}

static void
indexer_pause (void)
{
	TrackerStatusPrivate *private;

	/* NOTE: We don't need private, but we do this to check we
	 * are initialised before calling continue.
	 */
	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	org_freedesktop_Tracker_Indexer_pause_async (private->indexer_proxy,
						     indexer_pause_cb,
						     NULL);
}

gboolean
tracker_status_init (TrackerConfig *config)
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

	private->is_readonly = FALSE;
	private->is_ready = FALSE;
	private->is_running = FALSE;
	private->is_first_time_index = FALSE;
	private->is_paused_manually = FALSE;
	private->is_paused_for_batt = FALSE;
	private->is_paused_for_io = FALSE;
	private->is_paused_for_space = FALSE;
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
	gboolean	      emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	emit  = TRUE;
	emit |= private->is_paused_manually != value;
	emit |= private->status != TRACKER_STATUS_PAUSED;

	/* Set value */
	private->is_paused_manually = value;

	/* Set indexer state and our state to paused or not */ 
	indexer_recheck (TRUE);

	if (emit) {
		tracker_status_signal ();
	}
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
	gboolean	      emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	emit  = TRUE;
	emit |= private->is_paused_for_batt != value;
	emit |= private->status != TRACKER_STATUS_PAUSED;

	/* Set value */
	private->is_paused_for_batt = value;

	/* Set indexer state and our state to paused or not */ 
	indexer_recheck (TRUE);

	if (emit) {
		tracker_status_signal ();
	}
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

	emit  = TRUE;
	emit |= private->is_paused_for_batt != value;
	emit |= private->status != TRACKER_STATUS_PAUSED;

	/* Set value */
	private->is_paused_for_io = value;

	/* Set indexer state and our state to paused or not */ 
	indexer_recheck (TRUE);

	/* Tell the world */
	if (emit) {
		tracker_status_signal ();
	}
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
	gboolean	      emit;

	private = g_static_private_get (&private_key);
	g_return_if_fail (private != NULL);

	emit = private->is_paused_for_space != value;

	emit  = TRUE;
	emit |= private->is_paused_for_batt != value;
	emit |= private->status != TRACKER_STATUS_PAUSED;

	/* Set value */
	private->is_paused_for_space = value;

	/* Set indexer state and our state to paused or not */ 
	indexer_recheck (TRUE);

	if (emit) {
		tracker_status_signal ();
	}
}

