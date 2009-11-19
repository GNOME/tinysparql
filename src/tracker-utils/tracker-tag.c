/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia
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
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libtracker-client/tracker.h>
#include <libtracker-common/tracker-common.h>

static gint	     limit = 512;
static gint	     offset;
static gchar	   **files;
static gboolean      or_operator;
static gchar	    *add_tag;
static gchar	    *remove_tag;
static gboolean     *list;
static gboolean      show_files;
static gboolean      print_version;

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
	{ "list", 't', 0, G_OPTION_ARG_NONE, &list,
	  N_("List all tags (using FILTER if specified)"),
	  NULL,
	},
	{ "show-files", 's', 0, G_OPTION_ARG_NONE, &show_files,
	  N_("Show files associated with each tag (this is only used with --list)"),
	  NULL
	},
	{ "add", 'a', 0, G_OPTION_ARG_STRING, &add_tag,
	  N_("Add a tag (if FILEs are omitted, TAG is not associated with any files)"),
	  N_("TAG")
	},
	{ "delete", 'd', 0, G_OPTION_ARG_STRING, &remove_tag,
	  N_("Delete a tag (if FILEs are omitted, TAG is removed for all files)"),
	  N_("TAG")
	},
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
	  NULL
	},
	{ G_OPTION_REMAINING, 0, 
	  G_OPTION_FLAG_FILENAME,
	  G_OPTION_ARG_STRING_ARRAY, &files,
	  N_("FILE…"),
	  N_("FILE [FILE…]")},
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
get_escaped_sparql_string (const gchar *str)
{
	GString *sparql;

	sparql = g_string_new ("");
        g_string_append_c (sparql, '"');

        while (*str != '\0') {
                gsize len = strcspn (str, "\t\n\r\"\\");
                g_string_append_len (sparql, str, len);
                str += len;
                switch (*str) {
                case '\t':
                        g_string_append (sparql, "\\t");
                        break;
                case '\n':
                        g_string_append (sparql, "\\n");
                        break;
                case '\r':
                        g_string_append (sparql, "\\r");
                        break;
                case '"':
                        g_string_append (sparql, "\\\"");
                        break;
                case '\\':
                        g_string_append (sparql, "\\\\");
                        break;
                default:
                        continue;
                }
                str++;
        }

        g_string_append_c (sparql, '"');

	return g_string_free (sparql, FALSE);
}

static gchar *
get_fts_string (GStrv    search_words,
		gboolean use_or_operator,
		gboolean for_regex)
{
	GString *fts;
	gint i, len;

	if (!search_words) {
		return NULL;
	}

	len = g_strv_length (search_words);
	fts = g_string_new ("");

	for (i = 0; i < len; i++) {
		g_string_append (fts, search_words[i]);
		g_string_append_c (fts, '*');

		if (i < len - 1) { 
			if (for_regex) {
				if (use_or_operator) {
					g_string_append (fts, " || ");
				} else {
					g_string_append (fts, " && ");
				}
			} else {
				if (use_or_operator) {
					g_string_append (fts, " OR ");
				} else {
					g_string_append (fts, " ");
				}
			}
		}
	}

	return g_string_free (fts, FALSE);
}

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
get_all_tags_foreach (gpointer value,
		      gpointer user_data)
{
	TrackerClient *client;
	GError *error = NULL;
	GPtrArray *results;
	GStrv data;
	const gchar *id;
	const gchar *tag;
	gchar *query;
	gint files, i;

	data = value;
	client = user_data;

	files = atoi (data[2]);
	id = data[0];
	tag = data[1];

	g_print ("  %s\n", tag);
	
	g_print ("    %s\n", id);

	g_print ("    ");
	g_print (tracker_dngettext (NULL,
				    "%d file", 
				    "%d files",
				    files),
		 files);
	g_print ("\n");

	if (!client || files < 1) {
		return;
	}

	/* Get files associated */
	query = g_strdup_printf ("SELECT ?urn WHERE {"
				 "  ?urn a rdfs:Resource; "
				 "  nao:hasTag \"%s\" "
				 "}",
				 id);

	results = tracker_resources_sparql_query (client, query, &error);
	g_free (query);

	if (error) {
		g_print ("    %s, %s\n", 
			 _("Could not get files related to tag"),
			 error->message);
		g_error_free (error);
		return;
	}

	for (i = 0; i < results->len; i++) {
		GStrv files;
		
		files = g_ptr_array_index (results, i);
		g_print ("    %s\n", files[0]);
		g_strfreev (files);
	}

	g_ptr_array_free (results, TRUE);
}

static gboolean
get_all_tags (TrackerClient *client,
	      GStrv          files,
	      gint           search_offset,
	      gint           search_limit,
	      gboolean       use_or_operator,
	      gboolean       show_files)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *fts;
	gchar *query;

	fts = get_fts_string (files, use_or_operator, TRUE);

	if (fts) {
		query = g_strdup_printf ("SELECT ?tag ?label COUNT(?urns) AS urns "
					 "WHERE {" 
					 "  ?tag a nao:Tag ;"
					 "  nao:prefLabel ?label ."
					 "  OPTIONAL {"
					 "     ?urns nao:hasTag ?tag"
					 "  } ."
					 "  FILTER regex (?label, \"%s\")"
					 "} "
					 "GROUP BY ?tag "
					 "ORDER BY ASC(?label) "
					 "OFFSET %d "
					 "LIMIT %d",
					 fts,
					 search_offset, 
					 search_limit);
	} else {
		query = g_strdup_printf ("SELECT ?tag ?label COUNT(?urns) AS urns "
					 "WHERE {" 
					 "  ?tag a nao:Tag ;"
					 "  nao:prefLabel ?label ."
					 "  OPTIONAL {"
					 "     ?urns nao:hasTag ?tag"
					 "  }"
					 "} "
					 "GROUP BY ?tag "
					 "ORDER BY ASC(?label) "
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
			    _("Could not get all tags"),
			    error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!results) {
		g_print ("%s\n",
			 _("No tags were found"));
	} else {
		g_print (tracker_dngettext (NULL,
					    "Tag: %d (shown by name)", 
					    "Tags: %d (shown by name)",
					    results->len),
			 results->len);
		g_print ("\n");

		g_ptr_array_foreach (results, 
				     get_all_tags_foreach,
				     show_files ? client : NULL);

		if (results->len >= search_limit) {
			show_limit_warning ();
		}

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}
	
	return TRUE;
}

static gboolean
add_tag_for_urns (TrackerClient *client,
		  GStrv          files,
		  const gchar   *tag)
{
	GError *error = NULL;
	gchar *filter;
	gchar *tag_escaped;
	gchar *query;

	tag_escaped = get_escaped_sparql_string (tag);
	filter = get_filter_string (files, NULL);

	/* First we check if the tag is already set and only add if it
	 * is, then we add the urns specified to the new tag.
	 */
	if (filter) {
		/* Add tag to specific urns */
		query = g_strdup_printf ("INSERT { "
					 "  _:tag a nao:Tag;"
					 "  nao:prefLabel %s ."
					 "} "
					 "WHERE {"
					 "  OPTIONAL {"
					 "     ?tag a nao:Tag ;"
					 "     nao:prefLabel %s"
					 "  } ."
					 "  FILTER (!bound(?tag)) "
					 "} "
					 "INSERT { "
					 "  ?urn nao:hasTag ?id "
					 "} "
					 "WHERE {"
					 "  ?urn nie:isStoredAs ?f ."
					 "  ?id nao:prefLabel %s "
					 "  %s "
					 "}",
					 tag_escaped,
					 tag_escaped,
					 tag_escaped,
					 filter);
	} else {
		/* Add tag and do not link it to urns */
		query = g_strdup_printf ("INSERT { "
					 "  _:tag a nao:Tag;"
					 "  nao:prefLabel %s ."
					 "} "
					 "WHERE {"
					 "  OPTIONAL {"
					 "     ?tag a nao:Tag ;"
					 "     nao:prefLabel %s"
					 "  } ."
					 "  FILTER (!bound(?tag)) "
					 "}",
					 tag_escaped,
					 tag_escaped);
	}

	g_free (tag_escaped);
	g_free (filter);

	tracker_resources_sparql_update (client, query, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not add tag"),
			    error->message);
		g_error_free (error);

		return FALSE;
	}

	g_print ("%s\n",
		 _("Tag was added successfully"));

	return TRUE;
}

static gboolean
remove_tag_for_urns (TrackerClient *client,
		     GStrv          files,
		     const gchar   *tag)
{
	GError *error = NULL;
	gchar *tag_escaped;
	gchar *query;

	tag_escaped = get_escaped_sparql_string (tag);

	if (files && *files) {
		GPtrArray *results;
		gchar *filter;
		const gchar *urn;

		/* Get all tags urns */
		query = g_strdup_printf ("SELECT ?tag "
					 "WHERE {"
					 "  ?tag a nao:Tag ."
					 "  ?tag nao:prefLabel %s "
					 "}",
					 tag_escaped);

		results = tracker_resources_sparql_query (client, query, &error);
		g_free (query);

		if (error) {
			g_printerr ("%s, %s\n",
				    _("Could not get tag by label"),
				    error->message);
			g_error_free (error);
			g_free (tag_escaped);

			return FALSE;
		}
		
		if (!results || !results->pdata || !results->pdata[0]) {
			g_print ("%s\n",
				 _("No tags were found by that name"));

			g_free (tag_escaped);

			return TRUE;
		}

		urn = * (GStrv) results->pdata[0];
		filter = get_filter_string (files, urn);

		query = g_strdup_printf ("DELETE { "
					 "  ?f nao:hasTag ?t "
					 "} "
					 "WHERE { "
					 "  ?f nao:hasTag ?t ."
					 "  %s "
					 "}",
					 filter);
		g_free (filter);
#if 0
		filter = get_filter_string (files);

		query = g_strdup_printf ("DELETE {" 
					 "  <%s> nao:hasTag ?tag "
					 "} "
					 "WHERE {"
					 "  <%s> nao:hasTag ?tag"
					 "  FILTER (?tag = <%s>)"
					 "}",
					 files[0],
					 files[0],
					 urn);

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
#endif
	} else {
		/* Remove tag completely */
		query = g_strdup_printf ("DELETE { "
					 "  ?tag a nao:Tag "
					 "} "
					 "WHERE {"
					 "  ?tag nao:prefLabel %s "
					 "}",
					 tag_escaped);
	}

	g_free (tag_escaped);

	tracker_resources_sparql_update (client, query, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not remove tag"),
			    error->message);
		g_error_free (error);

		return FALSE;
	}

	g_print ("%s\n",
		 _("Tag was removed successfully"));

	return TRUE;
}

static void
get_tags_by_file_foreach (gpointer value,
			  gpointer user_data)
{
	GStrv data = value;

	g_print ("  %s\n", data[1]);
}

static gboolean 
get_tags_by_file (TrackerClient *client, 
		  const gchar   *file)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *query;

	query = g_strdup_printf ("SELECT ?tags ?labels "
				 "WHERE {"
				 "  <%s>"
				 "  nao:hasTag ?tags ."
				 "  ?tags a nao:Tag ;"
				 "  nao:prefLabel ?labels "
				 "} "
				 "ORDER BY ASC(?labels)",
				 file);

	results = tracker_resources_sparql_query (client, query, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
			    _("Could not get all tags"),
			    error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!results || results->len < 1) {
		g_print ("  %s\n",
			 _("No tags were found"));
	} else {
		g_ptr_array_foreach (results, 
				     get_tags_by_file_foreach,
				     NULL);

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
	}
	
	return TRUE;
}

int
main (int argc, char **argv)
{
	TrackerClient	*client;
	GOptionContext	*context;
	const gchar	*failed = NULL;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Translators: this messagge will apper immediately after the
	 * usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>
	 */
	context = g_option_context_new (_("Add, remove or list tags"));

	/* Translators: this message will appear after the usage string
	 * and before the list of options, showing an usage example.
	 */
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (print_version) {
		g_print ("%s\n", PACKAGE_STRING);
		g_option_context_free (context);

		return EXIT_SUCCESS;
	}

	if (!list && show_files) {
		failed = _("The --list option is required for --show-files");
	} else if (add_tag && remove_tag) {
		failed = _("Add and delete actions can not be used together");
	} else if (!list && !add_tag && !remove_tag && !files) {
		failed = _("No arguments were provided");
	}

	if (failed) {
		gchar *help;

		g_printerr ("%s\n\n", failed);

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	client = tracker_connect (FALSE, G_MAXINT);

	if (!client) {
		g_printerr ("%s\n",
			    _("Could not establish a D-Bus connection to Tracker"));
		return EXIT_FAILURE;
	}

	if (list) {
		gboolean success;

		success = get_all_tags (client, files, offset, limit, or_operator, show_files);
		tracker_disconnect (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	
	if (add_tag) {
		gboolean success;

		success = add_tag_for_urns (client, files, add_tag);
		tracker_disconnect (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (remove_tag) {
		gboolean success;

		success = remove_tag_for_urns (client, files, remove_tag);
		tracker_disconnect (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (files) {
		gboolean success = TRUE;
		gchar **p;

		for (p = files; *p; p++) {
			g_print ("<%s>\n", *p);
			success &= get_tags_by_file (client, *p);
			g_print ("\n");
		}

		tracker_disconnect (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	tracker_disconnect (client);

	/* This is a failure because we should have done something.
	 * This code should never be reached in practise. 
	 */ 
	return EXIT_FAILURE;
}
