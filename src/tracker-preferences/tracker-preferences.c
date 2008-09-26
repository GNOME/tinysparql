/* Tracker - indexer and metadata database engine
 * Copyright (C) 2007, Saleem Abdulrasool (compnerd@gentoo.org)
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

#include <string.h>
#include <glib/gi18n.h>
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <stdlib.h>

#include "../libtracker/tracker.h"
/* #include "../trackerd/tracker-dbus.h" */
#include "../libtracker-common/tracker-configuration.h"

#include "tracker-preferences.h"
#include "tracker-preferences-dialogs.h"

#define TRACKER_DBUS_SERVICE   "org.freedesktop.Tracker"
#define TRACKER_DBUS_PATH      "/org/freedesktop/Tracker"
#define TRACKER_DBUS_INTERFACE "org.freedesktop.Tracker"

#define TRACKER_PREFERENCES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TRACKER_TYPE_PREFERENCES, TrackerPreferencesPrivate))

typedef struct _TrackerPreferencesPrivate {
	GladeXML *gxml;
	DBusGConnection *connection;
	DBusGProxy *dbus_proxy;
	DBusGProxy *tracker_proxy;
} TrackerPreferencesPrivate;

static void tracker_preferences_class_init (TrackerPreferencesClass *klass);
static void tracker_preferences_init (GTypeInstance *instance, gpointer g_class);
static void tracker_preferences_finalize (GObject *object);
static void setup_page_general (TrackerPreferences *preferences);
static void setup_page_files (TrackerPreferences *preferences);
static void setup_page_emails (TrackerPreferences *preferences);
static void setup_page_ignored_files (TrackerPreferences *preferences);
static void setup_page_performance (TrackerPreferences *preferences);
static void tracker_preferences_cmd_quit (GtkWidget *widget, GdkEvent *event, gpointer data);
static void tracker_preferences_cmd_help (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_apply (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_cancel (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_ok (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_add_index_path (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_remove_index_path (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_add_crawled_path (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_remove_crawled_path (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_add_index_mailbox (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_remove_index_mailbox (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_add_ignore_path (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_remove_ignore_path (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_add_ignore_pattern (GtkWidget *widget, gpointer data);
static void tracker_preferences_cmd_remove_ignore_pattern (GtkWidget *widget, gpointer data);
static void append_item_to_list (TrackerPreferences *dialog, const gchar* const item,
				 const gchar* const widget);
static void remove_selection_from_list (TrackerPreferences *dialog,
					const gchar* const widget);
static GSList *treeview_get_values (GtkTreeView *treeview);
static gint _strcmp (gconstpointer a, gconstpointer b);
static void initialize_listview (GtkWidget *treeview);
static void populate_list (GtkWidget *treeview, GSList *list);
static gboolean str_slist_equal (GSList *a, GSList *b);

static GObjectClass *parent_class = NULL;
static gboolean flag_restart = FALSE;
static gboolean flag_reindex = FALSE;
static gboolean first_time = TRUE;
static gboolean flag_quit = FALSE;
static GtkWidget *main_window = NULL;


static void
tracker_preferences_class_init (TrackerPreferencesClass *klass)
{
	GObjectClass *g_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	g_type_class_add_private (klass, sizeof (TrackerPreferencesPrivate));

	g_class->finalize = tracker_preferences_finalize;
}

static void
tracker_preferences_init (GTypeInstance *instance, gpointer g_class)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (instance);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);

	tracker_configuration_load ();

	GtkWidget *widget = NULL;

	priv->gxml =
		glade_xml_new (TRACKER_DATADIR "/tracker-preferences.glade",
			       NULL, NULL);

	if (priv->gxml == NULL)
		g_error ("Unable to find locate tracker-preferences.glade");

	main_window = glade_xml_get_widget (priv->gxml, "dlgPreferences");

	/* Hide window first to allow the dialog to reize itself without redrawing */
	gtk_widget_hide (main_window);

	gtk_window_set_icon_name (GTK_WINDOW (main_window), "tracker");
	g_signal_connect (main_window, "delete-event",
			  G_CALLBACK (tracker_preferences_cmd_quit), self);

	/* Setup signals */
	widget = glade_xml_get_widget (priv->gxml, "cmdHelp");
	g_signal_connect (widget, "clicked", G_CALLBACK (tracker_preferences_cmd_help),
			  self);
	gtk_widget_hide (widget);

	widget = glade_xml_get_widget (priv->gxml, "dialog-action_area1");
	gtk_button_box_set_layout (GTK_BUTTON_BOX (widget),
				   GTK_BUTTONBOX_END);

	widget = glade_xml_get_widget (priv->gxml, "cmdApply");
	g_signal_connect (widget, "clicked", G_CALLBACK (tracker_preferences_cmd_apply),
			  self);

	widget = glade_xml_get_widget (priv->gxml, "cmdCancel");
	g_signal_connect (widget, "clicked", G_CALLBACK (tracker_preferences_cmd_cancel),
			  self);

	widget = glade_xml_get_widget (priv->gxml, "cmdOK");
	g_signal_connect (widget, "clicked", G_CALLBACK (tracker_preferences_cmd_ok),
			  self);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddIndexPath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (tracker_preferences_cmd_add_index_path), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIndexPath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (tracker_preferences_cmd_remove_index_path), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddCrawledPath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (tracker_preferences_cmd_add_crawled_path), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveCrawledPath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (tracker_preferences_cmd_remove_crawled_path), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddIndexMailbox");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (tracker_preferences_cmd_add_index_mailbox), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIndexMailbox");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (tracker_preferences_cmd_remove_index_mailbox), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddIgnorePath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (tracker_preferences_cmd_add_ignore_path), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIgnorePath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (tracker_preferences_cmd_remove_ignore_path), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddIgnorePattern");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (tracker_preferences_cmd_add_ignore_pattern), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIgnorePattern");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (tracker_preferences_cmd_remove_ignore_pattern), self);

	/* Init dbus */
	GError *error = NULL;
	g_type_init ();

	priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (priv->connection == NULL) {
		g_warning ("Unable to connect to dbus: %s\n", error->message);
		g_error_free (error);
		return;
	}

	priv->dbus_proxy = dbus_g_proxy_new_for_name (priv->connection,
						      DBUS_SERVICE_DBUS,
						      DBUS_PATH_DBUS,
						      DBUS_INTERFACE_DBUS);

	if (!priv->dbus_proxy) {
		g_warning ("could not create proxy");
		return;
	}

	priv->tracker_proxy = dbus_g_proxy_new_for_name (priv->connection,
							 TRACKER_DBUS_SERVICE,
							 TRACKER_DBUS_PATH,
							 TRACKER_DBUS_INTERFACE);

	if (!priv->tracker_proxy) {
		g_warning ("could not create proxy");
		return;
	}

	/* setup pages */
	setup_page_general (self);
	setup_page_files (self);
	setup_page_emails (self);
	setup_page_ignored_files (self);
	setup_page_performance (self);

	gtk_widget_show (main_window);
}

static void
tracker_preferences_finalize (GObject *object)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (object);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);

	g_object_unref (priv->gxml);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

TrackerPreferences *
tracker_preferences_new (void)
{
	TrackerPreferences *prefs;
	prefs = g_object_new (TRACKER_TYPE_PREFERENCES, NULL);
	return TRACKER_PREFERENCES (prefs);
}


static void
set_bool_option (TrackerPreferencesPrivate *priv, const gchar *name, gboolean value)
{
	dbus_g_proxy_begin_call (priv->tracker_proxy,
				 "SetBoolOption",
				 NULL,
				 NULL,
				 NULL,
				 G_TYPE_STRING, name,
				 G_TYPE_BOOLEAN, value,
				 G_TYPE_INVALID);
}


static void
set_int_option (TrackerPreferencesPrivate *priv, const gchar *name, int value)
{
	dbus_g_proxy_begin_call (priv->tracker_proxy,
				 "SetIntOption",
				 NULL,
				 NULL,
				 NULL,
				 G_TYPE_STRING, name,
				 G_TYPE_INT, value,
				 G_TYPE_INVALID);
}

static void
setup_page_general (TrackerPreferences *preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);

	gint sleep = 45;
	gchar *str_value = NULL;
	gboolean value = FALSE;
	GtkWidget *widget = NULL;

	widget = glade_xml_get_widget (priv->gxml, "spnInitialSleep");
	sleep = tracker_configuration_get_integer ("/General/InitialSleep", NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), sleep);

	widget = glade_xml_get_widget (priv->gxml, "chkEnableIndexing");
	value = tracker_configuration_get_boolean ("/Indexing/EnableIndexing", NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "chkEnableWatching");
	value = tracker_configuration_get_boolean ("/Watches/EnableWatching", NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "comLanguage");
	str_value = tracker_configuration_get_string ("/Indexing/Language", NULL);
	if (str_value == NULL) {
		/* no value for language? Default to "en" */
		str_value = g_strdup( "en" ) ;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);

	gint i;
	for (i = 0; i < 12; i++) {
		if (strcasecmp (LanguageMap[i].language, str_value) == 0) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
			break;
		}
	}

	widget = glade_xml_get_widget (priv->gxml, "chkDisableBatteryIndex");
	value = tracker_configuration_get_boolean ("/Indexing/BatteryIndex", NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !value);

	widget = glade_xml_get_widget (priv->gxml, "chkDisableBatteryInitialIndex");
	value = tracker_configuration_get_boolean ("/Indexing/BatteryIndexInitial", NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !value);
}

static void
setup_page_performance (TrackerPreferences *preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);

	GtkWidget *widget = NULL;
	gint value = 0;
	gboolean bvalue = FALSE;

	widget = glade_xml_get_widget (priv->gxml, "scaThrottle");
	value = tracker_configuration_get_integer("/Indexing/Throttle", NULL);
	gtk_range_set_value (GTK_RANGE (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "optReducedMemory");
	bvalue = tracker_configuration_get_boolean ("/General/LowMemoryMode", NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bvalue);

	widget = glade_xml_get_widget (priv->gxml, "optNormal");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !bvalue);

	widget = glade_xml_get_widget (priv->gxml, "chkFastMerges");
	bvalue = tracker_configuration_get_boolean ("/Indexing/FastMerges", NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bvalue);

	widget = glade_xml_get_widget (priv->gxml, "spnMaxText");
	value = tracker_configuration_get_integer ("/Performance/MaxTextToIndex", NULL);

	value = value / 1024;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "spnMaxWords");
	value = tracker_configuration_get_integer ("/Performance/MaxWordsToIndex", NULL);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

}

static void
setup_page_files (TrackerPreferences *preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);

	GSList *list = NULL, *entry = NULL;
	gboolean value = FALSE;
	GtkWidget *widget = NULL;

	widget = glade_xml_get_widget (priv->gxml, "chkIndexContents");
	value = tracker_configuration_get_boolean ("/Indexing/EnableFileContentIndexing",
						   NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "chkGenerateThumbs");
	value = tracker_configuration_get_boolean ("/Indexing/EnableThumbnails", NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "chkSkipMountPoints");
	value = tracker_configuration_get_boolean ("/Indexing/SkipMountPoints", NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !value);

	widget = glade_xml_get_widget (priv->gxml,
				       "lstAdditionalPathIndexes");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_configuration_get_list ("/Watches/WatchDirectoryRoots",
					       G_TYPE_STRING, NULL);

	widget = glade_xml_get_widget (priv->gxml, "chkIndexHomeDirectory");
	entry = g_slist_find_custom (list, g_get_home_dir (), _strcmp);

	if (entry) {
		list = g_slist_delete_link (list, entry);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
	}

	widget = glade_xml_get_widget (priv->gxml, "lstAdditionalPathIndexes");

	initialize_listview (widget);
	populate_list (widget, list);
	g_slist_free (list);

	widget = glade_xml_get_widget (priv->gxml, "lstCrawledPaths");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_configuration_get_list ("/Watches/CrawlDirectory",
					       G_TYPE_STRING, NULL);

	initialize_listview (widget);
	populate_list (widget, list);
	g_slist_free (list);
}

static void
setup_page_ignored_files (TrackerPreferences *preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);

	GSList *list = NULL;
	GtkWidget *widget = NULL;

	/* Ignore Paths */
	widget = glade_xml_get_widget (priv->gxml, "lstIgnorePaths");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_configuration_get_list ("/Watches/NoWatchDirectory",
					       G_TYPE_STRING, NULL);

	initialize_listview (widget);
	populate_list (widget, list);

	g_slist_free (list);

	/* Ignore File Patterns */
	widget = glade_xml_get_widget (priv->gxml, "lstIgnoreFilePatterns");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_configuration_get_list ("/Indexing/NoIndexFileTypes",
					       G_TYPE_STRING, NULL);

	initialize_listview (widget);
	populate_list (widget, list);

	g_slist_free (list);
}

static void
setup_page_emails (TrackerPreferences *preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);

	GtkWidget *widget = NULL;
	gboolean value = FALSE;

	widget = glade_xml_get_widget (priv->gxml,
				       "chkEnableEvolutionIndexing");
	value = tracker_configuration_get_boolean ("/Emails/IndexEvolutionEmails",
						   NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml,
				       "chkEnableModestIndexing");
	value = tracker_configuration_get_boolean ("/Emails/IndexModestEmails",
						   NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);


	widget = glade_xml_get_widget (priv->gxml,
				       "chkEnableThunderbirdIndexing");
	value = tracker_configuration_get_boolean ("/Emails/IndexThunderbirdEmails",
						   NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);
}

static void
tracker_preferences_cmd_quit (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	tracker_preferences_cmd_ok (NULL, data);
}

static void
tracker_preferences_cmd_help (GtkWidget *widget, gpointer data)
{
}

static void
name_owner_changed (DBusGProxy *proxy, const gchar *name,
		    const gchar *prev_owner, const gchar *new_owner,
		    gpointer data)
{
	if (!g_str_equal (name, TRACKER_DBUS_SERVICE))
		return;

	if (!first_time)
		return;

	if (g_str_equal (new_owner, "")) {
		/* tracker has exited */
		const gchar *command = TRACKER_BINDIR "/trackerd";

		if (!g_spawn_command_line_async (command, NULL))
			g_warning ("Unable to execute command: %s", command);

		first_time = FALSE;

		if (flag_quit)
			gtk_main_quit ();
	}
}

static gboolean
if_trackerd_start (TrackerPreferencesPrivate *priv)
{
	gchar *status = NULL;
	TrackerClient *client = NULL;

	client = tracker_connect (FALSE);

	if (!client)
		return FALSE;

	status = tracker_get_status (client, NULL);
	tracker_disconnect (client);

	if (strcmp (status, "Shutdown") == 0)
		return FALSE;
	else
		return TRUE;
}


static void
restart_tracker (GtkDialog *dialog, gint response, gpointer data)
{

	gtk_widget_destroy (GTK_WIDGET (dialog));

	if (response == GTK_RESPONSE_YES) {

		TrackerPreferences *self = TRACKER_PREFERENCES (data);
		TrackerPreferencesPrivate *priv = TRACKER_PREFERENCES_GET_PRIVATE (self);

		first_time = TRUE;
		dbus_g_proxy_add_signal (priv->dbus_proxy,
					 "NameOwnerChanged",
					 G_TYPE_STRING,
					 G_TYPE_STRING,
					 G_TYPE_STRING, G_TYPE_INVALID);
		dbus_g_proxy_connect_signal (priv->dbus_proxy,
					     "NameOwnerChanged",
					     G_CALLBACK (name_owner_changed),
					     self, NULL);

		dbus_g_proxy_begin_call (priv->tracker_proxy,
					 "Shutdown",
					 NULL,
					 NULL,
					 NULL,
					 G_TYPE_BOOLEAN,
					 flag_reindex, G_TYPE_INVALID);
		flag_restart = FALSE;
		flag_reindex = FALSE;

	} else if (flag_quit) {
		gtk_main_quit ();
	}
}

static void
tracker_preferences_cmd_apply (GtkWidget *widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);

	GSList *list = NULL;
	GSList *list_old = NULL;
	int ivalue, ivalue_old;
	gboolean bvalue, bvalue_old;
	gchar *str_value;

	/* save general settings */
	widget = glade_xml_get_widget (priv->gxml, "spnInitialSleep");
	ivalue = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
	tracker_configuration_set_integer ("/General/InitialSleep", ivalue);

	widget = glade_xml_get_widget (priv->gxml, "chkEnableIndexing");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_configuration_get_boolean ("/Indexing/EnableIndexing",
							NULL);
	if (bvalue != bvalue_old) {
		flag_restart = TRUE;
		set_bool_option (priv, "EnableIndexing", bvalue);
		tracker_configuration_set_boolean ("/Indexing/EnableIndexing", bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "chkEnableWatching");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_configuration_get_boolean ("/Watches/EnableWatching",
							NULL);
	if (bvalue != bvalue_old) {
		flag_restart = TRUE;
		set_bool_option (priv, "EnableIndexing", bvalue);
		tracker_configuration_set_boolean ("/Watches/EnableWatching", bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "comLanguage");
	gint i = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	str_value = tracker_configuration_get_string ("/Indexing/Language", NULL);
	if (str_value == NULL) {
		/* no value for language? Default to "en" */
		str_value = g_strdup( "en" ) ;
	}
	if (i != -1) {
		if (strcmp (str_value, LanguageMap[i].language) != 0) {
			flag_restart = TRUE;
			flag_reindex = TRUE;
		}
		tracker_configuration_set_string ("/Indexing/Language",
						  LanguageMap[i].language);
	}

	widget = glade_xml_get_widget (priv->gxml, "chkDisableBatteryIndex");
	bvalue = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_configuration_get_boolean ("/Indexing/BatteryIndex",
							NULL);
	if (bvalue != bvalue_old) {
		set_bool_option (priv, "BatteryIndex", bvalue);
		tracker_configuration_set_boolean ("/Indexing/BatteryIndex", bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "chkDisableBatteryInitialIndex");
	bvalue = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_configuration_get_boolean ("/Indexing/BatteryIndexInitial",
							NULL);
	if (bvalue != bvalue_old) {
		set_bool_option (priv, "BatteryIndexInitial", bvalue);
		tracker_configuration_set_boolean ("/Indexing/BatteryIndexInitial", bvalue);
	}

	/* files settings */
	widget = glade_xml_get_widget (priv->gxml, "chkIndexContents");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_configuration_get_boolean ("/Indexing/EnableFileContentIndexing",
							NULL);
	if (bvalue != bvalue_old) {
		flag_restart = TRUE;
		flag_reindex = TRUE;
		set_bool_option (priv, "IndexFileContents", bvalue);
		tracker_configuration_set_boolean ("/Indexing/EnableFileContentIndexing",
						   bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "chkGenerateThumbs");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_configuration_get_boolean ("/Indexing/EnableThumbnails",
							NULL);
	if (bvalue != bvalue_old) {
		flag_restart = TRUE;
		flag_reindex = TRUE;
		set_bool_option (priv, "GenerateThumbs", bvalue);
		tracker_configuration_set_boolean ("/Indexing/EnableThumbnails", bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "chkSkipMountPoints");
	bvalue = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_configuration_get_boolean ("/Indexing/SkipMountPoints",
							NULL);
	if (bvalue != bvalue_old) {
		flag_restart = TRUE;
		set_bool_option (priv, "SkipMountPoints", bvalue);
		tracker_configuration_set_boolean ("/Indexing/SkipMountPoints", bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "lstAdditionalPathIndexes");
	list = treeview_get_values (GTK_TREE_VIEW (widget));

	widget = glade_xml_get_widget (priv->gxml, "chkIndexHomeDirectory");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
		list = g_slist_prepend (list, g_strdup (g_get_home_dir ()));
	}
	list_old = tracker_configuration_get_list ("/Watches/WatchDirectoryRoots",
						   G_TYPE_STRING, NULL);
	if (!str_slist_equal (list, list_old)) {
		flag_restart = TRUE;
		tracker_configuration_set_list ("/Watches/WatchDirectoryRoots", G_TYPE_STRING,
						list);
	}

	g_slist_free (list);
	list = NULL;
	g_slist_free (list_old);
	list_old = NULL;

	widget = glade_xml_get_widget (priv->gxml, "lstCrawledPaths");
	list = treeview_get_values (GTK_TREE_VIEW (widget));
	list_old = tracker_configuration_get_list ("/Watches/CrawlDirectory",
						   G_TYPE_STRING, NULL);
	if (!str_slist_equal (list, list_old)) {
		flag_restart = TRUE;
		tracker_configuration_set_list ("/Watches/CrawlDirectory", G_TYPE_STRING,
						list);
	}

	g_slist_free (list);
	list = NULL;
	g_slist_free (list_old);
	list_old = NULL;

	/* ignored files settings */
	widget = glade_xml_get_widget (priv->gxml, "lstIgnorePaths");
	list = treeview_get_values (GTK_TREE_VIEW (widget));
	list_old = tracker_configuration_get_list ("/Watches/NoWatchDirectory",
						   G_TYPE_STRING, NULL);
	if (!str_slist_equal (list, list_old)) {
		flag_restart = TRUE;
		tracker_configuration_set_list ("/Watches/NoWatchDirectory", G_TYPE_STRING,
						list);
	}

	g_slist_free (list);
	list = NULL;
	g_slist_free (list_old);
	list_old = NULL;

	widget = glade_xml_get_widget (priv->gxml, "lstIgnoreFilePatterns");
	list = treeview_get_values (GTK_TREE_VIEW (widget));
	list_old = tracker_configuration_get_list ("/Indexing/NoIndexFileTypes",
						   G_TYPE_STRING, NULL);
	if (!str_slist_equal (list, list_old)) {
		flag_restart = TRUE;
		tracker_configuration_set_list ("/Indexing/NoIndexFileTypes", G_TYPE_STRING,
						list);
	}

	g_slist_free (list);
	list = NULL;
	g_slist_free (list_old);
	list_old = NULL;

	/* Email settings */
	widget = glade_xml_get_widget (priv->gxml, "chkEnableEvolutionIndexing");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_configuration_get_boolean ("/Emails/IndexEvolutionEmails",
							NULL);
	if (bvalue != bvalue_old) {
		set_bool_option (priv, "EnableEvolution", bvalue);
		tracker_configuration_set_boolean ("/Emails/IndexEvolutionEmails", bvalue);
	}


	widget = glade_xml_get_widget (priv->gxml, "chkEnableModestIndexing");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_configuration_get_boolean ("/Emails/IndexModestEmails",
							NULL);
	if (bvalue != bvalue_old) {
		set_bool_option (priv, "EnableModest", bvalue);
		tracker_configuration_set_boolean ("/Emails/IndexModestEmails", bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "chkEnableThunderbirdIndexing");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_configuration_get_boolean ("/Emails/IndexThunderbirdEmails",
							NULL);
	if (bvalue != bvalue_old) {
		set_bool_option (priv, "EnableThunderbird", bvalue);
		tracker_configuration_set_boolean ("/Emails/IndexThunderbirdEmails", bvalue);
	}

	/* Performance settings */
	widget = glade_xml_get_widget (priv->gxml, "scaThrottle");
	ivalue = gtk_range_get_value (GTK_RANGE (widget));
	ivalue_old = tracker_configuration_get_integer ("/Indexing/Throttle", NULL);
	if (ivalue != ivalue_old) {
		set_int_option (priv, "Throttle", ivalue);
		tracker_configuration_set_integer ("/Indexing/Throttle", ivalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "optReducedMemory");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_configuration_get_boolean ("/General/LowMemoryMode",
							NULL);
	if (bvalue != bvalue_old) {
		set_bool_option (priv, "LowMemoryMode", bvalue);
		tracker_configuration_set_boolean ("/General/LowMemoryMode", bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "chkFastMerges");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_configuration_get_boolean ("/Indexing/FastMerges", NULL);
	if (bvalue != bvalue_old) {
		set_bool_option (priv, "FastMerges", bvalue);
		tracker_configuration_set_boolean ("/Indexing/FastMerges", bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "spnMaxText");
	ivalue = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget))*1024;
	ivalue_old = tracker_configuration_get_integer ("/Performance/MaxTextToIndex",
							NULL);
	if (ivalue != ivalue_old) {
		set_int_option (priv, "MaxText", ivalue);
		tracker_configuration_set_integer ("/Performance/MaxTextToIndex", ivalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "spnMaxWords");
	ivalue = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
	ivalue_old = tracker_configuration_get_integer ("/Performance/MaxWordsToIndex",
							NULL);
	if (ivalue != ivalue_old) {
		set_int_option (priv, "MaxWords", ivalue);
		tracker_configuration_set_integer ("/Performance/MaxWordsToIndex", ivalue);
	}

	/* save config to distk */
	tracker_configuration_save ();

	if (flag_restart && if_trackerd_start (priv)) {
		GtkWidget *dialog;
		gchar *primary;
		gchar *secondary;
		gchar *button;

		if (flag_reindex) {
			primary = g_strdup (_("Data must be reindexed"));
			secondary = g_strdup (_("In order for your changes to "
						"take effect, Tracker must reindex your "
						"files. Click the Reindex button to "
						"start reindexing now, otherwise this "
						"action will be performed the "
						"next time the Tracker daemon "
						"is restarted."));
			button = g_strdup (_("_Reindex"));

		} else {
			primary = g_strdup (_("Tracker daemon must be "
					      "restarted"));
			secondary = g_strdup (_("In order for your changes to "
						"take effect, the Tracker daemon "
						"has to be restarted. Click the "
						"Restart button to restart the "
						"daemon now."));
			button = g_strdup (_("_Restart"));
		}

		dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_NONE,
						 primary);

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  secondary);

		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					GTK_STOCK_CANCEL, GTK_RESPONSE_NO,
					button, GTK_RESPONSE_YES, NULL);

		g_free (primary);
		g_free (secondary);
		g_free (button);

		gtk_window_set_title (GTK_WINDOW (dialog), "");
		gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
		gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

		g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (restart_tracker), self);

		gtk_widget_show (dialog);
	} else if (flag_quit) {
		tracker_configuration_free ();
		gtk_main_quit ();
	}
}

static void
tracker_preferences_cmd_cancel (GtkWidget *widget, gpointer data)
{
	tracker_configuration_free ();
	gtk_main_quit ();
}

static void
tracker_preferences_cmd_ok (GtkWidget *widget, gpointer data)
{
	flag_quit = TRUE;
	tracker_preferences_cmd_apply (widget, data);
}

static void
tracker_preferences_cmd_add_crawled_path (GtkWidget *widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	gchar *path = tracker_preferences_select_folder ();

	if (!path)
		return;

	append_item_to_list (self, path, "lstCrawledPaths");

	g_free (path);
}

static void
tracker_preferences_cmd_remove_crawled_path (GtkWidget *widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstCrawledPaths");
}

static void
tracker_preferences_cmd_add_index_path (GtkWidget *widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);

	GtkWidget *item = NULL;
	gchar *path = tracker_preferences_select_folder ();

	if (!path)
		return;

	if (!strcasecmp (path, g_get_home_dir ())) {
		item = glade_xml_get_widget (priv->gxml,
					     "chkIndexHomeDirectory");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);
	} else
		append_item_to_list (self, path, "lstAdditionalPathIndexes");

	g_free (path);
}

static void
tracker_preferences_cmd_remove_index_path (GtkWidget *widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstAdditionalPathIndexes");
}

static void
tracker_preferences_cmd_add_index_mailbox (GtkWidget *widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);

	gchar *path = tracker_preferences_select_folder ();

	if (!path)
		return;

	append_item_to_list (self, path, "lstAdditionalMBoxIndexes");
	g_free (path);
}

static void
tracker_preferences_cmd_remove_index_mailbox (GtkWidget *widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstAdditionalMBoxIndexes");
}

static void
tracker_preferences_cmd_add_ignore_path (GtkWidget *widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);

	gchar *path = tracker_preferences_select_folder ();

	if (!path)
		return;

	append_item_to_list (self, path, "lstIgnorePaths");
	g_free (path);
}

static void
tracker_preferences_cmd_remove_ignore_path (GtkWidget *widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstIgnorePaths");
}

static void
tracker_preferences_cmd_add_ignore_pattern (GtkWidget *widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);

	gchar *pattern = tracker_preferences_select_pattern ();

	if (!pattern)
		return;

	append_item_to_list (self, pattern, "lstIgnoreFilePatterns");
}

static void
tracker_preferences_cmd_remove_ignore_pattern (GtkWidget *widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstIgnoreFilePatterns");
}

static void
append_item_to_list (TrackerPreferences *dialog, const gchar *const item,
		     const gchar* const widget)
{
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (dialog);

	GtkTreeIter iter;
	GtkWidget *view = glade_xml_get_widget (priv->gxml, widget);
	GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter))
		do {
			gchar *value = NULL;
			gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 0,
					    &value, -1);

			if (!strcasecmp (item, value))
				return;
		} while (gtk_tree_model_iter_next
			 (GTK_TREE_MODEL (model), &iter));

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, item, -1);
}

static void
remove_selection_from_list (TrackerPreferences *dialog,
			    const gchar* const widget)
{
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (dialog);

	GtkTreeIter iter;
	GtkWidget *view = glade_xml_get_widget (priv->gxml, widget);
	GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
	GtkTreeSelection *selection =
		gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static GSList *
treeview_get_values (GtkTreeView *treeview)
{
	GtkTreeIter iter;
	GSList *list = NULL;
	GtkTreeModel *model =
		gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter))
		do {
			gchar *value = NULL;
			gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 0,
					    &value, -1);

			if (value) {
				list = g_slist_prepend (list,
							g_strdup (value));
			}
		} while (gtk_tree_model_iter_next
			 (GTK_TREE_MODEL (model), &iter));

	return list;
}

static gint
_strcmp (gconstpointer a, gconstpointer b)
{
	if (a == NULL && b != NULL)
		return -1;

	if (a == NULL && b == NULL)
		return 0;

	if (a != NULL && b == NULL)
		return 1;

	return strcmp (a, b);
}

static void
initialize_listview (GtkWidget *treeview)
{
	GtkListStore *store = NULL;
	GtkCellRenderer *renderer = NULL;
	GtkTreeViewColumn *column = NULL;

	store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
				 GTK_TREE_MODEL (store));
	g_object_unref (store);	/* this will delete the store when the view is destroyed */

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Column 0",
							   renderer, "text",
							   0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
}

static void
populate_list (GtkWidget *treeview, GSList *list)
{
	GtkTreeModel *store;
	GSList *tmp;

	store = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

	for (tmp = list; tmp; tmp = tmp->next) {
		if (tmp->data) {
			GtkTreeIter iter;
			gchar *data = tmp->data;

			gtk_list_store_append (GTK_LIST_STORE (store), &iter);
			gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0,
					    data, -1);
		}
	}
}

static gboolean
str_slist_equal (GSList *a, GSList *b)
{
	guint len_a = g_slist_length (a);
	guint len_b = g_slist_length (b);

	if (len_a != len_b)
		return FALSE;

	GSList *lst;
	for (lst = a; lst; lst = lst->next) {
		GSList *find = g_slist_find_custom (b, (const gchar *)(lst->data), _strcmp);
		if (!find)
			return FALSE;
	}

	return TRUE;
}

GType
tracker_preferences_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (TrackerPreferencesClass),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) tracker_preferences_class_init,	/* class_init */
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (TrackerPreferences),
			0,	/* n_preallocs */
			tracker_preferences_init	/* instance_init */
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "TrackerPreferencesType",
					       &info, 0);
	}

	return type;
}
