
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>

#include "config.h"

#include "tracker-applet.h"
#include "tracker-applet-private.h"
#include "tracker.h"
#include "tracker-applet-marshallers.h"

#define PROGRAM                 "tracker-applet"
#define PROGRAM_NAME            "Tracker Applet"

#define HOMEPAGE                "http://www.tracker-project.org/"
#define DESCRIPTION             "An applet for tracker"

#define DBUS_SERVICE_TRACKER    "org.freedesktop.Tracker"
#define DBUS_PATH_TRACKER       "/org/freedesktop/tracker"
#define DBUS_INTERFACE_TRACKER  "org.freedesktop.Tracker"

static TrayIcon *main_icon;

/* translatable strings */

static char *files;
static char *folders;
static char *emails;

static char *mail_boxes;

static char *status_indexing;
static char *status_idle;
static char *status_merge;

static char *status_paused;
static char *status_paused_io;
static char *status_battery_paused;

static char *initial_index_1;
static char *initial_index_2;

static char *end_index_initial_msg;
static char *end_index_hours_msg;
static char *end_index_minutes_msg;
static char *end_index_seconds_msg;
static char *end_index_final_msg;

static char *start_merge_msg;

static char *tracker_title;




typedef struct {
	char 		*name;
	char 		*label;
	GtkWidget 	*stat_label;
} Stat_Info;

static Stat_Info stat_info[13] = {

	{"Files",  		NULL,	NULL},
	{"Folders",  		NULL, 	NULL},
	{"Documents",  		NULL, 	NULL},
	{"Images",  		NULL, 	NULL},
	{"Music",  		NULL, 	NULL},
	{"Videos", 		NULL, 	NULL},
	{"Text",  		NULL, 	NULL},
	{"Development",  	NULL, 	NULL},
	{"Other",  		NULL, 	NULL},
	{"Applications",  	NULL, 	NULL},
	{"Conversations",  	NULL, 	NULL},
	{"Emails",  		NULL,	NULL},
	{ NULL, 		NULL,	NULL},
};


static void refresh_stats (TrayIcon *self);

static void
tray_icon_class_init (TrayIconClass *klass)
{
	g_type_class_add_private (klass, sizeof(TrayIconPrivate));
}


static void
search_menu_activated (GtkMenuItem *item, gpointer data)
{
	const gchar *command = "tracker-search-tool";

	if (!g_spawn_command_line_async(command, NULL))
		g_warning("Unable to execute command: %s", command);
}


static void
pause_menu_toggled (GtkCheckMenuItem *item, gpointer data)
{
	TrayIcon *self = TRAY_ICON (data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);
	GError *error = NULL;

	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item))) {
		tracker_set_bool_option	(priv->tracker, "Pause", TRUE, &error);
	} else {
		tracker_set_bool_option	(priv->tracker, "Pause", FALSE, &error);
	}
}



static void
create_context_menu (TrayIcon *icon)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	GtkWidget *item = NULL, *image = NULL;
	priv->menu = (GtkMenu *)gtk_menu_new();

	item = (GtkWidget *)gtk_check_menu_item_new_with_mnemonic (_("_Pause Indexing"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), FALSE);
	g_signal_connect (G_OBJECT (item), "toggled", G_CALLBACK (pause_menu_toggled), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic (_("_Search..."));
	image = gtk_image_new_from_icon_name (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (search_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic (_("Pre_ferences"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (preferences_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic (_("S_tatistics"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_INFO, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (statistics_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic (_("_Quit"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_QUIT, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (quit_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	gtk_widget_show_all (GTK_WIDGET (priv->menu));
}


static gboolean
hide_window_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	TrayIcon *self = TRAY_ICON(data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE(self);

	gtk_widget_hide_all (GTK_WIDGET (priv->window));

	return FALSE;
}


static gboolean
search_cb (GtkWidget *entry,  GdkEventKey *event, gpointer data)
{
	if (event->keyval != GDK_Return) return FALSE;

	const char *text = gtk_entry_get_text (GTK_ENTRY (entry));
	gchar *command = g_strconcat ("tracker-search-tool '", text, "'", NULL);

	if (!g_spawn_command_line_async(command, NULL))
		g_warning("Unable to execute command: %s", command);

	g_free (command);

	return TRUE;
}


static void
set_progress (TrayIcon *icon, gboolean for_files, gboolean for_merging, int folders_processed, int folders_total)
{
	char *txt;
	double progress;
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	if (folders_total == 0) {
		progress = 0;
		
	} else {
		progress = (double)folders_processed / (double)folders_total;
	}

	if (!for_files) {

		if (folders_total != 0) {

			txt = g_strdup_printf ("%s - %d/%d %s", emails, folders_processed, folders_total, mail_boxes);

			gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->email_progress_bar), progress);
  			gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->email_progress_bar), txt);

			g_free (txt);

		} else {
  			gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->email_progress_bar), emails);
		}

	} else {

		if (folders_total != 0) {

			txt = g_strdup_printf ("%s - %d/%d %s", files, folders_processed, folders_total, folders);

	  		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress_bar), progress);
		  	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->progress_bar), txt);

			g_free (txt);

		} else {
		  	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->progress_bar), files);
		}
	}	


}


static void
set_tracker_icon (TrayIconPrivate *priv)
{
	char *path;
	const char *name;

	name = index_icons [priv->index_icon];

	path = g_build_filename (TRACKER_DATADIR "/tracker/icons", name, NULL);

	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		gtk_status_icon_set_from_file (priv->icon, path);
	}
	
	g_free (path);

}


static gboolean
set_icon (TrayIconPrivate *priv)
{
	
	if (priv->paused) {
		if (priv->index_icon != ICON_PAUSED) {
			priv->index_icon = ICON_PAUSED;
			set_tracker_icon (priv);		
		}
		priv->animated = FALSE;
		return FALSE;

	}

	if (priv->indexing) {

		if (priv->index_icon == ICON_INDEX2) {
			priv->index_icon = ICON_DEFAULT;
		} else if (priv->index_icon != ICON_INDEX1) {
			priv->index_icon = ICON_INDEX1;
		} else {
			priv->index_icon = ICON_INDEX2;
		}

		set_tracker_icon (priv);

		return TRUE;
	}

	
	if (priv->index_icon != ICON_DEFAULT) {
		priv->index_icon = ICON_DEFAULT;
		priv->animated = FALSE;
		set_tracker_icon (priv);
	}
	
	return FALSE;

}

static void
create_window (TrayIcon *icon)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *table;

	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	priv->window = (GtkWindow *) gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_window_set_decorated (priv->window, FALSE);
	gtk_window_stick (priv->window);
	gtk_window_set_keep_above (priv->window, TRUE);
	gtk_window_set_skip_pager_hint (priv->window, TRUE);
	gtk_window_set_skip_taskbar_hint (priv->window, TRUE);

 	g_signal_connect (priv->window, "focus-out-event",
                          G_CALLBACK (hide_window_cb),
                          icon);

	GtkWidget *frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (priv->window), frame);

	vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);

	table = gtk_table_new (3, 2, FALSE);
  	gtk_widget_show (table);
  	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 8);
  	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  	gtk_table_set_col_spacings (GTK_TABLE (table), 11);

	/* search entry row */
  	label = gtk_label_new (_("Search:"));
  	gtk_widget_show (label);
  	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                          GTK_FILL,
                          (GtkAttachOptions) (0), 0, 0);
  	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

	priv->search_entry = gtk_entry_new ();
	gtk_widget_show (priv->search_entry);
	gtk_entry_set_activates_default (GTK_ENTRY (priv->search_entry), TRUE);
	gtk_table_attach (GTK_TABLE (table), priv->search_entry, 1, 2, 0, 1,
                          (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
  
	g_signal_connect (priv->search_entry, "key-press-event",
        	            G_CALLBACK (search_cb),
        	            icon);


	/* status row */
  	label = gtk_label_new (_("Status:"));
  	gtk_widget_show (label);
  	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                         (GtkAttachOptions) (GTK_FILL),
                         (GtkAttachOptions) (0), 0, 0);
  	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
  	gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 1, 2,
                          GTK_FILL, GTK_FILL, 0, 0);

	priv->status_label = gtk_label_new ("");
	gtk_widget_show (priv->status_label);
	gtk_box_pack_start (GTK_BOX (hbox), priv->status_label, FALSE, FALSE, 0);



	/* File progress row */
  	label = gtk_label_new (_("Progress:"));
  	gtk_widget_show (label);
  	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                    	 (GtkAttachOptions) (GTK_FILL),
                         (GtkAttachOptions) (0), 0, 0);

  	priv->progress_bar = gtk_progress_bar_new ();
  	gtk_widget_show (priv->progress_bar);
  	gtk_table_attach (GTK_TABLE (table), priv->progress_bar, 1, 2, 2, 3,
                    	 (GtkAttachOptions) (GTK_FILL),
                    	 (GtkAttachOptions) (0), 0, 0);

	set_progress (icon, TRUE, FALSE, 0, 0);

	/* Email progress row */

  	priv->email_progress_bar = gtk_progress_bar_new ();
  	gtk_widget_show (priv->email_progress_bar);
  	gtk_table_attach (GTK_TABLE (table), priv->email_progress_bar, 1, 2, 3, 4,
                    	 (GtkAttachOptions) (GTK_FILL),
                    	 (GtkAttachOptions) (0), 0, 0);

	set_progress (icon, FALSE, FALSE, 0, 0);


	priv->uri_label = gtk_label_new ("");
	gtk_label_set_ellipsize (GTK_LABEL (priv->uri_label), PANGO_ELLIPSIZE_START);
  	gtk_widget_show (priv->uri_label);
  	gtk_table_attach (GTK_TABLE (table), priv->uri_label, 0, 2, 4, 5,
                          GTK_FILL,
                          (GtkAttachOptions) (0), 0, 0);
  	gtk_misc_set_alignment (GTK_MISC (priv->uri_label), 0, 0.5);
	
}


static void
show_window (GtkStatusIcon *icon, gpointer data)
{
	GdkScreen *screen;
	GdkRectangle area;
	GtkOrientation orientation;
	GtkTextDirection direction;
	GtkRequisition window_req;
	gint monitor_num;
	GdkRectangle monitor;
	gint height, width, xoffset, yoffset, x, y;

	TrayIcon *self = TRAY_ICON(data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE(self);

	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (priv->window))) {
		gtk_widget_hide_all (GTK_WIDGET (priv->window));
		return;
	}
	
	gtk_status_icon_get_geometry (priv->icon, &screen, &area, &orientation);

	direction = gtk_widget_get_direction (GTK_WIDGET (priv->window));

	gtk_window_set_screen (GTK_WINDOW (priv->window), screen);

	monitor_num = gdk_screen_get_monitor_at_point (screen, area.x, area.y);

	if (monitor_num < 0)
		monitor_num = 0;

	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	x = area.x;
	y = area.y;
	gtk_widget_size_request (GTK_WIDGET (priv->window), &window_req);

	if (orientation == GTK_ORIENTATION_VERTICAL) {
		width = 0;
		height = area.height;
		xoffset = area.width;
		yoffset = 0;
	} else {
		width = area.width;
		height = 0;
		xoffset = 0;
		yoffset = area.height;
	}

	if (direction == GTK_TEXT_DIR_RTL) {
		if ((x - (window_req.width - width)) >= monitor.x)
			x -= window_req.width - width;
		else if ((x + xoffset + window_req.width) < (monitor.x + monitor.width))
			x += xoffset;
		else if ((monitor.x + monitor.width - (x + xoffset)) < x)
			x -= window_req.width - width;
		else
			x += xoffset;
	} else {
		if ((x + xoffset + window_req.width) < (monitor.x + monitor.width))
			x += xoffset;
		else if ((x - (window_req.width - width)) >= monitor.x)
			x -= window_req.width - width;
		else if ((monitor.x + monitor.width - (x + xoffset)) > x)
			x += xoffset;
		else
			x -= window_req.width - width;
	}

	if ((y + yoffset + window_req.height) < (monitor.y + monitor.height))
		y += yoffset;
	else if ((y - (window_req.height - height)) >= monitor.y)
		y -= window_req.height - height;
	else if (monitor.y + monitor.height - (y + yoffset) > y)
		y += yoffset;
	else
		y -= window_req.height - height;

	gtk_window_move (GTK_WINDOW (priv->window), x, y);

	gtk_widget_show_all (GTK_WIDGET (priv->window));

	gtk_window_activate_default  (priv->window);
}

static void
index_finished (DBusGProxy *proxy,  int time_taken, TrayIcon *self)
{
	char *format;

	int hours = time_taken/3600;

	int minutes = (time_taken/60 - (hours * 60));

	int seconds = (time_taken - ((minutes * 60) + (hours * 3600)));

	if (hours > 0) {
		format = g_strdup_printf (end_index_hours_msg, hours, minutes);
	} else if (minutes > 0) {
		format = g_strdup_printf (end_index_minutes_msg, minutes, seconds);
	} else {
		format = g_strdup_printf (end_index_seconds_msg, seconds);		
	}

	tray_icon_show_message (self, "%s%s\n\n%s", end_index_initial_msg, format, end_index_final_msg);
	g_free (format);
}


static void
index_state_changed (DBusGProxy *proxy, const gchar *state, gboolean initial_index, gboolean in_merge, gboolean is_manual_paused, gboolean is_battery_paused, gboolean is_io_paused, TrayIcon *self)
{


	if (!state) return;

	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);

	if (!priv->initial_index_msg_shown && initial_index) {
		priv->initial_index_msg_shown = TRUE;
		tray_icon_show_message (self, "%s\n\n%s\n", initial_index_1, initial_index_2); 

	}


	if (is_manual_paused) {
		gtk_label_set_text  (GTK_LABEL (priv->status_label), status_paused);
		priv->paused = TRUE;
		priv->animated = FALSE;
		set_icon (priv); 		
		return;
	} 

	if (is_battery_paused) {
		gtk_label_set_text  (GTK_LABEL (priv->status_label), status_battery_paused);  	
		priv->paused = TRUE;
		priv->animated = FALSE;
		set_icon (priv); 	
	
		return;
	}

	if (is_io_paused) {
		gtk_label_set_text  (GTK_LABEL (priv->status_label), status_paused_io);  		
		priv->paused = TRUE;
		priv->animated = FALSE;
		set_icon (priv); 		
		return;
	}

	priv->paused = FALSE;

	if (in_merge) {
		tray_icon_show_message (self, start_merge_msg); 
		gtk_label_set_text  (GTK_LABEL (priv->status_label), status_merge); 
		priv->indexing = TRUE;
		set_icon (priv); 		
		if (!priv->animated) {
			priv->animated = TRUE;
			g_timeout_add (1000, (GSourceFunc) set_icon, priv);
		}

		return; 						
	}
	
	if (strcasecmp (state, "Idle") == 0) {
		gtk_label_set_text  (GTK_LABEL (priv->status_label), status_idle);  
		priv->indexing = FALSE;
		priv->animated = FALSE;
		set_icon (priv);		
	} else { 
		gtk_label_set_text  (GTK_LABEL (priv->status_label), status_indexing);  		
		priv->indexing = TRUE;
		set_icon (priv); 		
		if (!priv->animated) {
			priv->animated = TRUE;
			g_timeout_add (1000, (GSourceFunc) set_icon, priv);
		}
	}


}

static void
index_progress_changed (DBusGProxy *proxy, const gchar *service, const char *uri, int index_count, int folders_processed, int folders_total,  TrayIcon *self)
{

	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);

	if (!service) return;

	if (folders_processed > folders_total) folders_processed = folders_total;

	if (strcmp (service, "Emails") == 0) {
		set_progress (self, FALSE, FALSE, folders_processed, folders_total);
	} else {
		set_progress (self, TRUE, FALSE, folders_processed, folders_total);
	}

	gtk_label_set_text  (GTK_LABEL (priv->uri_label), uri);  		  		

	/* update stat window if its active */
	refresh_stats (self);

}

static void
name_owner_changed (DBusGProxy * proxy, const gchar * name,
		    const gchar * prev_owner, const gchar * new_owner,
		    gpointer data)
{

	
	if (!g_str_equal (name, DBUS_SERVICE_TRACKER)) return;

	if (g_str_equal (new_owner, "")) {

	
		/* tracker has exited so reset status */
		index_state_changed (proxy, "Idle", FALSE, FALSE, FALSE, FALSE, TRUE, data);
		

	}
}



static gboolean
setup_dbus_connection (TrayIcon *self)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);

	priv->tracker = tracker_connect (FALSE);

	if (!priv->tracker) {
		g_print ("Could not initialise Tracker\n");
		exit (1);
	}


	/* set signal handlers */
	dbus_g_object_register_marshaller (tracker_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN,
 					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,  G_TYPE_INVALID);

	dbus_g_object_register_marshaller (tracker_VOID__STRING_STRING_INT_INT_INT,
 					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID);

   	dbus_g_proxy_add_signal (priv->tracker->proxy,
                           "IndexStateChange",
                           G_TYPE_STRING,
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
                           G_TYPE_INVALID);

	dbus_g_proxy_add_signal (priv->tracker->proxy,
                           "IndexFinished",
                           G_TYPE_INT,
                           G_TYPE_INVALID);

   	dbus_g_proxy_connect_signal (priv->tracker->proxy,
				"IndexStateChange",
                               	G_CALLBACK (index_state_changed),
                               	self,
                               	NULL);

   	dbus_g_proxy_connect_signal (priv->tracker->proxy,
				"IndexProgress",
                               	G_CALLBACK (index_progress_changed),
                               	self,
                               	NULL);

   	dbus_g_proxy_connect_signal (priv->tracker->proxy,
				"IndexFinished",
                               	G_CALLBACK (index_finished),
                               	self,
                               	NULL);

	DBusGConnection *connection;
	DBusGProxy *dbus_proxy;

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
				     self, NULL);


	/* prompt for updated signals */
	dbus_g_proxy_begin_call (priv->tracker->proxy, "PromptIndexSignals", NULL, NULL, NULL, G_TYPE_INVALID);

	gtk_status_icon_set_visible (priv->icon, TRUE);

	return FALSE;
}


static void
tray_icon_init (GTypeInstance *instance, gpointer g_class)
{
	TrayIcon *self = TRAY_ICON (instance);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);


	priv->icon = gtk_status_icon_new ();
	priv->indexing = FALSE;
	priv->paused = FALSE;
	priv->index_icon = ICON_DEFAULT;
	priv->animated = FALSE;

	set_tracker_icon (priv);
	
	gtk_status_icon_set_visible (priv->icon, TRUE);

	g_signal_connect(G_OBJECT(priv->icon), "activate", G_CALLBACK (show_window), instance);
	g_signal_connect(G_OBJECT(priv->icon), "popup-menu", G_CALLBACK (tray_icon_clicked), instance);

	priv->initial_index_msg_shown = FALSE;
	
	priv->stat_window_active = FALSE;
	priv->stat_request_pending = FALSE;

	/* build popup window */
	create_window (self);

	/* build context menu */
	create_context_menu (self);

	gtk_status_icon_set_visible (priv->icon, FALSE);

	g_timeout_add (2000,(GSourceFunc) setup_dbus_connection, self);

}



void
tray_icon_set_tooltip (TrayIcon *icon, const gchar *format, ...)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	gchar *tooltip = NULL;
	va_list args;

	va_start(args, format);
	tooltip = g_strdup_vprintf(format, args);
	va_end(args);

	gtk_status_icon_set_tooltip (priv->icon, tooltip);

	g_free(tooltip);
}

void 
tray_icon_show_message (TrayIcon *icon, const char *message, ...)
{
	va_list args;
   	gchar *msg = NULL;
	NotifyNotification *notification = NULL;
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
   
   	va_start (args, message);
   	msg = g_strdup_vprintf (message, args);
   	va_end (args);

	notification = notify_notification_new_with_status_icon (tracker_title, msg, NULL, priv->icon);

	notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);

	notify_notification_show (notification, NULL);

	g_object_unref (notification);

	g_free (msg);
}


static void
tray_icon_clicked (GtkStatusIcon *icon, guint button, guint timestamp, gpointer data)
{
	TrayIcon *self = TRAY_ICON (data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);

	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (priv->window))) {
		gtk_widget_hide_all (GTK_WIDGET (priv->window));
	}
  
	gtk_menu_popup (GTK_MENU (priv->menu), NULL, NULL, gtk_status_icon_position_menu, icon, button, timestamp);
}



static void
preferences_menu_activated (GtkMenuItem *item, gpointer data)
{
	const gchar *command = "tracker-preferences";

	if (!g_spawn_command_line_async (command, NULL))
		g_warning ("Unable to execute command: %s", command);
}



static gchar *
get_stat_value (gchar ***stat_array, const gchar *stat) 
{
	gchar **array;
	gint i = 0;

	while (stat_array[i][0]) {

		array = stat_array[i];

		if (array[0] && strcasecmp (stat, array[0]) == 0) {
			return array[1];
		}

		i++;
	}

	return NULL;
}

static void
stat_window_free (GtkWidget *widget, gint arg ,gpointer data)
{
	TrayIcon *self = TRAY_ICON (data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);

	priv->stat_window_active = FALSE;

	gtk_widget_destroy (widget);
}


static void
update_stats  (GPtrArray *array,
	       GError *error,
	       gpointer data)

{
	TrayIcon *self = TRAY_ICON (data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);

	if (error) {
		g_warning ("an error has occured: %s",  error->message);
		g_error_free (error);
		priv->stat_request_pending = FALSE;
		return;
	}

	if (!array) {
                return;
        }

	guint i = array->len;
	

	if (i < 1 || !priv->stat_window_active) {
		g_ptr_array_free (array, TRUE);
		return;
	}

	gchar ***pdata = (gchar ***) array->pdata;
	
	for (i=0; i<12; i++) {
		 gtk_label_set_text  (GTK_LABEL (stat_info[i].stat_label), get_stat_value (pdata, stat_info[i].name));  
	}

	g_ptr_array_free (array, TRUE);

	priv->stat_request_pending = FALSE;

}



static void
refresh_stats (TrayIcon *self)
{
	
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);

	if (!priv->stat_window_active || priv->stat_request_pending) {
		return;
	}

	priv->stat_request_pending = TRUE;
	
	tracker_get_stats_async (priv->tracker, (TrackerGPtrArrayReply) update_stats, self);

}


static void
statistics_menu_activated (GtkMenuItem *item, gpointer data)
{
	TrayIcon *self = TRAY_ICON (data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);
	int i;

	GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Statistics"),
                                                         GTK_WINDOW (priv->window),
                                                         GTK_DIALOG_NO_SEPARATOR | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_STOCK_CLOSE,
                                                         GTK_RESPONSE_CLOSE,
                                                         NULL);

        gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
 	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
 	gtk_window_set_icon_name (GTK_WINDOW (dialog), "gtk-info");
 	gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
 	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	

        GtkWidget *table = gtk_table_new (13, 2, TRUE) ;
        gtk_table_set_row_spacings (GTK_TABLE (table), 4);
        gtk_table_set_col_spacings (GTK_TABLE (table), 65);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);

        GtkWidget *title_label = gtk_label_new (NULL);
        gtk_label_set_markup (GTK_LABEL (title_label), _("<span weight=\"bold\" size=\"larger\">Index statistics</span>"));
        gtk_misc_set_alignment (GTK_MISC (title_label), 0, 0);
        gtk_table_attach_defaults (GTK_TABLE (table), title_label, 0, 2, 0, 1) ;
         
	for (i=0; i<12; i++) {
         	                                                                        
               	GtkWidget *label_to_add = gtk_label_new (stat_info[i].label);		   
                              
                gtk_label_set_selectable (GTK_LABEL (label_to_add), TRUE);                              
       	        gtk_misc_set_alignment (GTK_MISC (label_to_add), 0, 0);                                 
       	        gtk_table_attach_defaults (GTK_TABLE (table), label_to_add, 0, 1, i+1, i+2); 

       	        stat_info[i].stat_label = gtk_label_new ("") ;                                        

       	        gtk_label_set_selectable (GTK_LABEL (stat_info[i].stat_label), TRUE);                               
       	        gtk_misc_set_alignment (GTK_MISC (stat_info[i].stat_label), 0, 0);                                  
       	        gtk_table_attach_defaults (GTK_TABLE (table), stat_info[i].stat_label, 1, 2,  i+1, i+2);   

	}

	priv->stat_window_active = TRUE;

	refresh_stats (self);

        GtkWidget *dialog_hbox = gtk_hbox_new (FALSE, 12);
        GtkWidget *info_icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
        gtk_misc_set_alignment (GTK_MISC (info_icon), 0, 0);
        gtk_container_add (GTK_CONTAINER (dialog_hbox), info_icon);
        gtk_container_add (GTK_CONTAINER (dialog_hbox), table);

        gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), dialog_hbox);

	g_signal_connect (G_OBJECT (dialog),
	                  "response",
                          G_CALLBACK (stat_window_free), self);

	gtk_widget_show_all (dialog);

	
}


static void
quit_menu_activated (GtkMenuItem *item, gpointer data)
{
	gtk_main_quit();
}

GType
tray_icon_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (TrayIconClass),
			NULL,                                   /* base_init */
			NULL,                                   /* base_finalize */
			(GClassInitFunc) tray_icon_class_init,  /* class_init */
			NULL,                                   /* class_finalize */
			NULL,                                   /* class_data */
			sizeof (TrayIcon),
			0,                                      /* n_preallocs */
			tray_icon_init                          /* instance_init */
		};

		type = g_type_register_static (G_TYPE_OBJECT, "TrayIconType", &info, 0);
	}

	return type;
}


int
main (int argc, char *argv[])
{

	bindtextdomain (GETTEXT_PACKAGE, TRACKER_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	if (!notify_is_initted () && !notify_init (PROGRAM_NAME)) {
      		g_warning ("failed: notify_init()\n");
      		return EXIT_FAILURE;
   	}

	/* set translatable strings here */

	files = _("Files");
	folders = _("folders");

	emails = _("Emails");

	mail_boxes = _("mail boxes");

	status_indexing = _("Indexing in progress");
	status_idle = _("Indexing completed");
	status_merge = _("Indexes are being merged");

	status_paused = _("Paused by user");
	status_battery_paused = _("Paused while on battery power");
	status_paused_io = _("Paused temporarily");
	
	initial_index_1 = _("Your computer is about to be indexed so you can perform fast searches of your files and emails");
	initial_index_2 = _("You can pause indexing at any time and configure index settings by right clicking here");

	end_index_initial_msg = _("Tracker has finished indexing your system");
	end_index_hours_msg = _(" in %d hours and %d minutes");
	end_index_minutes_msg = _(" in %d minutes and %d seconds");
	end_index_seconds_msg = _(" in %d seconds");
	end_index_final_msg = _("You can now perform searches by clicking here");

	start_merge_msg = _("Tracker is now merging indexes which can degrade system performance for serveral minutes\n\nYou can pause this by right clicking here");

	tracker_title = _("Tracker search and indexing service");

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

   	main_icon = g_object_new (TYPE_TRAY_ICON, NULL);

	tray_icon_set_tooltip (main_icon, tracker_title);
	
   	gtk_main ();

   	notify_uninit();

   	return EXIT_SUCCESS;
}
