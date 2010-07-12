/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <libtracker-client/tracker-client.h>
#include <libtracker-common/tracker-common.h>

#define ABOUT	  \
	"Tracker " PACKAGE_VERSION "\n"

#define LICENSE	  \
	"This program is free software and comes without any warranty.\n" \
	"It is licensed under version 2 or later of the General Public " \
	"License which can be viewed at:\n" \
	"\n" \
	"  http://www.gnu.org/licenses/gpl.txt\n"

static gint limit = 512;
static gint offset;
static gchar **terms;
static gboolean or_operator;
static gboolean detailed;
static gboolean all;
static gboolean files;
static gboolean folders;
static gboolean music_albums;
static gboolean music_artists;
static gboolean music_files;
static gboolean image_files;
static gboolean video_files;
static gboolean document_files;
static gboolean emails;
static gboolean contacts;
static gboolean feeds;
static gboolean print_version;

static GOptionEntry entries[] = {
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
	  N_("Show URNs for results (doesn't apply to --music-albums, --music-artists, --feeds)"),
	  NULL
	},
	{ "all", 'a', 0, G_OPTION_ARG_NONE, &all,
	  N_("Return all non-existing matches too (i.e. include unmounted volumes)"),
	  NULL
	},
	{ "files", 'f', 0, G_OPTION_ARG_NONE, &files,
	  N_("Search for files"),
	  NULL
	},
	{ "folders", 's', 0, G_OPTION_ARG_NONE, &folders,
	  N_("Search for folders"),
	  NULL
	},
	{ "music", 'm', 0, G_OPTION_ARG_NONE, &music_files,
	  N_("Search for music files"),
	  NULL
	},
	{ "music-albums", 0, 0, G_OPTION_ARG_NONE, &music_albums,
	  N_("Search for music albums (--all has no effect on this)"),
	  NULL
	},
	{ "music-artists", 0, 0, G_OPTION_ARG_NONE, &music_artists,
	  N_("Search for music artists (--all has no effect on this) "),
	  NULL
	},
	{ "images", 'i', 0, G_OPTION_ARG_NONE, &image_files,
	  N_("Search for image files"),
	  NULL
	},
	{ "videos", 'v', 0, G_OPTION_ARG_NONE, &video_files,
	  N_("Search for video files"),
	  NULL
	},
	{ "documents", 't', 0, G_OPTION_ARG_NONE, &document_files,
	  N_("Search for document files"),
	  NULL
	},
	{ "emails", 'e', 0, G_OPTION_ARG_NONE, &emails,
	  N_("Search for emails"),
	  NULL
	},
	{ "contacts", 'c', 0, G_OPTION_ARG_NONE, &contacts,
	  N_("Search for contacts"),
	  NULL
	},
	{ "feeds", 0, 0, G_OPTION_ARG_NONE, &feeds,
	  N_("Search for feeds (--all has no effect on this) "),
	  NULL
	},
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
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

static gchar *
get_fts_string (GStrv    search_words,
                gboolean use_or_operator)
{
#if HAVE_TRACKER_FTS
	GString *fts;
	gint i, len;


	if (!search_words) {
		return NULL;
	}

	fts = g_string_new ("");
	len = g_strv_length (search_words);

	for (i = 0; i < len; i++) {
		gchar *escaped;

		/* Properly escape the input string as it's going to be passed
		 * in a sparql query */
		escaped = tracker_sparql_escape (search_words[i]);

		g_string_append (fts, escaped);

		if (i < len - 1) {
			if (use_or_operator) {
				g_string_append (fts, " OR ");
			} else {
				g_string_append (fts, " ");
			}
		}

		g_free (escaped);
	}

	return g_string_free (fts, FALSE);
#else
	/* If FTS support not enabled, always do non-fts searches */
	return NULL;
#endif
}

static void
get_contacts_foreach (gpointer value,
		    gpointer user_data)
{
	gchar **data;
	gboolean details;

	data = value;
	details = GPOINTER_TO_INT (user_data);

	if (details && data[1] && *data[1]) {
		g_print ("  %s, %s (%s)\n", data[0], data[1], data[2]);
	} else {
		g_print ("  %s, %s\n", data[0], data[1]);
	}
}

static gboolean
get_contacts_results (TrackerClient *client,
		      const gchar   *query,
		      gint           search_limit,
		      gboolean       details)
{
	GError *error = NULL;
	GPtrArray *results;

	results = tracker_resources_sparql_query (client, query, &error);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not get search results"),
		            error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!results) {
		g_print ("%s\n",
		         _("No contacts were found"));
	} else {
		g_print (g_dngettext (NULL,
		                      "Contact: %d",
		                      "Contacts: %d",
		                      results->len),
		         results->len);
		g_print ("\n");

		g_ptr_array_foreach (results,
		                     get_contacts_foreach,
		                     GINT_TO_POINTER (details));

		if (results->len >= search_limit) {
			show_limit_warning ();
		}

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}

	return TRUE;
}

static gboolean
get_contacts (TrackerClient *client,
	      GStrv          search_terms,
	      gboolean       show_all,
	      gint           search_offset,
	      gint           search_limit,
	      gboolean       use_or_operator,
	      gboolean       details)
{
	gchar *fts;
	gchar *query;
	gboolean success;

	fts = get_fts_string (search_terms, use_or_operator);

	if (fts) {
		query = g_strdup_printf ("SELECT tracker:coalesce(nco:fullname(?contact), \"Unknown\") nco:hasEmailAddress(?contact) ?contact "
		                         "WHERE { "
		                         "  ?contact a nco:Contact ;"
		                         "  fts:match \"%s\" ."
		                         "} "
		                         "ORDER BY ASC(nco:fullname(?contact)) ASC(nco:hasEmailAddress(?contact)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         fts,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT tracker:coalesce(nco:fullname(?contact), \"Unknown\") nco:hasEmailAddress(?contact) ?contact "
		                         "WHERE { "
		                         "  ?contact a nco:Contact ."
		                         "} "
		                         "ORDER BY ASC(nco:fullname(?contact)) ASC(nco:hasEmailAddress(?contact)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         search_offset,
		                         search_limit);
	}

	success = get_contacts_results (client, query, search_limit, details);
	g_free (query);
	g_free (fts);

	return success;
}

static void
get_emails_foreach (gpointer value,
		    gpointer user_data)
{
	gchar **data;
	gboolean details;

	data = value;
	details = GPOINTER_TO_INT (user_data);

	if (details && data[1] && *data[1]) {
		g_print ("  %s, %s (%s)\n", data[0], data[1], data[2]);
	} else {
		g_print ("  %s, %s\n", data[0], data[1]);
	}
}

static gboolean
get_emails_results (TrackerClient *client,
		    const gchar   *query,
		    gint           search_limit,
		    gboolean       details)
{
	GError *error = NULL;
	GPtrArray *results;

	results = tracker_resources_sparql_query (client, query, &error);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not get search results"),
		            error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!results) {
		g_print ("%s\n",
		         _("No emails were found"));
	} else {
		g_print (g_dngettext (NULL,
		                      "Email: %d",
		                      "Emails: %d",
		                      results->len),
		         results->len);
		g_print ("\n");

		g_ptr_array_foreach (results,
		                     get_emails_foreach,
		                     GINT_TO_POINTER (details));

		if (results->len >= search_limit) {
			show_limit_warning ();
		}

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}

	return TRUE;
}

static gboolean
get_emails (TrackerClient *client,
	    GStrv          search_terms,
	    gboolean       show_all,
	    gint           search_offset,
	    gint           search_limit,
	    gboolean       use_or_operator,
	    gboolean       details)
{
	gchar *fts;
	gchar *query;
	const gchar *show_all_str;
	gboolean success;

	show_all_str = show_all ? "" : "?email tracker:available true .";
	fts = get_fts_string (search_terms, use_or_operator);

	if (fts) {
		query = g_strdup_printf ("SELECT nmo:receivedDate(?email) nmo:messageSubject(?email) nie:url(?email) "
		                         "WHERE { "
		                         "  ?email a nmo:Email ;"
		                         "  fts:match \"%s\" ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nmo:messageSubject(?email)) ASC(nmo:receivedDate(?email))"
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         fts,
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT nmo:receivedDate(?email) nmo:messageSubject(?email) nie:url(?email) "
		                         "WHERE { "
		                         "  ?email a nmo:Email ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nmo:messageSubject(?email)) ASC(nmo:receivedDate(?email))"
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	}

	success = get_emails_results (client, query, search_limit, details);
	g_free (query);
	g_free (fts);

	return success;
}

static void
get_files_foreach (gpointer value,
                   gpointer user_data)
{
	gchar **data;
	gboolean details;

	data = value;
	details = GPOINTER_TO_INT (user_data);

	if (details && data[1] && *data[1]) {
		g_print ("  %s (%s)\n", data[1], data[0]);
	} else {
		g_print ("  %s\n", data[1]);
	}
}

static gboolean
get_files_results (TrackerClient *client,
                   const gchar   *query,
                   gint           search_limit,
		   gboolean       details)
{
	GError *error = NULL;
	GPtrArray *results;

	results = tracker_resources_sparql_query (client, query, &error);

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
		g_print (g_dngettext (NULL,
		                      "File: %d",
		                      "Files: %d",
		                      results->len),
		         results->len);
		g_print ("\n");

		g_ptr_array_foreach (results,
		                     get_files_foreach,
		                     GINT_TO_POINTER (details));

		if (results->len >= search_limit) {
			show_limit_warning ();
		}

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}

	return TRUE;
}

static gboolean
get_document_files (TrackerClient *client,
                    GStrv          search_terms,
                    gboolean       show_all,
                    gint           search_offset,
                    gint           search_limit,
                    gboolean       use_or_operator,
		    gboolean       details)
{
	gchar *fts;
	gchar *query;
	const gchar *show_all_str;
	gboolean success;

	show_all_str = show_all ? "" : "?document tracker:available true .";
	fts = get_fts_string (search_terms, use_or_operator);

	if (fts) {
		query = g_strdup_printf ("SELECT ?document nie:url(?document) "
		                         "WHERE { "
		                         "  ?document a nfo:Document ;"
		                         "  fts:match \"%s\" ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nie:url(?document)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         fts,
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT ?document nie:url(?document) "
		                         "WHERE { "
		                         "  ?document a nfo:Document ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nie:url(?document)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	}

	success = get_files_results (client, query, search_limit, details);
	g_free (query);
	g_free (fts);

	return success;
}

static gboolean
get_video_files (TrackerClient *client,
                 GStrv          search_terms,
                 gboolean       show_all,
                 gint           search_offset,
                 gint           search_limit,
                 gboolean       use_or_operator,
		 gboolean       details)
{
	gchar *fts;
	gchar *query;
	const gchar *show_all_str;
	gboolean success;

	show_all_str = show_all ? "" : "?video tracker:available true . ";
	fts = get_fts_string (search_terms, use_or_operator);

	if (fts) {
		query = g_strdup_printf ("SELECT ?video nie:url(?video) "
		                         "WHERE { "
		                         "  ?video a nfo:Video ;"
		                         "  fts:match \"%s\" ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nie:url(?video)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         fts,
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT ?video nie:url(?video) "
		                         "WHERE { "
		                         "  ?video a nfo:Video ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nie:url(?video)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	}

	success = get_files_results (client, query, search_limit, details);
	g_free (query);
	g_free (fts);

	return success;
}

static gboolean
get_image_files (TrackerClient *client,
                 GStrv          search_terms,
                 gboolean       show_all,
                 gint           search_offset,
                 gint           search_limit,
		 gboolean       use_or_operator,
		 gboolean       details)
{
	gchar *fts;
	gchar *query;
	const gchar *show_all_str;
	gboolean success;

	show_all_str = show_all ? "" : "?image tracker:available true . ";
	fts = get_fts_string (search_terms, use_or_operator);

	if (fts) {
		query = g_strdup_printf ("SELECT ?image nie:url(?image) "
		                         "WHERE { "
		                         "  ?image a nfo:Image ;"
		                         "  fts:match \"%s\" ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nie:url(?image)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         fts,
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT ?image nie:url(?image) "
		                         "WHERE { "
		                         "  ?image a nfo:Image ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nie:url(?image)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	}

	success = get_files_results (client, query, search_limit, details);
	g_free (query);
	g_free (fts);

	return success;
}

static gboolean
get_music_files (TrackerClient *client,
                 GStrv          search_terms,
                 gboolean       show_all,
                 gint           search_offset,
                 gint           search_limit,
                 gboolean       use_or_operator,
		 gboolean       details)
{
	gchar *fts;
	gchar *query;
	const gchar *show_all_str;
	gboolean success;

	show_all_str = show_all ? "" : "?song tracker:available true . ";
	fts = get_fts_string (search_terms, use_or_operator);

	if (fts) {
		query = g_strdup_printf ("SELECT ?song nie:url(?song) "
		                         "WHERE { "
		                         "  ?song a nmm:MusicPiece ;"
		                         "  fts:match \"%s\" ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nie:url(?song)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         fts,
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT ?song nie:url(?song) "
		                         "WHERE { "
		                         "  ?song a nmm:MusicPiece ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nie:url(?song)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	}

	success = get_files_results (client, query, search_limit, details);
	g_free (query);
	g_free (fts);

	return success;
}

static void
get_music_foreach (gpointer value,
                   gpointer user_data)
{
	gchar **data = value;

	g_print ("  '%s' (%s)\n", data[1], data[0]);
}

static gboolean
get_music_artists (TrackerClient *client,
                   GStrv          search_terms,
                   gint           search_offset,
                   gint           search_limit,
                   gboolean       use_or_operator)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *fts;
	gchar *query;

	fts = get_fts_string (search_terms, use_or_operator);

	if (fts) {
		query = g_strdup_printf ("SELECT ?artist ?title "
		                         "WHERE {"
		                         "  ?artist a nmm:Artist ;"
		                         "  nmm:artistName ?title ;"
		                         "  fts:match \"%s\" . "
		                         "} "
		                         "ORDER BY ASC(?title) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         fts,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT ?artist ?title "
		                         "WHERE {"
		                         "  ?artist a nmm:Artist ;"
		                         "  nmm:artistName ?title . "
		                         "} "
		                         "ORDER BY ASC(?title) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         search_offset,
		                         search_limit);
	}

	g_free (fts);

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
		g_print (g_dngettext (NULL,
		                      "Artist: %d",
		                      "Artists: %d",
		                      results->len),
		         results->len);
		g_print ("\n");

		g_ptr_array_foreach (results,
		                     get_music_foreach,
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
get_music_albums (TrackerClient *client,
                  GStrv          search_words,
                  gint           search_offset,
                  gint           search_limit,
                  gboolean       use_or_operator)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *fts;
	gchar *query;

	fts = get_fts_string (search_words, use_or_operator);

	if (fts) {
		query = g_strdup_printf ("SELECT ?album nie:title(?album) "
		                         "WHERE {"
		                         "  ?album a nmm:MusicAlbum ;"
		                         "  fts:match \"%s\" ."
		                         "} "
		                         "ORDER BY ASC(nie:title(?album)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         fts,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT ?album nie:title(?album) "
		                         "WHERE {"
		                         "  ?album a nmm:MusicAlbum ."
		                         "} "
		                         "ORDER BY ASC(nie:title(?album)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         search_offset,
		                         search_limit);
	}

	g_free (fts);

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
		g_print (g_dngettext (NULL,
		                      "Album: %d",
		                      "Albums: %d",
		                      results->len),
		         results->len);
		g_print ("\n");

		g_ptr_array_foreach (results,
		                     get_music_foreach,
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
get_feed_foreach (gpointer value,
                  gpointer user_data)
{
	gchar **data = value;

	g_print ("  '%s' (%s)\n", data[1], data[0]);
}

static gboolean
get_feeds (TrackerClient *client,
           GStrv          search_terms,
           gint           search_offset,
           gint           search_limit,
           gboolean       use_or_operator)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *fts;
	gchar *query;

	fts = get_fts_string (search_terms, use_or_operator);

	if (fts) {
		query = g_strdup_printf ("SELECT ?feed nie:title(?feed) "
		                         "WHERE {"
		                         "  ?feed a mfo:FeedMessage ;"
		                         "  fts:match \"%s\" . "
		                         "} "
		                         "ORDER BY ASC(nie:title(?feed)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         fts,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT ?feed nie:title(?feed) "
		                         "WHERE {"
		                         "  ?feed a mfo:FeedMessage ."
		                         "} "
		                         "ORDER BY ASC(nie:title(?feed)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         search_offset,
		                         search_limit);
	}

	g_free (fts);

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
		         _("No feeds were found"));
	} else {
		g_print (g_dngettext (NULL,
		                      "Feed: %d",
		                      "Feeds: %d",
		                      results->len),
		         results->len);
		g_print ("\n");

		g_ptr_array_foreach (results,
		                     get_feed_foreach,
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
           GStrv          search_terms,
           gboolean       show_all,
           gint           search_offset,
           gint           search_limit,
	   gboolean       use_or_operator,
	   gboolean       details)
{
	gchar *fts;
	gchar *query;
	const gchar *show_all_str;
	gboolean success;

	show_all_str = show_all ? "" : "?u tracker:available true . ";
	fts = get_fts_string (search_terms, use_or_operator);

	if (fts) {
		query = g_strdup_printf ("SELECT ?u nie:url(?u) "
		                         "WHERE { "
		                         "  ?u a nie:InformationElement ;"
		                         "  fts:match \"%s\" ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nie:url(?u)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         fts,
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT ?u nie:url(?u) "
		                         "WHERE { "
		                         "  ?u a nie:InformationElement ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nie:url(?u)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	}

	success = get_files_results (client, query, search_limit, details);
	g_free (query);
	g_free (fts);

	return success;
}

static gboolean
get_folders (TrackerClient *client,
             GStrv          search_terms,
             gboolean       show_all,
             gint           search_offset,
             gint           search_limit,
	     gboolean       use_or_operator,
	     gboolean       details)
{
	gchar *fts;
	gchar *query;
	const gchar *show_all_str;
	gboolean success;

	show_all_str = show_all ? "" : "?u tracker:available true . ";
	fts = get_fts_string (search_terms, use_or_operator);

	if (fts) {
		query = g_strdup_printf ("SELECT ?u nie:url(?u) "
		                         "WHERE { "
		                         "  ?u a nfo:Folder ;"
		                         "  fts:match \"%s\" ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nie:url(?u)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         fts,
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT ?u nie:url(?u) "
		                         "WHERE { "
		                         "  ?u a nfo:Folder ."
		                         "  %s"
		                         "} "
		                         "ORDER BY ASC(nie:url(?u)) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	}

	success = get_files_results (client, query, search_limit, details);
	g_free (query);
	g_free (fts);

	return success;
}

static void
get_all_by_search_foreach (gpointer value,
                           gpointer user_data)
{
	gchar **metadata;
	gboolean details;

	metadata = value;
	details = GPOINTER_TO_INT (user_data);

	if (!metadata || !*metadata) {
		return;
	}

	if (G_LIKELY (!details)) {
		gchar **p;
		gint i;

		for (p = metadata, i = 0; *p; p++, i++) {
			if (i == 0) {
				g_print ("  %s", *p);
			}
		}
	} else {
		const gchar *urn;
		const gchar *mime_type;
		const gchar *class;

		urn = metadata[0];
		mime_type = metadata[1];
		class = metadata[2];

		if (mime_type && mime_type[0] == '\0') {
			mime_type = NULL;
		}

		if (mime_type) {
			g_print ("  %s\n"
			         "    %s\n"
			         "    %s\n",
			         urn,
			         mime_type,
			         class);
		} else {
			g_print ("  %s\n"
			         "    %s\n",
			         urn,
			         class);
		}
	}

	g_print ("\n");
}

static gboolean
get_all_by_search (TrackerClient *client,
                   GStrv          search_words,
                   gboolean       show_all,
                   gint           search_offset,
                   gint           search_limit,
                   gboolean       use_or_operator,
                   gboolean       details)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *fts;
	gchar *query;
	const gchar *show_all_str;

	fts = get_fts_string (search_words, use_or_operator);
	if (!fts) {
		return FALSE;
	}

	show_all_str = show_all ? "" : "?s tracker:available true . ";

	if (details) {
		query = g_strdup_printf ("SELECT tracker:coalesce (nie:url (?s), ?s) nie:mimeType (?s) ?type "
		                         "WHERE {"
		                         "  ?s fts:match \"%s\" ;"
		                         "  rdf:type ?type ."
		                         "  %s"
		                         "} "
		                         "GROUP BY nie:url(?s) "
		                         "ORDER BY nie:url(?s) "
		                         "OFFSET %d LIMIT %d",
		                         fts,
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT tracker:coalesce (nie:url (?s), ?s) "
		                         "WHERE {"
		                         "  ?s fts:match \"%s\" ."
		                         "  %s"
		                         "} "
		                         "ORDER BY nie:url(?s) "
		                         "OFFSET %d LIMIT %d",
		                         fts,
		                         show_all_str,
		                         search_offset,
		                         search_limit);
	}

	g_free (fts);

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
		g_print (g_dngettext (NULL,
		                      "Result: %d",
		                      "Results: %d",
		                      results->len),
		         results->len);
		g_print ("\n");

		g_ptr_array_foreach (results,
		                     get_all_by_search_foreach,
		                     GINT_TO_POINTER (details));

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

	if (print_version) {
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		g_option_context_free (context);

		return EXIT_SUCCESS;
	}

	if (!music_albums && !music_artists && !music_files &&
	    !feeds &&
	    !image_files &&
	    !video_files &&
	    !document_files &&
	    !emails &&
	    !contacts &&
	    !files && !folders &&
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

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}


#if HAVE_TRACKER_FTS
	/* Only check stopwords if FTS is enabled */
	if (terms) {
		TrackerLanguage *language;
		gboolean stop_words_found;
		gchar **p;

		/* Check terms don't have additional quotes */
		for (p = terms; *p; p++) {
			gchar *term = *p;
			gint end = strlen (term) - 1;

			if ((term[0] == '"' && term[end] == '"') ||
			    (term[0] == '\'' && term[end] == '\'')) {
				/* We never have a quote JUST at the end */
				term[0] = term[end] = ' ';
				g_strstrip (term);
			}
		}

		/* Check if terms are stopwords, and warn if so */
		language = tracker_language_new (NULL);
		stop_words_found = FALSE;
		for (p = terms; *p; p++) {
			gchar *down;

			down = g_utf8_strdown (*p, -1);

			if (tracker_language_is_stop_word (language, down)) {
				g_printerr (_("Search term '%s' is a stop word."),
				            down);
				g_printerr ("\n");

				stop_words_found = TRUE;
			}

			g_free (down);
		}

		if (stop_words_found) {
			g_printerr (_("Stop words are common words which "
			              "may be ignored during the indexing "
			              "process."));
			g_printerr ("\n\n");
		}

		g_object_unref (language);
	}
#endif

	g_option_context_free (context);

	client = tracker_client_new (0, G_MAXINT);

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

		success = get_files (client, terms, all, offset, limit, or_operator, detailed);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (folders) {
		gboolean success;

		success = get_folders (client, terms, all, offset, limit, or_operator, detailed);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (music_albums) {
		gboolean success;

		success = get_music_albums (client, terms, offset, limit, or_operator);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (music_artists) {
		gboolean success;

		success = get_music_artists (client, terms, offset, limit, or_operator);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (music_files) {
		gboolean success;

		success = get_music_files (client, terms, all, offset, limit, or_operator, detailed);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (feeds) {
		gboolean success;

		success = get_feeds (client, terms, offset, limit, or_operator);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (image_files) {
		gboolean success;

		success = get_image_files (client, terms, all, offset, limit, or_operator, detailed);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (video_files) {
		gboolean success;

		success = get_video_files (client, terms, all, offset, limit, or_operator, detailed);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (document_files) {
		gboolean success;

		success = get_document_files (client, terms, all, offset, limit, or_operator, detailed);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (emails) {
		gboolean success;

		success = get_emails (client, terms, all, offset, limit, or_operator, detailed);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (contacts) {
		gboolean success;

		success = get_contacts (client, terms, all, offset, limit, or_operator, detailed);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (terms) {
		gboolean success;

		success = get_all_by_search (client, terms, all, offset, limit, or_operator, detailed);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	g_object_unref (client);

	return EXIT_SUCCESS;
}
