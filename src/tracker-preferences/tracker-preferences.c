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

#include <string.h>
#include <glib/gi18n.h>
#include "../trackerd/tracker-dbus.h"
#include "../libtracker/tracker.h"

#include "tracker-preferences.h"
#include "tracker-preferences-private.h"
#include "tracker-preferences-dialogs.h"

static GObjectClass *parent_class = NULL;
static gboolean flag_restart = FALSE;
static gboolean flag_reindex = FALSE;
static GtkWidget *main_window = NULL;

static void
tracker_preferences_class_init (TrackerPreferencesClass * klass)
{
	GObjectClass *g_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	g_type_class_add_private (klass, sizeof (TrackerPreferencesPrivate));

	g_class->finalize = tracker_preferences_finalize;
}

static void
tracker_preferences_init (GTypeInstance * instance, gpointer g_class)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (instance);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);

	priv->prefs = tracker_configuration_new ();

	GtkWidget *widget = NULL;
	

	priv->gxml =
		glade_xml_new (TRACKER_DATADIR "/tracker-preferences.glade",
			       NULL, NULL);
	/* priv->gxml = glade_xml_new("tracker-preferences.glade", NULL, NULL); */

	if (priv->gxml == NULL)
		g_error ("Unable to find locate tracker-preferences.glade");

	main_window = glade_xml_get_widget (priv->gxml, "dlgPreferences");

	/* Hide window first to allow the dialog to reize itself without redrawing */
	gtk_widget_hide (main_window);

	gtk_window_set_icon_name (GTK_WINDOW (main_window), "tracker");
	g_signal_connect (main_window, "delete-event",
			  G_CALLBACK (dlgPreferences_Quit), self);

	/* Setup signals */
	widget = glade_xml_get_widget (priv->gxml, "cmdHelp");
	g_signal_connect (widget, "clicked", G_CALLBACK (cmdHelp_Clicked),
			  self);
	gtk_widget_hide (widget);

	widget = glade_xml_get_widget (priv->gxml, "dialog-action_area1");
	gtk_button_box_set_layout (GTK_BUTTON_BOX (widget),
				   GTK_BUTTONBOX_END);

	widget = glade_xml_get_widget (priv->gxml, "cmdClose");
	g_signal_connect (widget, "clicked", G_CALLBACK (cmdClose_Clicked),
			  self);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddIndexPath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmdAddIndexPath_Clicked), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIndexPath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmdRemoveIndexPath_Clicked), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddCrawledPath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmdAddCrawledPath_Clicked), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveCrawledPath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmdRemoveCrawledPath_Clicked), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddIndexMailbox");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmdAddIndexMailbox_Clicked), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIndexMailbox");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmdRemoveIndexMailbox_Clicked), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddIgnorePath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmdAddIgnorePath_Clicked), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIgnorePath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmdRemoveIgnorePath_Clicked), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddIgnorePattern");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmdAddIgnorePattern_Clicked), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIgnorePattern");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmdRemoveIgnorePattern_Clicked), self);

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
							 TRACKER_SERVICE,
							 TRACKER_OBJECT,
							 TRACKER_INTERFACE);

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
tracker_preferences_finalize (GObject * object)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (object);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);

	g_object_unref (priv->prefs);
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
set_bool_option (TrackerPreferencesPrivate *priv, const char *name, gboolean value)
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
set_int_option (TrackerPreferencesPrivate *priv, const char *name, int value)
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


void
throttle_changed_cb (GtkRange *range, gpointer user_data)  
{
	TrackerPreferencesPrivate *priv = user_data;
	TrackerConfiguration *configuration = TRACKER_CONFIGURATION (priv->prefs);

	int value = gtk_range_get_value (range);

	tracker_configuration_set_int (configuration,
					"/Indexing/Throttle",
					value);

	set_int_option (priv, "Throttle", value);

}

void 
spin_value_changed_cb (GtkSpinButton *spin_button, gpointer user_data)
{
	TrackerPreferencesPrivate *priv = user_data;
	TrackerConfiguration *configuration = TRACKER_CONFIGURATION (priv->prefs);

	const char *name = gtk_widget_get_name (GTK_WIDGET (spin_button));
	int value = gtk_spin_button_get_value_as_int (spin_button);

	if (name) {
		g_print ("%s was clicked with value %d\n", name, value);
	}  else {
		g_print ("unknown widget was clicked with value %d\n", value);
	}

	if (g_str_equal (name, "spnMaxText")) {

		set_int_option (priv, "MaxText", value*1024);

		tracker_configuration_set_int (configuration,
						"/Performance/MaxTextToIndex",
						value);


	} else 	if (g_str_equal (name, "spnMaxWords")) {

		set_int_option (priv, "MaxWords", value);

		tracker_configuration_set_int (configuration,
					"/Performance/MaxWordsToIndex",
					value);

	}

}

void
check_toggled_cb (GtkToggleButton *check_button, gpointer user_data)  
{
	TrackerPreferencesPrivate *priv = user_data;
	TrackerConfiguration *configuration = TRACKER_CONFIGURATION (priv->prefs);

	const char *name = gtk_widget_get_name (GTK_WIDGET (check_button));

	gboolean value =  gtk_toggle_button_get_active (check_button);

	if (name) {
		g_print ("%s was clicked with value %d\n", name, value);
	}  else {
		g_print ("unknown widget was clicked with value %d\n", value);
	}

	if (g_str_equal (name, "chkEnableIndexing")) {

		set_bool_option (priv, "EnableIndexing", value);

		tracker_configuration_set_bool (configuration,
						 "/Indexing/EnableIndexing",
						 value);
		flag_restart = TRUE;

	} else 	if (g_str_equal (name, "chkEnableWatching")) {

		set_bool_option (priv, "EnableWatching", value);

		tracker_configuration_set_bool (configuration,
					"/Watches/EnableWatching",
					value);

		flag_restart = TRUE;

	} else 	if (g_str_equal (name, "chkEnableEvolutionIndexing")) {

		set_bool_option (priv, "EnableEvolution", value);

		tracker_configuration_set_bool (configuration,
					"/Emails/IndexEvolutionEmails",
					value);

		flag_restart = TRUE;

	} else 	if (g_str_equal (name, "chkIndexContents")) {

		set_bool_option (priv, "IndexFileContents", value);

		tracker_configuration_set_bool (configuration,
					"/Indexing/EnableFileContentIndexing",
					value);

		flag_restart = TRUE;

	} else 	if (g_str_equal (name, "chkGenerateThumbs")) {

		set_bool_option (priv, "GenerateThumbs", value);

		tracker_configuration_set_bool (configuration,
					"/Indexing/EnableThumbnails",
					value);


	} else 	if (g_str_equal (name, "chkSkipMountPoints")) {

		set_bool_option (priv, "SkipMountPoints", !value);

		tracker_configuration_set_bool (configuration,
					"/Indexing/SkipMountPoints",
					!value);


	} else 	if (g_str_equal (name, "chkFastMerges")) {

		set_bool_option (priv, "FastMerges", value);

		tracker_configuration_set_bool (configuration,
					"/Indexing/FastMerges",
					value);

	} else 	if (g_str_equal (name, "optReducedMemory")) {

		set_bool_option (priv, "LowMemoryMode", value);

		tracker_configuration_set_bool (configuration,
					"/General/LowMemoryMode",
					value);

	}  else if (g_str_equal (name, "chkDisableBatteryIndex")) {

		set_bool_option (priv, "BatteryIndex", !value);

		tracker_configuration_set_bool (configuration,
					"/Indexing/BatteryIndex",
					!value);

	}  else if (g_str_equal (name, "chkDisableBatteryInitialIndex")) {

		set_bool_option (priv, "BatteryIndexInitial", !value);

		tracker_configuration_set_bool (configuration,
					"/Indexing/BatteryIndexInitial",
					!value);
	}

}




static void
setup_page_general (TrackerPreferences * preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);
	TrackerConfiguration *configuration =
		TRACKER_CONFIGURATION (priv->prefs);

	gboolean value = FALSE;
	gint sleep = 45;
	GtkWidget *widget = NULL;

	widget = glade_xml_get_widget (priv->gxml, "spnInitialSleep");
	sleep = tracker_configuration_get_int (configuration,
					       "/General/InitialSleep", NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), sleep);

	widget = glade_xml_get_widget (priv->gxml, "chkEnableIndexing");
	value = tracker_configuration_get_bool (configuration,
						"/Indexing/EnableIndexing",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled",
			  G_CALLBACK (check_toggled_cb), priv);

	widget = glade_xml_get_widget (priv->gxml, "chkEnableWatching");
	value = tracker_configuration_get_bool (configuration,
						"/Watches/EnableWatching",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled",
			  G_CALLBACK (check_toggled_cb), priv);

	widget = glade_xml_get_widget (priv->gxml, "comLanguage");
	char *str_value = tracker_configuration_get_string (configuration,
							    "/Indexing/Language",
							    NULL);

	gint i;
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);

	for (i = 0; i < 12; i++) {
		if (strcasecmp (tmap[i].lang, str_value) == 0) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
			break;
		}
	}

	widget = glade_xml_get_widget (priv->gxml, "chkDisableBatteryIndex");
	value = tracker_configuration_get_bool (configuration,
						"/Indexing/BatteryIndex",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !value);

	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled",
			  G_CALLBACK (check_toggled_cb), priv);

	widget = glade_xml_get_widget (priv->gxml, "chkDisableBatteryInitialIndex");
	value = tracker_configuration_get_bool (configuration,
						"/Indexing/BatteryIndexInitial",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !value);

	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled",
			  G_CALLBACK (check_toggled_cb), priv);


}

static void
setup_page_performance (TrackerPreferences * preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);
	TrackerConfiguration *configuration =
		TRACKER_CONFIGURATION (priv->prefs);

	GtkWidget *widget = NULL;
	gint value = 0;
	gboolean bvalue = FALSE;

	widget = glade_xml_get_widget (priv->gxml, "scaThrottle");
	value = tracker_configuration_get_int (configuration,
					       "/Indexing/Throttle", NULL);
	gtk_range_set_value (GTK_RANGE (widget), value);
	g_signal_connect (GTK_RANGE (widget), "value-changed",
			  G_CALLBACK (throttle_changed_cb), priv);

	widget = glade_xml_get_widget (priv->gxml, "optReducedMemory");
	bvalue = tracker_configuration_get_bool (configuration,
						 "/General/LowMemoryMode",
						 NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bvalue);

	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled",
			  G_CALLBACK (check_toggled_cb), priv);


	widget = glade_xml_get_widget (priv->gxml, "optNormal");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !bvalue);
	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled",
			  G_CALLBACK (check_toggled_cb), priv);

	widget = glade_xml_get_widget (priv->gxml, "chkFastMerges");
	bvalue = tracker_configuration_get_bool (configuration,
						 "/Indexing/FastMerges",
						 NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bvalue);

	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled",
			  G_CALLBACK (check_toggled_cb), priv);


	widget = glade_xml_get_widget (priv->gxml, "spnMaxText");
	value = tracker_configuration_get_int (configuration,
					       "/Performance/MaxTextToIndex",
					       NULL);

	value = value / 1024;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	g_signal_connect (GTK_SPIN_BUTTON (widget), "value-changed",
			  G_CALLBACK (spin_value_changed_cb), priv);

	widget = glade_xml_get_widget (priv->gxml, "spnMaxWords");
	value = tracker_configuration_get_int (configuration,
					       "/Performance/MaxWordsToIndex",
					       NULL);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	g_signal_connect (GTK_SPIN_BUTTON (widget), "value-changed",
			  G_CALLBACK (spin_value_changed_cb), priv);
}



static void
setup_page_files (TrackerPreferences * preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);
	TrackerConfiguration *configuration =
		TRACKER_CONFIGURATION (priv->prefs);

	GSList *list = NULL;
	gboolean value = FALSE;
	GtkWidget *widget = NULL;

	widget = glade_xml_get_widget (priv->gxml, "chkIndexContents");
	value = tracker_configuration_get_bool (configuration,
						"/Indexing/EnableFileContentIndexing",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);
	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled",
			  G_CALLBACK (check_toggled_cb), priv);


	widget = glade_xml_get_widget (priv->gxml, "chkGenerateThumbs");
	value = tracker_configuration_get_bool (configuration,
						"/Indexing/EnableThumbnails",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);
	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled",
			  G_CALLBACK (check_toggled_cb), priv);


	widget = glade_xml_get_widget (priv->gxml, "chkSkipMountPoints");
	value = tracker_configuration_get_bool (configuration,
						"/Indexing/SkipMountPoints",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !value);
	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled",
			  G_CALLBACK (check_toggled_cb), priv);


	widget = glade_xml_get_widget (priv->gxml,
				       "lstAdditionalPathIndexes");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_configuration_get_list (configuration,
					       "/Watches/WatchDirectoryRoots",
					       G_TYPE_STRING, NULL);

	GSList *entry =
		g_slist_find_custom (list, g_get_home_dir (), _strcmp);

	widget = glade_xml_get_widget (priv->gxml, "chkIndexHomeDirectory");

	if (entry) {
		list = g_slist_delete_link (list, entry);


		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      FALSE);
	}

	widget = glade_xml_get_widget (priv->gxml,
				       "lstAdditionalPathIndexes");

	initialize_listview (widget);
	populate_list (widget, list);
	g_slist_free (list);

	widget = glade_xml_get_widget (priv->gxml, "lstCrawledPaths");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_configuration_get_list (configuration,
					       "/Watches/CrawlDirectory",
					       G_TYPE_STRING, NULL);

	initialize_listview (widget);
	populate_list (widget, list);
	g_slist_free (list);
}

static void
setup_page_ignored_files (TrackerPreferences * preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);
	TrackerConfiguration *configuration =
		TRACKER_CONFIGURATION (priv->prefs);

	GSList *list = NULL;
	GtkWidget *widget = NULL;

	/* Ignore Paths */
	widget = glade_xml_get_widget (priv->gxml, "lstIgnorePaths");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_configuration_get_list (configuration,
					       "/Watches/NoWatchDirectory",
					       G_TYPE_STRING, NULL);

	initialize_listview (widget);
	populate_list (widget, list);

	g_slist_free (list);

	/* Ignore File Patterns */
	widget = glade_xml_get_widget (priv->gxml, "lstIgnoreFilePatterns");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_configuration_get_list (configuration,
					       "/Indexing/NoIndexFileTypes",
					       G_TYPE_STRING, NULL);

	initialize_listview (widget);
	populate_list (widget, list);

	g_slist_free (list);
}

static void
setup_page_emails (TrackerPreferences * preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);
	TrackerConfiguration *configuration =
		TRACKER_CONFIGURATION (priv->prefs);

	GtkWidget *widget = NULL;
	gboolean value;

	widget = glade_xml_get_widget (priv->gxml,
				       "chkEnableEvolutionIndexing");

	value = tracker_configuration_get_bool (configuration,
						"/Emails/IndexEvolutionEmails",
						NULL);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled",
			  G_CALLBACK (check_toggled_cb), priv);
}

static void
dlgPreferences_Quit (GtkWidget * widget, GdkEvent * event, gpointer data)
{
	cmdClose_Clicked (NULL, data);
}

static void
cmdHelp_Clicked (GtkWidget * widget, gpointer data)
{
}

static void
name_owner_changed (DBusGProxy * proxy, const gchar * name,
		    const gchar * prev_owner, const gchar * new_owner,
		    gpointer data)
{
        static gboolean first_time = TRUE;

	if (!g_str_equal (name, TRACKER_SERVICE))
		return;

        if (!first_time)
                return;

	if (g_str_equal (new_owner, "")) {
		/* tracker has exited */
		const gchar *command = TRACKER_BINDIR "/trackerd";

		if (!g_spawn_command_line_async (command, NULL))
			g_warning ("Unable to execute command: %s", command);
                first_time = FALSE;
		gtk_main_quit ();
	}
}

static gboolean
if_trackerd_start (TrackerPreferencesPrivate * priv)
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

	} else {
		gtk_main_quit ();
	}

	
}

static void
cmdClose_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);
	TrackerConfiguration *configuration =
		TRACKER_CONFIGURATION (priv->prefs);

	GSList *list = NULL;
	gboolean value = FALSE;
	gboolean value_old = FALSE;
	gint ivalue, ivalue_old;
	char *str_value;


	/* save general settings */
	widget = glade_xml_get_widget (priv->gxml, "spnInitialSleep");
	value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
	tracker_configuration_set_int (configuration, "/General/InitialSleep",
				       value);

	widget = glade_xml_get_widget (priv->gxml, "comLanguage");
	gint i = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	str_value = tracker_configuration_get_string (configuration,
						      "/Indexing/Language",
						      NULL);
	if (i != -1) {
		if (strcmp (str_value, tmap[i].lang) != 0) {
			flag_restart = TRUE;
			flag_reindex = TRUE;
		}
		tracker_configuration_set_string (configuration,
						  "/Indexing/Language",
						  tmap[i].lang);
	}


	/* files settings */

	widget = glade_xml_get_widget (priv->gxml,
				       "lstAdditionalPathIndexes");
	list = treeview_get_values (GTK_TREE_VIEW (widget));

	widget = glade_xml_get_widget (priv->gxml, "chkIndexHomeDirectory");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
		list = g_slist_prepend (list, g_strdup (g_get_home_dir ()));
	}
	tracker_configuration_set_list (configuration,
					"/Watches/WatchDirectoryRoots", list,
					G_TYPE_STRING);
	g_slist_free (list);
	list = NULL;

	widget = glade_xml_get_widget (priv->gxml, "lstCrawledPaths");
	list = treeview_get_values (GTK_TREE_VIEW (widget));
	tracker_configuration_set_list (configuration,
					"/Watches/CrawlDirectory", list,
					G_TYPE_STRING);
	g_slist_free (list);
	list = NULL;


	/* ignored files settings */
	widget = glade_xml_get_widget (priv->gxml, "lstIgnorePaths");
	list = treeview_get_values (GTK_TREE_VIEW (widget));
	tracker_configuration_set_list (configuration,
					"/Watches/NoWatchDirectory", list,
					G_TYPE_STRING);
	g_slist_free (list);
	list = NULL;

	widget = glade_xml_get_widget (priv->gxml, "lstIgnoreFilePatterns");
	list = treeview_get_values (GTK_TREE_VIEW (widget));
	tracker_configuration_set_list (configuration,
					"/Indexing/NoIndexFileTypes", list,
					G_TYPE_STRING);
	g_slist_free (list);
	list = NULL;


	tracker_configuration_write (configuration);

	if (flag_restart && if_trackerd_start (priv)) {

		GtkWidget * dialog;
		char *msg;

		if (flag_reindex) {
			msg =  _("Your system must be re-indexed for your changes to take effect. Re-index now?");
		} else {
			msg =  _("Tracker indexer needs to be restarted for your changes to take effect. Restart now?");
		}
		
		dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
 	                                 	GTK_DIALOG_MODAL,
	                                 	GTK_MESSAGE_QUESTION,
	                                 	GTK_BUTTONS_YES_NO,
	                                 	msg);
		
		gtk_window_set_title (GTK_WINDOW (dialog), "");
		gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
		gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

		g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (restart_tracker), self);

		gtk_widget_show (dialog);

		
	} else {
		gtk_main_quit ();
	}
}

static void
cmdAddCrawledPath_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	gchar *path = tracker_preferences_select_folder ();

	if (!path)
		return;

	append_item_to_list (self, path, "lstCrawledPaths");

	g_free (path);
	flag_restart = TRUE;
}

static void
cmdRemoveCrawledPath_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstCrawledPaths");
	flag_restart = TRUE;
}

static void
cmdAddIndexPath_Clicked (GtkWidget * widget, gpointer data)
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
	flag_restart = TRUE;
}

static void
cmdRemoveIndexPath_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstAdditionalPathIndexes");
	flag_restart = TRUE;
}

static void
cmdAddIndexMailbox_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);

	gchar *path = tracker_preferences_select_folder ();

	if (!path)
		return;

	append_item_to_list (self, path, "lstAdditionalMBoxIndexes");
	g_free (path);
	flag_restart = TRUE;
}

static void
cmdRemoveIndexMailbox_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstAdditionalMBoxIndexes");
	flag_restart = TRUE;
}

static void
cmdAddIgnorePath_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);

	gchar *path = tracker_preferences_select_folder ();

	if (!path)
		return;

	append_item_to_list (self, path, "lstIgnorePaths");
	g_free (path);
	flag_restart = TRUE;
}

static void
cmdRemoveIgnorePath_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstIgnorePaths");
	flag_restart = TRUE;
}

static void
cmdAddIgnorePattern_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);

	gchar *pattern = tracker_preferences_select_pattern ();

	if (!pattern)
		return;

	append_item_to_list (self, pattern, "lstIgnoreFilePatterns");
	flag_restart = TRUE;
}

static void
cmdRemoveIgnorePattern_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstIgnoreFilePatterns");
	flag_restart = TRUE;
}

static void
append_item_to_list (TrackerPreferences * dialog, const gchar * const item,
		     const gchar * const widget)
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
remove_selection_from_list (TrackerPreferences * dialog,
			    const gchar * const widget)
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
treeview_get_values (GtkTreeView * treeview)
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
initialize_listview (GtkWidget * treeview)
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
populate_list (GtkWidget * treeview, GSList * list)
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
