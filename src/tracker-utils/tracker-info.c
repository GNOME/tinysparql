/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libtracker/tracker.h>
#include <libtracker-common/tracker-common.h>

static gchar	     *service;
static gchar        **metadata;
static gchar        **uris;

static GOptionEntry   entries[] = {
	{ "service", 's', 0, G_OPTION_ARG_STRING, &service,
	  N_("Service type of the file"),
	  N_("Files")
	},
	{ "metadata", 'm', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING_ARRAY, &metadata,
	  N_("Metadata to request (optional, multiple calls allowed)"),
	  N_("File:Size")
	},
	{ G_OPTION_REMAINING, 0, G_OPTION_FLAG_FILENAME, G_OPTION_ARG_FILENAME_ARRAY, &uris,
	  N_("FILE..."),
	  N_("FILE")
	},
	{ NULL }
};

static void
print_property_value (gpointer data,
		      gpointer user_data)
{
	GStrv results;
	GStrv uris_used;

	results = data;
	uris_used = user_data;
	
	if (uris_used) { 
		static gint uri_id = 0;
		GStrv p;
		gint i;

		g_print ("%s\n", uris_used[uri_id]);
		
		if (!results) {
			g_print ("  %s\n", _("No metadata available"));
			return;
		}

		for (p = results, i = 0; *p; p++, i++) {
			g_print ("  '%s' = '%s'\n", metadata[i], *p);
		}

		uri_id++;
	} else {
		g_print ("  '%s' = '%s'\n", results[0], results[1]);
	}
}

int
main (int argc, char **argv)
{
	TrackerClient	*client;
	ServiceType	 type;
	GFile           *file;
	gchar           *summary;
	gchar           *path;
	GOptionContext	*context;
	GError		*error = NULL;
	guint            count;
	gint             i;
	gint             exit_result = EXIT_SUCCESS;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new (_("- Get all information from a certain file"));

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.				*/
	summary = g_strconcat (_("For a list of services and metadata that "
				 "can be used here, see tracker-services."),
			       NULL);

	g_option_context_set_summary (context, summary);
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_free (summary);

	if (!uris) {
		gchar *help;

		g_printerr (_("URI missing"));
		g_printerr ("\n\n");

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	if (!metadata && g_strv_length (uris) > 1) {
		gchar *help;

		g_printerr (_("Requesting ALL information about multiple files is not supported"));
		g_printerr ("\n\n");

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	client = tracker_connect (FALSE);

	if (!client) {
		g_printerr (_("Could not establish a DBus connection to Tracker"));
		g_printerr ("\n");

		return EXIT_FAILURE;
	}

	if (!service) {
		g_print (_("Defaulting to 'files' service"));
		g_print ("\n");

		type = SERVICE_FILES;
	} else {
		type = tracker_service_name_to_type (service);

		if (type == SERVICE_OTHER_FILES && g_ascii_strcasecmp (service, "Other")) {
			g_printerr (_("Service type not recognized, using 'Other' ..."));
			g_printerr ("\n");
		}
	}

	count = g_strv_length (uris);

	if (count > 1 && metadata != NULL) {
		gchar     **strv;
		GPtrArray  *results;

		strv = g_new (gchar*, count + 1);

		/* Convert all files to real paths */
		for (i = 0; i < count; i++) {
			file = g_file_new_for_commandline_arg (uris[i]);
			path = g_file_get_path (file);
			g_object_unref (file);
		
			strv[i] = path;
		}

		strv[i] = NULL;
		
		results = tracker_metadata_get_multiple (client,
							 type,
							 (const gchar **) strv,
							 (const gchar **) metadata,
							 &error);

		if (error) {
			g_printerr (tracker_dngettext (NULL,
						       _("Unable to retrieve data for %d uri"), 
						       _("Unable to retrieve data for %d uris"), 
						       count),
				    count);
			g_printerr (", %s\n",
				    error->message);
			
			g_error_free (error);
			
			exit_result = EXIT_FAILURE;
		} else if (!results) {
			g_printerr (tracker_dngettext (NULL,
						       _("No metadata available for all %d uri"),
						       _("No metadata available for all %d uris"),
						       count),
				    count);
			g_print ("\n");
		} else {
			/* NOTE: This should be the same as count was before */
			count = g_strv_length ((gchar**) results);

			g_print (tracker_dngettext (NULL,
						    _("Result:"),
						    _("Results:"),
						    count),
				 count);
			g_print ("\n");
			
			g_ptr_array_foreach (results, print_property_value, strv);
			g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
			g_ptr_array_free (results, TRUE);
		}

		g_strfreev (strv);
	} else {
		GPtrArray *results;

		file = g_file_new_for_commandline_arg (uris[0]);
		path = g_file_get_path (file);
		
		if (G_LIKELY (!metadata)) {
			results = tracker_metadata_get_all (client,
							    type,
							    path,
							    &error);

			if (error) {
				g_printerr ("%s, %s\n",
					    _("Unable to retrieve data for uri"),
					    error->message);

				g_error_free (error);
				exit_result = EXIT_FAILURE;
			} else if (!results) {
				g_print (_("No metadata available for that uri"));
				g_print ("\n");
			} else {
				gint length;
				
				length = results->len;
				
				g_print (tracker_dngettext (NULL,
							    _("Result: %d for '%s'"), 
							    _("Results: %d for '%s'"),
							    length),
					 length,
					 path);
				g_print ("\n");
				
				g_ptr_array_foreach (results, print_property_value, NULL);
				g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
				g_ptr_array_free (results, TRUE);
			}		
		} else {
			GStrv results;

			results = tracker_metadata_get (client,
							type,
							path,
							(const gchar **) metadata,
							&error);
			if (error) {
				g_printerr ("%s, %s\n",
					    _("Unable to retrieve data for uri"),
					    error->message);

				g_error_free (error);
				exit_result = EXIT_FAILURE;
			} else if (!results) {
				g_print (_("No metadata available for that uri"));
				g_print ("\n");
			} else {
				gint i;
				
				count = g_strv_length (results);
				
				g_print (tracker_dngettext (NULL,
							    _("Result:"),
							    _("Results:"),
							    count),
					 count);
				g_print ("\n");

				for (i = 0; i < count; i++) {
					g_print ("  '%s' = '%s'\n",
						 metadata[i], results[i]);
				}
				
				g_strfreev (results);
			}		
		}

		g_object_unref (file);
		g_free (path);
	}

	tracker_disconnect (client);

	return exit_result;
}
