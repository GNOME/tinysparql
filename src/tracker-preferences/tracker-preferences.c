#include "tracker-preferences.h"
#include "tracker-preferences-private.h"

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

	gtk_window_set_icon_from_file(GTK_WINDOW(main_window),
				      PIXMAPS_DIR "/tracker.png", NULL);
	g_signal_connect (main_window, "delete-event",
			  G_CALLBACK (dlgPreferences_Quit), self);

	/* Setup signals */
	widget = glade_xml_get_widget (priv->gxml, "cmdHelp");

	gtk_widget_hide(widget);
	widget = glade_xml_get_widget (priv->gxml, "dialog-action_rea1");
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

	/* setup pages */
	setup_page_general (self);
	setup_page_indexing (self);
	setup_page_privacy (self);
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
	GtkWidget *widget = NULL;

	/* TODO :: Detect autostarting */
	widget = glade_xml_get_widget (priv->gxml, "fraStartup");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);

	widget = glade_xml_get_widget (priv->gxml, "chkIndexFileContents");
	value = tracker_configuration_get_bool (configuration,
						"/Indexing/EnableFileContentIndexing",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "chkGenerateThumbnails");
	if (convert_available ()) {
		value = tracker_configuration_get_bool (configuration,
							"/Indexing/EnableThumbnails",
							NULL);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      value);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	}
}

static void
setup_page_indexing (TrackerPreferences * preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);
	TrackerConfiguration *configuration =
		TRACKER_CONFIGURATION (priv->prefs);

	GSList *list = NULL;
	gboolean value = FALSE;
	GtkWidget *widget = NULL;
	guint available_services = 0;

	widget = glade_xml_get_widget (priv->gxml, "chkEnableIndexing");
	value = tracker_configuration_get_bool (configuration,
						"/Indexing/EnableIndexing",
						NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	if (!value) {
		widget = glade_xml_get_widget (priv->gxml,
					       "fraGeneralIndexing");
		gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
		widget = glade_xml_get_widget (priv->gxml, "fraServices");
		gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
		widget = glade_xml_get_widget (priv->gxml,
					       "fraIndexableMBoxes");
		gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);

		return;
	}

	widget = glade_xml_get_widget (priv->gxml,
				       "lstAdditionalPathIndexes");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_configuration_get_list (configuration,
					       "/Watches/WatchDirectoryRoots",
					       G_TYPE_STRING, NULL);

	GSList *entry =
		g_slist_find_custom (list, g_get_home_dir (), _strcmp);
	if (entry) {
		list = g_slist_delete_link (list, entry);

		widget = glade_xml_get_widget (priv->gxml,
					       "chkIndexHomeDirectory");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      TRUE);

		widget = glade_xml_get_widget (priv->gxml,
					       "lstAdditionalPathIndexes");
	}

	initialize_listview (widget);
	populate_list (widget, list);
	g_slist_free (list);

	widget = glade_xml_get_widget (priv->gxml,
				       "chkEnableEvolutionIndexing");
	if (evolution_available ()) {
		value = tracker_configuration_get_bool (configuration,
							"/Services/IndexEvolutionEmails",
							NULL);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      value);
		available_services++;
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	}

	widget = glade_xml_get_widget (priv->gxml,
				       "chkEnableThunderbirdIndexing");
	if (thunderbird_available ()) {
		value = tracker_configuration_get_bool (configuration,
							"/Services/IndexThunderbirdEmails",
							NULL);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      value);
		available_services++;
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	}

	widget = glade_xml_get_widget (priv->gxml, "chkEnableKMailIndexing");
	if (kmail_available ()) {
		value = tracker_configuration_get_bool (configuration,
							"/Services/IndexKmailEmails",
							NULL);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      value);
		available_services++;
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	}

	if (!available_services) {
		widget = glade_xml_get_widget (priv->gxml, "fraServices");
		gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	}

	widget = glade_xml_get_widget (priv->gxml,
				       "lstAdditionalMBoxIndexes");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_configuration_get_list (configuration,
					       "/Emails/AdditionalMBoxesToIndex",
					       G_TYPE_STRING, NULL);

	initialize_listview (widget);
	populate_list (widget, list);
	g_slist_free (list);
}

static void
setup_page_privacy (TrackerPreferences * preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);
	TrackerConfiguration *configuration =
		TRACKER_CONFIGURATION (priv->prefs);

	GSList *list = NULL;
	gboolean value = FALSE;
	GtkWidget *widget = NULL;

	/* Indexing Enabled */
	value = tracker_configuration_get_bool (configuration,
						"/Indexing/EnableIndexing",
						NULL);
	if (!value) {
		widget = glade_xml_get_widget (priv->gxml, "fraIgnoredPaths");
		gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
		widget = glade_xml_get_widget (priv->gxml,
					       "fraIgnoredPatterns");
		gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);

		return;
	}

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
setup_page_performance (TrackerPreferences * preferences)
{
	TrackerPreferences *self = TRACKER_PREFERENCES (preferences);
	TrackerPreferencesPrivate *priv =
		TRACKER_PREFERENCES_GET_PRIVATE (self);
	TrackerConfiguration *configuration =
		TRACKER_CONFIGURATION (priv->prefs);

	GtkWidget *widget = NULL;

	/* There is nothing usable on this page yet :-( */
	widget = glade_xml_get_widget (priv->gxml, "nbPreferences");
	widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (widget), 3);
	gtk_widget_set (GTK_WIDGET (widget), "visible", FALSE, NULL);

#if 0
	/* Disable until we can check if polling is used */
	widget = glade_xml_get_widget (priv->gxml, "fraPolling");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);

	/* Disable until this can be set in the config */
	widget = glade_xml_get_widget (priv->gxml, "fraThrottling");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);

	/* Disable until this can be set in the config */
	widget = glade_xml_get_widget (priv->gxml, "optReducedMemory");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);

	widget = glade_xml_get_widget (priv->gxml, "optNormal");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	/* Disable until this can be set in the config */
	widget = glade_xml_get_widget (priv->gxml, "optTurbo");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
#endif
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
	GtkWidget *item = NULL;

	widget = glade_xml_get_widget (priv->gxml, "chkIndexFileContents");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	tracker_configuration_set_bool (configuration,
					"/Indexing/EnableFileContentIndexing",
					value);

	widget = glade_xml_get_widget (priv->gxml, "chkGenerateThumbnails");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	tracker_configuration_set_bool (configuration,
					"/Indexing/EnableThumbnails", value);

	widget = glade_xml_get_widget (priv->gxml, "chkEnableIndexing");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	tracker_configuration_set_bool (configuration,
					"/Indexing/EnableIndexing", value);

	widget = glade_xml_get_widget (priv->gxml,
				       "lstAdditionalPathIndexes");
	list = treeview_get_values (GTK_TREE_VIEW (widget));

	widget = glade_xml_get_widget (priv->gxml, "chkIndexHomeDirectory");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		list = g_slist_insert (list, g_strdup (g_get_home_dir ()), 0);

	tracker_configuration_set_list (configuration,
					"/Watches/WatchDirectoryRoots", list,
					G_TYPE_STRING);
	g_slist_free (list);
	list = NULL;

	widget = glade_xml_get_widget (priv->gxml,
				       "chkEnableEvolutionIndexing");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	tracker_configuration_set_bool (configuration,
					"/Services/IndexEvolutionEmails",
					value);

	widget = glade_xml_get_widget (priv->gxml,
				       "chkEnableThunderbirdIndexing");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	tracker_configuration_set_bool (configuration,
					"/Services/IndexThunderbirdEmails",
					value);

	widget = glade_xml_get_widget (priv->gxml, "chkEnableKMailIndexing");
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	tracker_configuration_set_bool (configuration,
					"/Services/IndexKmailEmails", value);

	widget = glade_xml_get_widget (priv->gxml,
				       "lstAdditionalMBoxIndexes");
	list = treeview_get_values (GTK_TREE_VIEW (widget));
	tracker_configuration_set_list (configuration,
					"/Emails/AdditionalMBoxesToIndex",
					list, G_TYPE_STRING);
	g_slist_free (list);
	list = NULL;

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
	gtk_main_quit ();
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
	GSList *list = g_slist_alloc ();
	GtkTreeModel *model =
		gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter))
		do {
			gchar *value = NULL;
			gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 0,
					    &value, -1);

			if (value)
				list = g_slist_insert (list, g_strdup (value),
						       0);
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

	return g_strncasecmp (a, b, strlen (b));
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
	g_object_unref (store);	/* this will delete the store when the view is destoryed */

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Column 0",
							   renderer, "text",
							   0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
}

static void
populate_list (GtkWidget * treeview, GSList * list)
{
	guint index = 0;
	GtkTreeIter iter;
	gchar *data = NULL;
	GtkTreeModel *store = NULL;

	store = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

	for (; index < g_slist_length (list); ++index) {
		data = g_slist_nth_data (list, index);

		if (!data)
			continue;

		gtk_list_store_append (GTK_LIST_STORE (store), &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0, data,
				    -1);
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
