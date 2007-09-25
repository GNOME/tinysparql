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
#include "tracker-preferences.h"
#include "tracker-preferences-private.h"
#include "tracker-preferences-dialogs.h"

static GObjectClass *parent_class = NULL;

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
	GtkWidget *main_window = NULL;

	priv->gxml =
		glade_xml_new (TRACKER_DATADIR "/tracker-preferences.glade",
			       NULL, NULL);
	/* priv->gxml = glade_xml_new("tracker-preferences.glade", NULL, NULL); */

	if (priv->gxml == NULL)
		g_error ("Unable to find locate tracker-preferences.glade");

	main_window = glade_xml_get_widget (priv->gxml, "dlgPreferences");

	/* Hide window first to allow the dialog to reize itself without redrawing */
	gtk_widget_hide (main_window);

	gtk_window_set_icon_name(GTK_WINDOW(main_window), "tracker");
	g_signal_connect (main_window, "delete-event",
			  G_CALLBACK (dlgPreferences_Quit), self);

	/* Setup signals */
	widget = glade_xml_get_widget (priv->gxml, "cmdHelp");

	gtk_widget_hide(widget);
	widget = glade_xml_get_widget (priv->gxml, "dialog-action_area1");
        gtk_button_box_set_layout (GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_END);
        /*
	g_signal_connect (widget, "clicked", G_CALLBACK (cmdHelp_Clicked),
			  self);
         */

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



/*	widget = glade_xml_get_widget (priv->gxml, "cmdAddIndexMailbox");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmdAddIndexMailbox_Clicked), self);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIndexMailbox");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmdRemoveIndexMailbox_Clicked), self);
*/

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
setup_page_general (TrackerPreferences * preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);
	TrackerConfiguration *configuration =
		TRACKER_CONFIGURATION (priv->prefs);

	gboolean value = FALSE;
        gint     sleep = 45;
	GtkWidget *widget = NULL;

        widget = glade_xml_get_widget (priv->gxml, "spnInitialSleep");
        sleep = tracker_configuration_get_int (configuration,"/General/InitialSleep", NULL);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), sleep);
         
	widget = glade_xml_get_widget (priv->gxml, "chkEnableIndexing");
	value = tracker_configuration_get_bool (configuration,
						"/Indexing/EnableIndexing",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "chkEnableWatching");
	value = tracker_configuration_get_bool (configuration,
						"/Watches/EnableWatching",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);


	widget = glade_xml_get_widget (priv->gxml, "comLanguage");
	char *str_value = tracker_configuration_get_string (configuration,
						"/Indexing/Language",
						NULL);

	gint i;
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);

	for (i=0; i<12; i++) {

		if (strcasecmp (tmap[i].lang, str_value) == 0) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
			break;
		}
	}


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
						"/Indexing/Throttle",
						NULL);
	gtk_range_set_value (GTK_RANGE (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "optReducedMemory");
	bvalue = tracker_configuration_get_bool (configuration,
						"/General/LowMemoryMode",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bvalue);

	widget = glade_xml_get_widget (priv->gxml, "optNormal");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !bvalue);

	widget = glade_xml_get_widget (priv->gxml, "spnMaxText");
	value = tracker_configuration_get_int (configuration,
						"/Performance/MaxTextToIndex",
						NULL);

	value = value / 1024;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "spnMaxWords");
	value = tracker_configuration_get_int (configuration,
						"/Performance/MaxWordsToIndex",
						NULL);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

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


	widget = glade_xml_get_widget (priv->gxml, "chkGenerateThumbs");
	value = tracker_configuration_get_bool (configuration,
						"/Indexing/EnableThumbnails",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				      value);

	widget = glade_xml_get_widget (priv->gxml, "chkSkipMountPoints");
	value = tracker_configuration_get_bool (configuration,
						"/Indexing/SkipMountPoints",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !value);


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

	widget = glade_xml_get_widget (priv->gxml, "lstAdditionalPathIndexes");

	initialize_listview (widget);
	populate_list (widget, list);
	g_slist_free (list);



	widget = glade_xml_get_widget (priv->gxml,
				       "lstCrawledPaths");
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
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				      value);
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
cmdClose_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);
	TrackerConfiguration *configuration =
		TRACKER_CONFIGURATION (priv->prefs);

	GSList *list = NULL;
	gboolean value = FALSE;
	gint ivalue;

	/* save general settings */
        widget = glade_xml_get_widget (priv->gxml, "spnInitialSleep");
        gint sleep = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(widget));
        tracker_configuration_set_int (configuration,"/General/InitialSleep", sleep);

	widget = glade_xml_get_widget (priv->gxml, "chkEnableIndexing");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	tracker_configuration_set_bool (configuration,
					"/Indexing/EnableIndexing", value);

	widget = glade_xml_get_widget (priv->gxml, "chkEnableWatching");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	tracker_configuration_set_bool (configuration,
					"/Watches/EnableWatching", value);

	widget = glade_xml_get_widget (priv->gxml, "comLanguage");
	gint i = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

	if (i != -1) {
		tracker_configuration_set_string (configuration,
                                                  "/Indexing/Language",
                                                  tmap[i].lang);
	}


	/* save performance settings */

	widget = glade_xml_get_widget (priv->gxml, "scaThrottle");
	
	ivalue = gtk_range_get_value (GTK_RANGE (widget));

	tracker_configuration_set_int (configuration,
                                       "/Indexing/Throttle",
                                       ivalue);


	widget = glade_xml_get_widget (priv->gxml, "optReducedMemory");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	tracker_configuration_set_bool (configuration,
                                        "/General/LowMemoryMode",
                                        value);


	widget = glade_xml_get_widget (priv->gxml, "spnMaxText");
	ivalue = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(widget));
	ivalue = ivalue * 1024;
	tracker_configuration_set_int (configuration,
                                       "/Performance/MaxTextToIndex",
                                       ivalue);

	
	widget = glade_xml_get_widget (priv->gxml, "spnMaxWords");
	ivalue = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(widget));
	tracker_configuration_set_int (configuration,	
                                       "/Performance/MaxWordsToIndex",
                                       ivalue);


	/* files settings */


	widget = glade_xml_get_widget (priv->gxml, "chkIndexContents");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	tracker_configuration_set_bool (configuration,
					"/Indexing/EnableFileContentIndexing",
					value);

	widget = glade_xml_get_widget (priv->gxml, "chkGenerateThumbs");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	tracker_configuration_set_bool (configuration,
					"/Indexing/EnableThumbnails",
                                        value);

	widget = glade_xml_get_widget (priv->gxml, "chkSkipMountPoints");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	tracker_configuration_set_bool (configuration,
					"/Indexing/SkipMountPoints", !value);

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


	widget = glade_xml_get_widget (priv->gxml,
				       "lstCrawledPaths");
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


	/* email settings */

	widget = glade_xml_get_widget (priv->gxml,
				       "chkEnableEvolutionIndexing");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	tracker_configuration_set_bool (configuration,
					"/Emails/IndexEvolutionEmails",
					value);


	tracker_configuration_write (configuration);
	gtk_main_quit ();
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
}

static void
cmdRemoveCrawledPath_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstCrawledPaths");
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

	if (!g_strcasecmp (path, g_get_home_dir ())) {
		item = glade_xml_get_widget (priv->gxml,
					     "chkIndexHomeDirectory");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);
	} else
		append_item_to_list (self, path, "lstAdditionalPathIndexes");

	g_free (path);
}

static void
cmdRemoveIndexPath_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstAdditionalPathIndexes");
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
}

static void
cmdRemoveIndexMailbox_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstAdditionalMBoxIndexes");
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
}

static void
cmdRemoveIgnorePath_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstIgnorePaths");
}

static void
cmdAddIgnorePattern_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);

	gchar *pattern = tracker_preferences_select_pattern ();

	if (!pattern)
		return;

	append_item_to_list (self, pattern, "lstIgnoreFilePatterns");
}

static void
cmdRemoveIgnorePattern_Clicked (GtkWidget * widget, gpointer data)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (data);
	remove_selection_from_list (self, "lstIgnoreFilePatterns");
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

			if (!g_strcasecmp (item, value))
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
	GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter))
		do {
			gchar *value = NULL;
			gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 0,
					    &value, -1);

			if (value) {
				list = g_slist_prepend (list, g_strdup (value));
                        }
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter));

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
                        gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0, data, -1);
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
