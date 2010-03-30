/*
 * Copyright (C) 2009, Debarshi Ray <debarshir@src.gnome.org>
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>

#include <libnautilus-extension/nautilus-file-info.h>

#include <libtracker-client/tracker-client.h>

#include "tracker-tags-utils.h"
#include "tracker-tags-view.h"

#define TRACKER_TAGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TRACKER_TYPE_TAGS_VIEW, TrackerTagsViewPrivate))

struct _TrackerTagsViewPrivate {
	TrackerClient *tracker_client;
	GList *files;

	GtkListStore *store;

	GtkWidget *button_add;
	GtkWidget *button_remove;
	GtkWidget *entry;
	GtkWidget *view;
};

typedef struct {
	TrackerTagsView *tv;
	gchar *tag_id;
	GtkTreeIter *iter;
	gint items;
	gboolean update;
	gboolean selected;
} TagData;

typedef struct {
	TrackerTagsView *tv;
	const gchar *tag;
	gboolean found;
	GtkTreeIter found_iter;
} FindTag;

enum {
	COL_SELECTION,
	COL_TAG_ID,
	COL_TAG_NAME,
	COL_TAG_COUNT,
	COL_TAG_COUNT_VALUE,
	N_COLUMNS
};

enum {
	SELECTION_INCONSISTENT = -1,
	SELECTION_FALSE = 0,
	SELECTION_TRUE
};

static void tracker_tags_view_finalize (GObject         *object);
static void tags_view_create_ui        (TrackerTagsView *tv);

G_DEFINE_TYPE (TrackerTagsView, tracker_tags_view, GTK_TYPE_VBOX);

static void
tracker_tags_view_class_init (TrackerTagsViewClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = tracker_tags_view_finalize;
	g_type_class_add_private (gobject_class, sizeof (TrackerTagsViewPrivate));

	tracker_tags_view_parent_class = g_type_class_peek_parent (klass);
}

static void
tracker_tags_view_init (TrackerTagsView *tv)
{
	tv->private = TRACKER_TAGS_VIEW_GET_PRIVATE (tv);

	tv->private->tracker_client = tracker_client_new (TRACKER_CLIENT_ENABLE_WARNINGS, G_MAXINT);
	tv->private->files = NULL;
	tv->private->store = gtk_list_store_new (N_COLUMNS, 
	                                         G_TYPE_INT,      /* Selection type */ 
	                                         G_TYPE_STRING,   /* Tag ID */
	                                         G_TYPE_STRING,   /* Tag Name */
	                                         G_TYPE_STRING,   /* Tag Count String */
	                                         G_TYPE_INT);     /* Tag Count */
}

static void
tracker_tags_view_finalize (GObject *object)
{
	TrackerTagsView *tv = TRACKER_TAGS_VIEW (object);

	g_object_unref (tv->private->tracker_client);

	g_list_foreach (tv->private->files, (GFunc) g_object_unref, NULL);
	g_list_free (tv->private->files);

	G_OBJECT_CLASS (tracker_tags_view_parent_class)->finalize (object);
}

static TagData *
tag_data_new (gchar           *tag_id,
              GtkTreeIter     *iter,
              gboolean         update,
              gboolean         selected,
              gint             items,
              TrackerTagsView *tv)
{
	TagData *td;

	td = g_slice_new (TagData);

	td->tv = tv;

	td->tag_id = g_strdup (tag_id);

	if (iter) {
		td->iter = gtk_tree_iter_copy (iter);
	} else {
		td->iter = NULL;
	}

	td->items = items;
	td->update = update;
	td->selected = selected;

	return td;
}

static void
tag_data_free (TagData *td)
{
	g_free (td->tag_id);

	if (td->iter) {
		gtk_tree_iter_free (td->iter);
	}

	g_slice_free (TagData, td);
}

static TagData *
tag_data_copy (TagData *td)
{
	TagData *new_td;

	new_td = g_slice_new (TagData);

	new_td->tv = td->tv;

	new_td->tag_id = g_strdup (td->tag_id);

	if (td->iter) {
		new_td->iter = gtk_tree_iter_copy (td->iter);
	} else {
		new_td->iter = NULL;
	}

	new_td->items = td->items;
	new_td->update = td->update;
	new_td->selected = td->selected;

	return new_td;
}

static gboolean
tag_view_model_find_tag_foreach (GtkTreeModel *model,
                                 GtkTreePath  *path,
                                 GtkTreeIter  *iter,
                                 FindTag     *data)
{
	gchar *tag;

	gtk_tree_model_get (model, iter,
			    COL_TAG_NAME, &tag,
			    -1);

	if (!tag) {
		return FALSE;
	}

	if (data->tag && strcmp (data->tag, tag) == 0) {
		data->found = TRUE;
		data->found_iter = *iter;

		g_free (tag);

		return TRUE;
	}

	g_free (tag);

	return FALSE;
}

static gboolean
tag_view_model_find_tag (TrackerTagsView *tv,
                         const gchar     *tag,
                         GtkTreeIter     *iter)
{
	GtkTreeView  *view;
	GtkTreeModel *model;
	FindTag data;

	if (tracker_is_empty_string (tag)) {
		return FALSE;
	}

	data.tv = tv;
	data.tag = tag;
	data.found = FALSE;

	view = GTK_TREE_VIEW (tv->private->view);
	model = gtk_tree_view_get_model (view);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) tag_view_model_find_tag_foreach,
				&data);

	if (data.found == TRUE) {
		*iter = data.found_iter;
		return TRUE;
	}

	return FALSE;
}

static void
tags_view_tag_removed_cb (GError   *error, 
                          gpointer  user_data)
{
	TagData *td = user_data;

	if (error) {
		g_warning ("Could not remove tag, %s",
		           error->message ? error->message : "no error given");
		g_error_free (error);
		return;
	}

	g_debug ("Tag removed\n");

	gtk_list_store_remove (td->tv->private->store, td->iter);
	tag_data_free (td);
}

static void
tags_view_query_files_for_tag_id_cb (GPtrArray *result, 
                                     GError    *error, 
                                     gpointer   user_data)
{
	TagData *td = user_data;
	TrackerTagsView *tv = td->tv;
	GtkTreeIter *iter = td->iter;
	gchar *str;
	guint files_selected, files_with_tag, has_tag_in_selection;

	if (error) {
		GtkWidget *dialog;
		const gchar *str;

		str = error->message ? error->message : _("No error was given");
		
		dialog = gtk_message_dialog_new (NULL,
		                                 0,
		                                 GTK_MESSAGE_ERROR,
		                                 GTK_BUTTONS_OK,
		                                 "%s",
		                                 str);
		g_signal_connect (dialog, "response", 
		                  G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_dialog_run (GTK_DIALOG (dialog));
		
		g_error_free (error);
		tag_data_free (td);

		return;
	}

	has_tag_in_selection = 0;
	files_with_tag = result->len;
	files_selected = g_list_length (tv->private->files);

	if (result && result->len > 0) {
		GList *l;

		for (l = tv->private->files; l; l = l->next) {
			gchar *uri;
			gboolean equal;
			gint i;

			uri = nautilus_file_info_get_uri (NAUTILUS_FILE_INFO (l->data));

			for (i = 0, equal = FALSE; i < result->len && !equal; i++) {
				GStrv strv;

				strv = g_ptr_array_index (result, i);
				equal = g_strcmp0 (strv[0], uri) == 0;
				
				if (equal) {
					has_tag_in_selection++;
				}
			}

			g_free (uri);
		}
	}

	g_debug ("Querying files with tag, in selection:%d, in total:%d, selected:%d\n", 
	         has_tag_in_selection, files_with_tag, files_selected);

	g_ptr_array_foreach (result, (GFunc) g_strfreev, NULL);
	g_ptr_array_free (result, TRUE);
	
	if (has_tag_in_selection == 0) {
		gtk_list_store_set (tv->private->store, iter, 
		                    COL_SELECTION, SELECTION_FALSE, 
		                    -1);
	} else if (files_selected != has_tag_in_selection) {
		gtk_list_store_set (tv->private->store, iter, 
		                    COL_SELECTION, SELECTION_INCONSISTENT, 
		                    -1);
	} else {
		gtk_list_store_set (tv->private->store, iter, 
		                    COL_SELECTION, SELECTION_TRUE,
		                    -1);
	}

	str = g_strdup_printf ("%d", files_with_tag);
	gtk_list_store_set (tv->private->store, iter, 
	                    COL_TAG_COUNT, str,
	                    COL_TAG_COUNT_VALUE, files_with_tag,
	                    -1);
	g_free (str);
	
	tag_data_free (td);
}

static void
tags_view_query_files_for_tag_id (TagData *td)
{
	gchar *query;

	query = g_strdup_printf ("SELECT ?url "
	                         "WHERE {"
	                         "  ?urn a rdfs:Resource ;"
	                         "  nie:url ?url ;"
	                         "  nao:hasTag <%s> . "
	                         "}", td->tag_id);

	tracker_resources_sparql_query_async (td->tv->private->tracker_client,
	                                      query,
	                                      tags_view_query_files_for_tag_id_cb,
	                                      td);
	g_free (query);
}

static void
tags_view_append_foreach (gpointer data, 
                          gpointer user_data)
{
	TrackerTagsView *tv = user_data;
	TagData *td;
	GtkTreeIter iter;
	GStrv strv = data;

	g_debug ("Adding tag id:'%s' with label:'%s' to store\n", strv[0], strv[1]);

	gtk_list_store_append (tv->private->store, &iter);
	gtk_list_store_set (tv->private->store, &iter, 
	                    COL_TAG_ID, strv[0], 
	                    COL_TAG_NAME, strv[1], 
	                    COL_SELECTION, SELECTION_FALSE, 
	                    -1);

	td = tag_data_new (strv[0], &iter, FALSE, TRUE, 1, tv);
	tags_view_query_files_for_tag_id (td);
}

static void
tags_view_add_tags_cb (GPtrArray *result, 
                       GError    *error, 
                       gpointer   user_data)
{
	TrackerTagsView *tv = user_data;

	g_debug ("Clearing tags in store\n");

	gtk_list_store_clear (tv->private->store);

	if (error) {
		GtkWidget *dialog;
		const gchar *str;

		str = error->message ? error->message : _("No error was given");
		
		dialog = gtk_message_dialog_new (NULL,
		                                 0,
		                                 GTK_MESSAGE_ERROR,
		                                 GTK_BUTTONS_OK,
		                                 "%s",
		                                 str);
		g_signal_connect (dialog, "response", 
		                  G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_dialog_run (GTK_DIALOG (dialog));
		
		g_error_free (error);
	} else {
		g_debug ("Adding all tags...\n");
		g_ptr_array_foreach (result, tags_view_append_foreach, tv);
		g_ptr_array_foreach (result, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (result, TRUE);
	}
}

static void
tags_view_model_update_cb (GError   *error, 
                           gpointer  user_data)
{
	TagData *td = user_data;
	TrackerTagsView *tv = td->tv;

	g_debug ("Query callback\n");

	if (error) {
		GtkWidget *dialog;
		const gchar *str;

		str = error->message ? error->message : _("No error was given");
		
		dialog = gtk_message_dialog_new (NULL,
		                                 0,
		                                 GTK_MESSAGE_ERROR,
		                                 GTK_BUTTONS_OK,
		                                 "%s",
		                                 str);
		g_signal_connect (dialog, "response", 
		                  G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_dialog_run (GTK_DIALOG (dialog));
		
		g_error_free (error);
	} else {
		const gchar *tag;

		tag = gtk_entry_get_text (GTK_ENTRY (tv->private->entry));

		if (!td->update) {
			GtkTreeIter iter;
			gchar *str;

			g_debug ("Setting tag selection state to ON (new)\n");

			str = g_strdup_printf ("%d", td->items);
			gtk_list_store_append (tv->private->store, &iter);
			gtk_list_store_set (tv->private->store, &iter,
			                    COL_TAG_ID, td->tag_id,
			                    COL_TAG_NAME, tag,
			                    COL_TAG_COUNT, str,
			                    COL_TAG_COUNT_VALUE, td->items,
			                    COL_SELECTION, SELECTION_TRUE,
			                    -1);
			g_free (str);
		} else if (td->selected) {
			g_debug ("Setting tag selection state to ON\n");

			gtk_list_store_set (tv->private->store, td->iter,
			                    COL_SELECTION, SELECTION_TRUE,
			                    -1);

			tags_view_query_files_for_tag_id (tag_data_copy (td));
		} else {
			g_debug ("Setting tag selection state to FALSE\n");

			gtk_list_store_set (tv->private->store, td->iter,
			                    COL_SELECTION, SELECTION_FALSE,
			                    -1);

			tags_view_query_files_for_tag_id (tag_data_copy (td));
		}
	}

	gtk_entry_set_text (GTK_ENTRY (tv->private->entry), "");
	gtk_widget_set_sensitive (tv->private->entry, TRUE);

	tag_data_free (td);
}

static void
tags_view_add_tag (TrackerTagsView *tv,
                   const gchar     *tag)
{
	GString *query;
	gint files;

	gtk_widget_set_sensitive (tv->private->entry, FALSE);

	files = g_list_length (tv->private->files);

	if (files > 0) {
		GStrv files;
		gchar *tag_escaped;
		gchar *filter;
		guint i;

		query = g_string_new ("");

		files = tracker_glist_to_string_list_for_nautilus_files (tv->private->files);
		filter = tracker_tags_get_filter_string (files, NULL);
		tag_escaped = tracker_tags_escape_sparql_string (tag);

		for (i = 0; files[i] != NULL; i++) {
			g_string_append_printf (query, 
			                        "INSERT { _:file a nie:DataObject ; nie:url '%s' } "
			                        "WHERE { "
			                        "  OPTIONAL {"
			                        "     ?file a nie:DataObject ;"
			                        "     nie:url '%s'"
			                        "  } ."
			                        "  FILTER (!bound(?file)) "
			                        "} ",
			                        files[i], files[i]);
		}

		g_string_append_printf (query,
		                         "INSERT { "
		                         "  _:tag a nao:Tag;"
		                         "  nao:prefLabel %s . "
		                         "} "
		                         "WHERE {"
		                         "  OPTIONAL {"
		                         "     ?tag a nao:Tag ;"
		                         "     nao:prefLabel %s"
		                         "  } ."
		                         "  FILTER (!bound(?tag)) "
		                         "} "
		                         "INSERT { "
		                         "  ?urn nao:hasTag ?label "
		                         "} "
		                         "WHERE {"
		                         "  ?urn nie:url ?f ."
		                         "  ?label nao:prefLabel %s "
		                         "  %s "
		                         "}",
		                         tag_escaped,
		                         tag_escaped,
		                         tag_escaped,
		                         filter);

		g_free (tag_escaped);
		g_free (filter);
		g_strfreev (files);
	} else {
		query = g_string_new (tracker_tags_add_query (tag));
	}

	tracker_resources_sparql_update_async (tv->private->tracker_client,
	                                       query->str,
	                                       tags_view_model_update_cb,
	                                       tag_data_new (NULL, NULL, FALSE, TRUE, files, tv));

	g_string_free (query, TRUE);
}

static void
tags_view_model_toggle_cell_data_func (GtkTreeViewColumn *column,
                                       GtkCellRenderer   *cell_renderer,
                                       GtkTreeModel      *tree_model,
                                       GtkTreeIter       *iter,
                                       gpointer           user_data)
{
	GValue inconsistent = { 0 };
	gint selection;

	gtk_tree_model_get (tree_model, iter, COL_SELECTION, &selection, -1);
	gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell_renderer), 
	                                     SELECTION_TRUE == selection);

	g_value_init (&inconsistent, G_TYPE_BOOLEAN);
	g_value_set_boolean (&inconsistent, SELECTION_INCONSISTENT == selection);
	g_object_set_property (G_OBJECT (cell_renderer), "inconsistent", &inconsistent);
}

static void
tags_view_model_toggle_row (TrackerTagsView *tv,
                            GtkTreePath     *path)
{
	GStrv files;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *filter, *query;
	gchar *id, *tag, *tag_escaped;
	gint selection;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tv->private->view));

	if (gtk_tree_model_get_iter (model, &iter, path) == FALSE) {
		return;
	}

	gtk_tree_model_get (model, &iter,
	                    COL_SELECTION, &selection,
	                    COL_TAG_ID, &id,
	                    COL_TAG_NAME, &tag,
	                    -1);

	selection = selection == SELECTION_FALSE ? SELECTION_TRUE : SELECTION_FALSE;

	tag_escaped = tracker_tags_escape_sparql_string (tag);
	g_free (tag);

	files = tracker_glist_to_string_list_for_nautilus_files (tv->private->files);
	filter = tracker_tags_get_filter_string (files, NULL);
	g_strfreev (files);

	if (selection) {
		query = g_strdup_printf ("INSERT { "
		                         "  ?urn nao:hasTag ?label "
		                         "} "
		                         "WHERE {"
		                         "  ?urn nie:url ?f ." /* NB: ?f is used in filter. */
		                         "  ?label nao:prefLabel %s ."
		                         "  %s "
		                         "}", 
		                         tag_escaped, 
		                         filter);
	} else {
		TagData *td;

		query = g_strdup_printf ("DELETE { "
		                         "  ?urn nao:hasTag ?label "
		                         "} "
		                         "WHERE { "
		                         "  ?urn nie:url ?f ." /* NB: ?f is used in filter. */
		                         "  ?label nao:prefLabel %s ."
		                         "  %s "
		                         "}",
		                         tag_escaped, 
		                         filter);
		
		/* Check if there are any files left with this tag and
		 * remove tag if not.
		 */
		td = tag_data_new (id, &iter, FALSE, TRUE, 1, tv);
		tags_view_query_files_for_tag_id (td);
	}

	g_free (filter);
	g_free (tag_escaped);

	gtk_widget_set_sensitive (tv->private->entry, FALSE);

	g_debug ("Running query:'%s'\n", query);

	tracker_resources_sparql_update_async (tv->private->tracker_client, 
	                                       query, 
	                                       tags_view_model_update_cb, 
	                                       tag_data_new (id, &iter, TRUE, selection, 1, tv));

	g_free (id);
	g_free (query);
}

static void
tags_view_model_cell_toggled_cb (GtkCellRendererToggle *cell,
                                 gchar                 *path_string,
                                 TrackerTagsView       *tv)
{
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (path_string);
	tags_view_model_toggle_row (tv, path);
	gtk_tree_path_free (path);
}

static void
tags_view_model_row_activated_cb (GtkTreeView       *view, 
                                  GtkTreePath       *path, 
                                  GtkTreeViewColumn *column, 
                                  gpointer           user_data)
{
	tags_view_model_toggle_row (user_data, path);
}

static void
tags_view_entry_changed_cb (GtkEditable     *editable,
                            TrackerTagsView *tv)
{
	GtkTreeIter iter;
	const gchar *tag;

	tag = gtk_entry_get_text (GTK_ENTRY (tv->private->entry));

	if (tag_view_model_find_tag (tv, tag, &iter)) {
		gtk_widget_set_sensitive (GTK_WIDGET (tv->private->button_add), FALSE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (tv->private->button_add),
		                          !tracker_is_empty_string (tag));
	}
}

static void
tags_view_entry_activate_cb (GtkEditable     *editable,
                             TrackerTagsView *tv)
{
	gtk_widget_activate (tv->private->button_add);
}

static void
tags_view_add_clicked_cb (GtkButton *button, 
                          gpointer   user_data)
{
	TrackerTagsView *tv = user_data;
	const gchar *tag;

	tag = gtk_entry_get_text (GTK_ENTRY (tv->private->entry));
	tags_view_add_tag (tv, tag);
}

static void
tags_view_remove_tag (TrackerTagsView *tv,
                      TagData         *td)
{
	gchar *query;

	query = g_strdup_printf ("DELETE { "
	                         "  <%s> a rdfs:Resource "
	                         "}",
	                         td->tag_id);

	tracker_resources_sparql_update_async (tv->private->tracker_client, 
	                                       query, 
	                                       tags_view_tag_removed_cb,
	                                       tag_data_copy (td));
	g_free (query);
}

static void
tags_view_remove_clicked_cb (GtkButton *button, 
                             gpointer   user_data)
{
	TrackerTagsView *tv = user_data;
	TagData *td;
	GtkTreeIter iter;
	GtkTreeSelection *select;
	GtkTreeModel *model;
	gchar *id;

	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv->private->view));

	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (tv->private->store), &iter, COL_TAG_ID, &id, -1);
		td = tag_data_new (id, &iter, FALSE, TRUE, 1, tv);
		tags_view_remove_tag (tv, td);
		tag_data_free (td);
	}
}

static void
tags_view_model_row_selected_cb (GtkTreeSelection *selection,
                                 gpointer         user_data)
{
	TrackerTagsView *tv = user_data;
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_widget_set_sensitive (GTK_WIDGET (tv->private->button_remove), TRUE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (tv->private->button_remove), FALSE);
	}
}

static void
tags_view_create_ui (TrackerTagsView *tv)
{
	GtkCellRenderer *cell_renderer;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *button;
	GtkWidget *scrolled_window;
	GtkWidget *view;
	gchar *str;

	gtk_container_set_border_width (GTK_CONTAINER (tv), 6);
	gtk_box_set_homogeneous (GTK_BOX (tv), FALSE);
	gtk_box_set_spacing (GTK_BOX (tv), 6);

	/* Add entry/label part */
	str = g_strdup_printf (dngettext (NULL,
	                                  "_Set the tags you want to associate with the %d selected item:",
	                                  "_Set the tags you want to associate with the %d selected items:",
	                                  g_list_length (tv->private->files)),
	                       g_list_length (tv->private->files));

	label = gtk_label_new_with_mnemonic (str);
	g_free (str);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (tv), label, FALSE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (tv), hbox, FALSE, TRUE, 0);

	entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);

	g_signal_connect (entry, "changed", 
	                  G_CALLBACK (tags_view_entry_changed_cb), 
	                  tv);
	g_signal_connect (entry, "activate", 
	                  G_CALLBACK (tags_view_entry_activate_cb), 
	                  tv);

	button = gtk_button_new_from_stock (GTK_STOCK_ADD);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);

	gtk_widget_set_can_default (button, TRUE);
	gtk_widget_set_sensitive (button, FALSE);

	g_signal_connect (button, "clicked", 
	                  G_CALLBACK (tags_view_add_clicked_cb), 
	                  tv);

	tv->private->button_add = button;

	button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);

	gtk_widget_set_sensitive (button, FALSE);

	g_signal_connect (button, "clicked", 
	                  G_CALLBACK (tags_view_remove_clicked_cb), 
	                  tv);

	tv->private->button_remove = button;

	/* List */
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (tv), scrolled_window, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
	                                GTK_POLICY_AUTOMATIC, 
	                                GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), 
	                                     GTK_SHADOW_IN);

	view = gtk_tree_view_new ();
	gtk_container_add (GTK_CONTAINER (scrolled_window), view);

	/* List column: toggle */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width (column, 50);

	cell_renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (cell_renderer, "toggled",
	                  G_CALLBACK (tags_view_model_cell_toggled_cb),
	                  tv);

	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, 
	                                         cell_renderer, 
	                                         tags_view_model_toggle_cell_data_func, 
	                                         NULL, 
	                                         NULL);
	gtk_cell_renderer_toggle_set_radio (GTK_CELL_RENDERER_TOGGLE (cell_renderer), FALSE);

	/* List column: tag */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "text", COL_TAG_NAME);

	/* List column: count */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width (column, 50);

	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_end (column, cell_renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "text", COL_TAG_COUNT);

	/* List settings */
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
	gtk_tree_view_set_model (GTK_TREE_VIEW (view), 
	                         GTK_TREE_MODEL (tv->private->store));


	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (view, "row-activated", 
	                  G_CALLBACK (tags_view_model_row_activated_cb), 
	                  tv);

	g_signal_connect (selection, "changed", 
	                  G_CALLBACK (tags_view_model_row_selected_cb), 
	                  tv);

	tracker_resources_sparql_query_async (tv->private->tracker_client,
	                                      "SELECT ?urn ?label "
	                                      "WHERE {"
	                                      "  ?urn a nao:Tag ;"
	                                      "  nao:prefLabel ?label . "
	                                      "} ORDER BY ?label",
	                                      tags_view_add_tags_cb, tv);

	gtk_widget_show_all (GTK_WIDGET (tv));
	gtk_widget_grab_focus (entry);

	/* Save vars */
	tv->private->entry = entry;
	tv->private->view = view;
}

void
tracker_tags_view_register_type (GTypeModule *module)
{
	tracker_tags_view_get_type ();
}

GtkWidget *
tracker_tags_view_new (GList *files)
{
	TrackerTagsView *tv;

	g_return_val_if_fail (files != NULL, NULL);
	
	tv = g_object_new (TRACKER_TYPE_TAGS_VIEW, NULL);
	tv->private->files = tracker_glist_copy_with_nautilus_files (files);

	tags_view_create_ui (tv);

	return GTK_WIDGET (tv);
}
