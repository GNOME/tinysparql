
#include <stdlib.h>
#include <strings.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "tm-tray-icon.h"
#include "tm-tray-icon-private.h"

/* translatable strings */

static char *files;
static char *folders;
static char *conversations;
static char *emails;

static char *mail_boxes;

static char *status_indexing;
static char *status_idle;
static char *status_startup;

static char *status_paused;
static char *status_battery_paused;

static char *initial_index_1;
static char *initial_index_2;
static char *initial_index_3;

static char *end_index_msg;


static void
set_translatable_strings (void)
{
	files = _("Files");
	folders = _("folders");
	conversations = _("Conversations");
	emails = _("Emails");

	mail_boxes = _("mail boxes");

	status_indexing = _("Indexing");
	status_idle = _("Idle");
	status_startup = _("Initializing");
	status_paused = _("(paused)");
	status_battery_paused = _("(battery paused)");
	
	initial_index_1 = _("Tracker will shortly be indexing your system");
	initial_index_2 = _("Indexing may affect the performance of your computer");
	initial_index_3 = _("You can pause indexing at any time and configure index settings by right clicking here");

	end_index_msg = _("Tracker has finished indexing your system. You can now perform searches by clicking here");

}


static void
tray_icon_class_init (TrayIconClass *klass)
{
	GParamSpec *spec = NULL;
	GObjectClass *g_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private (klass, sizeof(TrayIconPrivate));

	/* Methods */
	klass->set_tooltip = _set_tooltip;

	g_class->set_property = tray_icon_set_property;

	/* Properties */
	spec = g_param_spec_object ("connection", NULL, NULL,
				    TYPE_TRACKERD_CONNECTION,
				    G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

	g_object_class_install_property (g_class, PROP_TRACKERD_CONNECTION, spec);
}


static void
search_menu_activated (GtkMenuItem *item, gpointer data)
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

	item = (GtkWidget *)gtk_check_menu_item_new_with_mnemonic ("_Pause indexing");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM (item), FALSE);
	g_signal_connect (G_OBJECT (item), "toggled", G_CALLBACK (active_menu_toggled), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic ("_Search...");
	image = gtk_image_new_from_icon_name (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (search_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic ("Pre_ferences...");
	image = gtk_image_new_from_icon_name (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (preferences_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic ("S_tatistics...");
	image = gtk_image_new_from_icon_name (GTK_STOCK_INFO, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (statistics_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);


	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic ("_Re-index system");
	image = gtk_image_new_from_icon_name (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (preferences_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic ("_Quit");
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

	priv->status_label = gtk_label_new ("Indexing files");
	gtk_widget_show (priv->status_label);
	gtk_box_pack_start (GTK_BOX (hbox), priv->status_label, FALSE, FALSE, 0);



	/* progress row */
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
  	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress_bar), 0.35);
  	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->progress_bar), _("Files - 256/841 folders"));
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
tray_icon_init (GTypeInstance *instance, gpointer g_class)
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
tray_icon_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
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
tray_icon_set_tooltip (TrayIcon *icon, const gchar *format, ...)
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
active_menu_toggled (GtkCheckMenuItem *item, gpointer data)
{
	TrayIcon *self = TRAY_ICON (data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);

	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM(item)))
		trackerd_connection_start (priv->trackerd);
	else
		trackerd_connection_stop (priv->trackerd);
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
statistics_menu_activated (GtkMenuItem *item, gpointer data)
{
	TrayIcon *self = TRAY_ICON (data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);

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

	GPtrArray *array = trackerd_connection_statistics (priv->trackerd);
	if (!array) {
                return;
        }

	guint i = array->len;
	gchar ***pdata = (gchar ***) array->pdata;

	if (i < 1) {
		g_ptr_array_free (array, TRUE);
		return;
	}

        GtkWidget *table = gtk_table_new (13, 2, TRUE) ;
        gtk_table_set_row_spacings (GTK_TABLE (table), 4);
        gtk_table_set_col_spacings (GTK_TABLE (table), 65);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);

        GtkWidget *title_label = gtk_label_new (NULL);
        gtk_label_set_markup (GTK_LABEL (title_label), _("<span weight=\"bold\" size=\"larger\">Index statistics</span>"));
        gtk_misc_set_alignment (GTK_MISC (title_label), 0, 0);
        gtk_table_attach_defaults (GTK_TABLE (table), title_label, 0, 2, 0, 1) ;



        #define ADD_ENTRY_IN_TABLE(Name, PrintedName, Value, LineNo)                                    \
          stat_value = get_stat_value (pdata, Name);                                                    \
          if (stat_value) {                                                                             \
                GtkWidget *label_to_add = gtk_label_new (PrintedName) ;                                 \
                gtk_label_set_selectable (GTK_LABEL (label_to_add), TRUE);                              \
                gtk_misc_set_alignment (GTK_MISC (label_to_add), 0, 0);                                 \
                gtk_table_attach_defaults (GTK_TABLE (table), label_to_add, 0, 1, LineNo, LineNo + 1) ; \
                GtkWidget *value_label = gtk_label_new (Value) ;                                        \
                gtk_label_set_selectable (GTK_LABEL (value_label), TRUE);                               \
                gtk_misc_set_alignment (GTK_MISC (value_label), 0, 0);                                  \
                gtk_table_attach_defaults (GTK_TABLE (table), value_label, 1, 2, LineNo, LineNo + 1);   \
          }

	gchar *stat_value;

        ADD_ENTRY_IN_TABLE ("Files", _("Files:"), stat_value, 1) ;
        ADD_ENTRY_IN_TABLE ("Folders", _("    Folders:"), stat_value, 2);
        ADD_ENTRY_IN_TABLE ("Documents", _("    Documents:"), stat_value, 3) ;
        ADD_ENTRY_IN_TABLE ("Images", _("    Images:"), stat_value, 4) ;
        ADD_ENTRY_IN_TABLE ("Music", _("    Music:"), stat_value, 5) ;
        ADD_ENTRY_IN_TABLE ("Videos", _("    Videos:"), stat_value, 6) ;
        ADD_ENTRY_IN_TABLE ("Text", _("    Text:"), stat_value, 7) ;
        ADD_ENTRY_IN_TABLE ("Development", _("    Development:"), stat_value, 8) ;
        ADD_ENTRY_IN_TABLE ("Other", _("    Other:"), stat_value, 9) ;
        ADD_ENTRY_IN_TABLE ("Applications", _("Applications:"), stat_value, 10) ;
        ADD_ENTRY_IN_TABLE ("Conversations", _("Conversations:"), stat_value, 11) ;
        ADD_ENTRY_IN_TABLE ("Emails", _("Emails:"), stat_value, 12) ;

        #undef ADD_ENTRY_IN_TABLE
	
	g_ptr_array_free (array, TRUE);

        GtkWidget *dialog_hbox = gtk_hbox_new (FALSE, 12);
        GtkWidget *info_icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
        gtk_misc_set_alignment (GTK_MISC (info_icon), 0, 0);
        gtk_container_add (GTK_CONTAINER (dialog_hbox), info_icon);
        gtk_container_add (GTK_CONTAINER (dialog_hbox), table);

        gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), dialog_hbox);

	g_signal_connect (G_OBJECT (dialog),
	                  "response",
                          G_CALLBACK (gtk_widget_destroy), NULL);

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
