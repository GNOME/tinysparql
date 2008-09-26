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

#include <libtracker/tracker.h>

static gint	      limit = 512;
static gint	      offset;
static gchar	    **terms;
static gchar	     *service;
static gboolean       detailed;

static GOptionEntry   entries[] = {
	{ "service", 's', 0, G_OPTION_ARG_STRING, &service,
	  N_("Search from a specific service"),
	  NULL
	},
	{ "limit", 'l', 0, G_OPTION_ARG_INT, &limit,
	  N_("Limit the number of results shown"),
	  N_("512")
	},
	{ "offset", 'o', 0, G_OPTION_ARG_INT, &offset,
	  N_("Offset the results"),
	  N_("0")
	},
	{ "detailed", 'd', 0, G_OPTION_ARG_NONE, &detailed,
	  N_("Show more detailed results with service and mime type"),
	  NULL
	},
	{ G_OPTION_REMAINING, 0, 0,
	  G_OPTION_ARG_STRING_ARRAY, &terms,
	  N_("search terms"),
	  NULL
	},
	{ NULL }
};

static void
get_meta_table_data (gpointer value)
{
	gchar **meta;
	gchar **p;
	gchar  *str;
	gint	i;

	meta = value;

	for (p = meta, i = 0; *p; p++, i++) {
		switch (i) {
		case 0:
			str = g_filename_from_utf8 (*p, -1, NULL, NULL, NULL);
			g_print ("  %s:'%s'", _("Path"), str);
			g_free (str);
			break;
		case 1:
			g_print (", %s:'%s'", _("Service"), *p);
			break;
		case 2:
			g_print (", %s:'%s'", _("MIME-type"), *p);
			break;
		default:
			break;
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
	gchar		*search;
	gchar		*summary;
	gchar	       **strv;
	gchar	       **p;
	GPtrArray	*array;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new (_("- Search files for certain terms"));

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.				*/
	summary = g_strconcat (_("Specifying multiple terms apply an AND "
				 "operator to the search performed"),
			       "\n",
			       _("This means if you search for 'foo' and 'bar', "
				 "they must BOTH exist"),
			       "\n",
			       "\n",
			       _("Recognized services include:"),
			       "\n"
			       "\n",
			       "  Documents\n"
			       "  Emails\n"
			       "  EmailAttachments\n"
			       "  Music\n"
			       "  Images\n"
			       "  Videos\n"
			       "  Text\n"
			       "  Development\n"
			       "  Applications\n"
			       "  Conversations\n"
			       "  Folders\n"
			       "  Files",
			       NULL);
	g_option_context_set_summary (context, summary);
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	g_free (summary);

	if (!terms) {
		gchar *help;

		g_printerr ("%s\n\n",
			    _("Search terms are missing"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr (help);
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

	if (limit <= 0) {
		limit = 512;
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

	search = g_strjoinv (" ", terms);

	if (detailed) {
		array = tracker_search_text_detailed (client,
						      time (NULL),
						      type,
						      search,
						      offset,
						      limit,
						      &error);
		g_free (search);

		if (error) {
			g_printerr ("%s, %s\n",
				    _("Could not get find detailed results by text"),
				    error->message);

			g_error_free (error);
			tracker_disconnect (client);

			return EXIT_FAILURE;
		}

		if (!array) {
			g_print ("%s\n",
				 _("No results found matching your query"));
		} else {
			g_print ("%s\n",
				 _("Results:"));

			g_ptr_array_foreach (array, (GFunc) get_meta_table_data, NULL);
			g_ptr_array_free (array, TRUE);
		}
	} else {
		strv = tracker_search_text (client,
					    time (NULL),
					    type,
					    search,
					    offset,
					    limit,
					    &error);
		g_free (search);

		if (error) {
			g_printerr ("%s, %s\n",
				    _("Could not get find results by text"),
				    error->message);

			g_error_free (error);
			tracker_disconnect (client);

			return EXIT_FAILURE;
		}

		if (!strv) {
			g_print ("%s\n",
				 _("No results found matching your query"));
		} else {
			g_print ("%s:\n",
				 _("Results"));

			for (p = strv; *p; p++) {
				gchar *s;

				s = g_locale_from_utf8 (*p, -1, NULL, NULL, NULL);

				if (!s) {
					continue;
				}

				g_print ("  %s\n", s);
				g_free (s);
			}

			g_free (strv);
		}
	}

	tracker_disconnect (client);

	return EXIT_SUCCESS;
}
