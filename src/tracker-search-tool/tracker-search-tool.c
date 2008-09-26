/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * TRACKER Search Tool - modfied from Gnome search tool
 *
 *  File:  tracker_search_tool.c
 *  (C) Me Jamie McCracken
 *
 *  Original Copyright:
 *  (C) 1998,2002 the Free Software Foundation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <config.h>

#include <fnmatch.h>
#ifndef FNM_CASEFOLD
#  define FNM_CASEFOLD 0
#endif

#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <glib/gi18n.h>
#include <gdk/gdkcursor.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <gtk/gtk.h>



#include "tracker-search-tool.h"
#include "tracker-search-tool-callbacks.h"
#include "tracker-search-tool-support.h"
#include "sexy-icon-entry.h"
#include "../libtracker-gtk/tracker-metadata-tile.h"

#define TRACKER_SEARCH_TOOL_DEFAULT_ICON_SIZE 32
#define TRACKER_SEARCH_TOOL_STOCK "panel-searchtool"
#define TRACKER_SEARCH_TOOL_REFRESH_DURATION  50000
#define LEFT_LABEL_SPACING "	 "

static GObjectClass * parent_class;
static TrackerClient *tracker_client;

static gchar **terms = NULL;
static gchar *service = NULL;

static GOptionEntry options[] = {
	{"service", 's', 0, G_OPTION_ARG_STRING, &service, N_("Search from a specific service"), N_("SERVICE")},
	{G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &terms, "search terms", NULL},
	{NULL}
};

typedef enum {
	SEARCH_CONSTRAINT_TYPE_BOOLEAN,
	SEARCH_CONSTRAINT_TYPE_NUMERIC,
	SEARCH_CONSTRAINT_TYPE_TEXT,
	SEARCH_CONSTRAINT_TYPE_DATE_BEFORE,
	SEARCH_CONSTRAINT_TYPE_DATE_AFTER,
	SEARCH_CONSTRAINT_TYPE_SEPARATOR,
	SEARCH_CONSTRAINT_TYPE_NONE
} GSearchConstraintType;

typedef struct _GSearchOptionTemplate GSearchOptionTemplate;



typedef struct {
	GSearchWindow * gsearch;
	char  *uri;
	ServiceType type;

} SnippetRow;


struct _GSearchOptionTemplate {
	GSearchConstraintType type; /* The available option type */
	gchar * option;		    /* An option string to pass to the command */
	gchar * desc;		    /* The description for display */
	gchar * units;		    /* Optional units for display */
	gboolean is_selected;
};


static char *search_service_types[] = {
"Files",
"Folders",
"Documents",
"Images",
"Music",
"Videos",
"Text",
"Development",
"Other",
"VFS",
"VFSFolders",
"VFSDocuments",
"VFSImages",
"VFSMusic",
"VFSVideos",
"VFSText",
"VFSDevelopment",
"VFSOther",
"Conversations",
"Playlists",
"Applications",
"Contacts",
"Emails",
"EmailAttachments",
"EvolutionEmails",
"ModestEmails",
"ThunderbirdEmails",
"Appointments",
"Tasks",
"Bookmarks",
"WebHistory",
"Projects",
NULL
};

static service_info_t services[16] = {
	{ "Emails",	   N_("Emails"),       "stock_mail",		   NULL, SERVICE_EMAILS,	    NULL, FALSE, 0, 0},
	{ "EvolutionEmails",
			   N_("Emails"),       "stock_mail",		   NULL, SERVICE_EMAILS,	    NULL, FALSE, 0, 0},
	{ "ModestEmails",  N_("Emails"),       "stock_mail",		   NULL, SERVICE_EMAILS,	    NULL, FALSE, 0, 0},
	{ "ThunderbirdEmails",
			   N_("Emails"),       "stock_mail",		   NULL, SERVICE_EMAILS,	    NULL, FALSE, 0, 0},
	{ "Files",	   N_("All Files"),    "system-file-manager",	   NULL, SERVICE_FILES,		    NULL, FALSE, 0, 0},
	{ "Folders",	   N_("Folders"),      "folder",		   NULL, SERVICE_FOLDERS,	    NULL, FALSE, 0, 0},
	{ "Documents",	   N_("Documents"),    "x-office-document",	   NULL, SERVICE_DOCUMENTS,	    NULL, FALSE, 0, 0},
	{ "Images",	   N_("Images"),       "image-x-generic",	   NULL, SERVICE_IMAGES,	    NULL, FALSE, 0, 0},
	{ "Music",	   N_("Music"),        "audio-x-generic",	   NULL, SERVICE_MUSIC,		    NULL, FALSE, 0, 0},
	{ "Videos",	   N_("Videos"),       "video-x-generic",	   NULL, SERVICE_VIDEOS,	    NULL, FALSE, 0, 0},
	{ "Text",	   N_("Text"),	       "text-x-generic",	   NULL, SERVICE_TEXT_FILES,	    NULL, FALSE, 0, 0},
	{ "Development",   N_("Development"),  "applications-development", NULL, SERVICE_DEVELOPMENT_FILES, NULL, FALSE, 0, 0},
	{ "Conversations", N_("Chat Logs"),    "stock_help-chat",	   NULL, SERVICE_CONVERSATIONS,     NULL, FALSE, 0, 0},
	{ "Applications",  N_("Applications"), "system-run",		   NULL, SERVICE_APPLICATIONS,	    NULL, FALSE, 0, 0},
	{ "WebHistory",    N_("WebHistory"),	"text-html",		   NULL, SERVICE_WEBHISTORY,	    NULL, FALSE, 0, 0},
	{ NULL,		   NULL,	       NULL,			   NULL, -1,			    NULL, FALSE, 0, 0},
};

static GSearchOptionTemplate GSearchOptionTemplates[] = {
	{ SEARCH_CONSTRAINT_TYPE_TEXT, NULL, "Contains the _text", NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_DATE_BEFORE, "-mtime -%d", "_Date modified less than", "days", FALSE },
	{ SEARCH_CONSTRAINT_TYPE_DATE_AFTER, "\\( -mtime +%d -o -mtime %d \\)", "Date modified more than", "days", FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_NUMERIC, "\\( -size %uc -o -size +%uc \\)", "S_ize at least", "kilobytes", FALSE },
	{ SEARCH_CONSTRAINT_TYPE_NUMERIC, "\\( -size %uc -o -size -%uc \\)", "Si_ze at most", "kilobytes", FALSE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "-size 0c \\( -type f -o -type d \\)", "File is empty", NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_TEXT, "-user '%s'", "Owned by _user", NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_TEXT, "-group '%s'", "Owned by _group", NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "\\( -nouser -o -nogroup \\)", "Owner is unrecognized", NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_TEXT, "'!' -name '*%s*'", "Na_me does not contain", NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_TEXT, "-regex '%s'", "Name matches regular e_xpression", NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "SHOW_HIDDEN_FILES", "Show hidden and backup files", NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "-follow", "Follow symbolic links", NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "INCLUDE_OTHER_FILESYSTEMS", "Include other filesystems", NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_NONE, NULL, NULL, NULL, FALSE}
};

enum {
	SEARCH_CONSTRAINT_CONTAINS_THE_TEXT,
	SEARCH_CONSTRAINT_TYPE_SEPARATOR_00,
	SEARCH_CONSTRAINT_DATE_MODIFIED_BEFORE,
	SEARCH_CONSTRAINT_DATE_MODIFIED_AFTER,
	SEARCH_CONSTRAINT_TYPE_SEPARATOR_01,
	SEARCH_CONSTRAINT_SIZE_IS_MORE_THAN,
	SEARCH_CONSTRAINT_SIZE_IS_LESS_THAN,
	SEARCH_CONSTRAINT_FILE_IS_EMPTY,
	SEARCH_CONSTRAINT_TYPE_SEPARATOR_02,
	SEARCH_CONSTRAINT_OWNED_BY_USER,
	SEARCH_CONSTRAINT_OWNED_BY_GROUP,
	SEARCH_CONSTRAINT_OWNER_IS_UNRECOGNIZED,
	SEARCH_CONSTRAINT_TYPE_SEPARATOR_03,
	SEARCH_CONSTRAINT_FILE_IS_NOT_NAMED,
	SEARCH_CONSTRAINT_FILE_MATCHES_REGULAR_EXPRESSION,
	SEARCH_CONSTRAINT_TYPE_SEPARATOR_04,
	SEARCH_CONSTRAINT_SHOW_HIDDEN_FILES_AND_FOLDERS,
	SEARCH_CONSTRAINT_FOLLOW_SYMBOLIC_LINKS,
	SEARCH_CONSTRAINT_SEARCH_OTHER_FILESYSTEMS,
	SEARCH_CONSTRAINT_MAXIMUM_POSSIBLE
};

static GtkTargetEntry GSearchDndTable[] = {
	{ "text/uri-list", 0, 1 },
	{ "text/plain",    0, 0 },
	{ "STRING",	   0, 0 }
};

static guint GSearchTotalDnds = sizeof (GSearchDndTable) / sizeof (GSearchDndTable[0]);


static GtkActionEntry GSearchUiEntries[] = {
  { "Open",	     GTK_STOCK_OPEN,	N_("_Open"),		   NULL, NULL, NULL },
  { "OpenFolder",    GTK_STOCK_OPEN,	N_("O_pen Folder"),	   NULL, NULL, NULL },
  { "MoveToTrash",   GTK_STOCK_DELETE,	N_("Mo_ve to Trash"),	   NULL, NULL, NULL },
  { "SaveResultsAs", GTK_STOCK_SAVE_AS, N_("_Save Results As..."), NULL, NULL, NULL },
};

static const char * GSearchUiDescription =
"<ui>"
"  <popup name='PopupMenu'>"
"      <menuitem action='Open'/>"
"      <menuitem action='OpenFolder'/>"
"      <separator/>"
"      <menuitem action='MoveToTrash'/>"
"      <separator/>"
"      <menuitem action='SaveResultsAs'/>"
"  </popup>"
"</ui>";

static void set_snippet (gchar * snippet, GError *error, gpointer user_data);

static void
display_dialog_character_set_conversion_error (GtkWidget * window,
					       gchar * string,
					       GError * error)
{
	GtkWidget * dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 _("Character set conversion failed for \"%s\""),
					 string);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  (error == NULL) ? " " : error->message);

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			   G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (dialog);
}

static void
display_error_dialog (GtkWidget * window,
		      const char *error)
{
	GtkWidget * dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
				 GTK_DIALOG_DESTROY_WITH_PARENT,
				 GTK_MESSAGE_ERROR,
				 GTK_BUTTONS_OK,
				 _("The following error has occurred :"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), error);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Error"));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	g_signal_connect (G_OBJECT (dialog),
		  "response",
		   G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (dialog);
}

static void
start_animation (GSearchWindow * gsearch,
		 gboolean first_pass)
{
	if (first_pass == TRUE) {

		gsearch->focus = gtk_window_get_focus (GTK_WINDOW (gsearch->window));

		gtk_widget_set_sensitive (gsearch->find_button, FALSE);
		if (gsearch->type < 10) {
			gtk_widget_set_sensitive (gsearch->search_results_save_results_as_item, FALSE);
		}
		gtk_widget_set_sensitive (gsearch->search_results_vbox, TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (gsearch->search_results_tree_view), TRUE);

		gtk_widget_set_sensitive (gsearch->name_and_folder_table, FALSE);
	}
}

static void
stop_animation (GSearchWindow * gsearch)
{
	gtk_window_set_default (GTK_WINDOW (gsearch->window), gsearch->find_button);
	gtk_widget_set_sensitive (gsearch->name_and_folder_table, TRUE);
	gtk_widget_set_sensitive (gsearch->find_button, TRUE);
	if (gsearch->type < 10) {
		gtk_widget_set_sensitive (gsearch->search_results_save_results_as_item, TRUE);
	}
	gtk_widget_show (gsearch->find_button);


	if (gtk_window_get_focus (GTK_WINDOW (gsearch->window)) == NULL) {
		gtk_window_set_focus (GTK_WINDOW (gsearch->window), gsearch->focus);
	}
}

static gboolean
process_snippets (GSearchWindow * gsearch)
{
	if (!gsearch->snippet_queue || g_queue_is_empty (gsearch->snippet_queue)) {
		return FALSE;
	}

	SnippetRow *snippet = g_queue_pop_head (gsearch->snippet_queue);

	tracker_search_get_snippet_async (tracker_client, snippet->type, snippet->uri, gsearch->search_term, set_snippet, snippet);

	return FALSE;
}

gchar *
build_search_command (GSearchWindow * gsearch,
		      gboolean first_pass)
{
	GString * command;
	GError * error = NULL;
	gchar * file_is_named_utf8;
	gchar * file_is_named_locale;


	gsearch->show_thumbnails = TRUE;

	file_is_named_utf8 = g_strdup ((gchar *) gtk_entry_get_text (GTK_ENTRY (gsearch->search_entry)));

	if (!file_is_named_utf8 || !*file_is_named_utf8) {
		g_free (file_is_named_utf8);
		file_is_named_utf8 = g_strdup ("");

	} else {
		gchar * locale;

		locale = g_locale_from_utf8 (file_is_named_utf8, -1, NULL, NULL, &error);
		if (locale == NULL) {

			display_dialog_character_set_conversion_error (gsearch->window, file_is_named_utf8, error);
			g_free (file_is_named_utf8);
			g_error_free (error);
			return NULL;
		}

		g_free (locale);
	}

	file_is_named_locale = g_locale_from_utf8 (file_is_named_utf8, -1, NULL, NULL, &error);
	if (file_is_named_locale == NULL) {

		display_dialog_character_set_conversion_error (gsearch->window, file_is_named_utf8, error);
		g_free (file_is_named_utf8);
		g_error_free (error);
		return NULL;
	}

	command = g_string_new (file_is_named_utf8);


	gsearch->command_details->is_command_show_hidden_files_enabled = FALSE;
	gsearch->command_details->name_contains_regex_string = NULL;
	gsearch->search_results_date_format_string = NULL;
	gsearch->command_details->name_contains_pattern_string = NULL;

	gsearch->command_details->is_command_first_pass = first_pass;
	if (gsearch->command_details->is_command_first_pass == TRUE) {
		gsearch->command_details->is_command_using_quick_mode = FALSE;
	}


	gsearch->show_thumbnails = TRUE;
	gsearch->show_thumbnails_file_size_limit = tracker_search_gconf_get_int ("/apps/nautilus/preferences/thumbnail_limit");


	return g_string_free (command, FALSE);
}

static void
free_snippet (SnippetRow * snippet_row)
{
	g_free (snippet_row->uri);
	g_free (snippet_row);
}

static void
set_snippet (gchar * snippet,
	     GError * error,
	     gpointer user_data)
{
	gchar *snippet_markup;
	GtkTreeIter iter;
	SnippetRow *snippet_row = user_data;

	g_return_if_fail (error == NULL);

	snippet_markup = g_strdup_printf ("<span size='small'>%s</span>", snippet);

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (snippet_row->gsearch->search_results_list_store), &iter)) {

		while (TRUE) {

			gchar *uri;

			gtk_tree_model_get (GTK_TREE_MODEL (snippet_row->gsearch->search_results_list_store), &iter,
					    COLUMN_URI, &uri,
					    -1);

			if ( (strcmp (snippet_row->uri, uri) == 0)) {
				gtk_list_store_set (GTK_LIST_STORE (snippet_row->gsearch->search_results_list_store), &iter, COLUMN_SNIPPET, snippet_markup, -1);
				g_free (uri);
				break;
			} else {
				g_free (uri);

				if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (snippet_row->gsearch->search_results_list_store), &iter)) {
					break;
				}
			}
		}
	}

	if (!g_queue_is_empty (snippet_row->gsearch->snippet_queue)) {
		g_idle_add ((GSourceFunc) process_snippets, snippet_row->gsearch);
	}

	free_snippet (snippet_row);

	g_free (snippet_markup);
}

static void
add_email_to_search_results (const gchar * uri,
			     const gchar  * mime,
			     const gchar  * subject,
			     const gchar  * sender,
			     GtkListStore * store,
			     GtkTreeIter * iter,
			     GSearchWindow * gsearch)
{
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (gsearch->search_results_tree_view), FALSE);

	gtk_list_store_append (GTK_LIST_STORE (store), iter);
	gtk_list_store_set (GTK_LIST_STORE (store), iter,
			    COLUMN_ICON, gsearch->email_pixbuf,
			    COLUMN_URI, uri,
			    COLUMN_NAME, subject,
			    COLUMN_PATH, sender,
			    COLUMN_MIME, mime,
			    COLUMN_TYPE, SERVICE_EMAILS,
			    COLUMN_NO_FILES_FOUND, FALSE,
			    -1);

	gchar * search_term;

	if (gsearch->search_term) {
		search_term = gsearch->search_term;
	} else {
		search_term = "";
	}

	SnippetRow * snippet_row;

	snippet_row = g_new (SnippetRow, 1);
	snippet_row->gsearch = gsearch;
	snippet_row->uri = g_strdup (uri);
	snippet_row->type = SERVICE_EMAILS;

	g_queue_push_tail (gsearch->snippet_queue, snippet_row);
	//tracker_search_get_snippet_async (tracker_client, SERVICE_EMAILS, uri, search_term, set_snippet, snippet_row);
}

static void
add_file_to_search_results (const gchar * file,
			    ServiceType service_type,
			    const gchar * mime,
			    GtkListStore * store,
			    GtkTreeIter * iter,
			    GSearchWindow * gsearch)
{
	GdkPixbuf * pixbuf;
	GnomeVFSFileInfo * vfs_file_info;

	gchar * description;
	gchar * base_name;
	gchar * dir_name;
	gchar * escape_path_string;
	gchar * uri;

	uri = g_filename_from_utf8 (file, -1, NULL, NULL, NULL);

	if (!g_file_test (uri, G_FILE_TEST_EXISTS)) {
		g_warning ("file %s does not exist", file);
		g_free (uri);
		return;
	}

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (gsearch->search_results_tree_view), FALSE);

	vfs_file_info = gnome_vfs_file_info_new ();

	escape_path_string = gnome_vfs_escape_path_string (uri);

	gnome_vfs_get_file_info (escape_path_string, vfs_file_info,
				 GNOME_VFS_FILE_INFO_DEFAULT |
				 GNOME_VFS_FILE_INFO_FOLLOW_LINKS);


	pixbuf = get_file_pixbuf (gsearch, uri, mime, vfs_file_info);

	description = get_file_type_description (uri, mime, vfs_file_info);

	if (!description) {
		description = g_strdup (mime);
	}

	base_name = g_path_get_basename (file);
	dir_name = g_path_get_dirname (file);

	gchar * search_term;

	if (gsearch->search_term) {
		search_term = gsearch->search_term;
	} else {
		search_term = NULL;
	}

	gtk_list_store_append (GTK_LIST_STORE (store), iter);
	gtk_list_store_set (GTK_LIST_STORE (store), iter,
			    COLUMN_ICON, pixbuf,
			    COLUMN_URI, file,
			    COLUMN_NAME, base_name,
			    COLUMN_PATH, dir_name,
			    COLUMN_MIME, (description != NULL) ? description : mime,
			    COLUMN_TYPE, service_type,
			    COLUMN_NO_FILES_FOUND, FALSE,
			    -1);

	if (search_term  &&
	    (service_type == SERVICE_DOCUMENTS ||
	     service_type == SERVICE_TEXT_FILES ||
	     service_type == SERVICE_DEVELOPMENT_FILES ||
	     gsearch->type == SERVICE_CONVERSATIONS)) {

		SnippetRow * snippet_row;

		snippet_row = g_new (SnippetRow, 1);
		snippet_row->gsearch = gsearch;
		snippet_row->uri = g_strdup (uri);
		snippet_row->type = service_type;

		g_queue_push_tail (gsearch->snippet_queue, snippet_row);
	}

	gnome_vfs_file_info_unref (vfs_file_info);
	g_free (base_name);
	g_free (dir_name);
	g_free (uri);
	g_free (escape_path_string);
	g_free (description);
}

static void
add_application_to_search_results (const gchar * uri,
				   gchar * display_name,
				   const gchar * exec,
				   const gchar * icon,
				   GtkListStore * store,
				   GtkTreeIter * iter,
				   GSearchWindow * gsearch)
{
	GdkPixbuf * pixbuf = NULL;

	if (!g_file_test (uri, G_FILE_TEST_EXISTS)) {
		return;
	}

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (gsearch->search_results_tree_view), FALSE);

	if (icon && icon[0] && icon[1]) {

		/* if icon is a full path then load it from file otherwise its an icon name in a theme */
		if (icon[0] == '/') {
			pixbuf = gdk_pixbuf_new_from_file_at_scale (icon, ICON_SIZE, ICON_SIZE, TRUE, NULL);
		} else {
			pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), icon,
								     ICON_SIZE, 0, NULL);
		}
	}

	if (!pixbuf) {
		pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), ICON_THEME_EXECUTABLE_ICON,
						   ICON_SIZE, 0, NULL);
	}


	gtk_list_store_append (GTK_LIST_STORE (store), iter);

	gtk_list_store_set (GTK_LIST_STORE (store), iter,
			    COLUMN_ICON, pixbuf,
			    COLUMN_URI, uri,
			    COLUMN_NAME, display_name,
			    COLUMN_PATH, "Application",
			    COLUMN_MIME, "",
			    COLUMN_TYPE, SERVICE_APPLICATIONS,
			    COLUMN_EXEC, exec,
			    COLUMN_NO_FILES_FOUND, FALSE,
			    -1);
}


static void
set_suggestion (gchar * suggestion,
		GError * error,
		gpointer user_data)
{
	gchar	      * str;
	GtkWidget     * label;
	GtkWidget     * box1, * box2;
	GtkWidget     * button;
	GSearchWindow * gsearch = user_data;
	gchar	      * search_term = (gchar *) gtk_entry_get_text (GTK_ENTRY (gsearch->search_entry));

	if (strcmp (search_term, suggestion) == 0) {
		return;
	}

	box1 = gtk_hbox_new (FALSE, 0);
	box2 = gtk_hbox_new (FALSE, 0);
	label = gtk_label_new (_("Did you mean"));
	gtk_box_pack_start (GTK_BOX (box2), label, FALSE, TRUE, 0);

	str = g_strconcat ("<b><i><u>", suggestion, "</u></i></b>?", NULL);
	button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_container_add (GTK_CONTAINER (button), label);
	gtk_box_pack_start (GTK_BOX (box2), button, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (gsearch->no_results), box1, FALSE, FALSE, 12);
	gtk_widget_show_all (box1);


	g_object_set_data (G_OBJECT (button), "suggestion", suggestion);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (suggest_search_cb), gsearch);
}

static void
add_no_files_found_message (GSearchWindow * gsearch)
{
	GtkWidget * label;
	gchar	  * search_term = (gchar *) gtk_entry_get_text (GTK_ENTRY (gsearch->search_entry));

	if (!gsearch->no_results) {
		gtk_widget_hide (gsearch->search_results_vbox);

		gsearch->no_results = gtk_vbox_new (FALSE, 0);
		label = gtk_label_new (_("Your search returned no results."));
		gtk_box_pack_start (GTK_BOX (gsearch->no_results), label, FALSE, FALSE, 12);

		gtk_box_pack_start (GTK_BOX (gsearch->message_box), gsearch->no_results, TRUE, TRUE, 12);
		gtk_widget_show_all (gsearch->no_results);

		tracker_search_suggest_async (tracker_client, search_term, 3, (TrackerStringReply) set_suggestion, gsearch);
	}
}

void
update_search_counts (GSearchWindow * gsearch)
{
	gchar * title_bar_string = NULL;

	title_bar_string = g_strconcat ( _("Tracker Search Tool-"), gsearch->search_term ,NULL);
	gtk_window_set_title (GTK_WINDOW (gsearch->window), title_bar_string);
	g_free (title_bar_string);
}

void
update_constraint_info (GSearchConstraint * constraint,
			gchar * info)
{
	switch (GSearchOptionTemplates[constraint->constraint_id].type) {
		case SEARCH_CONSTRAINT_TYPE_TEXT:
			constraint->data.text = info;
			break;
		case SEARCH_CONSTRAINT_TYPE_NUMERIC:
			sscanf (info, "%d", &constraint->data.number);
			break;
		case SEARCH_CONSTRAINT_TYPE_DATE_BEFORE:
		case SEARCH_CONSTRAINT_TYPE_DATE_AFTER:
			sscanf (info, "%d", &constraint->data.time);
			break;
		default:
			g_warning ("Entry changed called for a non entry option!");
			break;
	}
}

void
set_constraint_gconf_boolean (gint constraint_id,
			      gboolean flag)
{
	switch (constraint_id) {
		case SEARCH_CONSTRAINT_CONTAINS_THE_TEXT:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/contains_the_text",
						       flag);
			break;
		case SEARCH_CONSTRAINT_DATE_MODIFIED_BEFORE:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/date_modified_less_than",
						       flag);
			break;
		case SEARCH_CONSTRAINT_DATE_MODIFIED_AFTER:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/date_modified_more_than",
						       flag);
			break;
		case SEARCH_CONSTRAINT_SIZE_IS_MORE_THAN:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/size_at_least",
						       flag);
			break;
		case SEARCH_CONSTRAINT_SIZE_IS_LESS_THAN:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/size_at_most",
						       flag);
			break;
		case SEARCH_CONSTRAINT_FILE_IS_EMPTY:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/file_is_empty",
						       flag);
			break;
		case SEARCH_CONSTRAINT_OWNED_BY_USER:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/owned_by_user",
						       flag);
			break;
		case SEARCH_CONSTRAINT_OWNED_BY_GROUP:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/owned_by_group",
						       flag);
			break;
		case SEARCH_CONSTRAINT_OWNER_IS_UNRECOGNIZED:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/owner_is_unrecognized",
						       flag);
			break;
		case SEARCH_CONSTRAINT_FILE_IS_NOT_NAMED:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/name_does_not_contain",
						       flag);
			break;
		case SEARCH_CONSTRAINT_FILE_MATCHES_REGULAR_EXPRESSION:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/name_matches_regular_expression",
						       flag);
			break;
		case SEARCH_CONSTRAINT_SHOW_HIDDEN_FILES_AND_FOLDERS:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/show_hidden_files_and_folders",
						       flag);
			break;
		case SEARCH_CONSTRAINT_FOLLOW_SYMBOLIC_LINKS:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/follow_symbolic_links",
						       flag);
			break;
		case SEARCH_CONSTRAINT_SEARCH_OTHER_FILESYSTEMS:
			tracker_search_gconf_set_boolean ("/apps/tracker-search-tool/select/include_other_filesystems",
						       flag);
			break;

		default:
			break;
	}
}

/*
 * add_atk_namedesc
 * @widget    : The Gtk Widget for which @name and @desc are added.
 * @name      : Accessible Name
 * @desc      : Accessible Description
 * Description: This function adds accessible name and description to a
 *		Gtk widget.
 */

static void
add_atk_namedesc (GtkWidget * widget,
		  const gchar * name,
		  const gchar * desc)
{
	AtkObject * atk_widget;

	g_assert (GTK_IS_WIDGET (widget));

	atk_widget = gtk_widget_get_accessible (widget);

	if (name != NULL)
		atk_object_set_name (atk_widget, name);
	if (desc !=NULL)
		atk_object_set_description (atk_widget, desc);
}

/*
 * add_atk_relation
 * @obj1      : The first widget in the relation @rel_type
 * @obj2      : The second widget in the relation @rel_type.
 * @rel_type  : Relation type which relates @obj1 and @obj2
 * Description: This function establishes Atk Relation between two given
 *		objects.
 */

/* static void */
/* add_atk_relation (GtkWidget * obj1, */
/*		  GtkWidget * obj2, */
/*		  AtkRelationType rel_type) */
/* { */
/*	AtkObject * atk_obj1, * atk_obj2; */
/*	AtkRelationSet * relation_set; */
/*	AtkRelation * relation; */

/*	g_assert (GTK_IS_WIDGET (obj1)); */
/*	g_assert (GTK_IS_WIDGET (obj2)); */

/*	atk_obj1 = gtk_widget_get_accessible (obj1); */

/*	atk_obj2 = gtk_widget_get_accessible (obj2); */

/*	relation_set = atk_object_ref_relation_set (atk_obj1); */
/*	relation = atk_relation_new (&atk_obj2, 1, rel_type); */
/*	atk_relation_set_add (relation_set, relation); */
/*	g_object_unref (G_OBJECT (relation)); */

/* } */


gchar *
get_desktop_item_name (GSearchWindow * gsearch)
{
	GString * gs;
	gchar * file_is_named_utf8;
	gchar * file_is_named_locale;
	//GList * list;

	gs = g_string_new ("");
	g_string_append (gs, _("Tracker Search Tool"));
	g_string_append (gs, " (");

	file_is_named_utf8 = (gchar *) gtk_entry_get_text (GTK_ENTRY (gsearch->search_entry));
	file_is_named_locale = g_locale_from_utf8 (file_is_named_utf8 != NULL ? file_is_named_utf8 : "" ,
						   -1, NULL, NULL, NULL);
	g_string_append_printf (gs, "named=%s", file_is_named_locale);
	g_free (file_is_named_locale);

	g_string_append_c (gs, ')');
	return g_string_free (gs, FALSE);
}

static gchar *
crop_string (gchar *str,
	     gint max_length)
{
	gchar buffer[1024];
	gint len;
	gchar *s1, *s2;

	g_return_val_if_fail (str && max_length > 0, NULL);

	len = g_utf8_strlen (str, -1);

	if (len < (max_length+3)) {
		return g_strdup (str);
	}

	s1 = g_strdup (g_utf8_strncpy (buffer, str, max_length));

	s2 = g_strconcat (s1, "...", NULL);

	g_free (s1);

	return s2;
}

static void
filename_cell_data_func (GtkTreeViewColumn * column,
			 GtkCellRenderer * renderer,
			 GtkTreeModel * model,
			 GtkTreeIter * iter,
			 GSearchWindow * gsearch)
{
	GtkTreePath * path;
	PangoUnderline underline;
	gboolean underline_set;
	gchar * markup, * fpath, * name, * type = NULL;

	gtk_tree_model_get (model, iter, COLUMN_NAME, &name, -1);
	gtk_tree_model_get (model, iter, COLUMN_PATH, &fpath, -1);
	gtk_tree_model_get (model, iter, COLUMN_MIME, &type, -1);

	gchar * display_name = crop_string (name, 65);
	gchar * display_path = crop_string (fpath, 120);

	gchar * mark_name = g_markup_escape_text (display_name, -1);
	gchar * mark_dir =  g_markup_escape_text (display_path, -1);

	markup = g_strconcat ("<b>", mark_name, "</b>\n", "<span  size='small'>", mark_dir,"</span>\n",
			      "<span  size='small'>",type, "</span>", NULL);

	g_free (display_name);
	g_free (display_path);

	g_free (mark_name);
	g_free (mark_dir);

	g_free (fpath);
	g_free (name);
	g_free (type);

	if (gsearch->is_search_results_single_click_to_activate == TRUE) {

		path = gtk_tree_model_get_path (model, iter);

		if ((gsearch->search_results_hover_path == NULL) ||
		    (gtk_tree_path_compare (path, gsearch->search_results_hover_path) != 0)) {
			underline = PANGO_UNDERLINE_NONE;
			underline_set = FALSE;
		}
		else {
			underline = PANGO_UNDERLINE_SINGLE;
			underline_set = TRUE;
		}
		gtk_tree_path_free (path);

	} else {
		underline = PANGO_UNDERLINE_NONE;
		underline_set = FALSE;
	}

	g_object_set (renderer,
		      "markup", markup,
		      "underline", underline,
		      "underline-set", underline_set,
		      NULL);

	g_free (markup);
}

static void
snippet_cell_data_func (GtkTreeViewColumn * column,
			 GtkCellRenderer * renderer,
			 GtkTreeModel * model,
			 GtkTreeIter * iter,
			 GSearchWindow * gsearch)
{
	gchar * snippet;
	gint width;

	gtk_tree_model_get (model, iter, COLUMN_SNIPPET, &snippet, -1);

	g_object_set (renderer,
		      "markup", snippet,
		      NULL);

	g_free (snippet);

	/* set length of word wrap to available col size */

	width = gtk_tree_view_column_get_width (column);

	if (width > 20) {
		g_object_set (renderer, "wrap_width", width - 3, "wrap-mode",  PANGO_WRAP_WORD, NULL);
	}
}

static gboolean
gsearch_equal_func (GtkTreeModel * model,
		    gint column,
		    const gchar * key,
		    GtkTreeIter * iter,
		    gpointer search_data)
{
	gboolean results = TRUE;
	gchar * name;

	gtk_tree_model_get (model, iter, COLUMN_NAME, &name, -1);

	if (name != NULL) {
		gchar * casefold_key;
		gchar * casefold_name;

		casefold_key = g_utf8_casefold (key, -1);
		casefold_name = g_utf8_casefold (name, -1);

		if ((casefold_key != NULL) &&
		    (casefold_name != NULL) &&
		    (strstr (casefold_name, casefold_key) != NULL)) {
			results = FALSE;
		}
		g_free (casefold_key);
		g_free (casefold_name);
		g_free (name);
	}
	return results;
}

static GtkWidget *
create_search_results_section (GSearchWindow * gsearch)
{
	GtkWidget * label;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * label_box;
	GtkWidget * align_box;
	GtkWidget * image;
	GtkWidget * button_prev;
	GtkWidget * button_next;

	GtkWidget * window;
	GtkTreeViewColumn * column;
	GtkCellRenderer * renderer;

	vbox = gtk_vbox_new (FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

	align_box = gtk_alignment_new (0.0, 1.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (align_box), 18, 3, 0, 0);

	gtk_box_pack_start (GTK_BOX (hbox), align_box, FALSE, TRUE, 0);

	label_box = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (align_box), label_box);


	label = gtk_label_new_with_mnemonic (_("Search _results: "));

	gtk_box_pack_start (GTK_BOX (label_box), label, FALSE, TRUE, 0);
	gtk_label_set_justify	(GTK_LABEL (label), GTK_JUSTIFY_LEFT);

	/* Translators: this will appears as "Search results: no search performed" */
	gsearch->count_label = gtk_label_new (_("no search performed"));
	gtk_label_set_selectable (GTK_LABEL (gsearch->count_label), TRUE);
	tracker_set_atk_relationship(gsearch->count_label,
				     ATK_RELATION_LABELLED_BY,
				     label);
	tracker_set_atk_relationship(label, ATK_RELATION_LABEL_FOR,
				     gsearch->count_label);

	gtk_box_pack_start (GTK_BOX (label_box), gsearch->count_label, FALSE, TRUE, 0);

	button_next = gtk_button_new();
	gtk_button_set_relief (GTK_BUTTON(button_next), GTK_RELIEF_NONE);
	image = gtk_image_new_from_stock ("gtk-go-forward", GTK_ICON_SIZE_SMALL_TOOLBAR);
	//gtk_widget_set_tooltip_text (GTK_BUTTON(button_next), _("Add a meagniful tooltip here"));
	/*FIXME: maybe add an a11y name for this button*/
	gtk_container_add (GTK_CONTAINER(button_next), image);
	gtk_box_pack_end (GTK_BOX (hbox), button_next, FALSE, TRUE, 0);

	button_prev = gtk_button_new();
	gtk_button_set_relief (GTK_BUTTON(button_prev), GTK_RELIEF_NONE);
	image = gtk_image_new_from_stock ("gtk-go-back", GTK_ICON_SIZE_SMALL_TOOLBAR);
	//gtk_widget_set_tooltip_text (GTK_BUTTON(button_prev), _("Add a meagniful tooltip here"));
	/*FIXME: maybe add an a11y name for this button*/
	gtk_container_add (GTK_CONTAINER(button_prev), image);
	gtk_box_pack_end (GTK_BOX (hbox), button_prev, FALSE, TRUE, 0);

	gsearch->back_button = button_prev;
	g_signal_connect (G_OBJECT (gsearch->back_button), "clicked",
			  G_CALLBACK (prev_results_cb), (gpointer) gsearch);


	gsearch->forward_button = button_next;
	g_signal_connect (G_OBJECT (gsearch->forward_button), "clicked",
			  G_CALLBACK (next_results_cb), (gpointer) gsearch);

	gtk_widget_show_all (hbox);


	gtk_widget_set_sensitive (gsearch->forward_button, FALSE);
	gtk_widget_set_sensitive (gsearch->back_button, FALSE);

	gsearch->files_found_label = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX (label_box), gsearch->files_found_label, FALSE, FALSE, 0);

	window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (window), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (window), 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	gsearch->search_results_tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());

	gtk_tree_view_set_headers_visible (gsearch->search_results_tree_view, FALSE);
	gtk_tree_view_set_search_equal_func (gsearch->search_results_tree_view,
					     gsearch_equal_func, NULL, NULL);
	gtk_tree_view_set_rules_hint (gsearch->search_results_tree_view, TRUE);


	if (gsearch->is_window_accessible) {
		add_atk_namedesc (GTK_WIDGET (gsearch->search_results_tree_view), _("List View"), NULL);
	}

	gsearch->search_results_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gsearch->search_results_tree_view));

	gtk_tree_selection_set_mode (GTK_TREE_SELECTION (gsearch->search_results_selection),
				     GTK_SELECTION_MULTIPLE);

	gtk_drag_source_set (GTK_WIDGET (gsearch->search_results_tree_view),
			     GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			     GSearchDndTable, GSearchTotalDnds,
			     GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_ASK);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "drag_data_get",
			  G_CALLBACK (drag_file_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "drag_begin",
			  G_CALLBACK (drag_begin_file_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "event_after",
			  G_CALLBACK (file_event_after_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "button_release_event",
			  G_CALLBACK (file_button_release_event_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "button_press_event",
			  G_CALLBACK (file_button_press_event_cb),
			  (gpointer) gsearch->search_results_tree_view);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "key_press_event",
			  G_CALLBACK (file_key_press_event_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "motion_notify_event",
			  G_CALLBACK (file_motion_notify_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "leave_notify_event",
			  G_CALLBACK (file_leave_notify_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_selection),
			  "changed",
			  G_CALLBACK (select_changed_cb),
			  (gpointer) gsearch);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (gsearch->search_results_tree_view));

	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (gsearch->search_results_tree_view));


	/* metadata_tile */
	gsearch->metatile = tracker_metadata_tile_new ();
	//gtk_widget_show (gsearch->metatile);
	gtk_box_pack_end (GTK_BOX (vbox), gsearch->metatile, FALSE, FALSE, 0);

	gtk_box_pack_end (GTK_BOX (vbox), window, TRUE, TRUE, 0);

	/* create the name column */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Icon"));

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "pixbuf", COLUMN_ICON,
					     NULL);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_reorderable (column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (gsearch->search_results_tree_view), column);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Name"));
	gsearch->search_results_name_cell_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, gsearch->search_results_name_cell_renderer, TRUE);

	gtk_tree_view_column_set_attributes (column, gsearch->search_results_name_cell_renderer,
					     "text", COLUMN_NAME,
					     NULL);

	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	gtk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);
	gtk_tree_view_column_set_reorderable (column, TRUE);

	gtk_tree_view_column_set_cell_data_func (column, gsearch->search_results_name_cell_renderer,
						 (GtkTreeCellDataFunc) filename_cell_data_func,
						 gsearch, NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (gsearch->search_results_tree_view), column);

	/* create the snippet column */
	renderer = gtk_cell_renderer_text_new ();

	column = gtk_tree_view_column_new_with_attributes (_("Text"), renderer,
							   "text", COLUMN_SNIPPET,
							   NULL);

	gtk_tree_view_column_set_expand (column, TRUE);

	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 (GtkTreeCellDataFunc) snippet_cell_data_func,
						 gsearch, NULL);

	gtk_tree_view_column_set_reorderable (column, TRUE);

	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_min_width (column, 0);
	gtk_tree_view_column_set_max_width (column, 10000);

	gtk_tree_view_append_column (GTK_TREE_VIEW (gsearch->search_results_tree_view), column);

//	gtk_tree_view_set_grid_lines (GTK_TREE_VIEW (gsearch->search_results_tree_view), GTK_TREE_VIEW_GRID_LINES_VERTICAL);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (gsearch->search_results_tree_view), FALSE);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "columns-changed",
			  G_CALLBACK (columns_changed_cb),
			  (gpointer) gsearch);

	return vbox;
}

static GtkWidget *
create_sidebar (GSearchWindow * gsearch)
{
	GtkWidget * window;
	GtkTreeViewColumn * column;
	GtkCellRenderer * renderer;

	GtkWidget *vbox = gtk_vbox_new (FALSE, 11);

	GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

	window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (window), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (window), 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	gsearch->category_list =  gtk_tree_view_new ();

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (gsearch->category_list), FALSE);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (gsearch->category_list), TRUE);

	if (gsearch->is_window_accessible) {
		add_atk_namedesc (gsearch->category_list, _("List View"), NULL);
	}

	gsearch->category_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gsearch->category_list));

	gtk_tree_selection_set_mode (GTK_TREE_SELECTION (gsearch->category_selection),
				     GTK_SELECTION_BROWSE);

	g_signal_connect (G_OBJECT (gsearch->category_selection),
			  "changed",
			  G_CALLBACK (category_changed_cb),
			  (gpointer) gsearch);

	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (gsearch->category_list));

	/* create the  columns */
	column = gtk_tree_view_column_new ();

	gtk_tree_view_column_set_title (column, _("_Categories"));
	gsearch->category_name_cell_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_end (column, gsearch->category_name_cell_renderer, TRUE);

	gtk_tree_view_column_set_attributes (column, gsearch->category_name_cell_renderer,
					     "text", CATEGORY_TITLE,
					     NULL);

	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);



	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "pixbuf", CATEGORY_ICON_NAME,
					     NULL);

	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	gtk_tree_view_append_column (GTK_TREE_VIEW (gsearch->category_list), column);


	gtk_tree_view_set_model (GTK_TREE_VIEW (gsearch->category_list), GTK_TREE_MODEL (gsearch->category_store));

	gtk_box_pack_end (GTK_BOX (vbox), window, TRUE, TRUE, 0);

	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (gsearch->category_list), FALSE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (gsearch->category_list), TRUE);

	return vbox;
}

static void
register_tracker_search_icon (GtkIconFactory * factory)
{
	GtkIconSource * source;
	GtkIconSet * icon_set;

	source = gtk_icon_source_new ();

	gtk_icon_source_set_icon_name (source, TRACKER_SEARCH_TOOL_ICON);

	icon_set = gtk_icon_set_new ();
	gtk_icon_set_add_source (icon_set, source);

	gtk_icon_factory_add (factory, TRACKER_SEARCH_TOOL_STOCK, icon_set);

	gtk_icon_set_unref (icon_set);

	gtk_icon_source_free (source);
}

static void
tracker_search_init_stock_icons (void)
{
	GtkIconFactory * factory;
	GtkIconSize tracker_search_icon_size;

	tracker_search_icon_size = gtk_icon_size_register ("panel-menu",
							TRACKER_SEARCH_TOOL_DEFAULT_ICON_SIZE,
							TRACKER_SEARCH_TOOL_DEFAULT_ICON_SIZE);

	factory = gtk_icon_factory_new ();
	gtk_icon_factory_add_default (factory);

	register_tracker_search_icon (factory);

	g_object_unref (factory);
}

void
set_clone_command (GSearchWindow * gsearch,
		   gint * argcp,
		   gchar *** argvp,
		   gpointer client_data,
		   gboolean escape_values)
{
	gchar ** argv;
	gchar * file_is_named_utf8;
	gchar * file_is_named_locale;
	gchar * tmp;
	gint  i = 0;

	argv = g_new0 (gchar*, SEARCH_CONSTRAINT_MAXIMUM_POSSIBLE);

	argv[i++] = (gchar *) client_data;

	file_is_named_utf8 = (gchar *) gtk_entry_get_text (GTK_ENTRY (gsearch->search_entry));
	file_is_named_locale = g_locale_from_utf8 (file_is_named_utf8 != NULL ? file_is_named_utf8 : "" ,
						   -1, NULL, NULL, NULL);
	if (escape_values) {
		tmp = g_shell_quote (file_is_named_locale);
	} else {
		tmp = g_strdup (file_is_named_locale);
	}
	argv[i++] = g_strdup_printf ("--named=%s", tmp);
	g_free (tmp);
	g_free (file_is_named_locale);

	*argvp = argv;
	*argcp = i;
}

static void
tracker_search_ui_manager (GSearchWindow * gsearch)
{
	GtkActionGroup * action_group;
	GtkAccelGroup * accel_group;
	GtkAction * action;
	GError * error = NULL;

	action_group = gtk_action_group_new ("PopupActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group, GSearchUiEntries, G_N_ELEMENTS (GSearchUiEntries), gsearch->window);

	gsearch->window_ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (gsearch->window_ui_manager, action_group, 0);

	accel_group = gtk_ui_manager_get_accel_group (gsearch->window_ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (gsearch->window), accel_group);

	if (!gtk_ui_manager_add_ui_from_string (gsearch->window_ui_manager, GSearchUiDescription, -1, &error)) {
		g_message ("Building menus failed: %s", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}

	action = gtk_ui_manager_get_action (gsearch->window_ui_manager, "/PopupMenu/Open");
	g_signal_connect (G_OBJECT (action),
			  "activate",
			  G_CALLBACK (open_file_cb),
			  (gpointer) gsearch);

	action = gtk_ui_manager_get_action (gsearch->window_ui_manager, "/PopupMenu/OpenFolder");
	g_signal_connect (G_OBJECT (action),
			  "activate",
			  G_CALLBACK (open_folder_cb),
			  (gpointer) gsearch);

	action = gtk_ui_manager_get_action (gsearch->window_ui_manager, "/PopupMenu/MoveToTrash");
	g_signal_connect (G_OBJECT (action),
			  "activate",
			  G_CALLBACK (move_to_trash_cb),
			  (gpointer) gsearch);

	action = gtk_ui_manager_get_action (gsearch->window_ui_manager, "/PopupMenu/SaveResultsAs");
	g_signal_connect (G_OBJECT (action),
			  "activate",
			  G_CALLBACK (show_file_selector_cb),
			  (gpointer) gsearch);

	gsearch->search_results_popup_menu = gtk_ui_manager_get_widget (gsearch->window_ui_manager,
									"/PopupMenu");
	gsearch->search_results_save_results_as_item = gtk_ui_manager_get_widget (gsearch->window_ui_manager,
										  "/PopupMenu/SaveResultsAs");
}

static void
gsearch_window_size_allocate (GtkWidget * widget,
			      GtkAllocation * allocation,
			      GSearchWindow * gsearch)
{
	if (gsearch->is_window_maximized == FALSE) {
		gsearch->window_width = allocation->width;
		gsearch->window_height = allocation->height;
	}
}

static void
get_meta_table_data (gpointer value,
		     gpointer data)
{
	gchar **meta;
	GSearchWindow * gsearch = data;

	meta = (char **)value;

	if (gsearch->type == SERVICE_EMAILS) {

		if (meta[0] && meta[1] && meta[2]) {
			gchar * subject = "Unknown email subject", * sender = "Unknown email sender";

			if (meta[3]) {
				subject = meta[3];
				if (meta[4]) {
					sender = meta[4];
				}
			}

			add_email_to_search_results (meta[0], meta[2],
						     subject, sender, gsearch->search_results_list_store, &gsearch->search_results_iter, gsearch);
		}

	} else {
		if (meta[0] && meta[1] && meta[2]) {

			if (gsearch->type == SERVICE_APPLICATIONS) {

				if (!meta[3] || !meta[4]) {
					return;
				}

				gchar *icon=NULL, *exec = meta[4], *name = meta[3];

				if (meta[5]) {
					icon = meta[5];
				}

				add_application_to_search_results (meta[0], name, exec, icon,
								   gsearch->search_results_list_store, &gsearch->search_results_iter, gsearch);

			} else {
				add_file_to_search_results (meta[0], tracker_service_name_to_type (meta[1]), meta[2],
							    gsearch->search_results_list_store, &gsearch->search_results_iter, gsearch);
			}
		}
	}

}

static gint
str_in_array (const gchar *str,
	      gchar **array)
{
	gint  i;
	gchar **st;

	for (i = 0, st = array; *st; st++, i++) {
		if (strcasecmp (*st, str) == 0) {
			return i;
		}
	}

	return -1;
}

static void
populate_hit_counts (gpointer value,
		     gpointer data)

{
	gchar **meta;
	gint type;
	service_info_t *service;

	meta = (char **)value;

	if (meta[0] && meta[1]) {

		type = str_in_array (meta[0], (char**) search_service_types);

		if (type != -1) {
			for (service = services; service->service; ++service) {
				if (strcmp(service->service,meta[0]) == 0) {
					service->hit_count = atoi (meta[1]);
					break;
				}
			}
		}
	}
}

static void
update_page_count_label (GSearchWindow * gsearch)
{
	gint from, to, count;
	gchar * label_str;

	count = gsearch->current_service->hit_count;
	from = gsearch->current_service->offset+1;

	if (MAX_SEARCH_RESULTS + from > count) {
		to = count;
	} else {
		to = MAX_SEARCH_RESULTS + from -1;
	}

	if (count > 5) {
		/* Translators: this will appear like "Search results: 5 - 10 of 30 items" */
		label_str = g_strdup_printf (_("%d - %d of %d items"), from, to, count);
	} else
		/* Translators: this will appear like "Search results: 7 items" */
		label_str = g_strdup_printf (ngettext ("%d item", "%d items", count), count);

	gtk_label_set_text (GTK_LABEL (gsearch->count_label), label_str);
	g_free (label_str);

	if (gsearch->current_service->hit_count < gsearch->current_service->offset + 1 + MAX_SEARCH_RESULTS) {
		gtk_widget_set_sensitive (gsearch->forward_button, FALSE);
	} else {
		gtk_widget_set_sensitive (gsearch->forward_button, TRUE);
	}

	if (gsearch->current_service->offset > 0) {
		gtk_widget_set_sensitive (gsearch->back_button, TRUE);
	} else {
		gtk_widget_set_sensitive (gsearch->back_button, FALSE);
	}
}

static void
init_tab (GSearchWindow * gsearch,
	  service_info_t * service)
{
	gsearch->search_results_list_store = service->store;
	gtk_tree_view_set_model (gsearch->search_results_tree_view, GTK_TREE_MODEL (service->store));

	update_page_count_label (gsearch);

	GtkAction * action = gtk_ui_manager_get_action (gsearch->window_ui_manager, "/PopupMenu/OpenFolder");
	gtk_action_set_sensitive (action, (gsearch->type < 10));

	action = gtk_ui_manager_get_action (gsearch->window_ui_manager, "/PopupMenu/MoveToTrash");
	gtk_action_set_sensitive (action, (gsearch->type < 10));

	action = gtk_ui_manager_get_action (gsearch->window_ui_manager, "/PopupMenu/SaveResultsAs");
	gtk_action_set_sensitive (action, (gsearch->type < 10));
}

static void
get_hit_count (GPtrArray *out_array,
	       GError *error,
	       gpointer user_data)
{
	service_info_t	*service;
	gboolean	first_service = FALSE, has_hits = FALSE;

	GSearchWindow *gsearch = user_data;

	if (error) {
		display_error_dialog (gsearch->window, _("Could not connect to search service as it may be busy"));
		g_error_free (error);
		return;
	}

	if (out_array) {
		g_ptr_array_foreach (out_array, (GFunc) populate_hit_counts, NULL);
		g_ptr_array_free (out_array, TRUE);
		out_array = NULL;
	}

	/* reset and create categories with hits > 0 */

	gtk_list_store_clear (gsearch->category_store);


	for (service = services; service->service; ++service) {

		if (service->hit_count == 0) {
			continue;
		}

		has_hits = TRUE;

		gtk_list_store_append (gsearch->category_store, &gsearch->category_iter);

		gchar * label_tmp = g_strdup (_(service->display_name));
		gchar * label_str = g_strdup_printf ("%s (%d)", label_tmp, service->hit_count);
		g_free (label_tmp);


		gtk_list_store_set (gsearch->category_store, &gsearch->category_iter,
				    CATEGORY_ICON_NAME, service->pixbuf,
				    CATEGORY_TITLE, label_str,
				    CATEGORY_SERVICE, service->service,
				    -1);

		g_free (label_str);

		if (gsearch->old_type == service->service_type) {
			first_service = TRUE;
			gsearch->current_service = service;
			gsearch->type = service->service_type;
			init_tab (gsearch, service);
		}
	}

	if (!first_service) {

		if (!has_hits) {

			add_no_files_found_message (gsearch);
			gsearch->page_setup_mode = FALSE;
			gsearch->current_service = NULL;
			gsearch->type = -1;
			stop_animation (gsearch);
			tracker_update_metadata_tile (gsearch);
			return;
		}

		/* old category not found so go to first one with hits */
		for (service = services; service->service; ++service) {
			if (service->hit_count == 0) {
				continue;
			}

			gsearch->current_service = service;
			gsearch->type = service->service_type;
			gsearch->old_type = gsearch->type;
			init_tab (gsearch, service);

			break;
		}
	}

	gsearch->page_setup_mode = FALSE;

	do_search (gsearch, gsearch->search_term, TRUE, 0);
}

void
select_category (GtkTreeSelection * treeselection,
		 gpointer user_data)
{
	GSearchWindow * gsearch = user_data;
	GtkTreeIter iter;
	gchar * name;

	if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION (gsearch->category_selection)) == 0) {
		return;
	}

	GtkTreeModel * model = gtk_tree_view_get_model (GTK_TREE_VIEW (gsearch->category_list));

	gtk_tree_selection_get_selected (GTK_TREE_SELECTION (gsearch->category_selection),
					 &model,
					 &iter);

	gtk_tree_model_get (model, &iter, 2, &name, -1);

	service_info_t * service = g_hash_table_lookup (gsearch->category_table, name);

	g_free (name);

	if (!service) {
		return;
	}

	gsearch->current_service = service;
	gsearch->type = service->service_type;

	g_queue_foreach (gsearch->snippet_queue, (GFunc) free_snippet, NULL);
	g_queue_free (gsearch->snippet_queue);
	gsearch->snippet_queue = g_queue_new ();

	init_tab (gsearch, gsearch->current_service);

	gsearch->old_type = gsearch->type;

	do_search (gsearch, gsearch->search_term, FALSE, service->offset);
}

void
start_new_search (GSearchWindow * gsearch,
		  const gchar * query)
{
	service_info_t	* service;

	if (tracker_is_empty_string (query)) {
		return;
	}

	gtk_widget_set_sensitive (gsearch->category_list, TRUE);

	/* yes, we are comparing pointer addresses here */
	if (gsearch->search_term && gsearch->search_term != query) {
		g_free (gsearch->search_term);
		gsearch->search_term = NULL;
	}

	if (gsearch->search_term == NULL) {
		gsearch->search_term = g_strdup (query);
	}

	gsearch->page_setup_mode = TRUE;

	gtk_widget_show (gsearch->search_results_vbox);

	if (gsearch->no_results) {
		gtk_widget_destroy (gsearch->no_results);
		gsearch->no_results = NULL;
	}

	for (service = services; service->service; ++service) {
		service->has_hits = FALSE;

		service->hit_count = 0;
		service->offset = 0;

		gtk_list_store_clear (service->store);
	}

	tracker_search_text_get_hit_count_all_async (tracker_client, query, (TrackerGPtrArrayReply) get_hit_count, gsearch);
}

static void
end_refresh_count (int count, GError * error, gpointer user_data)
{
	GSearchWindow *gsearch = user_data;
	service_info_t	* service;

	for (service = services; service->service; ++service) {
		if (service->service_type == gsearch->current_service->service_type) {
			service->hit_count = count;
			break;
		}
	}

	update_page_count_label (gsearch);

}

void
end_search (GPtrArray * out_array,
	    GError * error,
	    gpointer user_data)
{
	GSearchWindow *gsearch = user_data;

	gsearch->is_locate_database_check_finished = TRUE;
	stop_animation (gsearch);

	if (error) {
		display_error_dialog (gsearch->window,	_("Could not connect to search service as it may be busy"));
		g_error_free (error);
		return;
	}

	GError *error2 = NULL;
	gchar* status = tracker_get_status (tracker_client, &error2);

	if (error2) {
		g_error_free (error2);
		status = g_strdup ("Indexing");
	}

	if (strcmp (status, "Idle") == 0) {
		gtk_widget_hide (gsearch->warning_label);
	} else {
		gtk_widget_show (gsearch->warning_label);
	}

	g_free (status);

	if (out_array) {

		gsearch->current_service->has_hits = TRUE;

		/* update hit count after search in case of dud hits */

		tracker_search_text_get_hit_count_async	(tracker_client, gsearch->current_service->service_type,
							 gsearch->search_term,
							 (TrackerIntReply)end_refresh_count,
							 gsearch);



		gsearch->search_results_list_store = gsearch->current_service->store;

		gsearch->command_details->command_status = RUNNING;

		gtk_list_store_clear (GTK_LIST_STORE (gsearch->search_results_list_store));

		g_ptr_array_foreach (out_array, (GFunc)get_meta_table_data, gsearch);
		g_ptr_array_free (out_array, TRUE);


		GtkTreeModel *model = gtk_tree_view_get_model (gsearch->search_results_tree_view);

		GtkTreeIter iter;
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			gtk_tree_selection_select_iter (gsearch->search_results_selection, &iter);
		}

		/* process snippets */
		g_idle_add ((GSourceFunc) process_snippets, gsearch);

	} else {
		gsearch->current_service->offset = 0;
		gsearch->current_service->hit_count = 0;
		gtk_widget_set_sensitive (gsearch->forward_button, FALSE);
		gtk_widget_set_sensitive (gsearch->category_list, FALSE);
	}

	tracker_update_metadata_tile (gsearch);
}

void
do_search (GSearchWindow * gsearch,
	   const gchar * query,
	   gboolean new_search,
	   gint search_offset)
{
	start_animation (gsearch, TRUE);

	if (!new_search) {

		if (gsearch->current_service->has_hits && (gsearch->current_service->offset == search_offset)) {
			update_page_count_label (gsearch);

			GtkTreeModel *model = gtk_tree_view_get_model (gsearch->search_results_tree_view);

			GtkTreeIter iter;
			if (gtk_tree_model_get_iter_first (model, &iter)) {
				gtk_tree_selection_select_iter (gsearch->search_results_selection, &iter);
			}
			stop_animation (gsearch);
			tracker_update_metadata_tile (gsearch);
			return;
		}
	}

	gsearch->current_service->offset = search_offset;
	tracker_search_text_detailed_async (tracker_client,
					    -1,
					    gsearch->current_service->service_type,
					    query,
					    search_offset, MAX_SEARCH_RESULTS,
					    (TrackerGPtrArrayReply)end_search,
					    gsearch);
}

static GtkWidget *
gsearch_app_create (GSearchWindow * gsearch)
{
	GtkWidget * hbox;
	GtkWidget * vbox;
	GtkWidget * entry;
	GtkWidget * label;
	GtkWidget * container;
	GtkWidget * main_container;
	service_info_t	*service;

	gsearch->snippet_queue = g_queue_new ();

	gsearch->category_table = g_hash_table_new (g_str_hash, g_str_equal);

	for (service = services; service->service; service++) {
		g_hash_table_insert (gsearch->category_table,
				     g_strdup (service->service),
				     service);
	}

	gsearch->category_store = gtk_list_store_new (NUM_CATEGORY_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	GtkIconTheme * theme = gtk_icon_theme_get_default ();

	for (service = services; service->service; ++service) {

		service->store = gtk_list_store_new (NUM_COLUMNS,
						     GDK_TYPE_PIXBUF,
						     G_TYPE_STRING,
						     G_TYPE_STRING,
						     G_TYPE_STRING,
						     G_TYPE_STRING,
						     G_TYPE_STRING,
						     G_TYPE_STRING,
						     G_TYPE_INT,
						     G_TYPE_BOOLEAN);

		service->pixbuf = gtk_icon_theme_load_icon (theme, service->icon_name,
							    24,
							    GTK_ICON_LOOKUP_USE_BUILTIN,
							    NULL);

		g_object_ref (service->pixbuf);
	}

	gsearch->email_pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default(), "email",
					 ICON_SIZE,
					 GTK_ICON_LOOKUP_USE_BUILTIN,
					 NULL);

	gsearch->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gsearch->is_window_maximized = tracker_search_gconf_get_boolean ("/apps/tracker-search-tool/default_window_maximized");
	g_signal_connect (G_OBJECT (gsearch->window), "size-allocate",
			  G_CALLBACK (gsearch_window_size_allocate),
			  gsearch);
	gsearch->command_details = g_slice_new0 (GSearchCommandDetails);
	gsearch->window_geometry.min_height = -1;
	gsearch->window_geometry.min_width  = -1;
	gsearch->search_term = NULL;

	gtk_window_set_position (GTK_WINDOW (gsearch->window), GTK_WIN_POS_CENTER);
	gtk_window_set_geometry_hints (GTK_WINDOW (gsearch->window), GTK_WIDGET (gsearch->window),
				       &gsearch->window_geometry, GDK_HINT_MIN_SIZE);

	tracker_search_get_stored_window_geometry (&gsearch->window_width,
						&gsearch->window_height);
	gtk_window_set_default_size (GTK_WINDOW (gsearch->window),
				     gsearch->window_width,
				     gsearch->window_height);

	if (gsearch->is_window_maximized == TRUE) {
		gtk_window_maximize (GTK_WINDOW (gsearch->window));
	}

	main_container = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (gsearch->window), main_container);
	gtk_container_set_border_width (GTK_CONTAINER (main_container), 0);

	container = gtk_vbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (main_container), container);
	gtk_container_set_border_width (GTK_CONTAINER (container), 1);


	GtkWidget * widget;
	char *search_label;

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 3);

	gsearch->name_and_folder_table = gtk_table_new (2, 4, FALSE);
	gtk_container_add (GTK_CONTAINER (hbox), gsearch->name_and_folder_table);

	label = gtk_label_new (NULL);
	search_label = g_strconcat ("<b>", _("_Search:"), "</b>", NULL);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), search_label);
	g_free (search_label);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (label), "xalign", 0.0, NULL);

	gtk_table_attach (GTK_TABLE (gsearch->name_and_folder_table), label, 0, 1, 0, 1, GTK_FILL, 0, 6, 1);

	gsearch->search_entry = sexy_icon_entry_new ();
	sexy_icon_entry_add_clear_button (SEXY_ICON_ENTRY (gsearch->search_entry));
	gtk_table_attach (GTK_TABLE (gsearch->name_and_folder_table), gsearch->search_entry, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
	entry =  (gsearch->search_entry);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);

	gsearch->warning_label = gtk_label_new (_("Tracker is still indexing so not all search results are available yet"));
	gtk_label_set_selectable (GTK_LABEL (gsearch->warning_label), TRUE);
	gtk_label_set_justify (GTK_LABEL (gsearch->warning_label), GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (gsearch->warning_label), "xalign", 0.0, NULL);
	gtk_table_attach (GTK_TABLE (gsearch->name_and_folder_table), gsearch->warning_label, 0, 2, 1, 2, GTK_FILL, 0, 6, 1);

	hbox = gtk_hbutton_box_new ();
	gtk_table_attach (GTK_TABLE (gsearch->name_and_folder_table), hbox, 3, 4, 0, 1, GTK_FILL, 0, 6, 0);

	gsearch->find_button = gtk_button_new_from_stock (GTK_STOCK_FIND);
	gtk_container_add (GTK_CONTAINER (hbox), gsearch->find_button);

	if (GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (gsearch->search_entry))) {
		gsearch->is_window_accessible = TRUE;
		add_atk_namedesc (gsearch->search_entry, NULL, _("Enter a search term with multiple words seperated with spaces."));
		add_atk_namedesc (entry, _("search_entry"), _("Enter a search term with multiple words seperated with spaces."));
	}

	g_signal_connect (G_OBJECT (gsearch->search_entry), "activate",
			  G_CALLBACK (name_contains_activate_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_entry), "changed",
			  G_CALLBACK (text_changed_cb),
			  (gpointer) gsearch);

	gsearch->show_more_options_expander = gtk_expander_new_with_mnemonic ("Select more _options");

	/* paned container for search results and category sections */

	gsearch->pane = gtk_hpaned_new ();
	gtk_paned_set_position (GTK_PANED (gsearch->pane), tracker_get_stored_separator_position ());

	gtk_box_pack_start (GTK_BOX (container), gsearch->pane, TRUE, TRUE, 3);

	/* layout container for results section */

	vbox = gtk_vbox_new (FALSE, 2);
	gsearch->message_box = vbox;

	gsearch->no_results = NULL;

	gtk_paned_pack2 (GTK_PANED (gsearch->pane), vbox, TRUE, FALSE);

	/* search results panel */

	gsearch->search_results_vbox = create_search_results_section (gsearch);
	gtk_tree_view_set_model (gsearch->search_results_tree_view, GTK_TREE_MODEL (services[0].store));
	gtk_widget_set_sensitive (GTK_WIDGET (gsearch->search_results_vbox), FALSE);

	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (gsearch->search_results_vbox), TRUE, TRUE, 0);

	GTK_WIDGET_SET_FLAGS (gsearch->find_button, GTK_CAN_DEFAULT);

	gtk_widget_set_sensitive (gsearch->find_button, TRUE);

	g_signal_connect (G_OBJECT (gsearch->find_button), "clicked",
			  G_CALLBACK (click_find_cb),
			  (gpointer) gsearch);

	if (gsearch->is_window_accessible) {
		add_atk_namedesc (GTK_WIDGET (gsearch->find_button), NULL, _("Click to perform a search."));
	}

	/* category sidebar */

	widget = create_sidebar (gsearch);
	gtk_paned_pack1 (GTK_PANED (gsearch->pane), widget, TRUE, FALSE);

	gtk_widget_set_sensitive (gsearch->category_list, FALSE);

	gtk_widget_set_sensitive (gsearch->forward_button, FALSE);
	gtk_widget_set_sensitive (gsearch->back_button, FALSE);

	gtk_window_set_focus (GTK_WINDOW (gsearch->window),
		GTK_WIDGET (gsearch->search_entry));

	gtk_window_set_default (GTK_WINDOW (gsearch->window), gsearch->find_button);


	gtk_widget_show_all (main_container);

	gtk_widget_hide (gsearch->warning_label);

	return gsearch->window;
}

static void
gsearch_window_finalize (GObject * object)
{
	parent_class->finalize (object);
}

static void
gsearch_window_class_init (GSearchWindowClass * klass)
{
	GObjectClass * object_class = (GObjectClass *) klass;

	object_class->finalize = gsearch_window_finalize;
	parent_class = g_type_class_peek_parent (klass);
}

GType
gsearch_window_get_type (void)
{
	static GType object_type = 0;

	if (!object_type) {
		static const GTypeInfo object_info = {
			sizeof (GSearchWindowClass),
			NULL,
			NULL,
			(GClassInitFunc) gsearch_window_class_init,
			NULL,
			NULL,
			sizeof (GSearchWindow),
			0,
			(GInstanceInitFunc) gsearch_app_create
		};
		object_type = g_type_register_static (GTK_TYPE_WINDOW, "GSearchWindow", &object_info, 0);
	}
	return object_type;
}

static void
tracker_search_setup_gconf_notifications (GSearchWindow * gsearch)
{
	gchar * click_to_activate_pref = NULL;

	/* Get value of nautilus click behavior (single or double click to activate items) */
/*	click_to_activate_pref = tracker_search_gconf_get_string ("/apps/nautilus/preferences/click_policy"); */

	if (click_to_activate_pref == NULL) {
		gsearch->is_search_results_single_click_to_activate = FALSE;
		return;
	}

	gsearch->is_search_results_single_click_to_activate =
		(strncmp (click_to_activate_pref, "single", 6) == 0) ? TRUE : FALSE;

	tracker_search_gconf_watch_key ("/apps/nautilus/preferences",
				     "/apps/nautilus/preferences/click_policy",
				     (GConfClientNotifyFunc) single_click_to_activate_key_changed_cb,
				     gsearch);

	g_free (click_to_activate_pref);
}

gchar *
tracker_search_pixmap_file (const gchar * partial_path)
{
	gchar * path;

	path = g_build_filename (TRACKER_DATADIR "/tracker/icons", partial_path, NULL);
	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		return path;
	} else {
		return g_strdup (partial_path);
	}
	g_free (path);
	return NULL;
}

gint
main (gint argc,
      gchar * argv[])
{
	GSearchWindow * gsearch;
	GOptionContext * option_context;
	GError *error = NULL;
	GnomeProgram * program;
	GnomeClient * client;
	GtkWidget * window;
	gchar * search_string;

	option_context = g_option_context_new ("tracker-search-tool");
	g_option_context_add_main_entries (option_context, options, NULL);

	if (error) {
		g_printerr ("invalid arguments: %s\n", error->message);
		return 1;
	}

	bindtextdomain (GETTEXT_PACKAGE, TRACKER_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	program = gnome_program_init ("tracker-search-tool",
				      VERSION,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_APP_DATADIR, TRACKER_DATADIR,
				      GNOME_PARAM_GOPTION_CONTEXT, option_context,
				      GNOME_PARAM_NONE);

	g_set_application_name (_("Tracker Search Tool"));


	tracker_search_init_stock_icons ();

	window = g_object_new (GSEARCH_TYPE_WINDOW, NULL);
	gsearch = GSEARCH_WINDOW (window);

	//gsearch->search_results_pixbuf_hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	tracker_search_ui_manager (gsearch);

	gtk_window_set_icon_name (GTK_WINDOW (gsearch->window), "system-search");

	gtk_window_set_default_icon_name ("tracker");

	gtk_window_set_wmclass (GTK_WINDOW (gsearch->window), "tracker-search-tool", "tracker-search-tool");
	gtk_window_set_policy (GTK_WINDOW (gsearch->window), TRUE, TRUE, TRUE);

	g_signal_connect (G_OBJECT (gsearch->window), "delete_event",
				    G_CALLBACK (quit_cb),
				    (gpointer) gsearch);
	g_signal_connect (G_OBJECT (gsearch->window), "key_press_event",
				    G_CALLBACK (key_press_cb),
				    (gpointer) gsearch);
	g_signal_connect (G_OBJECT (gsearch->window), "window_state_event",
				    G_CALLBACK (window_state_event_cb),
				    (gpointer) gsearch);

	if ((client = gnome_master_client ()) != NULL) {
		g_signal_connect (client, "save_yourself",
				  G_CALLBACK (save_session_cb),
				  (gpointer) gsearch);
		g_signal_connect (client, "die",
				  G_CALLBACK (die_cb),
				  (gpointer) gsearch);
	}

	gtk_widget_show (gsearch->window);

	tracker_client = tracker_connect (FALSE);

	tracker_search_setup_gconf_notifications (gsearch);


	if (terms) {
		search_string = g_strjoinv (" ", terms);
		gtk_entry_set_text (GTK_ENTRY (gsearch->search_entry), search_string);
		g_free (search_string);
		gtk_button_clicked (GTK_BUTTON (gsearch->find_button));

	} else {
		gtk_widget_set_sensitive (gsearch->find_button, FALSE);
	}

	gtk_main ();

	return 0;
}
