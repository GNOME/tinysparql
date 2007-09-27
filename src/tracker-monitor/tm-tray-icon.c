
#include <stdlib.h>
#include <strings.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "tm-tray-icon.h"
#include "tm-tray-icon-private.h"

/* transalatable strings */

static char *files;
static char *folders;
static char *docs;
static char *images;
static char *music;
static char *videos;
static char *text_files;
static char *dev_files;
static char *files_other;

static char *conversations;
static char *emails;
static char *apps;

static char *mail_boxes;
static char *of;

static char *status_indexing;
static char *status_idle;
static char *status_startup;

static char *initial_index_1;
static char *initial_index_2;
static char *initial_index_3;

static char *end_index_msg;


static void
set_translatable_strings ()
{
	files = _("Files");
	folders = _("Folders");
	docs = _("Documents");
	images = _("Images");
	music = _("Music");
	videos = _("Videos");
	text_files = _("Text");
	dev_files = _("Development");
	files_other = _("Other");

	conversations = _("Conversations");
	emails = _("Emails");
	apps = _("Applications");

	mail_boxes = _("mail boxes");
	of = _(" of ");

	status_indexing = _("Indexing");
	status_idle = _("Idle");
	status_startup = _("Initializing");
	
	initial_index_1 = _("Tracker will shortly be indexing your system");
	initial_index_2 = _("Indexing may affect the performance of your computer");
	initial_index_3 = _("You can pause indexing at any time and configure index settings by right clicking here");

	end_index_msg = _("Tracker has finished indexing your system. You can now perform searches by clicking here");

}




static void
tray_icon_class_init(TrayIconClass *klass)
{
	GParamSpec *spec = NULL;
	GObjectClass *g_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(TrayIconPrivate));

	/* Methods */
	klass->set_tooltip = _set_tooltip;

	g_class->set_property = tray_icon_set_property;

	/* Properties */
	spec = g_param_spec_object("connection", NULL, NULL,
				    TYPE_TRACKERD_CONNECTION,
				    G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

	g_object_class_install_property(g_class, PROP_TRACKERD_CONNECTION, spec);
}


static void
search_menu_activated(GtkMenuItem *item, gpointer data)
{
	const gchar *command = "tracker-search-tool";

	if (!g_spawn_command_line_async(command, NULL))
		g_warning("Unable to execute command: %s", command);
}


static void
create_context_menu (TrayIcon *icon)
{

	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	GtkWidget *item = NULL, *image = NULL;
	priv->menu = (GtkMenu *)gtk_menu_new();

	item = (GtkWidget *)gtk_check_menu_item_new_with_mnemonic("_Pause indexing");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
	g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(active_menu_toggled), icon);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->menu), item);

	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic("_Search...");
	image = gtk_image_new_from_icon_name(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(search_menu_activated), icon);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic("Pre_ferences...");
	image = gtk_image_new_from_icon_name(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(preferences_menu_activated), icon);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic("S_tatistics...");
	image = gtk_image_new_from_icon_name(GTK_STOCK_INFO, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(statistics_menu_activated), icon);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->menu), item);


	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic("_Re-index system");
	image = gtk_image_new_from_icon_name(GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(preferences_menu_activated), icon);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->menu), item);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL(priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic("_Quit");
	image = gtk_image_new_from_icon_name(GTK_STOCK_QUIT, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(quit_menu_activated), icon);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->menu), item);

	gtk_widget_show_all (GTK_WIDGET(priv->menu));

	
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
create_window (TrayIcon *icon)
{
	GtkWidget *vbox1;
	GtkWidget *hbox1;
	GtkWidget *label2;
	GtkWidget *hbox2;
	GtkWidget *vbox3;

	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	priv->window = (GtkWindow *) gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_window_set_decorated (priv->window, FALSE);
	gtk_window_stick (priv->window);
	gtk_window_set_keep_above (priv->window, TRUE);
	gtk_window_set_skip_pager_hint (priv->window, TRUE);
	gtk_window_set_skip_taskbar_hint (priv->window, TRUE);
	//gtk_window_set_type_hint (priv->window, GDK_WINDOW_TYPE_HINT_MENU);

 	g_signal_connect (priv->window, "focus-out-event",
        	            G_CALLBACK (hide_window_cb),
        	            icon);

	GtkWidget *frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (priv->window), frame);

	vbox1 = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (frame), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 4);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 4);

	label2 = gtk_label_new (_("Search"));
	gtk_widget_show (label2);
	gtk_box_pack_start (GTK_BOX (hbox1), label2, FALSE, FALSE, 0);

	priv->search_entry = gtk_entry_new ();
	gtk_widget_show (priv->search_entry);
	gtk_entry_set_activates_default (GTK_ENTRY (priv->search_entry), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox1), priv->search_entry, TRUE, TRUE, 0);

	g_signal_connect (priv->search_entry, "key-press-event",
        	            G_CALLBACK (search_cb),
        	            icon);

	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox2, FALSE, FALSE, 2);

	priv->status_label = gtk_label_new (_("Status: Indexing"));
	gtk_widget_show (priv->status_label);
	gtk_box_pack_start (GTK_BOX (hbox2), priv->status_label, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (priv->status_label), GTK_JUSTIFY_CENTER);

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox3, TRUE, TRUE, 6);

	priv->files_bar = gtk_progress_bar_new ();
	gtk_widget_show (priv->files_bar);
	gtk_box_pack_start (GTK_BOX (vbox3), priv->files_bar, FALSE, FALSE, 0);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->files_bar), _("Files"));

	priv->conversations_bar = gtk_progress_bar_new ();
	gtk_widget_show (priv->conversations_bar);
	gtk_box_pack_start (GTK_BOX (vbox3), priv->conversations_bar, FALSE, FALSE, 0);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->conversations_bar), _("Conversations"));

	priv->emails_bar = gtk_progress_bar_new ();
	gtk_widget_show (priv->emails_bar);
	gtk_box_pack_start (GTK_BOX (vbox3), priv->emails_bar, FALSE, FALSE, 0);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->emails_bar), _("Emails"));


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
tray_icon_init(GTypeInstance *instance, gpointer g_class)
{
	TrayIcon *self = TRAY_ICON(instance);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE(self);

	priv->icon = gtk_status_icon_new_from_icon_name (TRACKER_ICON);
	gtk_status_icon_set_visible(priv->icon, TRUE);

	g_signal_connect(G_OBJECT(priv->icon), "activate", G_CALLBACK (show_window), instance);
	g_signal_connect(G_OBJECT(priv->icon), "popup-menu", G_CALLBACK (tray_icon_clicked), instance);

	set_translatable_strings ();

	/* build popup window */
	create_window (self);

	/* build context menu */
	create_context_menu (self);

}

static void
tray_icon_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	TrayIcon *self = TRAY_ICON(object);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE(self);

	switch (property_id) {
		case PROP_TRACKERD_CONNECTION:
			priv->trackerd = TRACKERD_CONNECTION(g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}

void
tray_icon_set_tooltip(TrayIcon *icon, const gchar *format, ...)
{
	va_list args;
	gchar *tooltip = NULL;

	va_start(args, format);
	tooltip = g_strdup_vprintf(format, args);
	va_end(args);

	TRAY_ICON_GET_CLASS(icon)->set_tooltip(icon, tooltip);

	g_free(tooltip);
}

void 
tray_icon_show_message (TrayIcon *icon, const char *msg)
{
	NotifyNotification *notification = NULL;
	
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	gtk_status_icon_set_blinking (priv->icon, TRUE);

	notification = notify_notification_new_with_status_icon ("Tracker search and indexing service", msg, NULL, priv->icon);

	notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);

	notify_notification_show (notification, NULL);

	g_object_unref (notification);

}


static void
_set_tooltip (TrayIcon *icon, const gchar *tooltip)
{
	TrayIcon *self = TRAY_ICON (icon);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);

	gtk_status_icon_set_tooltip (priv->icon, tooltip);
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
active_menu_toggled(GtkCheckMenuItem *item, gpointer data)
{
	TrayIcon *self = TRAY_ICON(data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE(self);

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
		trackerd_connection_start(priv->trackerd);
	else
		trackerd_connection_stop(priv->trackerd);
}

static void
preferences_menu_activated(GtkMenuItem *item, gpointer data)
{
	const gchar *command = "tracker-preferences";

	if (!g_spawn_command_line_async(command, NULL))
		g_warning("Unable to execute command: %s", command);
}


static char *
get_stat_value (char ***stat_array, const char *stat) 
{
	char **array;
	int i=0;

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
statistics_menu_activated (GtkMenuItem *item, gpointer data)
{
	TrayIcon *self = TRAY_ICON (data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);

	GtkWidget * dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (priv->window),
 	                                 GTK_DIALOG_DESTROY_WITH_PARENT,
	                                 GTK_MESSAGE_INFO,
	                                 GTK_BUTTONS_OK,
	                                 _("Index statistics"));

	GPtrArray *array = trackerd_connection_statistics (priv->trackerd);

	if (!array) return;

	guint i = array->len;
	GString *string = NULL;
	gchar *statistics = NULL;
	gchar ***pdata = (gchar ***) array->pdata;

	if (i < 1) {
		g_ptr_array_free (array, TRUE);
		return;
	}

	string = g_string_new ("");
	

	char *stat_value;

	stat_value = get_stat_value (pdata, "Files");
	if (stat_value) {
		g_string_append_printf (string, "%s\t\t\t\t\t\t%d\n", files, atoi(stat_value));
	}

	stat_value = get_stat_value (pdata,"Folders");
	if (stat_value) {
		g_string_append_printf (string, "  %s\t\t\t\t\t%d\n", folders, atoi(stat_value));
	}

	stat_value = get_stat_value (pdata,"Documents");
	if (stat_value) {
		g_string_append_printf (string, "  %s\t\t\t\t%d\n", docs, atoi(stat_value));
	}

	stat_value = get_stat_value (pdata,"Images");
	if (stat_value) {
		g_string_append_printf (string, "  %s\t\t\t\t\t%d\n", images, atoi(stat_value));
	}

	stat_value = get_stat_value (pdata,"Music");
	if (stat_value) {
		g_string_append_printf (string, "  %s\t\t\t\t\t\t%d\n", music, atoi(stat_value));
	}

	stat_value = get_stat_value (pdata,"Videos");
	if (stat_value) {
		g_string_append_printf (string, "  %s\t\t\t\t\t\t%d\n", videos, atoi(stat_value));
	}

	stat_value = get_stat_value (pdata,"Text");
	if (stat_value) {
		g_string_append_printf (string, "  %s\t\t\t\t\t\t%d\n", text_files, atoi(stat_value));
	}

	stat_value = get_stat_value (pdata,"Development");
	if (stat_value) {
		g_string_append_printf (string, "  %s\t\t\t\t%d\n", dev_files, atoi(stat_value));
	}

	stat_value = get_stat_value (pdata,"Other");
	if (stat_value) {
		g_string_append_printf (string, "  %s\t\t\t\t\t\t%d\n", files_other, atoi(stat_value));
	}

	stat_value = get_stat_value (pdata,"Applications");
	if (stat_value) {
		g_string_append_printf (string, "%s\t\t\t\t\t%d\n", apps, atoi(stat_value));
	}

	stat_value = get_stat_value (pdata,"Conversations");
	if (stat_value) {
		g_string_append_printf (string, "%s\t\t\t\t%d\n", conversations, atoi(stat_value));
	}

	stat_value = get_stat_value (pdata,"Emails");
	if (stat_value) {
		g_string_append_printf (string, "%s\t\t\t\t\t\t%d\n", emails, atoi(stat_value));
	}


	statistics  = g_string_free (string, FALSE);
	
	g_ptr_array_free (array, TRUE);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
	                                          statistics);

	gtk_window_set_title (GTK_WINDOW (dialog), "Statistics");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	g_signal_connect (G_OBJECT (dialog),
	                  "response",
	                   G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (dialog);


	g_free (statistics);
}


static void
quit_menu_activated(GtkMenuItem *item, gpointer data)
{
	gtk_main_quit();
}

GType
tray_icon_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(TrayIconClass),
			NULL,											 /* base_init */
			NULL,											 /* base_finalize */
			(GClassInitFunc)tray_icon_class_init,  /* class_init */
			NULL,											 /* class_finalize */
			NULL,											 /* class_data */
			sizeof(TrayIcon),
			0,												 /* n_preallocs */
			tray_icon_init								 /* instance_init */
		};

		type = g_type_register_static(G_TYPE_OBJECT, "TrayIconType", &info, 0);
	}

	return type;
}
