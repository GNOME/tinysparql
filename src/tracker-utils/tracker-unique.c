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

#include <sys/param.h>
#include <stdlib.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <libtracker/tracker.h>
#include <libtracker-common/tracker-common.h>

#ifdef G_OS_WIN32
#include <trackerd/mingw-compat.h>
#endif /* G_OS_WIN32 */

static gchar	    **fields;
static gchar	     *service;
static gchar	     *path;
static gchar        **concat;
static gchar	    **count;
static gchar        **sum;
static gboolean       descending;

static GOptionEntry   entries[] = {
	{ "path", 'p', 0, G_OPTION_ARG_STRING, &path,
	  N_("Path to use in query"),
	  NULL
	},
	{ "service", 's', 0, G_OPTION_ARG_STRING, &service,
	  N_("Search from a specific service"),
	  NULL
	},
	{ "concat", 'n', 0, G_OPTION_ARG_STRING_ARRAY, &concat,
	  N_("Concatenate different values of this field"),
	  "e.g. File:Mime"
	},
	{ "count", 'c', 0, G_OPTION_ARG_STRING_ARRAY, &count,
	  N_("Count instances of unique fields of this type"),
	  "e.g. File:Mime"
	},
	{ "sum", 'u', 0, G_OPTION_ARG_STRING_ARRAY, &sum,
	  N_("Sum the values of this field"),
	  "e.g. File:Mime"
	},
	{ "desc", 'o', 0, G_OPTION_ARG_NONE, &descending,
	  N_("Sort to descending order"),
	  NULL},
	{ G_OPTION_REMAINING, 0, 0,
	  G_OPTION_ARG_STRING_ARRAY, &fields,
	  N_("Required fields"), N_("FIELD [FIELD...]")},
	{ NULL }
};

static void
get_meta_table_data (gpointer value)
{
	gchar **meta;
	gchar **p;
	gint	i;

	meta = value;

	for (p = meta, i = 0; *p; p++, i++) {
		if (i == 0) {
			g_print ("  %s", *p);
		} else {
			g_print (", %s", *p);
		}
	}

	g_print ("\n");
}

int
main (int argc, char **argv)
{
	TrackerClient	*client;
	ServiceType	 type;
	GOptionContext	*context;
	GError		*error = NULL;
	gchar		*content;
	gchar		*buffer = NULL;
	gsize		 size;
	GPtrArray	*array;
	gchar          **aggregates = NULL;
	gchar          **aggregate_fields = NULL;
	guint            aggregate_count;
	guint            aggregate;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new ("- Get unique values with an optional RDF query filter");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (!fields) {
		gchar *help;

		g_printerr ("%s\n\n",
			    _("Fields are missing"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	client = tracker_connect (FALSE);

	if (!client) {
		g_printerr ("%s\n",
			    _("Could not establish a DBus connection to Tracker"));
		return EXIT_FAILURE;
	}

	if (!service) {
		g_print ("%s\n",
			 _("Defaulting to 'files' service"));

		type = SERVICE_FILES;
	} else {
		type = tracker_service_name_to_type (service);

		if (type == SERVICE_OTHER_FILES && g_ascii_strcasecmp (service, "Other")) {
			g_printerr ("%s\n",
				    _("Service not recognized, searching in other files..."));
		}
	}

	if (path) {
		gchar *path_in_utf8;

		path_in_utf8 = g_filename_to_utf8 (path, -1, NULL, NULL, &error);
		if (error) {
			g_printerr ("%s:'%s', %s\n",
				    _("Could not get UTF-8 path from path"),
				    path,
				    error->message);
			g_error_free (error);
			tracker_disconnect (client);

			return EXIT_FAILURE;
		}

		g_file_get_contents (path_in_utf8, &content, &size, &error);
		if (error) {
			g_printerr ("%s:'%s', %s\n",
				    _("Could not read file"),
				    path_in_utf8,
				    error->message);
			g_error_free (error);
			g_free (path_in_utf8);
			tracker_disconnect (client);

			return EXIT_FAILURE;
		}

		g_free (path_in_utf8);

		buffer = g_locale_to_utf8 (content, size, NULL, NULL, &error);
		g_free (content);

		if (error) {
			g_printerr ("%s, %s\n",
				    _("Could not convert query file to UTF-8"),
				    error->message);
			g_error_free (error);
			tracker_disconnect (client);

			return EXIT_FAILURE;
		}
	}
	
	aggregate_count =
		(concat ? g_strv_length(concat) : 0) + 
		(sum    ? g_strv_length(sum)    : 0) +
		(count  ? g_strv_length(count)  : 0);	
	aggregate = 0;

	aggregates = g_new0 (gchar*, aggregate_count + 1);
	aggregate_fields = g_new0 (gchar*, aggregate_count + 1);

	/* g_debug ("Count of aggregates: %d", aggregate_count); */

	if (concat) {
		guint i;
		for (i=0;i<g_strv_length (concat);i++) {
			g_debug ("Concat added for %s", concat[i]);
			
			aggregates[aggregate] = g_strdup ("CONCAT");
			aggregate_fields[aggregate] = g_strdup (concat[i]);

			aggregate++;
		}
	}

	if (sum) {
		guint i;
		for (i=0;i<g_strv_length (sum);i++) {
			g_debug ("Sum added for %s", sum[i]);

			aggregates[aggregate] = g_strdup ("SUM");
			aggregate_fields[aggregate] = g_strdup (sum[i]);

			aggregate++;
		}
	}

	if (count) {
		guint i;
		for (i=0;i<g_strv_length (count);i++) {
			g_debug ("Count added for %s", count[i]);

			aggregates[aggregate] = g_strdup ("COUNT");
			aggregate_fields[aggregate] = g_strdup (count[i]);

			aggregate++;
		}
	}

	aggregates[aggregate_count] = NULL;
	aggregate_fields[aggregate_count] = NULL;

	array = tracker_metadata_get_unique_values_with_aggregates (client,
								    type,
								    fields,
								    buffer,
								    aggregates,
								    aggregate_fields,
								    descending,
								    0,
								    512,
								    &error);
	g_free (buffer);

	g_strfreev (aggregates);
	g_strfreev (aggregate_fields);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not query search"),
			    error->message);
		g_error_free (error);

		return EXIT_FAILURE;
	}

	if (!array) {
		g_print ("%s\n",
			 _("No results found matching your query"));
	} else {
		gint length;

		length = array->len;
		
		g_print (tracker_dngettext (NULL,
					    _("Result: %d"), 
					    _("Results: %d"),
					    length),
			 length);
		g_print ("\n");

		g_ptr_array_foreach (array, (GFunc) get_meta_table_data, NULL);
		g_ptr_array_free (array, TRUE);
	}

	tracker_disconnect (client);

	return EXIT_SUCCESS;
}
