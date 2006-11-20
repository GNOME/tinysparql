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
 *  
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
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

#include <gnome.h>

#include "../libtracker/tracker.h"
#include "tracker-search-tool.h"
#include "tracker-search-tool-callbacks.h"
#include "tracker-search-tool-support.h"
#include "sexy-icon-entry.h"

#define TRACKER_SEARCH_TOOL_DEFAULT_ICON_SIZE 32
#define TRACKER_SEARCH_TOOL_STOCK "panel-searchtool"
#define TRACKER_SEARCH_TOOL_REFRESH_DURATION  50000
#define LEFT_LABEL_SPACING "     "

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
   gchar        *service;
   gchar        *icon_name;
   ServiceType   service_type;
} service_info_t;

typedef struct {
	GSearchWindow * gsearch;
	char  * path;
	char  * name; 
} SnippetRow;


struct _GSearchOptionTemplate {
	GSearchConstraintType type; /* The available option type */
	gchar * option;             /* An option string to pass to the command */
	gchar * desc;               /* The description for display */
	gchar * units;              /* Optional units for display */
	gboolean is_selected;
};

static service_info_t services[8] = {
   { N_("All files"),    "system-file-manager",       SERVICE_FILES         },
   { N_("Development"),  "applications-development",  SERVICE_DEVELOPMENT_FILES },
   { N_("Documents"),    "x-office-document",         SERVICE_DOCUMENTS     },
   { N_("Images"),       "image",         	      SERVICE_IMAGES        },
   { N_("Music"),        "audio-x-generic",           SERVICE_MUSIC         },
   { N_("Plain text"),   "text-x-generic",            SERVICE_TEXT_FILES    },
   { N_("Videos"),       "video-x-generic",           SERVICE_VIDEOS        },
   { NULL,               NULL,                        -1                    }
};


static GSearchOptionTemplate GSearchOptionTemplates[] = {
	{ SEARCH_CONSTRAINT_TYPE_TEXT, NULL, N_("Contains the _text"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_DATE_BEFORE, "-mtime -%d", N_("_Date modified less than"), N_("days"), FALSE },
	{ SEARCH_CONSTRAINT_TYPE_DATE_AFTER, "\\( -mtime +%d -o -mtime %d \\)", N_("Date modified more than"), N_("days"), FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_NUMERIC, "\\( -size %uc -o -size +%uc \\)", N_("S_ize at least"), N_("kilobytes"), FALSE },
	{ SEARCH_CONSTRAINT_TYPE_NUMERIC, "\\( -size %uc -o -size -%uc \\)", N_("Si_ze at most"), N_("kilobytes"), FALSE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "-size 0c \\( -type f -o -type d \\)", N_("File is empty"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_TEXT, "-user '%s'", N_("Owned by _user"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_TEXT, "-group '%s'", N_("Owned by _group"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "\\( -nouser -o -nogroup \\)", N_("Owner is unrecognized"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_TEXT, "'!' -name '*%s*'", N_("Na_me does not contain"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_TEXT, "-regex '%s'", N_("Name matches regular e_xpression"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "SHOW_HIDDEN_FILES", N_("Show hidden and backup files"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "-follow", N_("Follow symbolic links"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "INCLUDE_OTHER_FILESYSTEMS", N_("Include other filesystems"), NULL, FALSE },
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
	{ "STRING",        0, 0 }
};

static guint GSearchTotalDnds = sizeof (GSearchDndTable) / sizeof (GSearchDndTable[0]);





static GtkActionEntry GSearchUiEntries[] = {
  { "Open",          GTK_STOCK_OPEN,    N_("_Open"),               NULL, NULL, NULL },
  { "OpenFolder",    GTK_STOCK_OPEN,    N_("O_pen Folder"),        NULL, NULL, NULL },
  { "MoveToTrash",   GTK_STOCK_DELETE,  N_("Mo_ve to Trash"),      NULL, NULL, NULL },
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

//static gchar * find_command_default_name_argument;
//static gchar * locate_command_default_options;
pid_t locate_database_check_command_pid;



/*
static gboolean
has_additional_constraints (GSearchWindow * gsearch)
{
	GList * list;

	if (gsearch->available_options_selected_list != NULL) {

		for (list = gsearch->available_options_selected_list; list != NULL; list = g_list_next (list)) {

			GSearchConstraint * constraint = list->data;

			switch (GSearchOptionTemplates[constraint->constraint_id].type) {
			case SEARCH_CONSTRAINT_TYPE_BOOLEAN:
			case SEARCH_CONSTRAINT_TYPE_NUMERIC:
			case SEARCH_CONSTRAINT_TYPE_DATE_BEFORE:
			case SEARCH_CONSTRAINT_TYPE_DATE_AFTER:
				return TRUE;
			case SEARCH_CONSTRAINT_TYPE_TEXT:
				if (strlen (constraint->data.text) > 0) {
					return TRUE;
				}
			default:
				break;
			}
		}
	}
	return FALSE;
}
*/

static void 
fill_services_combo_box (GSearchWindow * gsearch, GtkComboBox *combo)
{
	GtkListStore     *store;
  	GtkCellRenderer  *cell;
	GdkPixbuf        *pixbuf;
	GtkTreeIter       iter;
	GtkIconTheme     *theme;
	GError           *error = NULL;
   	service_info_t   *service;

	theme = gtk_icon_theme_get_default ();

   	store = gtk_list_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT);
	gsearch->combo_model = store; 
  	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));

  	cell = gtk_cell_renderer_pixbuf_new ();
  	gtk_cell_layout_pack_start ( GTK_CELL_LAYOUT (combo), cell, FALSE);
  	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), cell, "pixbuf", 0);

  	cell = gtk_cell_renderer_text_new ();
  	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
  	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), cell, "text", 1);
    	
   	for (service = services; service->service; ++service) {
  	   	pixbuf = gtk_icon_theme_load_icon (theme, service->icon_name,
                                         GTK_ICON_SIZE_MENU,
                                         GTK_ICON_LOOKUP_USE_BUILTIN,
                                         &error);
	   	gtk_list_store_append (store, &iter);
	   	gtk_list_store_set (store, &iter, 0, pixbuf, 1, _(service->service), 2, service->service_type, -1);
   	}

   	gtk_combo_box_set_active (combo, 0);
}


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
start_animation (GSearchWindow * gsearch, gboolean first_pass)
{
	if (first_pass == TRUE) {

		gchar *title = NULL;

//		title = g_strconcat (_("Searching..."), " - ", _("Search Tool"), NULL);
//		gtk_window_set_title (GTK_WINDOW (gsearch->window), title);

		gtk_label_set_text (GTK_LABEL (gsearch->files_found_label), "");
		
		g_free (title);

		gsearch->focus = gtk_window_get_focus (GTK_WINDOW (gsearch->window));

		gtk_widget_set_sensitive (gsearch->find_button, FALSE);
//		gtk_widget_set_sensitive (gsearch->forward_button, FALSE);
//		gtk_widget_set_sensitive (gsearch->back_button, FALSE);
		gtk_widget_set_sensitive (gsearch->search_results_save_results_as_item, FALSE);
		gtk_widget_set_sensitive (gsearch->search_results_vbox, TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (gsearch->search_results_tree_view), TRUE);
		//gtk_widget_set_sensitive (gsearch->available_options_vbox, FALSE);
		//gtk_widget_set_sensitive (gsearch->show_more_options_expander, FALSE);
		gtk_widget_set_sensitive (gsearch->name_and_folder_table, FALSE);
	}
}

static void
stop_animation (GSearchWindow * gsearch)
{
	gtk_window_set_default (GTK_WINDOW (gsearch->window), gsearch->find_button);
	//gtk_widget_set_sensitive (gsearch->available_options_vbox, TRUE);
	//gtk_widget_set_sensitive (gsearch->show_more_options_expander, TRUE);
	gtk_widget_set_sensitive (gsearch->name_and_folder_table, TRUE);
	gtk_widget_set_sensitive (gsearch->find_button, TRUE);
	gtk_widget_set_sensitive (gsearch->search_results_save_results_as_item, TRUE);
	gtk_widget_show (gsearch->find_button);
	gtk_widget_set_sensitive (gsearch->forward_button, TRUE);
	//gtk_widget_set_sensitive (gsearch->back_button, FALSE);


	if (gtk_window_get_focus (GTK_WINDOW (gsearch->window)) == NULL) {
		gtk_window_set_focus (GTK_WINDOW (gsearch->window), gsearch->focus);
	}
}

gchar *
build_search_command (GSearchWindow * gsearch,
                      gboolean first_pass)
{
	GString * command;
	GError * error = NULL;
	gchar * file_is_named_utf8;
	gchar * file_is_named_locale;

	start_animation (gsearch, first_pass);

	gsearch->show_thumbnails = TRUE;

	file_is_named_utf8 = g_strdup ((gchar *) gtk_entry_get_text (GTK_ENTRY (gsearch->search_entry)));

	if (!file_is_named_utf8 || !*file_is_named_utf8) {
		g_free (file_is_named_utf8);
		file_is_named_utf8 = g_strdup ("");
	}
	else {
		gchar * locale;

		locale = g_locale_from_utf8 (file_is_named_utf8, -1, NULL, NULL, &error);
		if (locale == NULL) {
			stop_animation (gsearch);
			display_dialog_character_set_conversion_error (gsearch->window, file_is_named_utf8, error);
			g_free (file_is_named_utf8);
			g_error_free (error);
			return NULL;
		}

		g_free (locale);
	}

	file_is_named_locale = g_locale_from_utf8 (file_is_named_utf8, -1, NULL, NULL, &error);
	if (file_is_named_locale == NULL) {
		stop_animation (gsearch);
		display_dialog_character_set_conversion_error (gsearch->window, file_is_named_utf8, error);
		g_free (file_is_named_utf8);
		g_error_free (error);
		return NULL;
	}

	/*look_in_folder_utf8 = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (gsearch->look_in_folder_button));

	if (look_in_folder_utf8 != NULL) {
		look_in_folder_locale = g_locale_from_utf8 (look_in_folder_utf8, -1, NULL, NULL, &error);
		if (look_in_folder_locale == NULL) {
			stop_animation (gsearch);
			display_dialog_character_set_conversion_error (gsearch->window, look_in_folder_utf8, error);
			g_free (look_in_folder_utf8);
			g_error_free (error);
			return NULL;
		}
		g_free (look_in_folder_utf8);
	}
	else {
		
		look_in_folder_locale = g_strdup (g_get_home_dir ());
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (gsearch->look_in_folder_button), look_in_folder_locale);
	}

	if (!g_str_has_suffix (look_in_folder_locale, G_DIR_SEPARATOR_S)) {
		gchar *tmp;

		tmp = look_in_folder_locale;
		look_in_folder_locale = g_strconcat (look_in_folder_locale, G_DIR_SEPARATOR_S, NULL);
		g_free (tmp);
	}
	g_free (gsearch->command_details->look_in_folder_string);

	gsearch->command_details->look_in_folder_string = g_strdup (look_in_folder_locale);
*/
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

/*

	if ((GTK_WIDGET_VISIBLE (gsearch->available_options_vbox) == FALSE) ||
	    (has_additional_constraints (gsearch) == FALSE)) {

		file_is_named_backslashed = backslash_special_characters (file_is_named_locale);
		file_is_named_escaped = escape_single_quotes (file_is_named_backslashed);
		gsearch->command_details->name_contains_pattern_string = g_strdup (file_is_named_utf8);

		if (gsearch->command_details->is_command_first_pass == TRUE) {

			gchar * locate;
			gchar * show_thumbnails_string;
			gboolean disable_quick_search;

			locate = g_find_program_in_path ("locate");
			disable_quick_search = tracker_search_gconf_get_boolean ("/apps/tracker-search-tool/disable_quick_search");
			gsearch->command_details->is_command_second_pass_enabled = !tracker_search_gconf_get_boolean ("/apps/tracker-search-tool/disable_quick_search_second_scan");

			show_thumbnails_string = tracker_search_gconf_get_string ("/apps/nautilus/preferences/show_image_thumbnails");
			if ((show_thumbnails_string != NULL) &&
			    ((strcmp (show_thumbnails_string, "always") == 0) ||
			     (strcmp (show_thumbnails_string, "local_only") == 0))) {
			    	gsearch->show_thumbnails = TRUE;
				gsearch->show_thumbnails_file_size_limit = tracker_search_gconf_get_int ("/apps/nautilus/preferences/thumbnail_limit");
			}
			else {
				gsearch->show_thumbnails = FALSE;
				gsearch->show_thumbnails_file_size_limit = 0;
			}

			if ((disable_quick_search == FALSE)
			    && (gsearch->is_locate_database_available == TRUE)
			    && (locate != NULL)
			    && (is_quick_search_excluded_path (look_in_folder_locale) == FALSE)) {

					g_string_append_printf (command, "%s %s '%s*%s'",
							locate,
							locate_command_default_options,
							look_in_folder_locale,
							file_is_named_escaped);
					gsearch->command_details->is_command_using_quick_mode = TRUE;
			}
			else {
				g_string_append_printf (command, "find \"%s\" %s '%s' -xdev -print",
							look_in_folder_locale,
							find_command_default_name_argument,
							file_is_named_escaped);
			}
			g_free (locate);
			g_free (show_thumbnails_string);
		}
		else {
			g_string_append_printf (command, "find \"%s\" %s '%s' -xdev -print",
						look_in_folder_locale,
						find_command_default_name_argument,
						file_is_named_escaped);
		}
	}
	else {
		GList * list;
		gboolean disable_mount_argument = FALSE;

		gsearch->command_details->is_command_regex_matching_enabled = FALSE;
		file_is_named_backslashed = backslash_special_characters (file_is_named_locale);
		file_is_named_escaped = escape_single_quotes (file_is_named_backslashed);


		for (list = gsearch->available_options_selected_list; list != NULL; list = g_list_next (list)) {

			GSearchConstraint * constraint = list->data;

			switch (GSearchOptionTemplates[constraint->constraint_id].type) {
			case SEARCH_CONSTRAINT_TYPE_BOOLEAN:
				if (strcmp (GSearchOptionTemplates[constraint->constraint_id].option, "INCLUDE_OTHER_FILESYSTEMS") == 0) {
					disable_mount_argument = TRUE;
				}
				else if (strcmp (GSearchOptionTemplates[constraint->constraint_id].option, "SHOW_HIDDEN_FILES") == 0) {
					gsearch->command_details->is_command_show_hidden_files_enabled = TRUE;
				}
				else {
					g_string_append_printf (command, "%s ",
						GSearchOptionTemplates[constraint->constraint_id].option);
				}
				break;
			case SEARCH_CONSTRAINT_TYPE_TEXT:
				if (strcmp (GSearchOptionTemplates[constraint->constraint_id].option, "-regex '%s'") == 0) {

					gchar * escaped;
					gchar * regex;

					escaped = backslash_special_characters (constraint->data.text);
					regex = escape_single_quotes (escaped);

					if (regex != NULL) {
						gsearch->command_details->is_command_regex_matching_enabled = TRUE;
						gsearch->command_details->name_contains_regex_string = g_locale_from_utf8 (regex, -1, NULL, NULL, NULL);
					}

					g_free (escaped);
					g_free (regex);
				}
				else {
					gchar * escaped;
					gchar * backslashed;
					gchar * locale;

					backslashed = backslash_special_characters (constraint->data.text);
					escaped = escape_single_quotes (backslashed);

					locale = g_locale_from_utf8 (escaped, -1, NULL, NULL, NULL);

					if (strlen (locale) != 0) {
						g_string_append_printf (command,
							  	        GSearchOptionTemplates[constraint->constraint_id].option,
						  		        locale);

						g_string_append_c (command, ' ');
					}

					g_free (escaped);
					g_free (backslashed);
					g_free (locale);
				}
				break;
			case SEARCH_CONSTRAINT_TYPE_NUMERIC:
				g_string_append_printf (command,
					  		GSearchOptionTemplates[constraint->constraint_id].option,
							(constraint->data.number * 1024),
					  		(constraint->data.number * 1024));
				g_string_append_c (command, ' ');
				break;
			case SEARCH_CONSTRAINT_TYPE_DATE_BEFORE:
				g_string_append_printf (command,
					 		GSearchOptionTemplates[constraint->constraint_id].option,
					  		constraint->data.time);
				g_string_append_c (command, ' ');
				break;
			case SEARCH_CONSTRAINT_TYPE_DATE_AFTER:
				g_string_append_printf (command,
					 		GSearchOptionTemplates[constraint->constraint_id].option,
					  		constraint->data.time,
					  		constraint->data.time);
				g_string_append_c (command, ' ');
				break;
			default:
		        	break;
			}
		}
		gsearch->command_details->name_contains_pattern_string = g_strdup ("*");

		if (disable_mount_argument != TRUE) {
			g_string_append (command, "-xdev ");
		}

		g_string_append (command, "-print ");
	}
	g_free (file_is_named_locale);
	g_free (file_is_named_utf8);
	g_free (file_is_named_backslashed);
	g_free (file_is_named_escaped);
	g_free (look_in_folder_locale);
*/
	return g_string_free (command, FALSE);
}

static void
set_snippet (char * snippet,  GError *error, gpointer user_data)
{
	char *snippet_markup;
	GtkTreeIter iter;
	SnippetRow *snippet_row = user_data;

	snippet_markup = g_strdup_printf ("<span foreground='DimGrey' size='small'>%s</span>", snippet);

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (snippet_row->gsearch->search_results_list_store), &iter)) {

		while (TRUE) {

			gchar * utf8_base_name;
			gchar * utf8_dir_name;

			gtk_tree_model_get (GTK_TREE_MODEL (snippet_row->gsearch->search_results_list_store), &iter,
				   	    COLUMN_NAME, &utf8_base_name,
			    		    COLUMN_PATH, &utf8_dir_name,
					    -1);

			if ( (strcmp (snippet_row->name, utf8_base_name) == 0) && (strcmp (snippet_row->path, utf8_dir_name) == 0)) {
				gtk_list_store_set (GTK_LIST_STORE (snippet_row->gsearch->search_results_list_store), &iter, COLUMN_SNIPPET, snippet_markup, -1);
				g_free (utf8_base_name);
				g_free (utf8_dir_name);
				break;
			} else {
				g_free (utf8_base_name);
				g_free (utf8_dir_name);

				if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (snippet_row->gsearch->search_results_list_store), &iter)) {
					break;
				}
			}
		}			

	}
 
	

	g_free (snippet_row->path);
	g_free (snippet_row->name);
	g_free (snippet_row);
	g_free (snippet_markup);
}

static void
add_file_to_search_results (const gchar * file,
			    const char * mime,
			    GtkListStore * store,
			    GtkTreeIter * iter,
			    GSearchWindow * gsearch)
{
	GdkPixbuf * pixbuf;
	GSearchMonitor * monitor;
	GnomeVFSFileInfo * vfs_file_info;
	GnomeVFSMonitorHandle * handle;
	GnomeVFSResult result;
	GtkTreePath *path;
	GtkTreeRowReference *reference;
	gchar * description;
	gchar * utf8_base_name;
	gchar * utf8_dir_name;
	gchar * base_name;
	gchar * dir_name;
	gchar * escape_path_string;


	if (g_hash_table_lookup_extended (gsearch->search_results_filename_hash_table, file, NULL, NULL) == TRUE) {
		return;
	}

	if ((g_file_test (file, G_FILE_TEST_EXISTS) != TRUE) &&
	    (g_file_test (file, G_FILE_TEST_IS_SYMLINK) != TRUE)) {
		return;
	}

	g_hash_table_insert (gsearch->search_results_filename_hash_table, g_strdup (file), NULL);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (gsearch->search_results_tree_view), FALSE);

	vfs_file_info = gnome_vfs_file_info_new ();
	escape_path_string = gnome_vfs_escape_path_string (file);

	gnome_vfs_get_file_info (escape_path_string, vfs_file_info,
	                         GNOME_VFS_FILE_INFO_DEFAULT |
	                         GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

	pixbuf = get_file_pixbuf (gsearch, file, mime, vfs_file_info);
	description = get_file_type_description (file, mime, vfs_file_info);

	if (!description) description = g_strdup (mime); 
	
	base_name = g_path_get_basename (file);
	dir_name = g_path_get_dirname (file);

	utf8_base_name = g_locale_to_utf8 (base_name, -1, NULL, NULL, NULL);
	utf8_dir_name = g_locale_to_utf8 (dir_name, -1, NULL, NULL, NULL);

	//char *snippet;
	//char *snippet_markup;
	char *search_term;

	if (gsearch->search_term) {
		search_term = gsearch->search_term;
	} else {
		search_term = "";
	}


	//snippet = tracker_search_get_snippet (tracker_client, SERVICE_FILES, file, search_term, NULL);
	//snippet_markup = g_strdup_printf ("<span foreground='DimGrey' size='small'>%s</span>", snippet);
	//g_free (snippet);

	gtk_list_store_append (GTK_LIST_STORE (store), iter);
	gtk_list_store_set (GTK_LIST_STORE (store), iter,
			    COLUMN_ICON, pixbuf,
			    COLUMN_NAME, utf8_base_name,
			    COLUMN_PATH, utf8_dir_name,
			    COLUMN_SERVICE, 0,
//			    COLUMN_SNIPPET, snippet_markup,
			    COLUMN_TYPE, (description != NULL) ? description : mime,
			    COLUMN_NO_FILES_FOUND, FALSE,
			    -1);

	SnippetRow *snippet_row;

	snippet_row = g_new (SnippetRow, 1);
	snippet_row->gsearch = gsearch;
	snippet_row->path = g_strdup (utf8_dir_name);
	snippet_row->name = g_strdup (utf8_base_name);
	tracker_search_get_snippet_async (tracker_client, SERVICE_FILES, file, search_term, set_snippet, snippet_row);
//	g_free (snippet_markup);
	
	monitor = g_slice_new0 (GSearchMonitor);
	if (monitor) {
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter);
		reference = gtk_tree_row_reference_new (GTK_TREE_MODEL (store), path);
		gtk_tree_path_free (path);

		result = gnome_vfs_monitor_add (&handle, file, GNOME_VFS_MONITOR_FILE,
						(GnomeVFSMonitorCallback) file_changed_cb, monitor);
		if (result == GNOME_VFS_OK) {
			monitor->gsearch = gsearch;
			monitor->reference = reference;
			monitor->handle = handle;
			gtk_list_store_set (GTK_LIST_STORE (store), iter,
			                    COLUMN_MONITOR, monitor, -1);
		}
		else {
			gtk_tree_row_reference_free (reference);
			g_slice_free (GSearchMonitor, monitor);
		}
	}

	gnome_vfs_file_info_unref (vfs_file_info);
	g_free (base_name);
	g_free (dir_name);
	g_free (utf8_base_name);
	g_free (utf8_dir_name);
	g_free (escape_path_string);
	g_free (description);
}

static void
add_no_files_found_message (GSearchWindow * gsearch)
{
	/* When the list is empty append a 'No Files Found.' message. */
	gtk_widget_set_sensitive (GTK_WIDGET (gsearch->search_results_tree_view), FALSE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (gsearch->search_results_tree_view), FALSE);
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (gsearch->search_results_tree_view));
	g_object_set (gsearch->search_results_name_cell_renderer,
	              "underline", PANGO_UNDERLINE_NONE,
	              "underline-set", FALSE,
	              NULL);
	gtk_list_store_append (GTK_LIST_STORE (gsearch->search_results_list_store), &gsearch->search_results_iter);
	gtk_list_store_set (GTK_LIST_STORE (gsearch->search_results_list_store), &gsearch->search_results_iter,
		    	    COLUMN_ICON, NULL,
			    COLUMN_NAME, _("No files found"),
			    COLUMN_PATH, "",
			    COLUMN_SERVICE, 0,
			    COLUMN_SNIPPET, "",
			    COLUMN_TYPE, "",
			    COLUMN_NO_FILES_FOUND, TRUE,
		    	    -1);
}

void
update_search_counts (GSearchWindow * gsearch)
{
	gchar * title_bar_string = NULL;
	gchar * message_string = NULL;
	gchar * stopped_string = NULL;
	gchar * tmp;
	gint total_files;

	if (gsearch->command_details->command_status == ABORTED) {
		stopped_string = g_strdup (_("(stopped)"));
	}

	total_files = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (gsearch->search_results_list_store), NULL);

	if (total_files == 0) {
		title_bar_string = g_strdup (_("No Files Found"));
		message_string = g_strdup (_("No files found"));
		add_no_files_found_message (gsearch);
	}
	else {
		title_bar_string = g_strdup_printf (ngettext ("%d File Found",
					                      "%d Files Found",
					                      total_files),
						    total_files);
		message_string = g_strdup_printf (ngettext ("%d file found",
					                    "%d files found",
					                    total_files),
						  total_files);
	}

	if (stopped_string != NULL) {
		tmp = message_string;
		message_string = g_strconcat (message_string, " ", stopped_string, NULL);
		g_free (tmp);

		tmp = title_bar_string;
		title_bar_string = g_strconcat (title_bar_string, " ", stopped_string, NULL);
		g_free (tmp);
	}

	tmp = title_bar_string;
	title_bar_string = g_strconcat (title_bar_string, " - ", _("Search for Files"), NULL);
	gtk_window_set_title (GTK_WINDOW (gsearch->window), title_bar_string);
	g_free (tmp);

	gtk_label_set_text (GTK_LABEL (gsearch->files_found_label), message_string);

	g_free (title_bar_string);
	g_free (message_string);
	g_free (stopped_string);
}

/*
static void
intermediate_file_count_update (GSearchWindow * gsearch)
{
	gchar * string;
	gint count;

	count = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (gsearch->search_results_list_store), NULL);

	if (count > 0) {

		string = g_strdup_printf (ngettext ("%d file found",
		                                    "%d files found",
		                                    count),
		                          count);

		gtk_label_set_text (GTK_LABEL (gsearch->files_found_label), string);
		g_free (string);
	}
}
*/

gboolean
tree_model_iter_free_monitor (GtkTreeModel * model,
                              GtkTreePath * path,
                              GtkTreeIter * iter,
                              gpointer data)
{
	GSearchMonitor * monitor;

	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);

	gtk_tree_model_get (model, iter, COLUMN_MONITOR, &monitor, -1);
	if (monitor) {
		gnome_vfs_monitor_cancel (monitor->handle);
		gtk_tree_row_reference_free (monitor->reference);
		g_slice_free (GSearchMonitor, monitor);
	}
	return FALSE;
}

/*
static GtkTreeModel *
gsearch_create_list_of_templates (void)
{
	GtkListStore * store;
	GtkTreeIter iter;
	gint index;

	store = gtk_list_store_new (1, G_TYPE_STRING);

	for (index = 0; GSearchOptionTemplates[index].type != SEARCH_CONSTRAINT_TYPE_NONE; index++) {

		if (GSearchOptionTemplates[index].type == SEARCH_CONSTRAINT_TYPE_SEPARATOR) {
		        gtk_list_store_append (store, &iter);
		        gtk_list_store_set (store, &iter, 0, "separator", -1);
		}
		else {
			gchar * text = remove_mnemonic_character (_(GSearchOptionTemplates[index].desc));
			gtk_list_store_append (store, &iter);
		        gtk_list_store_set (store, &iter, 0, text, -1);
			g_free (text);
		}
	}
	return GTK_TREE_MODEL (store);
}

static void
set_constraint_info_defaults (GSearchConstraint * opt)
{
	switch (GSearchOptionTemplates[opt->constraint_id].type) {
	case SEARCH_CONSTRAINT_TYPE_BOOLEAN:
		break;
	case SEARCH_CONSTRAINT_TYPE_TEXT:
		opt->data.text = "";
		break;
	case SEARCH_CONSTRAINT_TYPE_NUMERIC:
		opt->data.number = 0;
		break;
	case SEARCH_CONSTRAINT_TYPE_DATE_BEFORE:
	case SEARCH_CONSTRAINT_TYPE_DATE_AFTER:
		opt->data.time = 0;
		break;
	default:
	        break;
	}
}
*/

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
		g_warning (_("Entry changed called for a non entry option!"));
		break;
	}
}

/*
void
set_constraint_selected_state (GSearchWindow * gsearch,
                               gint constraint_id,
			       gboolean state)
{
	gint index;

	GSearchOptionTemplates[constraint_id].is_selected = state;

	for (index = 0; GSearchOptionTemplates[index].type != SEARCH_CONSTRAINT_TYPE_NONE; index++) {
		if (GSearchOptionTemplates[index].is_selected == FALSE) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (gsearch->available_options_combo_box), index);
			gtk_widget_set_sensitive (gsearch->available_options_add_button, TRUE);
			gtk_widget_set_sensitive (gsearch->available_options_combo_box, TRUE);
			gtk_widget_set_sensitive (gsearch->available_options_label, TRUE);
			return;
		}
	}
	gtk_widget_set_sensitive (gsearch->available_options_add_button, FALSE);
	gtk_widget_set_sensitive (gsearch->available_options_combo_box, FALSE);
	gtk_widget_set_sensitive (gsearch->available_options_label, FALSE);
}
*/

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
 *              Gtk widget.
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
 *              objects.
 */
/*
static void
add_atk_relation (GtkWidget * obj1,
		  GtkWidget * obj2,
		  AtkRelationType rel_type)
{
	AtkObject * atk_obj1, * atk_obj2;
	AtkRelationSet * relation_set;
	AtkRelation * relation;

	g_assert (GTK_IS_WIDGET (obj1));
	g_assert (GTK_IS_WIDGET (obj2));

	atk_obj1 = gtk_widget_get_accessible (obj1);

	atk_obj2 = gtk_widget_get_accessible (obj2);

	relation_set = atk_object_ref_relation_set (atk_obj1);
	relation = atk_relation_new (&atk_obj2, 1, rel_type);
	atk_relation_set_add (relation_set, relation);
	g_object_unref (G_OBJECT (relation));

}
*/

gchar *
get_desktop_item_name (GSearchWindow * gsearch)
{

	GString * gs;
	gchar * file_is_named_utf8;
	gchar * file_is_named_locale;
	//GList * list;

	gs = g_string_new ("");
	g_string_append (gs, _("Search for Files"));
	g_string_append (gs, " (");

	file_is_named_utf8 = (gchar *) gtk_entry_get_text (GTK_ENTRY (gsearch->search_entry));
	file_is_named_locale = g_locale_from_utf8 (file_is_named_utf8 != NULL ? file_is_named_utf8 : "" ,
	                                           -1, NULL, NULL, NULL);
	g_string_append_printf (gs, "named=%s", file_is_named_locale);
	g_free (file_is_named_locale);

	/*look_in_folder_utf8 = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (gsearch->look_in_folder_button));
	look_in_folder_locale = g_locale_from_utf8 (look_in_folder_utf8 != NULL ? look_in_folder_utf8 : "",
	                                            -1, NULL, NULL, NULL);
	g_string_append_printf (gs, "&path=%s", look_in_folder_locale);
	g_free (look_in_folder_locale);
	g_free (look_in_folder_utf8);
	

	if (GTK_WIDGET_VISIBLE (gsearch->available_options_vbox)) {
		for (list = gsearch->available_options_selected_list; list != NULL; list = g_list_next (list)) {
			GSearchConstraint * constraint = list->data;
			gchar * locale = NULL;

			switch (constraint->constraint_id) {
			case SEARCH_CONSTRAINT_CONTAINS_THE_TEXT:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				g_string_append_printf (gs, "&contains=%s", locale);
				break;
			case SEARCH_CONSTRAINT_DATE_MODIFIED_BEFORE:
				g_string_append_printf (gs, "&mtimeless=%d", constraint->data.time);
				break;
			case SEARCH_CONSTRAINT_DATE_MODIFIED_AFTER:
				g_string_append_printf (gs, "&mtimemore=%d", constraint->data.time);
				break;
			case SEARCH_CONSTRAINT_SIZE_IS_MORE_THAN:
				g_string_append_printf (gs, "&sizemore=%u", constraint->data.number);
				break;
			case SEARCH_CONSTRAINT_SIZE_IS_LESS_THAN:
				g_string_append_printf (gs, "&sizeless=%u", constraint->data.number);
				break;
			case SEARCH_CONSTRAINT_FILE_IS_EMPTY:
				g_string_append (gs, "&empty");
				break;
			case SEARCH_CONSTRAINT_OWNED_BY_USER:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				g_string_append_printf (gs, "&user=%s", locale);
				break;
			case SEARCH_CONSTRAINT_OWNED_BY_GROUP:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				g_string_append_printf (gs, "&group=%s", locale);
				break;
			case SEARCH_CONSTRAINT_OWNER_IS_UNRECOGNIZED:
				g_string_append (gs, "&nouser");
				break;
			case SEARCH_CONSTRAINT_FILE_IS_NOT_NAMED:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				g_string_append_printf (gs, "&notnamed=%s", locale);
				break;
			case SEARCH_CONSTRAINT_FILE_MATCHES_REGULAR_EXPRESSION:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				g_string_append_printf (gs, "&regex=%s", locale);
				break;
			case SEARCH_CONSTRAINT_SHOW_HIDDEN_FILES_AND_FOLDERS:
				g_string_append (gs, "&hidden");
				break;
			case SEARCH_CONSTRAINT_FOLLOW_SYMBOLIC_LINKS:
				g_string_append (gs, "&follow");
				break;
			case SEARCH_CONSTRAINT_SEARCH_OTHER_FILESYSTEMS:
				g_string_append (gs, "&allmounts");
				break;
			default:
				break;
			}
		g_free (locale);
		}
	}*/
	g_string_append_c (gs, ')');
	return g_string_free (gs, FALSE);
}



/*
static GtkWidget *
create_constraint_box (GSearchWindow * gsearch,
                       GSearchConstraint * opt,
                       gchar * value)
{
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * entry;
	GtkWidget * entry_hbox;
	GtkWidget * button;

	hbox = gtk_hbox_new (FALSE, 12);

	switch (GSearchOptionTemplates[opt->constraint_id].type) {
	case SEARCH_CONSTRAINT_TYPE_BOOLEAN:
		{
			gchar * text = remove_mnemonic_character (GSearchOptionTemplates[opt->constraint_id].desc);
			gchar * desc = g_strconcat (LEFT_LABEL_SPACING, _(text), ".", NULL);
			label = gtk_label_new (desc);
			gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
			g_free (desc);
			g_free (text);
		}
		break;
	case SEARCH_CONSTRAINT_TYPE_TEXT:
	case SEARCH_CONSTRAINT_TYPE_NUMERIC:
	case SEARCH_CONSTRAINT_TYPE_DATE_BEFORE:
	case SEARCH_CONSTRAINT_TYPE_DATE_AFTER:
		{
			gchar * desc = g_strconcat (LEFT_LABEL_SPACING, _(GSearchOptionTemplates[opt->constraint_id].desc), ":", NULL);
			label = gtk_label_new_with_mnemonic (desc);
			g_free (desc);
		}


		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

		if (GSearchOptionTemplates[opt->constraint_id].type == SEARCH_CONSTRAINT_TYPE_TEXT) {
			entry = gtk_entry_new ();
			if (value != NULL) {
				gtk_entry_set_text (GTK_ENTRY (entry), value);
				opt->data.text = value;
			}
		}
		else {
			entry = gtk_spin_button_new_with_range (0, 999999999, 1);
			if (value != NULL) {
				gtk_spin_button_set_value (GTK_SPIN_BUTTON (entry), atoi (value));
				opt->data.time = atoi (value);
				opt->data.number = atoi (value);
			}
		}

		if (gsearch->is_window_accessible) {
			gchar * text = remove_mnemonic_character (GSearchOptionTemplates[opt->constraint_id].desc);
			gchar * name;
			gchar * desc;

			if (GSearchOptionTemplates[opt->constraint_id].units == NULL) {
				name = g_strdup (_(text));
				desc = g_strdup_printf (_("Enter a text value for the \"%s\" search option."), _(text));
			}
			else {
				name = g_strdup_printf (_("\"%s\" in %s"), _(text),
				                        _(GSearchOptionTemplates[opt->constraint_id].units));
				desc = g_strdup_printf (_("Enter a value in %s for the \"%s\" search option."),
				                        _(GSearchOptionTemplates[opt->constraint_id].units),
				                        _(text));
			}
			add_atk_namedesc (GTK_WIDGET (entry), name, desc);
			g_free (name);
			g_free (desc);
			g_free (text);
		}

		gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (entry));

		g_signal_connect (G_OBJECT (entry), "changed",
			 	  G_CALLBACK (constraint_update_info_cb), opt);

		g_signal_connect (G_OBJECT (entry), "activate",
				  G_CALLBACK (constraint_activate_cb),
				  (gpointer) gsearch);


		entry_hbox = gtk_hbox_new (FALSE, 6);
		gtk_box_pack_start (GTK_BOX (hbox), entry_hbox, TRUE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (entry_hbox), entry, TRUE, TRUE, 0);


		if (GSearchOptionTemplates[opt->constraint_id].units != NULL)
		{
			label = gtk_label_new_with_mnemonic (_(GSearchOptionTemplates[opt->constraint_id].units));
			gtk_box_pack_start (GTK_BOX (entry_hbox), label, FALSE, FALSE, 0);
		}

		break;
	default:

		label = gtk_label_new ("???");
		gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	        break;
	}

	button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_DEFAULT);

	{
		GList * list = NULL;

		list = g_list_append (list, (gpointer) gsearch);
		list = g_list_append (list, (gpointer) opt);

		g_signal_connect (G_OBJECT (button), "clicked",
		                  G_CALLBACK (remove_constraint_cb),
		                  (gpointer) list);

	}
	gtk_size_group_add_widget (gsearch->available_options_button_size_group, button);
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	if (gsearch->is_window_accessible) {
		gchar * text = remove_mnemonic_character (GSearchOptionTemplates[opt->constraint_id].desc);
		gchar * name = g_strdup_printf (_("Remove \"%s\""), _(text));
		gchar * desc = g_strdup_printf (_("Click to remove the \"%s\" search option."), _(text));
		add_atk_namedesc (GTK_WIDGET (button), name, desc);
		g_free (name);
		g_free (desc);
		g_free (text);
	}
	return hbox;

}

void
add_constraint (GSearchWindow * gsearch,
                gint constraint_id,
                gchar * value,
                gboolean show_constraint)
{
	GSearchConstraint * constraint = g_slice_new (GSearchConstraint);
	GtkWidget * widget;

	if (show_constraint) {
		if (GTK_WIDGET_VISIBLE (gsearch->available_options_vbox) == FALSE) {
			gtk_expander_set_expanded (GTK_EXPANDER (gsearch->show_more_options_expander), TRUE);
			gtk_widget_show (gsearch->available_options_vbox);
		}
	}

	gsearch->window_geometry.min_height += WINDOW_HEIGHT_STEP;

	if (GTK_WIDGET_VISIBLE (gsearch->available_options_vbox)) {
		gtk_window_set_geometry_hints (GTK_WINDOW (gsearch->window),
		                               GTK_WIDGET (gsearch->window),
		                               &gsearch->window_geometry,
		                               GDK_HINT_MIN_SIZE);
	}

	constraint->constraint_id = constraint_id;
	set_constraint_info_defaults (constraint);
	set_constraint_gconf_boolean (constraint->constraint_id, TRUE);

	widget = create_constraint_box (gsearch, constraint, value);
	gtk_box_pack_start (GTK_BOX (gsearch->available_options_vbox), widget, FALSE, FALSE, 0);
	gtk_widget_show_all (widget);

	gsearch->available_options_selected_list =
		g_list_append (gsearch->available_options_selected_list, constraint);

	set_constraint_selected_state (gsearch, constraint->constraint_id, TRUE);

}


static void
set_sensitive (GtkCellLayout * cell_layout,
               GtkCellRenderer * cell,
               GtkTreeModel * tree_model,
               GtkTreeIter * iter,
               gpointer data)
{
	GtkTreePath * path;
	gint index;

	path = gtk_tree_model_get_path (tree_model, iter);
	index = gtk_tree_path_get_indices (path)[0];
	gtk_tree_path_free (path);

	g_object_set (cell, "sensitive", !(GSearchOptionTemplates[index].is_selected), NULL);
}

static gboolean
is_separator (GtkTreeModel * model,
              GtkTreeIter * iter,
              gpointer data)
{
	GtkTreePath * path;
	gint index;

	path = gtk_tree_model_get_path (model, iter);
	index = gtk_tree_path_get_indices (path)[0];
	gtk_tree_path_free (path);

	return (GSearchOptionTemplates[index].type == SEARCH_CONSTRAINT_TYPE_SEPARATOR);
}

static void
create_additional_constraint_section (GSearchWindow * gsearch)
{
	GtkCellRenderer * renderer;
	GtkTreeModel * model;
	GtkWidget * hbox;
	gchar * desc;

	gsearch->available_options_vbox = gtk_vbox_new (FALSE, 6);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_end (GTK_BOX (gsearch->available_options_vbox), hbox, FALSE, FALSE, 0);

	desc = g_strconcat (LEFT_LABEL_SPACING, _("A_vailable options:"), NULL);
	gsearch->available_options_label = gtk_label_new_with_mnemonic (desc);
	g_free (desc);

	gtk_box_pack_start (GTK_BOX (hbox), gsearch->available_options_label, FALSE, FALSE, 0);

	model = gsearch_create_list_of_templates ();
	gsearch->available_options_combo_box = gtk_combo_box_new_with_model (model);
	g_object_unref (model);

	gtk_label_set_mnemonic_widget (GTK_LABEL (gsearch->available_options_label), GTK_WIDGET (gsearch->available_options_combo_box));
	gtk_combo_box_set_active (GTK_COMBO_BOX (gsearch->available_options_combo_box), 0);
	gtk_box_pack_start (GTK_BOX (hbox), gsearch->available_options_combo_box, TRUE, TRUE, 0);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (gsearch->available_options_combo_box),
	                            renderer,
	                            TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (gsearch->available_options_combo_box), renderer,
	                                "text", 0,
	                                NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (gsearch->available_options_combo_box),
	                                    renderer,
	                                    set_sensitive,
	                                    NULL, NULL);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (gsearch->available_options_combo_box),
	                                      is_separator, NULL, NULL);

	if (gsearch->is_window_accessible) {
		add_atk_namedesc (GTK_WIDGET (gsearch->available_options_combo_box), _("Available options"),
				  _("Select a search option from the drop-down list."));
	}

	gsearch->available_options_add_button = gtk_button_new_from_stock (GTK_STOCK_ADD);
	GTK_WIDGET_UNSET_FLAGS (gsearch->available_options_add_button, GTK_CAN_DEFAULT);
	gsearch->available_options_button_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
	gtk_size_group_add_widget (gsearch->available_options_button_size_group, gsearch->available_options_add_button);

	g_signal_connect (G_OBJECT (gsearch->available_options_add_button),"clicked",
			  G_CALLBACK (add_constraint_cb), (gpointer) gsearch);

	if (gsearch->is_window_accessible) {
		add_atk_namedesc (GTK_WIDGET (gsearch->available_options_add_button), _("Add search option"),
				  _("Click to add the selected available search option."));
	}

	gtk_box_pack_end (GTK_BOX (hbox), gsearch->available_options_add_button, FALSE, FALSE, 0);

}
*/

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
	char *markup, *fpath, *name, *type;


	gtk_tree_model_get (model, iter, COLUMN_NAME, &name, -1);
	gtk_tree_model_get (model, iter, COLUMN_PATH, &fpath, -1);
	gtk_tree_model_get (model, iter, COLUMN_TYPE, &type, -1);

	char *mark_name = g_markup_escape_text (name, -1);
	char *mark_dir =  g_markup_escape_text (fpath, -1);

	markup = g_strconcat ("<b>", mark_name, "</b>\n", "<span foreground='DimGrey' size='small'>", mark_dir,"</span>\n",
			      "<span foreground='DimGrey' size='small'>",type, "</span>", NULL);

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
	}
	else {
		
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
	char *snippet;
	int width;

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
	GtkWidget * window;
	GtkTreeViewColumn * column;
	GtkCellRenderer * renderer;

	vbox = gtk_vbox_new (FALSE, 6);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic (_("S_earch results:"));
	g_object_set (G_OBJECT (label), "xalign", 0.0, NULL);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

	gsearch->files_found_label = gtk_label_new (NULL);
	gtk_label_set_selectable (GTK_LABEL (gsearch->files_found_label), TRUE);
	g_object_set (G_OBJECT (gsearch->files_found_label), "xalign", 1.0, NULL);
	gtk_box_pack_start (GTK_BOX (hbox), gsearch->files_found_label, FALSE, FALSE, 0);

	window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (window), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (window), 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);

	gsearch->search_results_list_store = gtk_list_store_new (NUM_COLUMNS,
					      GDK_TYPE_PIXBUF,
					      G_TYPE_STRING,
					      G_TYPE_STRING,
					      G_TYPE_INT,
					      G_TYPE_STRING,
					      G_TYPE_STRING,
					      G_TYPE_POINTER,
					      G_TYPE_BOOLEAN);


	gsearch->search_results_tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (gsearch->search_results_list_store)));

	gtk_tree_view_set_headers_visible (gsearch->search_results_tree_view, FALSE);
	gtk_tree_view_set_search_equal_func (gsearch->search_results_tree_view,
	                                     gsearch_equal_func, NULL, NULL);
	gtk_tree_view_set_rules_hint (gsearch->search_results_tree_view, TRUE);
  	g_object_unref (G_OBJECT (gsearch->search_results_list_store));

	if (gsearch->is_window_accessible) {
		add_atk_namedesc (GTK_WIDGET (gsearch->search_results_tree_view), _("List View"), NULL);
	}

	gsearch->search_results_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gsearch->search_results_tree_view));

	gtk_tree_selection_set_mode (GTK_TREE_SELECTION (gsearch->search_results_selection),
				     GTK_SELECTION_BROWSE);

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

	gtk_tree_view_column_set_min_width (column, 200);
	gtk_tree_view_column_set_max_width (column, 400);

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
	gtk_tree_view_append_column (GTK_TREE_VIEW (gsearch->search_results_tree_view), column);
	
	tracker_search_set_columns_order (gsearch->search_results_tree_view);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
	                  "columns-changed",
	                  G_CALLBACK (columns_changed_cb),
	                  (gpointer) gsearch);
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
	//GList * list;
	gint  i = 0;

	argv = g_new0 (gchar*, SEARCH_CONSTRAINT_MAXIMUM_POSSIBLE);

	argv[i++] = (gchar *) client_data;

	file_is_named_utf8 = (gchar *) gtk_entry_get_text (GTK_ENTRY (gsearch->search_entry));
	file_is_named_locale = g_locale_from_utf8 (file_is_named_utf8 != NULL ? file_is_named_utf8 : "" ,
	                                           -1, NULL, NULL, NULL);
	if (escape_values)
		tmp = g_shell_quote (file_is_named_locale);
	else
		tmp = g_strdup (file_is_named_locale);
	argv[i++] = g_strdup_printf ("--named=%s", tmp);
	g_free (tmp);
	g_free (file_is_named_locale);

/*	if (GTK_WIDGET_VISIBLE (gsearch->available_options_vbox)) {
		for (list = gsearch->available_options_selected_list; list != NULL; list = g_list_next (list)) {
			GSearchConstraint * constraint = list->data;
			gchar * locale = NULL;

			switch (constraint->constraint_id) {
			case SEARCH_CONSTRAINT_CONTAINS_THE_TEXT:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				if (escape_values)
					tmp = g_shell_quote (locale);
				else
					tmp = g_strdup (locale);
				argv[i++] = g_strdup_printf ("--contains=%s", tmp);
				g_free (tmp);
				break;
			case SEARCH_CONSTRAINT_DATE_MODIFIED_BEFORE:
				argv[i++] = g_strdup_printf ("--mtimeless=%d", constraint->data.time);
				break;
			case SEARCH_CONSTRAINT_DATE_MODIFIED_AFTER:
				argv[i++] = g_strdup_printf ("--mtimemore=%d", constraint->data.time);
				break;
			case SEARCH_CONSTRAINT_SIZE_IS_MORE_THAN:
				argv[i++] = g_strdup_printf ("--sizemore=%u", constraint->data.number);
				break;
			case SEARCH_CONSTRAINT_SIZE_IS_LESS_THAN:
				argv[i++] = g_strdup_printf ("--sizeless=%u", constraint->data.number);
				break;
			case SEARCH_CONSTRAINT_FILE_IS_EMPTY:
				argv[i++] = g_strdup ("--empty");
				break;
			case SEARCH_CONSTRAINT_OWNED_BY_USER:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				if (escape_values)
					tmp = g_shell_quote (locale);
				else
					tmp = g_strdup (locale);
				argv[i++] = g_strdup_printf ("--user=%s", tmp);
				g_free (tmp);
				break;
			case SEARCH_CONSTRAINT_OWNED_BY_GROUP:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				if (escape_values)
					tmp = g_shell_quote (locale);
				else
					tmp = g_strdup (locale);
				argv[i++] = g_strdup_printf ("--group=%s", tmp);
				g_free (tmp);
				break;
			case SEARCH_CONSTRAINT_OWNER_IS_UNRECOGNIZED:
				argv[i++] = g_strdup ("--nouser");
				break;
			case SEARCH_CONSTRAINT_FILE_IS_NOT_NAMED:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				if (escape_values)
					tmp = g_shell_quote (locale);
				else
					tmp = g_strdup (locale);
				argv[i++] = g_strdup_printf ("--notnamed=%s", tmp);
				g_free (tmp);
				break;
			case SEARCH_CONSTRAINT_FILE_MATCHES_REGULAR_EXPRESSION:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				if (escape_values)
					tmp = g_shell_quote (locale);
				else
					tmp = g_strdup (locale);
				argv[i++] = g_strdup_printf ("--regex=%s", tmp);
				g_free (tmp);
				break;
			case SEARCH_CONSTRAINT_SHOW_HIDDEN_FILES_AND_FOLDERS:
				argv[i++] = g_strdup ("--hidden");
				break;
			case SEARCH_CONSTRAINT_FOLLOW_SYMBOLIC_LINKS:
				argv[i++] = g_strdup ("--follow");
				break;
			case SEARCH_CONSTRAINT_SEARCH_OTHER_FILESYSTEMS:
				argv[i++] = g_strdup ("--allmounts");
				break;
			default:
				break;
			}
			g_free (locale);
		}
	}*/
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
get_meta_table_data (gpointer value, gpointer data)
		    
{
	char **meta;
	GSearchWindow * gsearch = data;

	meta = (char **)value;

	if (meta[0] && meta[1] && meta[2]) {
		add_file_to_search_results (meta[0], meta[2], gsearch->search_results_list_store, &gsearch->search_results_iter, gsearch);
		gsearch->hit_count++;
	}

}

void
click_find_cb (GtkWidget * widget,
               gpointer data)
{
	GSearchWindow * gsearch = data;
	gchar * command;
	GPtrArray *out_array = NULL;
	GtkTreeIter iter;
	int type;

	gsearch->hit_count = 0;

	if (widget == gsearch->forward_button) {
		gsearch->offset += MAX_SEARCH_RESULTS;
	} else if (widget == gsearch->back_button) {
		gsearch->offset -= MAX_SEARCH_RESULTS;
	} else {
		gsearch->offset = 0;
	}

	if (gsearch->offset < 0) {
		gsearch->offset = 0;
	}


	command = build_search_command (gsearch, TRUE);
	if (command != NULL) {
		gsearch->search_term = g_strdup (command);

		if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (gsearch->combo), &iter)) {
			gtk_tree_model_get (GTK_TREE_MODEL (gsearch->combo_model), &iter, 2, &type, -1);
		} else {
			type = SERVICE_FILES;
		}
		
   		
		out_array = tracker_search_text_detailed (tracker_client, -1, type, command, gsearch->offset, MAX_SEARCH_RESULTS, NULL);
		gsearch->is_locate_database_check_finished = TRUE;
		stop_animation (gsearch);
		g_free (command);


		if (out_array) {
			gsearch->command_details->command_status = RUNNING;
			gsearch->search_results_pixbuf_hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
			gsearch->search_results_filename_hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

			gtk_tree_view_scroll_to_point (GTK_TREE_VIEW (gsearch->search_results_tree_view), 0, 0);
			gtk_tree_model_foreach (GTK_TREE_MODEL (gsearch->search_results_list_store),
					(GtkTreeModelForeachFunc) tree_model_iter_free_monitor, gsearch);
			gtk_list_store_clear (GTK_LIST_STORE (gsearch->search_results_list_store));

			g_ptr_array_foreach (out_array, (GFunc)get_meta_table_data, gsearch);
			g_ptr_array_free (out_array, TRUE);
		


			GtkTreeModel *model = gtk_tree_view_get_model (gsearch->search_results_tree_view);

			GtkTreeIter iter;
			if (gtk_tree_model_get_iter_first (model, &iter)) {
				gtk_tree_selection_select_iter (gsearch->search_results_selection, &iter);
			}
	
		} else {
			gtk_widget_set_sensitive (gsearch->forward_button, FALSE);
		}


	}

	if (gsearch->hit_count < MAX_SEARCH_RESULTS) {
		gtk_widget_set_sensitive (gsearch->forward_button, FALSE);
	}

	if (gsearch->offset > 0) {
		gtk_widget_set_sensitive (gsearch->back_button, TRUE);
	} else {
		gtk_widget_set_sensitive (gsearch->back_button, FALSE);
	}
	
}


static GtkWidget *
gsearch_app_create (GSearchWindow * gsearch)
{
//	GtkTargetEntry drag_types[] = {{ "text/uri-list", 0, 0 }};
//	gchar * locale_string;
//	gchar * utf8_string;
	GtkWidget * hbox;
	GtkWidget * vbox;
	GtkWidget * entry;
	GtkWidget * label;
	GtkWidget * image;
//	GtkWidget * button;
	GtkWidget * container;
	GtkWidget * main_container;
//	GdkPixbuf * pixbuf;

	gsearch->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gsearch->is_window_maximized = tracker_search_gconf_get_boolean ("/apps/tracker-search-tool/default_window_maximized");
	g_signal_connect (G_OBJECT (gsearch->window), "size-allocate",
			  G_CALLBACK (gsearch_window_size_allocate),
			  gsearch);
	gsearch->command_details = g_slice_new0 (GSearchCommandDetails);
	gsearch->window_geometry.min_height = -1;
	gsearch->window_geometry.min_width  = -1;

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
	
	container = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (main_container), container);
	gtk_container_set_border_width (GTK_CONTAINER (container), 12);


	GtkWidget *widget = gtk_statusbar_new ();   
	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (main_container), widget, FALSE, FALSE, 0);


	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 0);

	gsearch->name_and_folder_table = gtk_table_new (1, 4, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (gsearch->name_and_folder_table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (gsearch->name_and_folder_table), 12);
	gtk_container_add (GTK_CONTAINER (hbox), gsearch->name_and_folder_table);

	label = gtk_label_new_with_mnemonic (_("_Search:"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (label), "xalign", 0.0, NULL);

	gtk_table_attach (GTK_TABLE (gsearch->name_and_folder_table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 1);

	gsearch->search_entry = sexy_icon_entry_new ();
	sexy_icon_entry_add_clear_button (SEXY_ICON_ENTRY (gsearch->search_entry));
	gtk_table_attach (GTK_TABLE (gsearch->name_and_folder_table), gsearch->search_entry, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
	entry =  (gsearch->search_entry);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);

	gsearch->combo = gtk_combo_box_new ();
	fill_services_combo_box (gsearch, GTK_COMBO_BOX (gsearch->combo));
	g_signal_connect (G_OBJECT (gsearch->combo), "changed",
	                  G_CALLBACK (click_find_cb), (gpointer) gsearch);
	gtk_table_attach (GTK_TABLE (gsearch->name_and_folder_table), gsearch->combo, 2, 3, 0, 1, GTK_FILL, 0, 0, 0);
	

	hbox = gtk_hbutton_box_new ();
	gtk_table_attach (GTK_TABLE (gsearch->name_and_folder_table), hbox, 3, 4, 0, 1, GTK_FILL, 0, 0, 0);

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

	gsearch->show_more_options_expander = gtk_expander_new_with_mnemonic (_("Select more _options"));
	//gtk_box_pack_start (GTK_BOX (container), gsearch->show_more_options_expander, FALSE, FALSE, 0);
	//g_signal_connect (G_OBJECT (gsearch->show_more_options_expander), "notify::expanded",
	//		  G_CALLBACK (click_expander_cb), (gpointer) gsearch);

	//create_additional_constraint_section (gsearch);
	//gtk_box_pack_start (GTK_BOX (container), GTK_WIDGET (gsearch->available_options_vbox), FALSE, FALSE, 0);

	//if (gsearch->is_window_accessible) {
	//	add_atk_namedesc (GTK_WIDGET (gsearch->show_more_options_expander), _("Select more options"), _("Click to expand or collapse the list of available options."));
	//	add_atk_relation (GTK_WIDGET (gsearch->available_options_vbox), GTK_WIDGET (gsearch->show_more_options_expander), ATK_RELATION_CONTROLLED_BY);
	//	add_atk_relation (GTK_WIDGET (gsearch->show_more_options_expander), GTK_WIDGET (gsearch->available_options_vbox), ATK_RELATION_CONTROLLER_FOR);
	//}

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (container), vbox, TRUE, TRUE, 0);

	gsearch->search_results_vbox = create_search_results_section (gsearch);
	gtk_widget_set_sensitive (GTK_WIDGET (gsearch->search_results_vbox), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), gsearch->search_results_vbox, TRUE, TRUE, 0);

	GTK_WIDGET_SET_FLAGS (gsearch->find_button, GTK_CAN_DEFAULT);
	gtk_widget_set_sensitive (gsearch->find_button, TRUE);

	g_signal_connect (G_OBJECT (gsearch->find_button), "clicked",
	                  G_CALLBACK (click_find_cb), (gpointer) gsearch);
    	//g_signal_connect (G_OBJECT (gsearch->find_button), "size_allocate",
	 //                 G_CALLBACK (size_allocate_cb), (gpointer) gsearch->available_options_add_button);

	if (gsearch->is_window_accessible) {
		add_atk_namedesc (GTK_WIDGET (gsearch->find_button), NULL, _("Click to perform a search."));
	}

	hbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (hbox), 6);
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 0);

	widget = gtk_button_new_with_mnemonic (_("_Previous"));
	image = gtk_image_new_from_stock (GTK_STOCK_GO_BACK, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (widget), image);
	gsearch->back_button = widget;
	g_signal_connect (G_OBJECT (gsearch->back_button), "clicked",
	                  G_CALLBACK (click_find_cb), (gpointer) gsearch);
	gtk_container_add (GTK_CONTAINER (hbox), widget);

	widget = gtk_button_new_with_mnemonic (_("_Next"));
	image = gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (widget), image);
	gsearch->forward_button = widget;
	g_signal_connect (G_OBJECT (gsearch->forward_button), "clicked",
	                  G_CALLBACK (click_find_cb), (gpointer) gsearch);
	gtk_container_add (GTK_CONTAINER (hbox), widget);

//	widget = gtk_label_new ("");
//	gtk_box_pack_end (GTK_BOX (vbox), widget, TRUE, TRUE, 2);
	

	//widget = gtk_expander_new_with_mnemonic (_("Selected document _details:"));
	//gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
//	g_signal_connect (G_OBJECT (gsearch->show_more_options_expander), "notify::expanded",
//			  G_CALLBACK (click_expander_cb), (gpointer) gsearch);

	gtk_widget_show_all (main_container);
	//gtk_widget_hide (gsearch->available_options_vbox);

	gtk_widget_set_sensitive (gsearch->forward_button, FALSE);
	gtk_widget_set_sensitive (gsearch->back_button, FALSE);


	gtk_window_set_focus (GTK_WINDOW (gsearch->window),
		GTK_WIDGET (gsearch->search_entry));

	gtk_window_set_default (GTK_WINDOW (gsearch->window), gsearch->find_button);

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
	//click_to_activate_pref = tracker_search_gconf_get_string ("/apps/nautilus/preferences/click_policy");

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

static gboolean 
tracker_search_select_service_type_by_string (GtkComboBox *combo, gchar *service)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *current_value;
	
	model = gtk_combo_box_get_model (combo);
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return FALSE;

	do {
		gtk_tree_model_get (model, &iter, 1, &current_value, -1);
		if (!strcmp (service, current_value)) {
			gtk_combo_box_set_active_iter (combo, &iter);
			return TRUE;
		}
	} while (gtk_tree_model_iter_next (model, &iter));

	return FALSE;
}

gchar *
tracker_search_pixmap_file (const gchar * partial_path)
{
	gchar * path;

	path = g_build_filename (DATADIR "/pixmaps/tracker", partial_path, NULL);
	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		return path;
	} else {
		return g_strdup (partial_path);
	}
	g_free (path);
	return NULL;
}


int
main (int argc,
      char * argv[])
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
	                              GNOME_PARAM_APP_DATADIR, DATADIR,
	                              GNOME_PARAM_GOPTION_CONTEXT, option_context,
	                              GNOME_PARAM_NONE);

	g_set_application_name (_("Desktop Search"));


	tracker_search_init_stock_icons ();

	window = g_object_new (GSEARCH_TYPE_WINDOW, NULL);
	gsearch = GSEARCH_WINDOW (window);

	tracker_search_ui_manager (gsearch);

	gtk_window_set_icon_name (GTK_WINDOW (gsearch->window), "gnome-searchtool");

	gchar * icon_path;
	icon_path = tracker_search_pixmap_file ("tracker.png");
	gtk_window_set_default_icon_from_file (icon_path, NULL);
	g_free (icon_path);

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

	add_no_files_found_message (gsearch);

	if (service && !(tracker_search_select_service_type_by_string (GTK_COMBO_BOX (gsearch->combo), service))) {
		g_printerr (_("Invalid service type: %s\n"), service);
		return 1;
	}

	if (terms) {
		search_string = g_strjoinv(" ", terms);
		gtk_entry_set_text (GTK_ENTRY (gsearch->search_entry), search_string);
		g_free (search_string);
		gtk_button_clicked (GTK_BUTTON(gsearch->find_button));
	}
	
	gtk_main ();
	return 0;
}
