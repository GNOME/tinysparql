/* 
 * Copyright (C) 2007, Saleem Abdulrasool (compnerd@gentoo.org)
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

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libtracker/tracker.h>
#include <libtracker-common/tracker-common.h>

#include "tracker-preferences.h"
#include "tracker-preferences-dialogs.h"

#define TRACKER_DBUS_SERVICE   "org.freedesktop.Tracker"
#define TRACKER_DBUS_PATH      "/org/freedesktop/Tracker"
#define TRACKER_DBUS_INTERFACE "org.freedesktop.Tracker"

#define TRACKER_PREFERENCES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TRACKER_TYPE_PREFERENCES, TrackerPreferencesPrivate))

typedef struct _TrackerPreferencesPrivate TrackerPreferencesPrivate;

struct _TrackerPreferencesPrivate {
	GladeXML *gxml;

        TrackerConfig *config;

	DBusGConnection *connection;
	DBusGProxy *dbus_proxy;
	DBusGProxy *tracker_proxy;

        GtkWidget *main_window;

        gboolean should_restart;
        gboolean should_reindex;
        gboolean should_quit;
        gboolean is_first_time;
};

static void tracker_preferences_finalize (GObject            *object);
static void create_ui                    (TrackerPreferences *preferences);


G_DEFINE_TYPE (TrackerPreferences, tracker_preferences, G_TYPE_OBJECT);

static void
tracker_preferences_class_init (TrackerPreferencesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = tracker_preferences_finalize;

	g_type_class_add_private (object_class, sizeof (TrackerPreferencesPrivate));
}

static void
tracker_preferences_init (TrackerPreferences *object)
{
	TrackerPreferencesPrivate *priv;
        gchar *filename;

        priv = TRACKER_PREFERENCES_GET_PRIVATE (object);
      
        filename = g_build_filename (TRACKER_DATADIR, "tracker-preferences.glade", NULL);
	priv->gxml = glade_xml_new (filename, NULL, NULL);

	if (!priv->gxml) {
		g_error ("Unable to find %s", filename);
                g_free (filename);
                return;
        }

        g_free (filename);

	priv->config = tracker_config_new ();

        priv->is_first_time = TRUE;

        create_ui (object);
}

static void
tracker_preferences_finalize (GObject *object)
{
	TrackerPreferencesPrivate *priv;

        priv = TRACKER_PREFERENCES_GET_PRIVATE (object);

	g_object_unref (priv->config);
	g_object_unref (priv->gxml);

	(G_OBJECT_CLASS (tracker_preferences_parent_class)->finalize) (object);
}

TrackerPreferences *
tracker_preferences_new (void)
{
	return g_object_new (TRACKER_TYPE_PREFERENCES, NULL);
}

static void
set_bool_option (TrackerPreferencesPrivate *priv, 
                 const gchar               *name, 
                 gboolean                   value)
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
set_int_option (TrackerPreferencesPrivate *priv, 
                const gchar               *name, 
                gint                       value)
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
name_owner_changed (DBusGProxy  *proxy,
                    const gchar *name,
		    const gchar *prev_owner, 
                    const gchar *new_owner,
		    gpointer     data)
{
        TrackerPreferencesPrivate *priv;

	if (!g_str_equal (name, TRACKER_DBUS_SERVICE)) {
		return;
        }

        priv = TRACKER_PREFERENCES_GET_PRIVATE (data);

	if (!priv->is_first_time) {
		return;
        }

	if (g_str_equal (new_owner, "")) {
		/* Tracker has exited */
		gchar *command;

                command = g_build_filename (TRACKER_LIBEXECDIR, "trackerd", NULL);

                if (!g_spawn_command_line_async (command, NULL)) {
			g_warning ("Unable to execute command: %s", command);
                }
                
                g_free (command);
                
		priv->is_first_time = FALSE;
                
		if (priv->should_quit) {
			gtk_main_quit ();
                }
	}
}

static gboolean
str_slist_equal (GSList *a, 
                 GSList *b)
{
	GSList *l;

	if (g_slist_length (a) != g_slist_length (b)) {
		return FALSE;
        }

	for (l = a; l; l = l->next) {
                if (!g_slist_find_custom (b, l->data, (GCompareFunc) strcmp)) {
                        return FALSE;
                }
	}

	return TRUE;
}

static gboolean
if_trackerd_start (TrackerPreferencesPrivate *priv)
{
	TrackerClient *client;
	gchar *status;

	client = tracker_connect (FALSE);

	if (!client) {
		return FALSE;
        }

	status = tracker_get_status (client, NULL);
	tracker_disconnect (client);

	if (strcmp (status, "Shutdown") == 0) {
		return FALSE;
	} else {
		return TRUE;
        }
}

static void
restart_tracker (GtkDialog *dialog, 
                 gint       response, 
                 gpointer   data)
{
        TrackerPreferencesPrivate *priv;
        
        priv = TRACKER_PREFERENCES_GET_PRIVATE (data);

	gtk_widget_destroy (GTK_WIDGET (dialog));

	if (response == GTK_RESPONSE_YES) {
		dbus_g_proxy_add_signal (priv->dbus_proxy,
					 "NameOwnerChanged",
					 G_TYPE_STRING,
					 G_TYPE_STRING,
					 G_TYPE_STRING, G_TYPE_INVALID);
		dbus_g_proxy_connect_signal (priv->dbus_proxy,
					     "NameOwnerChanged",
					     G_CALLBACK (name_owner_changed),
					     data, NULL);
		dbus_g_proxy_begin_call (priv->tracker_proxy,
					 "Shutdown",
					 NULL,
					 NULL,
					 NULL,
					 G_TYPE_BOOLEAN,
					 priv->should_reindex, G_TYPE_INVALID);

		priv->is_first_time = TRUE;
	} else if (priv->should_quit) {
		gtk_main_quit ();
	}

        priv->should_restart = FALSE;
        priv->should_reindex = FALSE;
}

static void
model_append_to_list (TrackerPreferences  *preferences, 
                      const gchar * const  item,
                      const gchar * const  widget)
{
	TrackerPreferencesPrivate *priv;
	GtkTreeIter iter;
	GtkWidget *view;
	GtkTreeModel *model;

        priv = TRACKER_PREFERENCES_GET_PRIVATE (preferences);

        view = glade_xml_get_widget (priv->gxml, widget);
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter)) {
		do {
			gchar *value;

			gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 0, &value, -1);

			if (!strcasecmp (item, value)) {
                                g_free (value);
				return;
                        }
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter));
        }

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, item, -1);
}

static void
model_remove_selected_from_list (TrackerPreferences  *preferences,
                                 const gchar * const  widget)
{
	TrackerPreferencesPrivate *priv;
	GtkWidget *view;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

        priv = TRACKER_PREFERENCES_GET_PRIVATE (preferences);

        view = glade_xml_get_widget (priv->gxml, widget);
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
        }

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static GSList *
model_get_values (GtkTreeView *treeview)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *list = NULL;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter)) {
		do {
			gchar *value;

			gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 0, &value, -1);

			if (value) {
				list = g_slist_prepend (list, value);
			}
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter));
        }

        list = g_slist_reverse (list);

	return list;
}

static void
model_set_up (GtkWidget *treeview)
{
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
				 GTK_TREE_MODEL (store));
	g_object_unref (store);	

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Column 0", renderer,
							   "text", 0,
                                                           NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
}

static void
model_populate (GtkWidget *treeview, 
                GSList    *list)
{
	GtkTreeModel *store;
	GSList *l;

	store = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

	for (l = list; l; l = l->next) {
                GtkTreeIter iter;
                gchar *data;

		if (!l->data) {
                        continue;
                }

                data = l->data;

                gtk_list_store_append (GTK_LIST_STORE (store), &iter);
                gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0, data, -1);
	}
}

static void
cmd_help (GtkWidget *widget,
          gpointer   data)
{
}

static void
cmd_apply (GtkWidget *widget, 
           gpointer   data)
{
	TrackerPreferencesPrivate *priv;
	GSList *list;
	GSList *list_old;
        gchar *lang_code;
        const gchar *lang_code_old;
	gboolean bvalue, bvalue_old;
	gint ivalue, ivalue_old;
        GtkTreeIter   iter;
        GtkTreeModel *model;

        priv = TRACKER_PREFERENCES_GET_PRIVATE (data);

	/* Save general settings */
	widget = glade_xml_get_widget (priv->gxml, "spnInitialSleep");
        ivalue_old = tracker_config_get_initial_sleep (priv->config);
	ivalue = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
        if (ivalue_old != ivalue) {
                tracker_config_set_initial_sleep (priv->config, ivalue);
        }

	widget = glade_xml_get_widget (priv->gxml, "chkEnableIndexing");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_config_get_enable_indexing (priv->config);

	if (bvalue != bvalue_old) {
		priv->should_restart = TRUE;
		set_bool_option (priv, "EnableIndexing", bvalue);
		tracker_config_set_enable_indexing (priv->config, bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "comLanguage");

        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
        
        gtk_tree_model_get (model, &iter, 1, &lang_code, -1);
        lang_code_old = tracker_config_get_language (priv->config);

        if (lang_code 
            && lang_code_old 
            && strcmp (lang_code, lang_code_old) == 0) {
                /* Same, do nothing */
        } else {
                priv->should_restart = TRUE;
                priv->should_reindex = TRUE;

                /* Note, language can be NULL??? */
		tracker_config_set_language (priv->config, lang_code);
        }
        
	widget = glade_xml_get_widget (priv->gxml, "chkDisableBatteryIndex");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_config_get_disable_indexing_on_battery (priv->config);

	if (bvalue != bvalue_old) {
		set_bool_option (priv, "BatteryIndex", bvalue);
                tracker_config_set_disable_indexing_on_battery (priv->config, bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "chkDisableBatteryInitialIndex");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
        bvalue_old = tracker_config_get_disable_indexing_on_battery_init (priv->config);

	if (bvalue != bvalue_old) {
		set_bool_option (priv, "BatteryIndexInitial", bvalue);
                tracker_config_set_disable_indexing_on_battery_init (priv->config, bvalue);
	}

	/* Files settings */
	widget = glade_xml_get_widget (priv->gxml, "chkIndexContents");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
        bvalue_old = tracker_config_get_enable_content_indexing (priv->config);

	if (bvalue != bvalue_old) {
		priv->should_restart = TRUE;
		priv->should_reindex = TRUE;
		set_bool_option (priv, "IndexFileContents", bvalue);
                tracker_config_set_enable_content_indexing (priv->config, bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "chkGenerateThumbs");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
        bvalue_old = tracker_config_get_enable_thumbnails (priv->config);

	if (bvalue != bvalue_old) {
		priv->should_restart = TRUE;
		priv->should_reindex = TRUE;
		set_bool_option (priv, "GenerateThumbs", bvalue);
                tracker_config_set_enable_thumbnails (priv->config, bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "chkSkipMountPoints");
	bvalue = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
        bvalue_old = !tracker_config_get_index_mounted_directories (priv->config);

	if (bvalue != bvalue_old) {
		priv->should_restart = TRUE;
		set_bool_option (priv, "SkipMountPoints", bvalue);
                tracker_config_set_index_mounted_directories (priv->config, !bvalue);
                tracker_config_set_index_removable_devices (priv->config, !bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "lstAdditionalPathIndexes");
	list = model_get_values (GTK_TREE_VIEW (widget));

	widget = glade_xml_get_widget (priv->gxml, "chkIndexHomeDirectory");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
		list = g_slist_prepend (list, g_strdup (g_get_home_dir ()));
	}

        list_old = tracker_config_get_watch_directory_roots (priv->config);
	if (!str_slist_equal (list, list_old)) {
		priv->should_restart = TRUE;
		tracker_config_set_watch_directory_roots (priv->config, list);
	}

	g_slist_free (list);

	widget = glade_xml_get_widget (priv->gxml, "lstCrawledPaths");
	list = model_get_values (GTK_TREE_VIEW (widget));

        list_old = tracker_config_get_crawl_directory_roots (priv->config);
	if (!str_slist_equal (list, list_old)) {
		priv->should_restart = TRUE;
		tracker_config_set_crawl_directory_roots (priv->config, list);
	}

	g_slist_free (list);

	/* Ignored files settings */
	widget = glade_xml_get_widget (priv->gxml, "lstIgnorePaths");
	list = model_get_values (GTK_TREE_VIEW (widget));

        list_old = tracker_config_get_no_watch_directory_roots (priv->config);
	if (!str_slist_equal (list, list_old)) {
		priv->should_restart = TRUE;
		tracker_config_set_no_watch_directory_roots (priv->config, list);
	}

	g_slist_free (list);

	widget = glade_xml_get_widget (priv->gxml, "lstIgnoreFilePatterns");
	list = model_get_values (GTK_TREE_VIEW (widget));

        list_old = tracker_config_get_no_index_file_types (priv->config);
	if (!str_slist_equal (list, list_old)) {
		priv->should_restart = TRUE;
		tracker_config_set_no_index_file_types (priv->config, list);
	}

	g_slist_free (list);

	/* Email settings */
	widget = glade_xml_get_widget (priv->gxml, "chkEnableEvolutionIndexing");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

        list = tracker_config_get_disabled_modules (priv->config);
        bvalue_old = !tracker_string_in_gslist ("evolution", list);
	if (bvalue != bvalue_old) {

                priv->should_restart = TRUE;
                if (bvalue) {
                        /* Activated evolution module (was disabled)*/
                        GSList *evo_module;

                        evo_module = g_slist_find_custom (list, "evolution", (GCompareFunc)strcmp);
                        list = g_slist_remove_link (list, evo_module);
                        tracker_config_set_disabled_modules (priv->config, list);
                } else {
                        /*
                         * Desactivated evolution module (was enabled)
                         * Force reindex to remove emails from the DBs
                         */
                        priv->should_reindex = TRUE;
                
                        list = g_slist_prepend (list, "evolution");
                        tracker_config_set_disabled_modules (priv->config, list);
                }
	}

	/* Performance settings */
	widget = glade_xml_get_widget (priv->gxml, "scaThrottle");
	ivalue = gtk_range_get_value (GTK_RANGE (widget));
	ivalue_old = tracker_config_get_throttle (priv->config);

	if (ivalue != ivalue_old) {
		set_int_option (priv, "Throttle", ivalue);
		tracker_config_set_throttle (priv->config, ivalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "optReducedMemory");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_config_get_low_memory_mode (priv->config);

	if (bvalue != bvalue_old) {
		set_bool_option (priv, "LowMemoryMode", bvalue);
		tracker_config_set_low_memory_mode (priv->config, bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "chkFastMerges");
	bvalue = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	bvalue_old = tracker_config_get_fast_merges (priv->config);

	if (bvalue != bvalue_old) {
		set_bool_option (priv, "FastMerges", bvalue);
		tracker_config_set_fast_merges (priv->config, bvalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "spnMaxText");
	ivalue = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget))*1024;
	ivalue_old = tracker_config_get_max_text_to_index (priv->config);

	if (ivalue != ivalue_old) {
		set_int_option (priv, "MaxText", ivalue);
		tracker_config_set_max_text_to_index (priv->config, ivalue);
	}

	widget = glade_xml_get_widget (priv->gxml, "spnMaxWords");
	ivalue = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
	ivalue_old = tracker_config_get_max_words_to_index (priv->config);

	if (ivalue != ivalue_old) {
		set_int_option (priv, "MaxWords", ivalue);
		tracker_config_set_max_words_to_index (priv->config, ivalue);
	}

	/* save config to distk */
	tracker_config_save (priv->config);

	if (priv->should_restart && if_trackerd_start (priv)) {
		GtkWidget *dialog;
		gchar *primary;
		gchar *secondary;
		gchar *button;
                
		if (priv->should_reindex) {
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

		dialog = gtk_message_dialog_new (GTK_WINDOW (priv->main_window),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_NONE,
                                                 "%s",
						 primary);

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          "%s",
							  secondary);

		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					GTK_STOCK_NO, GTK_RESPONSE_NO,
					button, GTK_RESPONSE_YES, NULL);

		g_free (primary);
		g_free (secondary);
		g_free (button);

		gtk_window_set_title (GTK_WINDOW (dialog), "");
		gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
		gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

		g_signal_connect (G_OBJECT (dialog), "response", 
                                  G_CALLBACK (restart_tracker), data);

		gtk_widget_show (dialog);
	} else if (priv->should_quit) {
		gtk_main_quit ();
	}
}

static void
cmd_cancel (GtkWidget *widget, 
            gpointer   data)
{
	gtk_main_quit ();
}

static void
cmd_ok (GtkWidget *widget, 
        gpointer   data)
{
        TrackerPreferencesPrivate *priv;

        priv = TRACKER_PREFERENCES_GET_PRIVATE (data);

	priv->should_quit = TRUE;

	cmd_apply (widget, data);
}

static void
cmd_quit (GtkWidget *widget, 
          GdkEvent  *event, 
          gpointer   data)
{
	cmd_ok (NULL, data);
}

static void
cmd_add_crawled_path (GtkWidget *widget,
                      gpointer   data)
{
	gchar *path;

        path = tracker_preferences_select_folder ();

	if (!path) {
		return;
        }

	model_append_to_list (data, path, "lstCrawledPaths");

	g_free (path);
}

static void
cmd_remove_crawled_path (GtkWidget *widget,
                         gpointer   data)
{
	model_remove_selected_from_list (data, "lstCrawledPaths");
}

static void
cmd_add_index_path (GtkWidget *widget, 
                    gpointer   data)
{
	TrackerPreferencesPrivate *priv;
	gchar *path;
        
        priv = TRACKER_PREFERENCES_GET_PRIVATE (data);

        path = tracker_preferences_select_folder ();
	if (!path) {
		return;
        }

	if (!strcasecmp (path, g_get_home_dir ())) {
                GtkWidget *item;

		item = glade_xml_get_widget (priv->gxml,
					     "chkIndexHomeDirectory");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);
	} else {
		model_append_to_list (data, path, "lstAdditionalPathIndexes");
        }

	g_free (path);
}

static void
cmd_remove_index_path (GtkWidget *widget, 
                       gpointer   data)
{
	model_remove_selected_from_list (data, "lstAdditionalPathIndexes");
}

static void
cmd_add_index_mailbox (GtkWidget *widget, 
                       gpointer   data)
{
	gchar *path;

        path = tracker_preferences_select_folder ();

	if (!path) {
		return;
        }

	model_append_to_list (data, path, "lstAdditionalMBoxIndexes");

	g_free (path);
}

static void
cmd_remove_index_mailbox (GtkWidget *widget,
                          gpointer   data)
{
	model_remove_selected_from_list (data, "lstAdditionalMBoxIndexes");
}

static void
cmd_add_ignore_path (GtkWidget *widget,
                     gpointer   data)
{
	gchar *path;

        path = tracker_preferences_select_folder ();

	if (!path) {
		return;
        }

	model_append_to_list (data, path, "lstIgnorePaths");

	g_free (path);
}

static void
cmd_remove_ignore_path (GtkWidget *widget,
                        gpointer   data)
{
	model_remove_selected_from_list (data, "lstIgnorePaths");
}

static void
cmd_add_ignore_pattern (GtkWidget *widget, 
                        gpointer   data)
{
	gchar *pattern;

        pattern = tracker_preferences_select_pattern ();

	if (!pattern) {
		return;
        }

	model_append_to_list (data, pattern, "lstIgnoreFilePatterns");
}

static void
cmd_remove_ignore_pattern (GtkWidget *widget,
                           gpointer   data)
{
	model_remove_selected_from_list (data, "lstIgnoreFilePatterns");
}

static void
setup_page_general (TrackerPreferences *preferences)
{
	TrackerPreferencesPrivate *priv;
	GtkWidget *widget;
        const gchar *language;
        GSList *language_codes, *l;
        GtkTreeStore *language_model;
	gboolean value;
	gint sleep;

        priv = TRACKER_PREFERENCES_GET_PRIVATE (preferences);

	widget = glade_xml_get_widget (priv->gxml, "spnInitialSleep");
	sleep = tracker_config_get_initial_sleep (priv->config);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), sleep);

	widget = glade_xml_get_widget (priv->gxml, "chkEnableIndexing");
	value = tracker_config_get_enable_indexing (priv->config);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "comLanguage");
        language_model = gtk_tree_store_new (2, 
                                             G_TYPE_STRING, 
                                             G_TYPE_STRING);
        gtk_combo_box_set_model (GTK_COMBO_BOX (widget), 
                                 GTK_TREE_MODEL (language_model));

	language = tracker_config_get_language (priv->config);
	if (!language) {
		/* No value for language? Default to "en" */
		language = tracker_language_get_default_code ();
	}
        
        language_codes = tracker_language_get_all_by_code ();
        
	for (l = language_codes; l; l = l->next) {

                GtkTreeIter iter;
                gtk_tree_store_append (language_model, &iter, NULL);
                gtk_tree_store_set (language_model, &iter, 
                                    0, tracker_language_get_name_by_code (l->data), 
                                    1, l->data,
                                    -1);

		if (strcasecmp (language, l->data) == 0) {
                        gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 
                                                  g_slist_index (language_codes, l->data));
                }
	}

	widget = glade_xml_get_widget (priv->gxml, "chkDisableBatteryIndex");
	value = tracker_config_get_disable_indexing_on_battery (priv->config);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "chkDisableBatteryInitialIndex");
	value = tracker_config_get_disable_indexing_on_battery_init (priv->config);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);
}

static void
setup_page_performance (TrackerPreferences *preferences)
{
	TrackerPreferencesPrivate *priv;
	GtkWidget *widget;
	gint value;
	gboolean bvalue;

        priv = TRACKER_PREFERENCES_GET_PRIVATE (preferences);

	widget = glade_xml_get_widget (priv->gxml, "scaThrottle");
	value = tracker_config_get_throttle (priv->config);
	gtk_range_set_value (GTK_RANGE (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "optReducedMemory");
	bvalue = tracker_config_get_low_memory_mode (priv->config);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bvalue);

	widget = glade_xml_get_widget (priv->gxml, "optNormal");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !bvalue);

	widget = glade_xml_get_widget (priv->gxml, "chkFastMerges");
	bvalue = tracker_config_get_fast_merges (priv->config);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bvalue);

	widget = glade_xml_get_widget (priv->gxml, "spnMaxText");
	value = tracker_config_get_max_text_to_index (priv->config);

	value /= 1024;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "spnMaxWords");
	value = tracker_config_get_max_words_to_index (priv->config);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
}

static void
setup_page_files (TrackerPreferences *preferences)
{
	TrackerPreferencesPrivate *priv;
	GtkWidget *widget;
	GSList *list, *list_copy, *l;
	gboolean value;

        priv = TRACKER_PREFERENCES_GET_PRIVATE (preferences);

	widget = glade_xml_get_widget (priv->gxml, "chkIndexContents");
	value = tracker_config_get_enable_content_indexing (priv->config);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "chkGenerateThumbs");
        value = tracker_config_get_enable_thumbnails (priv->config);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	widget = glade_xml_get_widget (priv->gxml, "chkSkipMountPoints");
	value = !tracker_config_get_index_mounted_directories (priv->config);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !value);

	widget = glade_xml_get_widget (priv->gxml,
				       "lstAdditionalPathIndexes");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_config_get_watch_directory_roots (priv->config);
        list_copy = tracker_gslist_copy_with_string_data (list);

	widget = glade_xml_get_widget (priv->gxml, "chkIndexHomeDirectory");
        l = g_slist_find_custom (list_copy, g_get_home_dir (), (GCompareFunc) strcmp);

        if (l) {
                g_free (l->data);
                list_copy = g_slist_delete_link (list_copy, l);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
	}

	widget = glade_xml_get_widget (priv->gxml, "lstAdditionalPathIndexes");

	model_set_up (widget);
	model_populate (widget, list_copy);

        g_slist_foreach (list_copy, (GFunc) g_free, NULL);
        g_slist_free (list_copy);

	widget = glade_xml_get_widget (priv->gxml, "lstCrawledPaths");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_config_get_crawl_directory_roots (priv->config);

	model_set_up (widget);
	model_populate (widget, list);
}

static void
setup_page_ignored_files (TrackerPreferences *preferences)
{
	TrackerPreferencesPrivate *priv;
	GtkWidget *widget;
	GSList *list;

        priv = TRACKER_PREFERENCES_GET_PRIVATE (preferences);

	/* Ignore Paths */
	widget = glade_xml_get_widget (priv->gxml, "lstIgnorePaths");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_config_get_no_watch_directory_roots (priv->config);

	model_set_up (widget);
	model_populate (widget, list);

	/* Ignore File Patterns */
	widget = glade_xml_get_widget (priv->gxml, "lstIgnoreFilePatterns");
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	list = tracker_config_get_no_index_file_types (priv->config);

	model_set_up (widget);
	model_populate (widget, list);
}

static void
setup_page_emails (TrackerPreferences *preferences)
{
	TrackerPreferencesPrivate *priv;
	GtkWidget *widget;
        gboolean no_evo;
        GSList  *disabled_mods;

        priv = TRACKER_PREFERENCES_GET_PRIVATE (preferences);

	widget = glade_xml_get_widget (priv->gxml,
				       "chkEnableEvolutionIndexing");

        disabled_mods = tracker_config_get_disabled_modules (priv->config);
        no_evo = tracker_string_in_gslist ("evolution", disabled_mods);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), 
                                      !no_evo);
}

static void
create_ui (TrackerPreferences *preferences)
{
        TrackerPreferencesPrivate *priv;
	GError *error = NULL;
        GtkWidget *widget;

        priv = TRACKER_PREFERENCES_GET_PRIVATE (preferences);

	priv->main_window = glade_xml_get_widget (priv->gxml, "dlgPreferences");

	/* Hide window first to allow the preferences to reize itself without redrawing */
	gtk_widget_hide (priv->main_window);

	gtk_window_set_icon_name (GTK_WINDOW (priv->main_window), "tracker");
	g_signal_connect (priv->main_window, "delete-event",
			  G_CALLBACK (cmd_quit), preferences);

	/* Setup signals */
	widget = glade_xml_get_widget (priv->gxml, "cmdHelp");
	g_signal_connect (widget, "clicked", 
                          G_CALLBACK (cmd_help), preferences);
	gtk_widget_hide (widget);

	widget = glade_xml_get_widget (priv->gxml, "dialog-action_area1");
	gtk_button_box_set_layout (GTK_BUTTON_BOX (widget),
				   GTK_BUTTONBOX_END);

	widget = glade_xml_get_widget (priv->gxml, "cmdApply");
	g_signal_connect (widget, "clicked",
                          G_CALLBACK (cmd_apply), preferences);

	widget = glade_xml_get_widget (priv->gxml, "cmdCancel");
	g_signal_connect (widget, "clicked", 
                          G_CALLBACK (cmd_cancel), preferences);

	widget = glade_xml_get_widget (priv->gxml, "cmdOK");
	g_signal_connect (widget, "clicked", G_CALLBACK (cmd_ok),
			  preferences);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddIndexPath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmd_add_index_path), preferences);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIndexPath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmd_remove_index_path), preferences);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddCrawledPath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmd_add_crawled_path), preferences);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveCrawledPath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmd_remove_crawled_path),
                          preferences);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddIndexMailbox");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmd_add_index_mailbox), preferences);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIndexMailbox");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmd_remove_index_mailbox), preferences);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddIgnorePath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmd_add_ignore_path), preferences);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIgnorePath");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmd_remove_ignore_path), preferences);

	widget = glade_xml_get_widget (priv->gxml, "cmdAddIgnorePattern");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmd_add_ignore_pattern), preferences);

	widget = glade_xml_get_widget (priv->gxml, "cmdRemoveIgnorePattern");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cmd_remove_ignore_pattern), preferences);

	/* Init dbus */
	priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (priv->connection == NULL) {
		g_warning ("Unable to connect to dbus, %s",
                           error->message);
		g_error_free (error);
		return;
	}

	priv->dbus_proxy = dbus_g_proxy_new_for_name (priv->connection,
						      DBUS_SERVICE_DBUS,
						      DBUS_PATH_DBUS,
						      DBUS_INTERFACE_DBUS);

	if (!priv->dbus_proxy) {
		g_warning ("Could not create proxy for '%s'", 
                           DBUS_SERVICE_DBUS);
		return;
	}

	priv->tracker_proxy = dbus_g_proxy_new_for_name (priv->connection,
							 TRACKER_DBUS_SERVICE,
							 TRACKER_DBUS_PATH,
							 TRACKER_DBUS_INTERFACE);

	if (!priv->tracker_proxy) {
		g_warning ("Could not create proxy for '%s'", 
                           TRACKER_DBUS_SERVICE);
		return;
	}

	/* Setup pages */
	setup_page_general (preferences);
	setup_page_files (preferences);
	setup_page_emails (preferences);
	setup_page_ignored_files (preferences);
	setup_page_performance (preferences);

	gtk_widget_show (priv->main_window);
}
