/*
 * Copyright (C) 2009  Debarshi Ray <debarshir@src.gnome.org>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "config.h"

#include <string.h>

#include <libnautilus-extension/nautilus-file-info.h>
#include <libtracker-client/tracker.h>

#include "tracker-tags-add-dialog.h"
#include "tracker-tags-utils.h"
#include "tracker-tags-view.h"

#define TRACKER_TAGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TRACKER_TYPE_TAGS_VIEW, TrackerTagsViewPrivate))

struct _TrackerTagsViewPrivate {
	TrackerClient *tracker_client;
	GList *files;
	GtkListStore *list_store;
	GtkWidget *remove_button;
	gchar *selected_tag_label;
};

enum {
	COLUMN_SELECTED,
	COLUMN_TAG_NAME,
	N_COLUMNS
};

enum {
	SELECTED_INCONSISTENT = -1,
	SELECTED_FALSE = 0,
	SELECTED_TRUE
};

G_DEFINE_TYPE (TrackerTagsView, tracker_tags_view, GTK_TYPE_VBOX);

/* Copied from src/tracker-utils/tracker-tags.c */
static gchar *
get_filter_string (GStrv        files,
		   const gchar *tag)
{
	GString *filter;
	gint i, len;

	if (!files) {
		return NULL;
	}

	len = g_strv_length (files);

	if (len < 1) {
		return NULL;
	}

	filter = g_string_new ("");

	g_string_append_printf (filter, "FILTER (");

	if (tag) {
		g_string_append (filter, "(");
	}

	for (i = 0; i < len; i++) {
		g_string_append_printf (filter, "?f = <%s>", files[i]);

		if (i < len - 1) {
			g_string_append (filter, " || ");
		}
	}

	if (tag) {
		g_string_append_printf (filter, ") && ?t = <%s>", tag);
	}

	g_string_append (filter, ")");

	return g_string_free (filter, FALSE);
}

static void
tracker_tags_view_check_foreach (gpointer data, 
                                 gpointer user_data)
{
	const GStrv element = data;
	TrackerTagsView *view = TRACKER_TAGS_VIEW (user_data);
	GList *node;

	for (node = view->private->files; node; node = g_list_next (node)) {
		gchar *uri;
		gint cmp;

		uri = nautilus_file_info_get_uri (NAUTILUS_FILE_INFO (node->data));
		cmp = g_strcmp0 (element[0], uri);
		g_free (uri);

		if (cmp == 0) {
			view->private->files = g_list_delete_link (view->private->files, node);
			break;
		}
	}
}

static void
tracker_tags_view_query_each_tag_finished (GPtrArray *result, 
                                           GError    *error, 
                                           gpointer   user_data)
{
	void **const arg = user_data;
	TrackerTagsView *const view = TRACKER_TAGS_VIEW (arg[0]);
	GtkTreeIter *const iter = arg[1];
	GList *files;
	guint post_num;
	guint pre_num;

	g_free (arg);

	if (error != NULL) {
		if (error->message != NULL) {
			GtkWidget *error_dialog;

			error_dialog = gtk_message_dialog_new (NULL, 
			                                       GTK_DIALOG_NO_SEPARATOR, 
			                                       GTK_MESSAGE_ERROR, 
			                                       GTK_BUTTONS_OK, 
			                                       "%s", 
			                                       error->message);
			g_signal_connect (error_dialog, "response", 
			                  G_CALLBACK (gtk_widget_destroy), NULL);
			gtk_dialog_run (GTK_DIALOG (error_dialog));
		}
		g_error_free (error);
		goto end;
	}

	files = g_list_copy (view->private->files);

	pre_num = g_list_length (view->private->files);
	g_ptr_array_foreach (result, tracker_tags_view_check_foreach, view);
	g_ptr_array_foreach (result, (GFunc) g_strfreev, NULL);
	g_ptr_array_free (result, TRUE);
	post_num = g_list_length (view->private->files);

	if (pre_num == post_num) {
		gtk_list_store_set (view->private->list_store, iter, 
		                    COLUMN_SELECTED, SELECTED_FALSE, 
		                    -1);
	} else if (post_num == 0) {
		gtk_list_store_set (view->private->list_store, iter, 
		                    COLUMN_SELECTED, SELECTED_TRUE, 
		                    -1);
	} else {
		gtk_list_store_set (view->private->list_store, iter, 
		                    COLUMN_SELECTED, SELECTED_INCONSISTENT, 
		                    -1);
	}

	g_list_free (view->private->files);
	view->private->files = files;

end:
	gtk_tree_iter_free (iter);
}

static void
tracker_tags_view_append_foreach (gpointer data, 
                                  gpointer user_data)
{
	const GStrv element = data;
	TrackerTagsView *view = TRACKER_TAGS_VIEW (user_data);
	GtkTreeIter iter;
	gchar *query;
	gchar *tag_escaped;
	void **arg;

	gtk_list_store_append (view->private->list_store, &iter);
	gtk_list_store_set (view->private->list_store, &iter, 
	                    COLUMN_SELECTED, SELECTED_FALSE, 
	                    COLUMN_TAG_NAME, element[1], 
	                    -1);

	tag_escaped = tracker_tags_escape_sparql_string (element[0]);
	query = g_strdup_printf ("SELECT ?f "
				 "WHERE {"
				 "  ?f a rdfs:Resource ;"
				 "  nao:hasTag %s ."
				 "}",
				 tag_escaped);
	g_free (tag_escaped);
	arg = g_new (void *, 2);
	arg[0] = view;
	arg[1] = gtk_tree_iter_copy (&iter);
	tracker_resources_sparql_query_async (view->private->tracker_client,
                                              query,
                                              tracker_tags_view_query_each_tag_finished,
                                              arg);
	g_free (query);
}

static void
tracker_tags_view_query_all_tags_finished (GPtrArray *result, 
                                           GError    *error, 
                                           gpointer   user_data)
{
	TrackerTagsView *view = TRACKER_TAGS_VIEW (user_data);

	if (error != NULL) {
		if (error->message != NULL) {
			GtkWidget *error_dialog;

			error_dialog = gtk_message_dialog_new (NULL,
                                                               GTK_DIALOG_NO_SEPARATOR,
                                                               GTK_MESSAGE_ERROR,
                                                               GTK_BUTTONS_OK,
                                                               "%s",
                                                               error->message);
			g_signal_connect (error_dialog, "response", 
			                  G_CALLBACK (gtk_widget_destroy), NULL);
			gtk_dialog_run (GTK_DIALOG (error_dialog));
		}
		g_error_free (error);
		return;
	}

	g_ptr_array_foreach (result, tracker_tags_view_append_foreach, view);
	g_ptr_array_foreach (result, (GFunc) g_strfreev, NULL);
	g_ptr_array_free (result, TRUE);
}

static void
tracker_tags_view_toggle_cell_data_func (GtkTreeViewColumn *column,
					 GtkCellRenderer   *cell_renderer,
					 GtkTreeModel      *tree_model,
					 GtkTreeIter       *iter,
					 gpointer           user_data)
{
	GValue inconsistent = {0};
	gint selected;

	gtk_tree_model_get (tree_model, iter, COLUMN_SELECTED, &selected, -1);
	gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell_renderer), 
	                                     SELECTED_TRUE == selected);

	g_value_init (&inconsistent, G_TYPE_BOOLEAN);
	g_value_set_boolean (&inconsistent, SELECTED_INCONSISTENT == selected);
	g_object_set_property (G_OBJECT (cell_renderer), "inconsistent", &inconsistent);
}

static void
tracker_tags_view_update_finished (GError   *error, 
                                   gpointer  user_data)
{
	if (error != NULL) {
		if (error->message != NULL) {
			GtkWidget *error_dialog;

			error_dialog = gtk_message_dialog_new (NULL,
                                                               GTK_DIALOG_NO_SEPARATOR,
                                                               GTK_MESSAGE_ERROR,
                                                               GTK_BUTTONS_OK,
                                                               "%s",
                                                               error->message);
			g_signal_connect (error_dialog, "response", 
			                  G_CALLBACK (gtk_widget_destroy), NULL);
			gtk_dialog_run (GTK_DIALOG (error_dialog));
		}

		g_error_free (error);
	}
}

static void
tracker_tags_view_add_dialog_response_cb (GtkDialog *dialog, 
                                          gint       response_id, 
                                          gpointer   user_data)
{
	TrackerTagsView *view = TRACKER_TAGS_VIEW (user_data);

	switch (response_id) {
	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_DELETE_EVENT:
		break;

	case GTK_RESPONSE_OK: {
		gchar *query;
		const gchar *tag_label;

		tag_label = tracker_tags_add_dialog_get_text (TRACKER_TAGS_ADD_DIALOG (dialog));
		query = tracker_tags_add_query (tag_label);
		tracker_resources_sparql_update_async (view->private->tracker_client,
                                                       query,
                                                       tracker_tags_view_update_finished,
                                                       NULL);
		g_free (query);
		break;
	}

	default:
		g_assert_not_reached ();
		break;
	}
}

static void
tracker_tags_view_add_clicked_cb (GtkButton *button, 
                                  gpointer   user_data)
{
	TrackerTagsView *view = TRACKER_TAGS_VIEW (user_data);
	GtkWidget *add_dialog;

	add_dialog = tracker_tags_add_dialog_new ();
	gtk_window_set_screen (GTK_WINDOW (add_dialog), 
	                       gtk_widget_get_screen (GTK_WIDGET (view)));
	g_signal_connect (add_dialog, "response", 
	                  G_CALLBACK (tracker_tags_view_add_dialog_response_cb), 
	                  view);
	gtk_widget_show_all (add_dialog);
}

static void
tracker_tags_view_copy_uri_foreach (gpointer data, 
                                    gpointer user_data)
{
	NautilusFileInfo *file_info = NAUTILUS_FILE_INFO (data);
	GStrv *const arg = user_data;
	gchar *uri;

	uri = nautilus_file_info_get_uri (file_info);
	(*arg)[0] = uri;
	(*arg)++;
}

static void
tracker_tags_view_remove_clicked_cb (GtkButton *button, 
                                     gpointer   user_data)
{
	TrackerTagsView *view = TRACKER_TAGS_VIEW (user_data);
	const gchar *query;

	query = tracker_tags_remove_query (view->private->selected_tag_label);
	tracker_resources_sparql_update_async (view->private->tracker_client, query, 
	                                       tracker_tags_view_update_finished, 
	                                       NULL);
	g_free ((gpointer) query);
}

static void
tracker_tags_view_row_activated_cb (GtkTreeView       *tree_view, 
                                    GtkTreePath       *path, 
                                    GtkTreeViewColumn *column, 
                                    gpointer           user_data)
{
	TrackerTagsView *view = TRACKER_TAGS_VIEW (user_data);
	GStrv files;
	GStrv arg;
	GtkTreeIter iter;
	GtkTreeModel *tree_model;
	gchar *filter;
	gchar *query;
	gchar *tag_label_escaped;
	gint selected;
	guint num;

	tree_model = gtk_tree_view_get_model (tree_view);

	if (gtk_tree_model_get_iter (tree_model, &iter, path) == FALSE) {
		return;
	}

	gtk_tree_model_get (tree_model,
                            &iter,
                            COLUMN_SELECTED,
                            &selected,
                            COLUMN_TAG_NAME,
                            &view->private->selected_tag_label,
                            -1);
	selected = selected == SELECTED_FALSE ? SELECTED_TRUE : SELECTED_FALSE;
	gtk_list_store_set (view->private->list_store, &iter, COLUMN_SELECTED, selected, -1);

	tag_label_escaped = tracker_tags_escape_sparql_string (view->private->selected_tag_label);

	num = g_list_length (view->private->files);
	arg = files = g_new0 (gchar *, num + 1);
	g_list_foreach (view->private->files, tracker_tags_view_copy_uri_foreach, &arg);
	filter = get_filter_string (files, NULL);
	g_strfreev (files);

	if (selected == TRUE) {
		query = g_strdup_printf ("INSERT { "
					 "  ?urn nao:hasTag ?tag "
					 "} "
					 "WHERE {"
					 "  ?urn nie:isStoredAs ?f ." /* NB: ?f is used in filter. */
					 "  ?tag nao:prefLabel %s ."
					 "  %s "
					 "}",
					 tag_label_escaped, filter);
	} else {
		query = g_strdup_printf ("DELETE { "
					 "  ?urn nao:hasTag ?tag "
					 "} "
					 "WHERE { "
					 "  ?urn nie:isStoredAs ?f ." /* NB: ?f is used in filter. */
					 "  ?tag nao:prefLabel %s ."
					 "  %s "
					 "}",
					 tag_label_escaped, filter);
	}

	g_free (filter);
	g_free (tag_label_escaped);
	tracker_resources_sparql_update_async (view->private->tracker_client, 
	                                       query, 
	                                       tracker_tags_view_update_finished, 
	                                       NULL);
	g_free (query);
}

static void
tracker_tags_view_selection_changed_cb (GtkTreeSelection *tree_selection, 
                                        gpointer         *user_data)
{
	TrackerTagsView *view = TRACKER_TAGS_VIEW (user_data);
	GtkTreeIter iter;
	GtkTreeModel *tree_model;

	gtk_widget_set_sensitive (GTK_WIDGET (view->private->remove_button), FALSE);
	view->private->selected_tag_label = NULL;

	if (!gtk_tree_selection_get_selected (tree_selection, &tree_model, &iter)) {
		return;
	}

	gtk_tree_model_get (tree_model, &iter, 
	                    COLUMN_TAG_NAME, &view->private->selected_tag_label, 
	                    -1);
	gtk_widget_set_sensitive (GTK_WIDGET (view->private->remove_button), TRUE);
}

void
tracker_tags_view_register_type (GTypeModule *module)
{
	tracker_tags_view_get_type ();
}

static void
tracker_tags_view_finalize (GObject *object)
{
	TrackerTagsView *view = TRACKER_TAGS_VIEW (object);

	tracker_disconnect (view->private->tracker_client);

	g_list_foreach (view->private->files, (GFunc) g_object_unref, NULL);
	g_list_free (view->private->files);

	G_OBJECT_CLASS (tracker_tags_view_parent_class)->finalize (object);
}

static void
tracker_tags_view_class_init (TrackerTagsViewClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = tracker_tags_view_finalize;
	g_type_class_add_private (gobject_class, sizeof (TrackerTagsViewPrivate));

	tracker_tags_view_parent_class = g_type_class_peek_parent (klass);
}

static void
tracker_tags_view_init (TrackerTagsView *view)
{
	GtkCellRenderer *cell_renderer;
	GtkTreeSelection *tree_selection;
	GtkTreeViewColumn *column;
	GtkWidget *button;
	GtkWidget *button_box;
	GtkWidget *scrolled_window;
	GtkWidget *tree_view;

	view->private = TRACKER_TAGS_VIEW_GET_PRIVATE (view);
	view->private->tracker_client = tracker_connect (TRUE, G_MAXINT);
	view->private->files = NULL;
	view->private->list_store = gtk_list_store_new (N_COLUMNS, G_TYPE_INT, G_TYPE_STRING);

	gtk_container_set_border_width (GTK_CONTAINER (view), 6);
	gtk_box_set_homogeneous (GTK_BOX (view), FALSE);
	gtk_box_set_spacing (GTK_BOX (view), 0);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (view), scrolled_window, TRUE, TRUE, 6);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);

	tree_view = gtk_tree_view_new ();
	gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
	cell_renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell_renderer, tracker_tags_view_toggle_cell_data_func, NULL, NULL);
	gtk_cell_renderer_toggle_set_radio (GTK_CELL_RENDERER_TOGGLE (cell_renderer), FALSE);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, cell_renderer, "text", COLUMN_TAG_NAME);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);
	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (view->private->list_store));

	tree_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	gtk_tree_selection_set_mode (tree_selection, GTK_SELECTION_SINGLE);
	g_signal_connect (tree_selection, "changed", G_CALLBACK (tracker_tags_view_selection_changed_cb), view);

	g_signal_connect (tree_view, "row-activated", G_CALLBACK (tracker_tags_view_row_activated_cb), view);

	button_box = gtk_hbutton_box_new ();
	gtk_box_pack_start (GTK_BOX (view), button_box, FALSE, FALSE, 6);
	gtk_box_set_spacing (GTK_BOX (button_box), 6);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);

	button = gtk_button_new_from_stock (GTK_STOCK_ADD);
        gtk_box_pack_start (GTK_BOX (button_box), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked", G_CALLBACK (tracker_tags_view_add_clicked_cb), view);

	view->private->remove_button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
        gtk_box_pack_start (GTK_BOX (button_box), view->private->remove_button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive (GTK_WIDGET (view->private->remove_button), FALSE);
	g_signal_connect (view->private->remove_button, "clicked", G_CALLBACK (tracker_tags_view_remove_clicked_cb), view);

	view->private->selected_tag_label = NULL;

	tracker_resources_sparql_query_async (view->private->tracker_client,
					      "SELECT ?u ?t "
					      "WHERE {"
					      "  ?u a nao:Tag ;"
					      "  nao:prefLabel ?t ."
					      "}",
					      tracker_tags_view_query_all_tags_finished, view);

	gtk_widget_show_all (GTK_WIDGET (view));
}

GtkWidget *
tracker_tags_view_new (GList *files)
{
	TrackerTagsView *self;

	self = g_object_new (TRACKER_TYPE_TAGS_VIEW, NULL);
	g_list_foreach (files, (GFunc) g_object_ref, NULL);
	self->private->files = g_list_copy (files);

	return GTK_WIDGET (self);
}
