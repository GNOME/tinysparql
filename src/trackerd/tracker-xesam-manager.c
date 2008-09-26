/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 * Authors: Philip Van Hoof (pvanhoof@gnome.org)
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

#include <sys/types.h>
#include <unistd.h>

#include <libtracker-common/tracker-config.h>
#include <libtracker-common/tracker-utils.h>

#include <libtracker-db/tracker-db-manager.h>

#include "tracker-xesam-manager.h"
#include "tracker-dbus.h"
#include "tracker-main.h"

static gboolean		   initialized;
static TrackerDBInterface *xesam_db_iface;
static GHashTable	  *xesam_sessions;
static gchar		  *xesam_dir;
static gboolean		   indexing_finished;
static guint		   live_search_handler_id;

static void
indexer_status_cb (DBusGProxy  *proxy,
		   gdouble	seconds_elapsed,
		   const gchar *current_module_name,
		   guint	items_done,
		   guint	items_remaining,
		   gpointer	user_data)
{
	tracker_xesam_manager_wakeup ();
}

static void
indexer_started_cb (DBusGProxy *proxy,
		    gpointer	user_data)
{
	/* So now when we get status updates we DO NOT process live
	 * events and update live searches. The indexer is using the cache.
	 */
	g_message ("Disabling live search event updates (indexer started)");
	indexing_finished = FALSE;
}

static void
indexer_finished_cb (DBusGProxy *proxy,
		     gdouble	 seconds_elapsed,
		     guint	 items_done,
		     gboolean	 interrupted,
		     gpointer	 user_data)
{
	/* So now when we get status updates we can process live
	 * events and update live searches.
	 */
	g_message ("Enabling live search event updates (indexer finished)");
	indexing_finished = TRUE;

	/* Shouldn't we release ref (A) here? (see below) */
	/* g_object_unref (proxy) */
}

GQuark
tracker_xesam_manager_error_quark (void)
{
	static GQuark quark = 0;

	if (quark == 0) {
		quark = g_quark_from_static_string ("TrackerXesam");
	}

	return quark;
}

void
tracker_xesam_manager_init (void)
{
	DBusGProxy *proxy;

	if (initialized) {
		return;
	}

	/* Set up sessions hash table */
	xesam_sessions = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						(GDestroyNotify) g_free,
						(GDestroyNotify) g_object_unref);

	/* Set up locations */
	xesam_dir = g_build_filename (g_get_home_dir (), ".xesam", NULL);

	/* Set up DBus proxy to the indexer process */
	proxy = tracker_dbus_indexer_get_proxy ();

	/* When is this ref released? (A) */
	g_object_ref (proxy);

	dbus_g_proxy_connect_signal (proxy, "Status",
				     G_CALLBACK (indexer_status_cb),
				     NULL,
				     NULL);
	dbus_g_proxy_connect_signal (proxy, "Started",
				     G_CALLBACK (indexer_started_cb),
				     NULL,
				     NULL);
	dbus_g_proxy_connect_signal (proxy, "Finished",
				     G_CALLBACK (indexer_finished_cb),
				     NULL,
				     NULL);

	/* Set the indexing finished state back to unfinished */
	indexing_finished = FALSE;

	/* Get the DB interface now instead of later when the database
	 * is potentially being hammered with new information by the
	 * indexer. Before, if we just got it in the live update from
	 * the indexer, we couldn't create the interface quickly
	 * because the database is being used heavily by the indexer
	 * already. It is best to do this initially to avoid that.
	 */
	xesam_db_iface = tracker_db_manager_get_db_interface_by_service (TRACKER_DB_FOR_XESAM_SERVICE);
}

void
tracker_xesam_manager_shutdown (void)
{
	DBusGProxy *proxy;

	if (!initialized) {
		return;
	}

	g_object_unref (xesam_db_iface);
	xesam_db_iface = NULL;

	proxy = tracker_dbus_indexer_get_proxy ();
	dbus_g_proxy_disconnect_signal (proxy, "Status",
					G_CALLBACK (indexer_status_cb),
					NULL);
	dbus_g_proxy_disconnect_signal (proxy, "Started",
					G_CALLBACK (indexer_started_cb),
					NULL);
	dbus_g_proxy_disconnect_signal (proxy, "Finished",
					G_CALLBACK (indexer_finished_cb),
					NULL);
	g_object_unref (proxy);

	indexing_finished = FALSE;

	if (live_search_handler_id != 0) {
		g_source_remove (live_search_handler_id);
		live_search_handler_id = 0;
	}

	g_free (xesam_dir);
	xesam_dir = NULL;

	g_hash_table_unref (xesam_sessions);
	xesam_sessions = NULL;
}

TrackerXesamSession *
tracker_xesam_manager_create_session (TrackerXesam  *xesam,
				      gchar	   **session_id,
				      GError	   **error)
{
	TrackerXesamSession *session;

	session = tracker_xesam_session_new ();
	tracker_xesam_session_set_id (session, tracker_xesam_manager_generate_unique_key ());

	g_hash_table_insert (xesam_sessions,
			     g_strdup (tracker_xesam_session_get_id (session)),
			     g_object_ref (session));

	if (session_id) {
		*session_id = g_strdup (tracker_xesam_session_get_id (session));
	}

	return session;
}

void
tracker_xesam_manager_close_session (const gchar  *session_id,
				     GError	 **error)
{
	gpointer inst = g_hash_table_lookup (xesam_sessions, session_id);

	if (!inst) {
		g_set_error (error,
			     TRACKER_XESAM_ERROR_DOMAIN,
			     TRACKER_XESAM_ERROR_SESSION_ID_NOT_REGISTERED,
			     "Session ID is not registered");
	} else {
		g_hash_table_remove (xesam_sessions, session_id);
	}
}

TrackerXesamSession *
tracker_xesam_manager_get_session (const gchar	*session_id,
				   GError      **error)
{
	TrackerXesamSession *session = g_hash_table_lookup (xesam_sessions, session_id);

	if (session) {
		g_object_ref (session);
	} else {
		g_set_error (error,
			     TRACKER_XESAM_ERROR_DOMAIN,
			     TRACKER_XESAM_ERROR_SESSION_ID_NOT_REGISTERED,
			     "Session ID is not registered");
	}

	return session;
}

TrackerXesamSession *
tracker_xesam_manager_get_session_for_search (const gchar	      *search_id,
					      TrackerXesamLiveSearch **search_in,
					      GError		     **error)
{
	TrackerXesamSession *session = NULL;
	GList		    *sessions;

	sessions = g_hash_table_get_values (xesam_sessions);

	while (sessions) {
		TrackerXesamLiveSearch *search;

		search = tracker_xesam_session_get_search (sessions->data, search_id, NULL);

		if (search) {
			/* Search got a reference added already */
			if (search_in) {
				*search_in = search;
			} else {
				g_object_unref (search);
			}

			session = g_object_ref (sessions->data);
			break;
		}

		sessions = g_list_next (sessions);
	}

	g_list_free (sessions);

	if (!session) {
		g_set_error (error,
			     TRACKER_XESAM_ERROR_DOMAIN,
			     TRACKER_XESAM_ERROR_SEARCH_ID_NOT_REGISTERED,
			     "Search ID is not registered");
	}

	return session;
}

TrackerXesamLiveSearch *
tracker_xesam_manager_get_live_search (const gchar  *search_id,
				       GError	   **error)
{
	TrackerXesamLiveSearch *search = NULL;
	GList		       *sessions;

	sessions = g_hash_table_get_values (xesam_sessions);

	while (sessions) {
		TrackerXesamLiveSearch *p;

		p = tracker_xesam_session_get_search (sessions->data, search_id, NULL);

		if (p) {
			/* Search got a reference added already */
			search = p;
			break;
		}

		sessions = g_list_next (sessions);
	}

	g_list_free (sessions);

	if (!search) {
		g_set_error (error,
			     TRACKER_XESAM_ERROR_DOMAIN,
			     TRACKER_XESAM_ERROR_SEARCH_ID_NOT_REGISTERED,
			     "Search ID is not registered");
	}

	return search;
}

static gboolean
live_search_handler (gpointer data)
{
	GList	 *sessions;
	gboolean  reason_to_live = FALSE;

	sessions = g_hash_table_get_values (xesam_sessions);

	while (sessions) {
		GList *searches;

		g_debug ("Session being handled, ID :%s",
			 tracker_xesam_session_get_id (sessions->data));

		searches = tracker_xesam_session_get_searches (sessions->data);

		while (searches) {
			TrackerXesamLiveSearch *search;
			GArray		       *added = NULL;
			GArray		       *removed = NULL;
			GArray		       *modified = NULL;

			g_debug ("Search being handled, ID :%s",
				 tracker_xesam_live_search_get_id (searches->data));

			search = searches->data;

			/* TODO: optimize by specifying what exactly got changed
			 * during this event ping in the MatchWithEventsFlags
			 being passed (second parameter) */

			tracker_xesam_live_search_match_with_events (search,
								     MATCH_WITH_EVENTS_ALL_FLAGS,
								     &added,
								     &removed,
								     &modified);

			if (added && added->len > 0) {
				reason_to_live = TRUE;
				tracker_xesam_live_search_emit_hits_added (search, added->len);
			}

			if (added) {
				g_array_free (added, TRUE);
			}

			if (removed && removed->len > 0) {
				reason_to_live = TRUE;
				tracker_xesam_live_search_emit_hits_removed (search, removed);
			}

			if (removed) {
				g_array_free (removed, TRUE);
			}

			if (modified && modified->len > 0) {
				reason_to_live = TRUE;
				tracker_xesam_live_search_emit_hits_modified (search, modified);
			}

			if (modified) {
				g_array_free (modified, TRUE);
			}

			searches = g_list_next (searches);
		}

		g_list_free (searches);

		sessions = g_list_next (sessions);
	}

	g_list_free (sessions);

	tracker_db_xesam_delete_handled_events (xesam_db_iface);

	return reason_to_live;
}

static void
live_search_handler_destroy (gpointer data)
{
	live_search_handler_id = 0;
}

void
tracker_xesam_manager_wakeup (void)
{
	/* This happens each time a new event is created:
	 *
	 * We could do this in a thread too, in case blocking the
	 * GMainLoop is not ideal (it's not, because during these
	 * blocks of code, no DBus request handler can run).
	 *
	 * In case of a thread we could use usleep() and stop the
	 * thread if we didn't get a wakeup-call nor we had items to
	 * process this loop
	 *
	 * There are problems with this. Right now we WAIT until
	 * after indexing has completed otherwise we are in a
	 * situation where a "status" signal from the indexer makes us
	 * delete events from the Events table. This requires the
	 * cache db and means we end up waiting for the indexer to
	 * finish doing what it is doing first. The daemon then stops
	 * pretty much and blocks. This is bad. So we wait for the
	 * indexing to be finished before doing this.
	 */
	if (!indexing_finished) {
		return;
	}

	if (live_search_handler_id == 0) {
		live_search_handler_id =
			g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
					    2000, /* 2 seconds */
					    live_search_handler,
					    NULL,
					    live_search_handler_destroy);
	}
}

gchar *
tracker_xesam_manager_generate_unique_key (void)
{
	static guint  serial = 0;
	gchar	     *key;
	guint	      t, ut, p, u, r;
	GTimeVal      tv;

	g_get_current_time (&tv);

	t = tv.tv_sec;
	ut = tv.tv_usec;

	p = getpid ();

#ifdef HAVE_GETUID
	u = getuid ();
#else
	u = 0;
#endif

	r = g_random_int ();
	key = g_strdup_printf ("%ut%uut%uu%up%ur%uk%u",
			       serial, t, ut, u, p, r,
			       GPOINTER_TO_UINT (&key));

	++serial;

	return key;
}

gboolean
tracker_xesam_manager_is_uri_in_xesam_dir (const gchar *uri)
{
	g_return_val_if_fail (uri != NULL, FALSE);

	return g_str_has_prefix (uri, xesam_dir);
}
