/* Tracker Applet - tray icon for the tracker indexing daemon
 *
 * Copyright (C) 2007, Saleem Abdulrasool <compnerd@gentoo.org>
 * Copyright (C) 2007, Jamie McCracken <jamiemcc@blueyonder.co.uk>
 * Copyright (C) 2008, Nokia
 *
 * Portions derived from xscreensaver and gnome-screensaver
 * Copyright (c) 1991-2004 Jamie Zawinski <jwz@jwz.org>
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
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

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <X11/X.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include <gtk/gtk.h>

#include <libnotify/notify.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>

#include <glade/glade.h>

#include <libtracker/tracker.h>

#include <libtracker-common/tracker-utils.h>

#include "tracker-applet.h"
#include "tracker-applet-marshallers.h"

#define TRAY_ICON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TYPE_TRAY_ICON, TrayIconPrivate))

#define PROGRAM			"tracker-applet"
#define PROGRAM_NAME		N_("Tracker Applet")

#define HOMEPAGE		"http://www.tracker-project.org/"
#define DESCRIPTION		"An applet for tracker"

#define DBUS_SERVICE_TRACKER	"org.freedesktop.Tracker"
#define DBUS_PATH_TRACKER	"/org/freedesktop/tracker"
#define DBUS_INTERFACE_TRACKER	"org.freedesktop.Tracker"

#define TRACKER_TYPE_G_STRV_ARRAY  (dbus_g_type_get_collection ("GPtrArray", G_TYPE_STRV))

#define DISABLE_DEBUG

#ifdef G_HAVE_ISO_VARARGS
#  ifdef DISABLE_DEBUG
#    define debug(...)
#  else
#    define debug(...) debug_impl (__VA_ARGS__)
#  endif
#elif defined(G_HAVE_GNUC_VARARGS)
#  if DISABLE_DEBUG
#    define debug(fmt...)
#  else
#    define debug(fmt...) debug_impl(fmt)
#  endif
#else
#  if DISABLE_DEBUG
#    define debug(x)
#  else
#    define debug debug_impl
#  endif
#endif

typedef enum {
	ICON_DEFAULT,
	ICON_PAUSED,
	ICON_INDEX1,
	ICON_INDEX2,
} IndexIcon;

typedef enum {
	STATE_INITIALIZING,
        STATE_WATCHING,
        STATE_INDEXING,
        STATE_PAUSED,
        STATE_PENDING,
        STATE_OPTIMIZING,
        STATE_IDLE,
        STATE_SHUTDOWN,
} State;

typedef enum {
	PAUSE_REASON_NONE,
	PAUSE_REASON_IO,
	PAUSE_REASON_BATTERY
} PauseReason;

typedef enum {
	AUTO_PAUSE_NONE,
	AUTO_PAUSE_INDEXING,
} AutoPause;

typedef struct {
	const gchar *name;
	gchar *label;
	GtkWidget *stat_label;
} StatInfo;

typedef struct _TrayIconPrivate {
	GtkStatusIcon *icon;
	GKeyFile *keyfile;
	gchar *filename;

	/* Settings */
	gboolean auto_hide;
	gboolean indexing_disabled;
	gboolean show_animation;
	gboolean reindex;
	AutoPause auto_pause_setting;

	/* Auto pause vars */
	gboolean auto_pause_timer_active;
	time_t auto_pause_last_time_event;

	gboolean user_pause;
	gboolean auto_pause;

	/* States */
	State state;
	PauseReason pause_reason;
	IndexIcon index_icon;
        gboolean initial_index;
	gboolean animated;
	gboolean animated_timer_active;
	gboolean is_watching_events;
	gboolean email_indexing;
	gboolean indexer_stopped;

	/* Status hints */
	gint items_done;
	gint items_remaining;
	gint items_total;
	gdouble seconds_elapsed;

	/* Main window */
	GtkMenu *menu;

	gboolean shown_starting_msg;
	gboolean shown_finished_msg;

	/* Tracker connection */
	TrackerClient *tracker;

	/* Stats window table shown */
	gboolean stat_window_active;
	gboolean stat_request_pending;

	/* Prefs window */
	GtkWidget *prefs_window;
	GtkWidget *chk_animate;
	GtkWidget *chk_show_icon;
	GtkWidget *opt_pause_off;
	GtkWidget *opt_pause_index;
	GtkWidget *btn_close;
} TrayIconPrivate;

static void set_auto_pause (TrayIcon *icon,
			    gboolean  pause);

static TrayIcon *main_icon;

static const gchar *index_icons[4] = {
	"tracker-applet-default.png",
	"tracker-applet-paused.png",
	"tracker-applet-indexing1.png",
	"tracker-applet-indexing2.png"
};

static StatInfo stat_info[13] = {
	{ "Files", NULL, NULL },
	{ "Folders", NULL, NULL },
	{ "Documents", NULL, NULL },
	{ "Images", NULL, NULL },
	{ "Music", NULL, NULL},
	{ "Videos", NULL, NULL },
	{ "Text", NULL, NULL },
	{ "Development", NULL, NULL },
	{ "Other", NULL, NULL },
	{ "Applications", NULL, NULL },

        /* These are particular supported apps */
	{ "GaimConversations", NULL, NULL },
	{ "EvolutionEmails", NULL, NULL },
	{ NULL, NULL, NULL },
};

static gboolean disable_daemon_start;

static GOptionEntry entries[] = {
	{ "disable-daemon-start", 'd', 0, G_OPTION_ARG_NONE, &disable_daemon_start,
	  NULL,
	  NULL
	},
	{ NULL }
};

#ifndef DISABLE_DEBUG

#include <glib/gprintf.h>

static void
debug_impl (const gchar *msg, ...)
{
	va_list args;

	va_start (args, msg);
	g_vfprintf (stderr, msg, args);
	va_end (args);

	g_fprintf (stderr, "\n");
}

#endif /* DISABLE_DEBUG */

static gboolean
query_pointer_timeout (Window window)
{
	Window root;
	Window child;
	int root_x;
	int root_y;
	int win_x;
	int win_y;
	unsigned int mask;

	gdk_error_trap_push ();
	XQueryPointer (GDK_DISPLAY (),
		       window,
		       &root, &child, &root_x, &root_y, &win_x, &win_y,
		       &mask);
	gdk_display_sync (gdk_display_get_default ());
	gdk_error_trap_pop ();

	return FALSE;
}

static void
set_status_hint (TrayIcon *icon)
{
	TrayIconPrivate *priv;
	const char *index_status = NULL;
	const char *pause_status = NULL;
	GString *hint;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	hint = g_string_new (NULL);

	switch (priv->state) {
	case STATE_INITIALIZING:
		/* Translators: this will be a status hint like: 
                 * Tracker: Initializing 
                 */
		index_status = _("Initializing");
		break;
	case STATE_WATCHING:
		/* Translators: this will be a status hint like: 
                 * Tracker: Adding File System Monitors
                 */
		index_status = _("Adding File System Monitors");
		break;
	case STATE_INDEXING:
		/* Translators: this will be a status hint like: 
                 * Tracker: Indexing
                 */
		index_status = _("Indexing");
		break;
	case STATE_PAUSED:
		/* Translators: this will be a status hint like: 
                 * Tracker: Paused 
                 */
		index_status = _("Paused");
		break;
	case STATE_PENDING:
		/* Translators: this will be a status hint like: 
                 * Tracker: Crawling File System
                 *  e.g. to crawl the file system for changes
                 */
		index_status = _("Crawling File System");
		break;
	case STATE_IDLE:
		/* Translators: this will be a status hint like: 
                 * Tracker: Idle 
                 */
		index_status = _("Idle");
		break;
	case STATE_OPTIMIZING:
		/* Translators: this will be a status hint like: 
                 * Tracker: Optimizing Databases 
                 */
		index_status = _("Optimizing Databases");
		break;
	case STATE_SHUTDOWN:
		/* Translators: this will be a status hint like: 
                 * Tracker: Shutting Down
                 */
		index_status = _("Shutting Down");
		break;
	}

	if (priv->user_pause) {
		/* Translators: this will be a status hint like: 
                 * Tracker: Initializing/Idle/Indexing/Merging (paused by user)
                 */
		pause_status = _("by user");
	} else if (priv->auto_pause) {
		/* Translators: this will be a status hint like: 
                 * Tracker: Initializing/Idle/Indexing/Merging (paused by system) 
                 */
		pause_status = _("by system");
	} else {
		switch (priv->pause_reason) {
		case PAUSE_REASON_IO:
                        /* Translators: this will be a status hint like: 
                         * Tracker: Initializing/Idle/Indexing/Merging (paused by system)
                         */
			pause_status = _("low disk space or heavy disk use");
			break;
		case PAUSE_REASON_BATTERY:
			/* FIXME: We need to check if we are on the
			 * battery first, this state purely means we
			 * WILL pause on battery.
			 */
                        /* Translators: this will be a status hint like: 
                         * Tracker: Initializing/Idle/Indexing/Merging (paused by battery)
                         */
			pause_status = _("low battery");
			break;
		default:
		case PAUSE_REASON_NONE:
			pause_status = NULL;
			break;
		}
	}

	if (pause_status) {
		/* Translators: this will be a status hint like: 
                 * Tracker: Indexing (paused by system)
                 */
		g_string_printf (hint, _("Tracker: %s (%s)"), index_status, pause_status);
	} else {
		/* Translators: this will be a status hint like: 
                 * Tracker: Indexing
                 */
		g_string_printf (hint, _("Tracker: %s"), index_status);
	}

	if (priv->state == STATE_INDEXING) {
		gchar *str1;
		gchar *str2;

		str1 = tracker_seconds_estimate_to_string (priv->seconds_elapsed,
							   FALSE,
							   priv->items_done,
							   priv->items_remaining);
		str2 = tracker_seconds_to_string (priv->seconds_elapsed, FALSE);

		if (str1) {
			str1[0] = g_ascii_toupper (str1[0]);
		}

		if (str2) {
			str2[0] = g_ascii_toupper (str2[0]);
		}

		g_string_append (hint,"\n\n");
		g_string_append_printf (hint,
					_("Done: %d of %d\nEstimated: %s\nElapsed: %s"),
					priv->items_done,
					priv->items_total,
					str1,
					str2);

		g_free (str2);
		g_free (str1);
	}

	tray_icon_set_tooltip (icon, hint->str);

	g_string_free (hint, TRUE);
}

static gboolean
can_auto_pause (TrayIcon *icon)
{
	TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	if (priv->user_pause ||
	    priv->pause_reason == PAUSE_REASON_BATTERY ||
	    priv->indexing_disabled ||
	    priv->indexer_stopped) {
		return FALSE;
	}

	switch (priv->auto_pause_setting) {
	case AUTO_PAUSE_NONE:
		return FALSE;
	case AUTO_PAUSE_INDEXING:
		return priv->state != STATE_IDLE;
	}

	return TRUE;
}

static void
set_tracker_icon (TrayIconPrivate *priv)
{
	const gchar *name;
	gchar *path;

	name = index_icons[priv->index_icon];
	path = g_build_filename (SHAREDIR,
				 "tracker",
				 "icons",
				 name,
				 NULL);

	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		gtk_status_icon_set_from_file (priv->icon, path);
	} else {
		g_critical ("Could not find icon:'%s'", path);
	}

	g_free (path);
}

static void
set_icon (TrayIconPrivate *priv)
{
	if (!priv->user_pause && 
            (priv->state == STATE_INITIALIZING ||
             priv->state == STATE_IDLE)) {
                priv->animated = FALSE;
                priv->animated_timer_active = FALSE;
                
                if (priv->index_icon != ICON_DEFAULT) {
                        priv->index_icon = ICON_DEFAULT;
                        set_tracker_icon (priv);
                }
                
                return;
	}

	if (priv->user_pause ||
	    priv->auto_pause ||
	    priv->pause_reason != PAUSE_REASON_NONE) {
		if (priv->index_icon != ICON_PAUSED) {
			priv->index_icon = ICON_PAUSED;
			set_tracker_icon (priv);
		}

		priv->animated = FALSE;
		priv->animated_timer_active = FALSE;

		return;
	}

	if (priv->state != STATE_INITIALIZING &&
	    priv->state != STATE_IDLE) {
		if (priv->index_icon == ICON_INDEX2 || !priv->show_animation) {
			priv->index_icon = ICON_DEFAULT;
		} else if (priv->index_icon != ICON_INDEX1) {
			priv->index_icon = ICON_INDEX1;
		} else {
			priv->index_icon = ICON_INDEX2;
		}

		set_tracker_icon (priv);
	}
}

static gboolean
auto_pause_timeout (gpointer data)
{
	TrayIcon *icon;
	TrayIconPrivate *priv;
	time_t t;

	icon = data;
	priv = TRAY_ICON_GET_PRIVATE (icon);

	t = time (NULL);

	if (priv->indexer_stopped) {
		return FALSE;
	}

	if (t >= priv->auto_pause_last_time_event + 2) {
		set_auto_pause (icon, FALSE);
		return FALSE;
	}

	dbus_g_proxy_begin_call (priv->tracker->proxy,
				 "PromptIndexSignals",
				 NULL,
				 NULL,
				 NULL,
				 G_TYPE_INVALID);

	return TRUE;
}

static void
set_auto_pause (TrayIcon *icon,
		gboolean  pause)
{
	TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	/* Do not pause/unpause if in user pause  */
	if (priv->user_pause) {
		priv->auto_pause_timer_active = FALSE;
		priv->auto_pause = FALSE;
		return;
	}

	priv->auto_pause = pause;

	if (pause) {
		priv->auto_pause_last_time_event = time (NULL);

		if (!priv->auto_pause_timer_active) {
			g_timeout_add_seconds (2, auto_pause_timeout, icon);

			priv->auto_pause_timer_active = TRUE;
			tracker_set_bool_option (priv->tracker, "Pause", TRUE, NULL);
		}

		priv->animated = FALSE;
	} else {
		priv->auto_pause_timer_active = FALSE;
		priv->auto_pause = FALSE;

		tracker_set_bool_option (priv->tracker, "Pause", FALSE,  NULL);
	}

	set_icon (priv);
}

static void
start_auto_pause_timer (TrayIcon *icon)
{
	TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	if (!can_auto_pause (icon)) {
		return;
	}

	priv->auto_pause_last_time_event = time (NULL);

	if (!priv->auto_pause_timer_active) {
		g_timeout_add_seconds (2, auto_pause_timeout, icon);
		set_auto_pause (icon, TRUE);
	}
}

static void
set_user_pause (TrayIcon *icon,
		gboolean  pause)
{
	TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (icon);
	priv->user_pause = pause;

	tracker_set_bool_option (priv->tracker, "Pause", pause, NULL);
}

static void
notice_events_inner (Window   window,
		     gboolean enable,
		     gboolean top)
{
	XWindowAttributes attrs;
	unsigned long events;
	Window root;
	Window parent;
	Window *kids;
	unsigned int nkids;
	int status;
	GdkWindow *gwindow;

	gwindow = gdk_window_lookup (window);
	if (gwindow != NULL && window != GDK_ROOT_WINDOW ()) {
		/* If it's one of ours, don't mess up its event mask. */
		return;
	}

	kids = NULL;
	status = XQueryTree (GDK_DISPLAY (),
			     window,
			     &root,
			     &parent,
			     &kids,
			     &nkids);

	if (status == 0) {
		if (kids != NULL) {
			XFree (kids);
		}

		return;
	}

	if (window == root) {
		top = FALSE;
	}

	memset (&attrs, 0, sizeof (attrs));
	XGetWindowAttributes (GDK_DISPLAY (), window, &attrs);

	if (enable) {
		/* Select for KeyPress on all windows that already have it selected */
		events = ((attrs.all_event_masks | attrs.
			   do_not_propagate_mask) & KeyPressMask);

		/* Keep already selected events.  This is important when the
		 * window == GDK_ROOT_WINDOW () since the mask will contain
		 * StructureNotifyMask that is essential for RANDR support */
		events |= attrs.your_event_mask;

		/* Select for SubstructureNotify on all windows */
		events |= SubstructureNotifyMask;

		/* Select for PropertyNotify events to get user time changes */
		events |= PropertyChangeMask;

		/* As with keypress events, only select mouse motion events
		 * for windows which already have them selected. */
		events |=
			((attrs.all_event_masks | attrs.
			  do_not_propagate_mask) & (PointerMotionMask |
						    PointerMotionHintMask));
	} else {
		/* We want to disable all events */

		/* Don't mess up the root window */
		if (window == GDK_ROOT_WINDOW ()) {
			events = attrs.your_event_mask;
		} else {
			events = 0;
		}
	}

	/* Select for SubstructureNotify on all windows.
	 * Select for KeyPress on all windows that already have it selected.
	 *
	 * Note that we can't select for ButtonPress, because of X braindamage:
	 * only one client at a time may select for ButtonPress on a given
	 * window, though any number can select for KeyPress.  Someone explain
	 * *that* to me.
	 *
	 * So, if the user spends a while clicking the mouse without ever moving
	 * the mouse or touching the keyboard, we won't know that they've been
	 * active, and the screensaver will come on.  That sucks, but I don't
	 * know how to get around it.
	 *
	 * Since X presents mouse wheels as clicks, this applies to those, too:
	 * scrolling through a document using only the mouse wheel doesn't
	 * count as activity...  Fortunately, /proc/interrupts helps, on
	 * systems that have it.  Oh, if it's a PS/2 mouse, not serial or USB.
	 * This sucks!
	 */
	XSelectInput (GDK_DISPLAY (), window, events);

	if (top && (events & KeyPressMask)) {
		/* Only mention one window per tree */
		top = FALSE;
	}

	if (kids != NULL) {
		while (nkids > 0) {
			notice_events_inner (kids[--nkids], enable, top);
		}

		XFree (kids);
	}
}

static void
notice_events (Window	window,
	       gboolean enable)
{
	gdk_error_trap_push ();

	notice_events_inner (window, enable, TRUE);

	gdk_display_sync (gdk_display_get_default ());
	gdk_error_trap_pop ();
}

static inline void
start_notice_events (Window window)
{
	notice_events (window, TRUE);
}

static GdkFilterReturn
filter_x_events (GdkXEvent *xevent,
		 GdkEvent  *event,
		 gpointer   data)
{
	XEvent *ev;
	TrayIcon *icon;

	icon = data;
	ev = xevent;

	switch (ev->xany.type) {
	case KeyPress:
	case KeyRelease:
	case ButtonPress:
	case ButtonRelease:
		start_auto_pause_timer (icon);
		break;

	case PropertyNotify:
		if (ev->xproperty.atom == gdk_x11_get_xatom_by_name ("_NET_WM_USER_TIME")) {
			start_auto_pause_timer (icon);
		}
		break;

	case CreateNotify: {
		Window window;

		window = ev->xcreatewindow.window;
		start_notice_events (window);

		break;
	}

	case MotionNotify:
		if (ev->xmotion.is_hint) {
			/* need to respond to hints so we continue to get events */
			g_timeout_add_seconds (1,
				       (GSourceFunc) query_pointer_timeout,
				       GINT_TO_POINTER (ev->xmotion.window));
		}

		start_auto_pause_timer (icon);
		break;

	default:
		break;
	}

	return GDK_FILTER_CONTINUE;
}

static inline void
stop_notice_events (Window window)
{
	notice_events (window, FALSE);
}

static void
start_watching_events (TrayIcon *icon)
{
	TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	if (priv->is_watching_events) {
		return;
	}

	gdk_window_add_filter (NULL, (GdkFilterFunc) filter_x_events, icon);
	start_notice_events (DefaultRootWindow (GDK_DISPLAY ()));
	priv->is_watching_events = TRUE;
}

static void
stop_watching_events (TrayIcon *icon)
{
	TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	if (!priv->is_watching_events) {
		return;
	}

	stop_notice_events (DefaultRootWindow (GDK_DISPLAY ()));
	gdk_window_remove_filter (NULL, (GdkFilterFunc) filter_x_events, icon);
	priv->is_watching_events = FALSE;
}

static void
tray_icon_class_init (TrayIconClass *klass)
{
	g_type_class_add_private (klass, sizeof (TrayIconPrivate));
}

static void
activate_icon (GtkStatusIcon *icon,
	       gpointer       data)
{
	GError *error = NULL;
	const gchar *command = "tracker-search-tool";

	debug ("Spawning command:'%s'\n", command);

	if (!g_spawn_command_line_async (command, &error)) {
		g_warning ("Unable to execute command:'%s', %s",
			   command,
			   error->message);
		g_error_free (error);
	}
}

static void
search_menu_activated (GtkMenuItem *item,
		       gpointer     data)
{
	activate_icon (NULL, NULL);
}

static void
pause_menu_toggled (GtkCheckMenuItem *item,
		    gpointer	      data)
{
	TrayIcon *icon;

	icon = TRAY_ICON (data);

	set_user_pause (icon,
			gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)));
}

static inline void
set_auto_pause_setting (TrayIcon  *icon,
			AutoPause  auto_pause)
{
	TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	priv->auto_pause_setting = auto_pause;
}

static void
save_options (TrayIcon *icon)
{
	TrayIconPrivate *priv;
	GError *error = NULL;
	gsize size;
	gchar *contents;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	g_key_file_set_boolean (priv->keyfile,
				"Applet", "AnimateWhenIndexing",
				gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->chk_animate)));
	g_key_file_set_boolean (priv->keyfile,
				"Applet", "AutoHideIcon",
				gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->chk_show_icon)));
	g_key_file_set_integer (priv->keyfile, "Applet", "SmartPause",
				priv->auto_pause_setting);

	contents = g_key_file_to_data (priv->keyfile, &size, &error);

	if (error) {
		g_error ("failed: g_key_file_to_data(): %s\n",
			 error->message);
		return;
	}

	g_file_set_contents (priv->filename, contents, -1, NULL);
	g_free (contents);
}

static void
prefs_closed (GtkWidget *widget,
	      gpointer	 data)
{
	TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (data);

	save_options (data);

	gtk_widget_destroy (priv->prefs_window);

        priv->prefs_window = NULL;
}

static void
opt_pause_off_group_changed_cb (GtkToggleButton *check_button,
				gpointer	 user_data)
{
	TrayIcon *icon;
	TrayIconPrivate *priv;
	const gchar *name;

	if (!gtk_toggle_button_get_active (check_button)) {
		return;
	}

	icon = user_data;
	priv = TRAY_ICON_GET_PRIVATE (icon);
	name = gtk_widget_get_name (GTK_WIDGET (check_button));

	if (g_str_equal (name, "opt_pause_off")) {
		priv->auto_pause_setting = AUTO_PAUSE_NONE;
		priv->auto_pause = FALSE;
		stop_watching_events (icon);
		return;
	}

	if (g_str_equal (name, "opt_pause_index")) {
		priv->auto_pause_setting = AUTO_PAUSE_INDEXING;

		if (can_auto_pause (icon)) {
			start_watching_events (icon);
		} else {
			stop_watching_events (icon);
		}

		return;
	}
}

static void
chk_animate_toggled_cb (GtkToggleButton *check_button,
			gpointer	 user_data)
{
	TrayIcon *icon;
	TrayIconPrivate *priv;

	icon = user_data;
	priv = TRAY_ICON_GET_PRIVATE (icon);

	priv->show_animation = gtk_toggle_button_get_active (check_button);
}

static void
chk_show_icon_toggled_cb (GtkToggleButton *check_button,
			  gpointer	   user_data)
{
	TrayIcon *icon;
	TrayIconPrivate *priv;

	icon = user_data;
	priv = TRAY_ICON_GET_PRIVATE (icon);

	if (gtk_toggle_button_get_active (check_button)) {
		priv->auto_hide = TRUE;
		gtk_status_icon_set_visible (priv->icon, FALSE);
	} else {
		priv->auto_hide = FALSE;
		if (!priv->indexing_disabled) {
			gtk_status_icon_set_visible (priv->icon, TRUE);
		}
	}
}

static void
create_prefs (TrayIcon *icon)
{
	TrayIconPrivate *priv;
	GladeXML *glade;
	gchar *filename;

	priv = TRAY_ICON_GET_PRIVATE (icon);
	filename = g_build_filename (SHAREDIR,
				     "tracker",
				     "tracker-applet-prefs.glade",
				     NULL);
	glade = glade_xml_new (filename, NULL, "tracker");

	if (!glade) {
		g_error ("Unable to find locate '%s'", filename);
		g_free (filename);
		priv->prefs_window = NULL;
		return;
	}

	g_free (filename);

	priv->prefs_window = glade_xml_get_widget (glade, "wnd_prefs");
	gtk_widget_hide (priv->prefs_window);
	gtk_window_set_deletable (GTK_WINDOW (priv->prefs_window), FALSE);

	priv->chk_animate = glade_xml_get_widget (glade, "chk_animate");
	priv->chk_show_icon = glade_xml_get_widget (glade, "chk_show_icon");
	priv->opt_pause_off = glade_xml_get_widget (glade, "opt_pause_off");
	priv->opt_pause_index =
		glade_xml_get_widget (glade, "opt_pause_index");
	priv->btn_close = glade_xml_get_widget (glade, "btn_close");

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->chk_animate),
				      priv->show_animation);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->chk_show_icon),
				      priv->auto_hide);

	switch (priv->auto_pause_setting) {
	case AUTO_PAUSE_NONE:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
					      (priv->opt_pause_off), TRUE);
		break;

	case AUTO_PAUSE_INDEXING:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
					      (priv->opt_pause_index), TRUE);
		break;

        default:
                g_critical ("Unreachable state in auto pause.");
	}

	/* connect signal handlers */
	g_signal_connect (GTK_TOGGLE_BUTTON (priv->chk_animate), "toggled",
			  G_CALLBACK (chk_animate_toggled_cb), main_icon);
	g_signal_connect (GTK_TOGGLE_BUTTON (priv->chk_show_icon), "toggled",
			  G_CALLBACK (chk_show_icon_toggled_cb), main_icon);
	g_signal_connect (GTK_TOGGLE_BUTTON (priv->opt_pause_off), "toggled",
			  G_CALLBACK (opt_pause_off_group_changed_cb),
			  main_icon);
	g_signal_connect (GTK_TOGGLE_BUTTON (priv->opt_pause_index),
			  "toggled",
			  G_CALLBACK (opt_pause_off_group_changed_cb),
			  main_icon);
	g_signal_connect (priv->btn_close, "clicked",
			  G_CALLBACK (prefs_closed), main_icon);
}

static void
restart_tracker (GtkDialog *dialog,
		 gint	    response,
		 TrayIcon  *icon)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));

	if (response == GTK_RESPONSE_YES) {
		TrayIconPrivate *priv;

		debug ("Attempting to restart tracker\n");

		priv = TRAY_ICON_GET_PRIVATE (icon);
		priv->reindex = TRUE;

		dbus_g_proxy_begin_call (priv->tracker->proxy,
					 "Shutdown",
					 NULL,
					 NULL,
					 NULL,
					 G_TYPE_BOOLEAN,
					 TRUE, G_TYPE_INVALID);
	}
}

static void
reindex (GtkMenuItem *item,
	 TrayIcon    *icon)
{
	GtkWidget *dialog;
	gchar	  *primary;
	gchar	  *secondary;

	primary = g_strdup (_("Re-index your system?"));
	secondary =
		g_strdup (_("Indexing can take a long time. "
			    "Are you sure you want to re-index?"));

	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_YES_NO, 
                                         "%s",
                                         primary);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  "%s",
						  secondary);

	g_free (primary);
	g_free (secondary);

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (restart_tracker), icon);
	gtk_widget_show (dialog);
}

static void
applet_preferences_menu_activated (GtkMenuItem *item,
				   gpointer	data)
{
	TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (data);

        if (priv->prefs_window == NULL) {
                create_prefs (data);
        }

	gtk_widget_show (priv->prefs_window);
}

static void
preferences_menu_activated (GtkMenuItem *item,
			    gpointer	 data)
{
	GError *error = NULL;
	const gchar *command = "tracker-preferences";

	debug ("Spawning command:'%s'\n", command);

	if (!g_spawn_command_line_async (command, &error)) {
		g_warning ("Unable to execute command:'%s', %s",
			   command,
			   error->message);
		g_error_free (error);
	}
}

static void
update_stats (GPtrArray *new_stats,
	      GError	*error,
	      gpointer	 data)
{
	TrayIcon *icon;
	TrayIconPrivate *priv;
	gint i, j;

	icon = data;
	priv = TRAY_ICON_GET_PRIVATE (icon);

        priv->stat_request_pending = FALSE;

	if (error) {
		g_warning ("Could not update statistics, %s",
			   error->message);
		g_error_free (error);
		return;
	}

        if (!new_stats) {
                return;
        }

        if (new_stats->len < 1 || !priv->stat_window_active) {
                return;
        }

        for (i = 0; i < new_stats->len; i++) {
                const gchar **p;
                const gchar  *service_type = NULL;
                
                p = g_ptr_array_index (new_stats, i);
                
                service_type = p[0];
		
                if (!service_type) {
                        continue;
                }

                for (j = 0; j < G_N_ELEMENTS (stat_info); j++) {
                        if (g_strcmp0 (stat_info[j].name, service_type) == 0) {
                                gtk_label_set_text (GTK_LABEL (stat_info[j].stat_label), p[1]);
                        }
                }
        }
}

static void
refresh_stats (TrayIcon *icon)
{
	TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	if (!priv->stat_window_active || priv->stat_request_pending) {
		return;
	}

	priv->stat_request_pending = TRUE;

	tracker_get_stats_async (priv->tracker,
				 (TrackerGPtrArrayReply) update_stats,
				 icon);
}

static void
statistics_dialog_response (GtkWidget *widget,
                            gint       arg,
                            gpointer   data)
{
	TrayIcon *icon;
	TrayIconPrivate *priv;

	icon = data;
	priv = TRAY_ICON_GET_PRIVATE (icon);

        /* Refresh stats */
        if (arg == GTK_RESPONSE_APPLY) {
                refresh_stats (icon);
                return;
        }

	priv->stat_window_active = FALSE;

	gtk_widget_destroy (widget);
}

static void
statistics_menu_activated (GtkMenuItem *item,
			   gpointer	data)
{
	TrayIcon *icon;
	TrayIconPrivate *priv;
	GtkWidget *dialog;
	GtkWidget *table;
	GtkWidget *hbox;
	GtkWidget *image;
	GtkWidget *label;
	gint i;

	icon = data;
	priv = TRAY_ICON_GET_PRIVATE (icon);

	dialog = gtk_dialog_new_with_buttons (_("Tracker Statistics"),
					      NULL,
					      GTK_DIALOG_NO_SEPARATOR |
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_REFRESH,
					      GTK_RESPONSE_APPLY,
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_CLOSE,
					      NULL);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
        gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 18);

	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "gtk-info");
	gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (statistics_dialog_response),
			  icon);

        /* Containers */
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);

        /* Icon */
	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO,
                                          GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);

        /* Stats */
	table = gtk_table_new (G_N_ELEMENTS (stat_info), 2, TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, TRUE, 0);

	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 65);

	for (i = 0; i < G_N_ELEMENTS (stat_info); i++) {
                if (stat_info[i].label == NULL) {
                        continue;
                }
                
		label = gtk_label_new (stat_info[i].label);
		gtk_label_set_selectable (GTK_LABEL (label), TRUE);

		gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
		gtk_table_attach_defaults (GTK_TABLE (table), 
                                           label, 
                                           0, 1, i + 1, i + 2);

                stat_info[i].stat_label = gtk_label_new ("0");
                
                gtk_label_set_selectable (GTK_LABEL (stat_info[i].stat_label), TRUE);
                gtk_misc_set_alignment (GTK_MISC (stat_info[i].stat_label), 0, 0);
                gtk_table_attach_defaults (GTK_TABLE (table),
                                           stat_info[i].stat_label,
                                           1, 2,
                                           i + 1, i + 2);
	}

        /* Set flags and get stats */
	priv->stat_window_active = TRUE;
	refresh_stats (icon);

        /* Show */
	gtk_widget_show_all (dialog);
}

static void
open_uri (GtkWindow  *parent,
	  const char *uri)
{
	GtkWidget *dialog;
	GdkScreen *screen;
	GError *error = NULL;
	gchar *cmdline;

	screen = gtk_window_get_screen (parent);

	cmdline = g_strconcat ("xdg-open ", uri, NULL);

	if (gdk_spawn_command_line_on_screen (screen, cmdline, &error) == FALSE) {
		dialog = gtk_message_dialog_new (parent,
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
                                                 "%s",
						 error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_error_free (error);
	}

	g_free (cmdline);
}

static void
about_url_hook (GtkAboutDialog *dialog,
		const gchar    *url,
		gpointer	data)
{
	open_uri (GTK_WINDOW (dialog), url);
}

static void
about_email_hook (GtkAboutDialog *dialog,
		  const gchar	 *email,
		  gpointer	  data)
{
	gchar *uri;

	uri = g_strconcat ("mailto:", email, NULL);
	open_uri (GTK_WINDOW (dialog), uri);
	g_free (uri);
}

static void
about_menu_activated (GtkMenuItem * item, gpointer data)
{
	const gchar *authors[] = {
		"Jamie McCracken <jamiemcc at gnome.org>",
		"Saleem Abdulrasool <compnerd at compnerd.org>"
			"Laurent Aguerreche <laurent.aguerreche at free fr>",
		"Luca Ferretti <elle.uca@libero.it>",
		"Eugenio <me at eugesoftware com>",
		"Michael Biebl <mbiebl at gmail com>",
		"Edward Duffy <eduffy at gmail com>",
		"Gergan Penkov <gergan at gmail com>",
		"Deji Akingunola <dakingun gmail com>",
		"Julien <julienc psychologie-fr org>",
		"Tom <tpgww@onepost.net>",
		"Samuel Cormier-Iijima <sciyoshi at gmail com>",
		"Eskil Bylund <eskil at letterboxes org>",
		"Ulrik Mikaelsson <ulrik mikaelsson gmail com>",
		"tobutaz <tobutaz gmail com>",
		"Mikkel Kamstrup Erlandsen <mikkel kamstrup gmail com>",
		"Baptiste Mille-Mathias <baptiste.millemathias gmail com>",
		"Richard Quirk <quirky@zoom.co.uk>",
		"Marcus Fritzsch <fritschy at googlemail com>",
		"Jedy Wang <Jedy Wang at Sun COM>",
		"Anders Aagaard <aagaande at gmail com>",
		"Fabien VALLON <fabien at sonappart net>",
		"Jaime Frutos Morales <acidborg at gmail com>",
		"Christoph Laimburg <christoph laimburg at rolmail net>",
		NULL
	};

	const gchar *documenters[] = {
		NULL
	};

	const gchar *license[] = {
		N_("Tracker is free software; you can redistribute it and/or modify " "it under the terms of the GNU General Public License as published by " "the Free Software Foundation; either version 2 of the License, or " "(at your option) any later version."),
		N_("Tracker is distributed in the hope that it will be useful, " "but WITHOUT ANY WARRANTY; without even the implied warranty of " "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the " "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License " "along with Tracker; if not, write to the Free Software Foundation, Inc., " "51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.")
	};

	gchar *license_trans;

	license_trans = g_strjoin ("\n\n", _(license[0]), _(license[1]),
				   _(license[2]), NULL);

	/* Make URLs and email clickable in about dialog */
	gtk_about_dialog_set_url_hook (about_url_hook, NULL, NULL);
	gtk_about_dialog_set_email_hook (about_email_hook, NULL, NULL);

	gtk_show_about_dialog (NULL,
			       "version", PACKAGE_VERSION,
			       "comments",
			       _
			       ("Tracker is a tool designed to extract info and metadata about your personal data so that it can be searched easily and quickly"),
			       "copyright",
			       _("Copyright \xC2\xA9 2005-2008 "
				 "The Tracker authors"), "license",
			       license_trans, "wrap-license", TRUE, "authors",
			       authors, "documenters", documenters,
			       /* Translators should localize the following string
				* which will be displayed at the bottom of the about
				* box to give credit to the translator(s).
				*/
			       "translator-credits", _("translator-credits"),
			       "logo-icon-name", "tracker",
			       "website", "http://www.tracker-project.org/",
			       "website-label", _("Tracker Web Site"), NULL);

	g_free (license_trans);
}

static void
quit_menu_activated (GtkMenuItem *item,
		     gpointer	  data)
{
	gtk_main_quit ();
}

static void
create_context_menu (TrayIcon *icon)
{
	TrayIconPrivate *priv;
	GtkWidget *item, *image;

	priv = TRAY_ICON_GET_PRIVATE (icon);
	priv->menu = (GtkMenu *) gtk_menu_new ();

	item = gtk_check_menu_item_new_with_mnemonic (_("_Pause All Indexing"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), FALSE);
	g_signal_connect (G_OBJECT (item), "toggled",
			  G_CALLBACK (pause_menu_toggled), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);


	item = gtk_image_menu_item_new_with_mnemonic (_("_Search"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_FIND,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (search_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);


	item = gtk_image_menu_item_new_with_mnemonic (_("_Re-index"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_FIND,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (reindex),
			  icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Preferences"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_PREFERENCES,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (applet_preferences_menu_activated),
			  icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Indexer Preferences"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_PREFERENCES,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (preferences_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_image_menu_item_new_with_mnemonic (_("S_tatistics"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_INFO,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (statistics_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_About"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_ABOUT,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (about_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Quit"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_QUIT,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (quit_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	gtk_widget_show_all (GTK_WIDGET (priv->menu));
}

static void
index_finished (DBusGProxy *proxy,
		gdouble     seconds_elapsed,
		TrayIcon   *icon)
{
        TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	debug ("Indexing finished in %f seconds\n",
		 seconds_elapsed);

	priv->indexer_stopped = FALSE;

        priv->state = STATE_IDLE;
	priv->pause_reason = PAUSE_REASON_NONE;

	priv->user_pause = FALSE;
	priv->auto_pause = FALSE;

	if (!priv->shown_finished_msg && priv->initial_index) {
                gchar *str;

		priv->shown_finished_msg = TRUE;
                
                str = tracker_seconds_to_string (seconds_elapsed, FALSE);
                tray_icon_show_message (icon,
                                        "%s in %s.\n"
                                        "\n"
                                        "%s",
                                        _("Tracker has finished indexing your system"),
                                        str,
                                        _("You can now perform searches by clicking here"));
                g_free (str);
        }

	set_icon (priv);
	set_status_hint (icon);

	stop_watching_events (icon);
}

static void
index_service_stats_updated (DBusGProxy *proxy,
                             GPtrArray  *new_stats,
                             TrayIcon   *icon)
{
        update_stats (new_stats, NULL, icon);
}

static void
index_state_changed (DBusGProxy  *proxy,
		     const gchar *state,
		     gboolean	  initial_index,
		     gboolean	  in_merge,
		     gboolean	  is_manual_paused,
		     gboolean	  is_battery_paused,
		     gboolean	  is_io_paused,
		     gboolean	  is_indexing_enabled,
		     TrayIcon	 *icon)
{
	TrayIconPrivate *priv;

	if (!state) {
		return;
	}

	priv = TRAY_ICON_GET_PRIVATE (icon);

	priv->indexer_stopped = FALSE;
        priv->initial_index = initial_index;

	if (!is_indexing_enabled) {
		priv->indexing_disabled = TRUE;
		gtk_status_icon_set_visible (priv->icon, FALSE);
		return;
	} else {
		priv->indexing_disabled = FALSE;
		if (!priv->auto_hide) {
			gtk_status_icon_set_visible (priv->icon, TRUE);
		}
	}

	if (!priv->shown_starting_msg && priv->initial_index) {
                static gchar *initial_index_1;
                static gchar *initial_index_2;
                              
		priv->shown_starting_msg = TRUE;

		g_usleep (G_USEC_PER_SEC / 10);

                initial_index_1 =
                        _("Your computer is about to be indexed so "
                          "you can perform fast searches of your files and emails");
                initial_index_2 =
                        _("You can pause indexing at any time and "
                          "configure index settings by right clicking here");
		tray_icon_show_message (icon,
					"%s\n"
					"\n"
					"%s\n",
					initial_index_1,
					initial_index_2);

                /* Reset stats if shown */
                if (priv->stat_window_active) {
                        gint i;
                        
                        for (i = 0; i < G_N_ELEMENTS (stat_info); i++) {
                                if (!stat_info[i].name ||
                                    !stat_info[i].stat_label) {
                                        continue;
                                }
                                
                                if (g_strcmp0 (stat_info[i].name, "Files") == 0) {
                                        continue;
                                }
                                
                                gtk_label_set_text (GTK_LABEL (stat_info[i].stat_label), "0");
                        }
                }
	}

	/* Set pause states if applicable */
	if (is_manual_paused) {
                priv->pause_reason = PAUSE_REASON_NONE;

		stop_watching_events (icon);

		if (!priv->auto_pause) {
			priv->user_pause = TRUE;
		}
	} else if (is_battery_paused) {
		priv->pause_reason = PAUSE_REASON_BATTERY;
	} else if (is_io_paused) {
		priv->pause_reason = PAUSE_REASON_IO;
	} else {
                priv->pause_reason = PAUSE_REASON_NONE;
        }

	if (g_ascii_strcasecmp (state, "Initializing") == 0) {
		priv->state = STATE_INITIALIZING;
		priv->animated = FALSE;
	} else if (g_ascii_strcasecmp (state, "Watching") == 0) {
		priv->state = STATE_WATCHING;
		priv->animated = TRUE;
	} else if (g_ascii_strcasecmp (state, "Indexing") == 0) {
		priv->state = STATE_INDEXING;
		priv->animated = TRUE;
	} else if (g_ascii_strcasecmp (state, "Paused") == 0) {
		priv->state = STATE_PAUSED;
		priv->animated = FALSE;
	} else if (g_ascii_strcasecmp (state, "Pending") == 0) {
		priv->state = STATE_PENDING;
		priv->animated = TRUE;
	} else if (g_ascii_strcasecmp (state, "Optimizing") == 0) {
		priv->state = STATE_OPTIMIZING;
		priv->animated = TRUE;
	} else if (g_ascii_strcasecmp (state, "Idle") == 0) {
		priv->state = STATE_IDLE;
		priv->animated = FALSE;
	} else if (g_ascii_strcasecmp (state, "Shutdown") == 0) {
		priv->state = STATE_SHUTDOWN;
		priv->animated = TRUE;
        } else {
                g_critical ("Unknown state '%s'", state);
	}

	set_icon (priv);

	/* Should we animate? */
	if (priv->state != STATE_PAUSED && 
            priv->show_animation && 
            priv->animated && 
            !priv->animated_timer_active) {
                priv->animated_timer_active = TRUE;
	}

	set_status_hint (icon);

	if (can_auto_pause (icon)) {
		start_watching_events (icon);
	} else {
		stop_watching_events (icon);
	}
}

static void
index_progress_changed (DBusGProxy  *proxy,
			const gchar *service,
			const gchar *uri,
			gint	     items_done,
			gint	     items_remaining,
			gint	     items_total,
			gdouble      seconds_elapsed,
			TrayIcon    *icon)
{
	TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	priv->items_done = items_done;
	priv->items_remaining = items_remaining;
	priv->items_total = items_total;
	priv->seconds_elapsed = seconds_elapsed;

	priv->email_indexing = strcmp (service, "Emails") == 0;

	debug ("Indexed %d/%d, seconds elapsed:%f\n",
               items_done,
               items_total,
               seconds_elapsed);

	set_status_hint (icon);
	set_icon (priv);
}

static void
init_settings (TrayIcon *icon)
{
	TrayIconPrivate *priv;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	priv->state = STATE_INITIALIZING;
	priv->pause_reason = PAUSE_REASON_NONE;
	priv->auto_pause_setting = AUTO_PAUSE_NONE;
	priv->index_icon = ICON_DEFAULT;
	priv->show_animation = TRUE;

	set_tracker_icon (priv);
}

static void
name_owner_changed (DBusGProxy	*proxy,
		    const gchar *name,
		    const gchar *prev_owner,
		    const gchar *new_owner,
		    gpointer	 data)
{
	if (!g_str_equal (name, DBUS_SERVICE_TRACKER)) {
		return;
	}

	if (g_str_equal (new_owner, "")) {
		TrayIconPrivate *priv;

		priv = TRAY_ICON_GET_PRIVATE (data);

		/* Tracker has exited so reset status and make
		 * invisible until trackerd relaunched.
		 */
		index_state_changed (proxy,
				     "Idle",
				     FALSE,
				     FALSE,
				     FALSE,
				     FALSE,
				     FALSE,
				     FALSE,
				     data);

		init_settings (data);
		gtk_status_icon_set_visible (priv->icon, FALSE);
		priv->indexer_stopped = TRUE;

		debug ("The Tracker daemon has exited (%s)\n",
                       priv->reindex ? "reindexing" : "not reindexing");

		if (priv->reindex) {
			GError *error = NULL;
			const gchar *command;

			command = g_build_filename (TRACKER_LIBEXECDIR, "trackerd", NULL);

			priv->reindex = FALSE;

			debug ("Restarting Tracker daemon in 1 second\n");
			g_usleep (G_USEC_PER_SEC);

			debug ("Spawning command:'%s'\n", command);

			if (!g_spawn_command_line_async (command, &error)) {
				g_warning ("Unable to execute command:'%s', %s",
					   command,
					   error->message);
				g_error_free (error);
			}
		}
	}
}

static gboolean
setup_dbus_connection (TrayIcon *icon)
{
	TrayIconPrivate *priv;
	DBusGConnection *connection;
	DBusGProxy *dbus_proxy;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	priv->tracker = tracker_connect (FALSE);

	if (!priv->tracker) {
		g_critical ("Could not connect to the Tracker daemon\n");
		exit (EXIT_FAILURE);
	}

	/* Set signal handlers */
	dbus_g_object_register_marshaller (tracker_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN,
					   G_TYPE_NONE,
					   G_TYPE_STRING,
					   G_TYPE_BOOLEAN,
					   G_TYPE_BOOLEAN,
					   G_TYPE_BOOLEAN,
					   G_TYPE_BOOLEAN,
					   G_TYPE_BOOLEAN,
					   G_TYPE_BOOLEAN,
					   G_TYPE_INVALID);

	dbus_g_object_register_marshaller (tracker_VOID__STRING_STRING_INT_INT_INT_DOUBLE,
					   G_TYPE_NONE,
					   G_TYPE_STRING,
					   G_TYPE_STRING,
					   G_TYPE_INT,
					   G_TYPE_INT,
					   G_TYPE_INT,
					   G_TYPE_DOUBLE,
					   G_TYPE_INVALID);

	dbus_g_proxy_add_signal (priv->tracker->proxy,
				 "IndexStateChange",
				 G_TYPE_STRING,
				 G_TYPE_BOOLEAN,
				 G_TYPE_BOOLEAN,
				 G_TYPE_BOOLEAN,
				 G_TYPE_BOOLEAN,
				 G_TYPE_BOOLEAN,
				 G_TYPE_BOOLEAN,
				 G_TYPE_INVALID);

	dbus_g_proxy_add_signal (priv->tracker->proxy,
				 "IndexProgress",
				 G_TYPE_STRING,
				 G_TYPE_STRING,
				 G_TYPE_INT,
				 G_TYPE_INT,
				 G_TYPE_INT,
				 G_TYPE_DOUBLE,
				 G_TYPE_INVALID);

	dbus_g_proxy_add_signal (priv->tracker->proxy,
				 "IndexFinished",
				 G_TYPE_DOUBLE,
				 G_TYPE_INVALID);

	dbus_g_proxy_add_signal (priv->tracker->proxy,
				 "ServiceStatisticsUpdated",
				 TRACKER_TYPE_G_STRV_ARRAY,
				 G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (priv->tracker->proxy,
				     "IndexStateChange",
				     G_CALLBACK (index_state_changed),
				     icon,
				     NULL);

	dbus_g_proxy_connect_signal (priv->tracker->proxy,
				     "IndexProgress",
				     G_CALLBACK (index_progress_changed),
				     icon,
				     NULL);

	dbus_g_proxy_connect_signal (priv->tracker->proxy,
				     "IndexFinished",
				     G_CALLBACK (index_finished),
				     icon,
				     NULL);

	dbus_g_proxy_connect_signal (priv->tracker->proxy,
				     "ServiceStatisticsUpdated",
				     G_CALLBACK (index_service_stats_updated),
				     icon,
				     NULL);

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
	dbus_proxy = dbus_g_proxy_new_for_name (connection,
						DBUS_SERVICE_DBUS,
						DBUS_PATH_DBUS,
						DBUS_INTERFACE_DBUS);

	dbus_g_proxy_add_signal (dbus_proxy,
				 "NameOwnerChanged",
				 G_TYPE_STRING,
				 G_TYPE_STRING,
				 G_TYPE_STRING, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (dbus_proxy,
				     "NameOwnerChanged",
				     G_CALLBACK (name_owner_changed),
				     icon, NULL);

	/* Prompt for updated signals */
	dbus_g_proxy_begin_call (priv->tracker->proxy,
				 "PromptIndexSignals",
				 NULL,
				 NULL,
				 NULL,
				 G_TYPE_INVALID);

	return FALSE;
}

static void
tray_icon_clicked (GtkStatusIcon *status_icon,
		   guint	  button,
		   guint	  timestamp,
		   gpointer	  data)
{
	TrayIcon *icon;
	TrayIconPrivate *priv;

	icon = TRAY_ICON (data);
	priv = TRAY_ICON_GET_PRIVATE (icon);

	gtk_menu_popup (GTK_MENU (priv->menu),
			NULL, NULL,
			gtk_status_icon_position_menu,
			status_icon,
			button,
			timestamp);
}

static void
tray_icon_init (GTypeInstance *instance,
		gpointer       klass)
{
	TrayIcon *icon;
	TrayIconPrivate *priv;

	icon = TRAY_ICON (instance);
	priv = TRAY_ICON_GET_PRIVATE (icon);

	priv->icon = gtk_status_icon_new ();

	init_settings (icon);

	priv->reindex = FALSE;

	g_signal_connect (G_OBJECT (priv->icon), "activate",
			  G_CALLBACK (activate_icon),
			  instance);
	g_signal_connect (G_OBJECT (priv->icon), "popup-menu",
			  G_CALLBACK (tray_icon_clicked),
			  instance);

	/* Build context menu */
	create_context_menu (icon);

	gtk_status_icon_set_visible (priv->icon, FALSE);

	setup_dbus_connection (icon);
}

void
tray_icon_set_tooltip (TrayIcon    *icon,
		       const gchar *format,
		       ...)
{
	TrayIconPrivate *priv;
	gchar *tooltip = NULL;
	va_list args;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	va_start (args, format);
	tooltip = g_strdup_vprintf (format, args);
	va_end (args);

	gtk_status_icon_set_tooltip (priv->icon, tooltip);

	g_free (tooltip);
}

void
tray_icon_show_message (TrayIcon    *icon,
			const gchar *message,
			...)
{
	TrayIconPrivate *priv;
	NotifyNotification *notification;
	gchar *msg = NULL;
	va_list args;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	va_start (args, message);
	msg = g_strdup_vprintf (message, args);
	va_end (args);

	if (priv->indexing_disabled) {
		return;
	}

	if (!priv->auto_hide && !gtk_status_icon_get_visible (priv->icon)) {
		gtk_status_icon_set_visible (priv->icon, TRUE);
	}

	notification =
		notify_notification_new_with_status_icon ("Tracker",
							  msg,
							  NULL,
							  priv->icon);

	notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);
	notify_notification_show (notification, NULL);

	g_object_unref (notification);

	g_free (msg);
}

GType
tray_icon_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (TrayIconClass),
			NULL,
			NULL,
			(GClassInitFunc) tray_icon_class_init,
			NULL,
			NULL,
			sizeof (TrayIcon),
			0,
			tray_icon_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "TrayIconType",
					       &info,
					       0);
	}

	return type;
}

static gboolean
load_options (TrayIcon *icon)
{
	TrayIconPrivate *priv;
	GError *error = NULL;

	priv = TRAY_ICON_GET_PRIVATE (icon);

	if (!priv->keyfile) {
		priv->keyfile = g_key_file_new ();
	}

	if (!g_file_test (priv->filename, G_FILE_TEST_EXISTS)) {
		gchar *tracker_dir;
		gchar *contents;

		tracker_dir = g_build_filename (g_get_user_config_dir (),
					        "tracker",
					        NULL);

		if (!g_file_test (tracker_dir, G_FILE_TEST_EXISTS)) {
			g_mkdir_with_parents (tracker_dir, 0700);
		}

		g_free (tracker_dir);

		contents = g_strconcat ("[Applet]\n",
					"AnimateWhenIndexing=true\n"
					"\n",
					"AutoHideIcon=false\n"
					"\n",
					"SmartPause=2\n",
					NULL);

		g_file_set_contents (priv->filename,
				     contents,
				     strlen (contents),
				     NULL);
		g_free (contents);
	}

	if (!g_key_file_load_from_file (priv->keyfile,
					priv->filename,
					G_KEY_FILE_KEEP_COMMENTS,
					&error) || error) {
		if (error) {
			g_warning ("Call to g_key_file_load_from_file() failed for filename:'%s', %s\n",
				   priv->filename,
				   error->message);
		}

		priv->show_animation = TRUE;
		priv->auto_hide = FALSE;
		priv->auto_pause_setting = AUTO_PAUSE_NONE;

		return FALSE;
	}

	if (g_key_file_has_key (priv->keyfile,
				"Applet",
				"AnimateWhenIndexing",
				NULL)) {
		priv->show_animation = g_key_file_get_boolean (priv->keyfile,
							       "Applet",
							       "AnimateWhenIndexing",
							       NULL);
	} else {
		priv->show_animation = TRUE;
	}

	if (g_key_file_has_key (priv->keyfile,
				"Applet",
				"AutoHideIcon",
				NULL)) {
		priv->auto_hide = g_key_file_get_boolean (priv->keyfile,
							  "Applet",
							  "AutoHideIcon",
							  NULL);
	} else {
		priv->auto_hide = FALSE;
	}

	if (g_key_file_has_key (priv->keyfile,
				"Applet",
				"SmartPause",
				NULL)) {
		priv->auto_pause_setting = g_key_file_get_integer (priv->keyfile,
								   "Applet",
								   "SmartPause",
								   NULL);
	} else {
		priv->auto_pause_setting = AUTO_PAUSE_NONE;
	}

	switch (priv->auto_pause_setting) {
        default:
	case AUTO_PAUSE_NONE:
		priv->auto_pause_setting = AUTO_PAUSE_NONE;
		priv->auto_pause = FALSE;
		stop_watching_events (icon);
		break;
	case AUTO_PAUSE_INDEXING:
		if (can_auto_pause (icon)) {
			start_watching_events (icon);
		} else {
			stop_watching_events (icon);
		}
		break;
	}

	return TRUE;
}

int
main (int argc, char *argv[])
{
	TrayIconPrivate *priv;
	GOptionContext *context;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	setlocale (LC_ALL, "");

	context = g_option_context_new (_("- Tracker applet for quick control of "
					  "your desktop search tools"));

	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	debug ("Initializing GTK+\n");

	gtk_init (&argc, &argv);

	debug ("Initializing libnotify\n");

	if (!notify_is_initted () &&
	    !notify_init (PROGRAM_NAME)) {
		g_warning ("Call to notify_init() failed\n");
		return EXIT_FAILURE;
	}

	debug ("Initializing strings\n");

	gtk_window_set_default_icon_name ("tracker");
	g_set_application_name (_("Tracker"));

	stat_info[0].label = _("Files:");
	stat_info[1].label = _("    Folders:");
	stat_info[2].label = _("    Documents:");
	stat_info[3].label = _("    Images:");
	stat_info[4].label = _("    Music:");
	stat_info[5].label = _("    Videos:");
	stat_info[6].label = _("    Text:");
	stat_info[7].label = _("    Development:");
	stat_info[8].label = _("    Other:");
	stat_info[9].label = _("Applications:");
	stat_info[10].label = _("Conversations:");
	stat_info[11].label = _("Emails:");

	debug ("Initializing tray icon\n");

	main_icon = g_object_new (TYPE_TRAY_ICON, NULL);

	priv = TRAY_ICON_GET_PRIVATE (main_icon);

	debug ("Initializing config\n");

	priv->keyfile = NULL;
	priv->filename = g_build_filename (g_get_user_config_dir (),
					   "tracker",
					   "tracker-applet.cfg",
					   NULL);

	if (!load_options (main_icon)) {
		return EXIT_FAILURE;
	}

	debug ("Starting main loop\n");

	gtk_main ();

	debug ("Shutting down\n");

	notify_uninit ();

	return EXIT_SUCCESS;
}
