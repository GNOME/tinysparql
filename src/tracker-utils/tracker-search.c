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

#include <libtracker-client/tracker.h>
#include <libtracker-common/tracker-common.h>

static gint	      limit = 512;
static gint	      offset;
static gchar	    **terms;
static gboolean       or_operator;
static gboolean       detailed;
static gboolean       files;
static gboolean       music_albums;
static gboolean       music_artists;
static gboolean       music_files;
static gboolean       image_files;

static GOptionEntry   entries[] = {
	{ "limit", 'l', 0, G_OPTION_ARG_INT, &limit,
	  N_("Limit the number of results shown"),
	  N_("512")
	},
	{ "offset", 'o', 0, G_OPTION_ARG_INT, &offset,
	  N_("Offset the results"),
	  N_("0")
	},
	{ "or-operator", 'r', 0, G_OPTION_ARG_NONE, &or_operator,
	  N_("Use OR for search terms instead of AND (the default)"),
	  NULL
	},
	{ "detailed", 'd', 0, G_OPTION_ARG_NONE, &detailed,
	  N_("Show more detailed results (only applies to general search)"),
	  NULL
	},
	{ "files", 'f', 0, G_OPTION_ARG_NONE, &files,
	  N_("List all files"),
	  NULL
	},
	{ "music-albums", 'a', 0, G_OPTION_ARG_NONE, &music_albums,
	  N_("List all music albums (includes song count and duration sum)"),
	  NULL
	},
	{ "music-artists", 's', 0, G_OPTION_ARG_NONE, &music_artists,
	  N_("List all music artists"),
	  NULL
	},
	{ "music-files", 'u', 0, G_OPTION_ARG_NONE, &music_files,
	  N_("List all music files"),
	  NULL
	},
	{ "image-files", 'i', 0, G_OPTION_ARG_NONE, &image_files,
	  N_("List all image files"),
	  NULL
	},
	{ G_OPTION_REMAINING, 0, 0,
	  G_OPTION_ARG_STRING_ARRAY, &terms,
	  N_("search terms"),
	  N_("EXPRESSION")
	},
	{ NULL }
};

static void
show_limit_warning (void)
{
	/* Display '...' so the user thinks there is
	 * more items.
	 */
	g_print ("  ...\n");
	
	/* Display warning so the user knows this is
	 * not the WHOLE data set.
	 */
	g_printerr ("\n%s\n",
		    _("NOTE: Limit was reached, there are more items in the database not listed here"));
}

static void
get_files_foreach (gpointer value,
		   gpointer user_data)
{
	gchar **data = value;

	g_print ("  %s\n", data[0]);
}

static gboolean
get_image_files (TrackerClient *client,
		 gint           search_offset,
		 gint           search_limit)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *query;

	query = g_strdup_printf ("SELECT ?image "
				 "WHERE { "
				 "  ?image a nfo:Image "
				 "} "
				 "ORDER BY ASC(?image) "
				 "OFFSET %d "
				 "LIMIT %d",
				 search_offset, 
				 search_limit);

	results = tracker_resources_sparql_query (client, query, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not get search results"),
			    error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!results) {
		g_print ("%s\n",
			 _("No files were found"));
	} else {
		g_print (tracker_dngettext (NULL,
					    _("File: %d"), 
					    _("Files: %d"),
					    results->len),
			 results->len);
		g_print ("\n");

		g_ptr_array_foreach (results, 
				     get_files_foreach, 
				     NULL);

		if (results->len >= search_limit) {
			show_limit_warning ();
		}

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}

	return TRUE;
}

static gboolean
get_music_files (TrackerClient *client,
		 gint           search_offset,
		 gint           search_limit)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *query;

	query = g_strdup_printf ("SELECT ?song "
				 "WHERE { "
				 "  ?song a nmm:MusicPiece "
				 "} "
				 "ORDER BY ASC(?song) "
				 "OFFSET %d "
				 "LIMIT %d",
				 search_offset, 
				 search_limit);

	results = tracker_resources_sparql_query (client, query, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not get search results"),
			    error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!results) {
		g_print ("%s\n",
			 _("No files were found"));
	} else {
		g_print (tracker_dngettext (NULL,
					    _("File: %d"), 
					    _("Files: %d"),
					    results->len),
			 results->len);
		g_print ("\n");

		g_ptr_array_foreach (results, 
				     get_files_foreach, 
				     NULL);

		if (results->len >= search_limit) {
			show_limit_warning ();
		}

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}

	return TRUE;
}

static void
get_music_artists_foreach (gpointer value,
			   gpointer user_data)
{
	gchar **data = value;

	g_print ("  '%s'\n", data[1]);
}

static gboolean
get_music_artists (TrackerClient *client,
		   gint           search_offset,
		   gint           search_limit)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *query;

	query = g_strdup_printf ("SELECT ?artist ?title "
				 "WHERE {"
				 "  ?artist a nmm:Artist ;"
				 "  nmm:artistName ?title "
                                 "} "
				 "GROUP BY ?artist "
				 "OFFSET %d "
				 "LIMIT %d",
				 search_offset, 
				 search_limit);

	results = tracker_resources_sparql_query (client, query, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not get search results"),
			    error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!results) {
		g_print ("%s\n",
			 _("No artists were found"));
	} else {
		g_print (tracker_dngettext (NULL,
					    _("Artist: %d"), 
					    _("Artists: %d"),
					    results->len),
			 results->len);
		g_print ("\n");

		g_ptr_array_foreach (results, 
				     get_music_artists_foreach, 
				     NULL);

		if (results->len >= search_limit) {
			show_limit_warning ();
		}

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}

	return TRUE;
}

static void
get_music_albums_foreach (gpointer value,
			  gpointer user_data)
{
	gchar **data = value;

	g_print ("  "); /*'%s', ", data[1]);*/
	g_print (tracker_dngettext (NULL,
				    _("%d Song"), 
				    _("%d Songs"),
				    atoi (data[2])),
		 atoi (data[2]));

	g_print (", ");
	g_print (tracker_dngettext (NULL,
				    _("%d Second"), 
				    _("%d Seconds"),
				    atoi (data[3])),
		 atoi (data[3]));
	g_print (", ");
	g_print (_("Album '%s'"), data[1]);
	g_print ("\n");
}

static gboolean
get_music_albums (TrackerClient *client,
		  gint           search_offset,
		  gint           search_limit)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *query;

	query = g_strdup_printf ("SELECT ?album ?title COUNT(?song) "
				 "AS songs "
				 "SUM(?length) AS totallength "
				 "WHERE {"
				 "  ?album a nmm:MusicAlbum ;"
				 "  nie:title ?title ."
				 "  ?song nmm:musicAlbum ?album ;"
				 "  nmm:length ?length "
                                 "} "
				 "GROUP BY ?album "
				 "OFFSET %d "
				 "LIMIT %d",
				 search_offset, 
				 search_limit);

	results = tracker_resources_sparql_query (client, query, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not get search results"),
			    error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!results) {
		g_print ("%s\n",
			 _("No music was found"));
	} else {
		g_print (tracker_dngettext (NULL,
					    _("Album: %d"), 
					    _("Albums: %d"),
					    results->len),
			 results->len);
		g_print ("\n");

		g_ptr_array_foreach (results, 
				     get_music_albums_foreach, 
				     NULL);

		if (results->len >= search_limit) {
			show_limit_warning ();
		}

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}

	return TRUE;
}

static gboolean
get_files (TrackerClient *client,
	   gint           search_offset,
	   gint           search_limit)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *query;

	query = g_strdup_printf ("SELECT ?u "
				 "WHERE { "
				 "  ?u a nie:InformationElement "
				 "} "
				 "ORDER BY ASC(?u) "
				 "OFFSET %d "
				 "LIMIT %d",
				 search_offset, 
				 search_limit);

	results = tracker_resources_sparql_query (client, query, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not get search results"),
			    error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!results) {
		g_print ("%s\n",
			 _("No files were found"));
	} else {
		g_print (tracker_dngettext (NULL,
					    _("File: %d"), 
					    _("Files: %d"),
					    results->len),
			 results->len);
		g_print ("\n");

		g_ptr_array_foreach (results, 
				     get_files_foreach, 
				     NULL);

		if (results->len >= search_limit) {
			show_limit_warning ();
		}

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}

	return TRUE;
}

static void
get_all_by_search_foreach (gpointer value, 
			   gpointer user_data)
{
	gchar **metadata;
	gchar **p;
	gboolean detailed;
	gint i;

	metadata = value;
	detailed = GPOINTER_TO_INT (user_data);

	for (p = metadata, i = 0; *p; p++, i++) {
		if (i == 0) {
			g_print ("  %s", *p);
		} else if (detailed) {
			g_print (", %s", *p);
		}
	}

	g_print ("\n");
}

static gboolean
get_all_by_search (TrackerClient *client, 
		   GStrv          search_words,
		   gint           search_offset,
		   gint           search_limit,
		   gboolean       use_or_operator,
		   gboolean       detailed_results)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *search_words_joined;
	gchar *query;

	if (use_or_operator) {
		search_words_joined = g_strjoinv (" OR ", search_words);
	} else {
		search_words_joined = g_strjoinv (" ", search_words);
	}

	if (detailed_results) {
		query = g_strdup_printf ("SELECT ?s ?type ?mimeType WHERE { ?s fts:match \"%s\" ; rdf:type ?type . "
					 "OPTIONAL { ?s nie:mimeType ?mimeType } } OFFSET %d LIMIT %d",
					 search_words_joined, 
					 search_offset, 
					 search_limit);
	} else {
		query = g_strdup_printf ("SELECT ?s WHERE { ?s fts:match \"%s\" } OFFSET %d LIMIT %d",
					 search_words_joined, 
					 search_offset, 
					 search_limit);
	}

	g_free (search_words_joined);

	results = tracker_resources_sparql_query (client, query, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not get search results"),
			    error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!results) {
		g_print ("%s\n",
			 _("No results were found matching your query"));
	} else {
		g_print (tracker_dngettext (NULL,
					    _("Result: %d"), 
					    _("Results: %d"),
					    results->len),
			 results->len);
		g_print ("\n");

		g_ptr_array_foreach (results, 
				     get_all_by_search_foreach, 
				     GINT_TO_POINTER (detailed_results));

		if (results->len >= search_limit) {
			show_limit_warning ();
		}

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}

	return TRUE;
}

int
main (int argc, char **argv)
{
	TrackerClient *client;
	GOptionContext *context;
	gchar *summary;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the
	 * usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>
	 */
	context = g_option_context_new (_("- Search for terms in all data"));

	/* Translators: this message will appear after the usage string
	 * and before the list of options.
	 */
	summary = g_strconcat (_("Applies an AND operator to all terms separated "
				 "by a space (see --or-operator)"),
			       "\n",
			       "\n",
			       _("This means if you search for 'foo' and 'bar', "
				 "they must BOTH exist (unless you use --or-operator)"),
			       NULL);
	g_option_context_set_summary (context, summary);
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	g_free (summary);
	
	if (!music_albums && !music_artists && !music_files &&
	    !image_files &&
	    !files &&
	    !terms) {
		gchar *help;

		g_printerr ("%s\n\n",
			    _("Search terms are missing"));

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	client = tracker_connect (FALSE, -1);

	if (!client) {
		g_printerr ("%s\n",
			    _("Could not establish a DBus connection to Tracker"));
		return EXIT_FAILURE;
	}

	if (limit <= 0) {
		limit = 512;
	}

	if (files) {
		gboolean success;

		success = get_files (client, offset, limit);
		tracker_disconnect (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (music_albums) {
		gboolean success;

		success = get_music_albums (client, offset, limit);
		tracker_disconnect (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (music_artists) {
		gboolean success;

		success = get_music_artists (client, offset, limit);
		tracker_disconnect (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (music_files) {
		gboolean success;

		success = get_music_files (client, offset, limit);
		tracker_disconnect (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (image_files) {
		gboolean success;

		success = get_image_files (client, offset, limit);
		tracker_disconnect (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (terms) {
		gboolean success;
		
		success = get_all_by_search (client, terms, offset, limit, or_operator, detailed);
		tracker_disconnect (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
