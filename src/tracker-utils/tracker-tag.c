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
#include <time.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

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
static gchar **files;
static gboolean or_operator;
static gchar *add_tag;
static gchar *remove_tag;
static gchar *description;
static gboolean *list;
static gboolean show_files;
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
	{ "description", 'e', 0, G_OPTION_ARG_STRING, &description,
	  N_("Description for a tag (this is only used with --add)"),
	  N_("STRING")
	},
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
	  N_("Print version"),
	  NULL
	},
	{ G_OPTION_REMAINING, 0,
	  G_OPTION_FLAG_FILENAME,
	  G_OPTION_ARG_FILENAME_ARRAY, &files,
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
                gboolean for_regex,
                gboolean use_asterisk)
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

                if (use_asterisk) {
                        g_string_append_c (fts, '*');
                }

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
                   gboolean     files_are_urns,
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
		if (files_are_urns) {
			g_string_append_printf (filter, "?urn = <%s>", files[i]);
		} else {
			g_string_append_printf (filter, "?f = \"%s\"", files[i]);
		}

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

static GStrv
get_uris (GStrv files)
{
	GStrv uris;
	gint len, i;

	if (!files) {
		return NULL;
	}

	len = g_strv_length (files);

	if (len < 1) {
		return NULL;
	}

	uris = g_new0 (gchar *, len + 1);

	for (i = 0; files[i]; i++) {
		GFile *file;

		file = g_file_new_for_commandline_arg (files[i]);
		uris[i] = g_file_get_uri (file);
		g_object_unref (file);
	}

	return uris;
}

static GPtrArray *
get_file_urns (TrackerClient *client,
	       GStrv          uris,
	       const gchar   *tag)
{
	GPtrArray *results;
	gchar *query, *filter;
	GError *error = NULL;

	filter = get_filter_string (uris, FALSE, tag);
	query = g_strdup_printf ("SELECT ?urn ?f "
	                         "WHERE { "
	                         "  ?urn "
	                         "    %s "
	                         "    nie:url ?f . "
	                         "  %s "
	                         "}",
	                         tag ? "nao:hasTag ?t ; " : "",
	                         filter);

	results = tracker_resources_sparql_query (client, query, &error);

	g_free (query);
	g_free (filter);

	if (error) {
		g_print ("    %s, %s\n",
		         _("Could not get file URNs"),
		         error->message);
		g_error_free (error);
		return NULL;
	}

	return results;
}

static GStrv
result_to_strv (GPtrArray *result,
                gint       n_col)
{
	GStrv strv;
	gint i;

	if (!result || result->len == 0) {
		return NULL;
	}

	strv = g_new0 (gchar *, result->len + 1);

	for (i = 0; i < result->len; i++) {
		gchar **row;

		row = g_ptr_array_index (result, i);
		strv[i] = g_strdup (row[n_col]);
	}

	return strv;
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
	const gchar *description;
	gchar *query;
	gint files, i;

	data = value;
	client = user_data;

	files = atoi (data[3]);
	id = data[0];
	tag = data[1];
	description = data[2];

	if (description && *description == '\0') {
		description = NULL;
	}

	g_print ("  %s %s%s%s\n", 
		 tag,
		 description ? "(" : "",
		 description ? description : "",
		 description ? ")" : "");

	g_print ("    %s\n", id);

	g_print ("    ");
	g_print (g_dngettext (NULL,
	                      "%d file",
	                      "%d files",
	                      files),
	         files);
	g_print ("\n");

	if (!client || files < 1) {
		return;
	}

	/* Get files associated */
	query = g_strdup_printf ("SELECT ?uri WHERE {"
	                         "  ?urn a rdfs:Resource; "
				 "  nie:url ?uri ; "
	                         "  nao:hasTag \"%s\" . "
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

	fts = get_fts_string (files, use_or_operator, TRUE, FALSE);

	if (fts) {
		query = g_strdup_printf ("SELECT ?tag ?label nao:description(?tag) COUNT(?urns) AS urns "
		                         "WHERE {"
		                         "  ?tag a nao:Tag ;"
		                         "  nao:prefLabel ?label ."
		                         "  OPTIONAL {"
		                         "     ?urns nao:hasTag ?tag"
		                         "  } ."
		                         "  FILTER (?label = \"%s\")"
		                         "} "
		                         "GROUP BY ?tag "
		                         "ORDER BY ASC(?label) "
		                         "OFFSET %d "
		                         "LIMIT %d",
		                         fts,
		                         search_offset,
		                         search_limit);
	} else {
		query = g_strdup_printf ("SELECT ?tag ?label nao:description(?tag) COUNT(?urns) AS urns "
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
		g_print (g_dngettext (NULL,
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

static void
print_file_report (GPtrArray   *urns,
                   GStrv        uris,
                   const gchar *found_msg,
                   const gchar *not_found_msg)
{
	gint i, j;

	if (!urns || !uris) {
		g_print ("  No files were modified.\n");
		return;
	}

	for (i = 0; uris[i]; i++) {
		gboolean found = FALSE;

		for (j = 0; j < urns->len; j++) {
			gchar **row;

			row = g_ptr_array_index (urns, j);

			if (g_strcmp0 (row[1], uris[i]) == 0) {
				found = TRUE;
				break;
			}
		}

		g_print ("  %s: %s\n",
		         found ? found_msg : not_found_msg,
		         uris[i]);
	}
}

static gboolean
add_tag_for_urns (TrackerClient *client,
                  GStrv          files,
                  const gchar   *tag,
		  const gchar   *description)
{
	GPtrArray *urns = NULL;
	GError *error = NULL;
	GStrv  uris;
	gchar *tag_escaped;
	gchar *query;

	tag_escaped = get_escaped_sparql_string (tag);

	if (description) {
		gchar *description_escaped;

		description_escaped = get_escaped_sparql_string (description);

		query = g_strdup_printf ("INSERT { "
					 "  _:tag a nao:Tag;"
					 "  nao:prefLabel %s ;"
					 "  nao:description %s ."
					 "} "
					 "WHERE {"
					 "  OPTIONAL {"
					 "     ?tag a nao:Tag ;"
					 "     nao:prefLabel %s ."
					 "  } ."
					 "  FILTER (!bound(?tag)) "
					 "}",
					 tag_escaped,
					 description_escaped,
					 tag_escaped);

		g_free (description_escaped);
	} else {
		query = g_strdup_printf ("INSERT { "
					 "  _:tag a nao:Tag;"
					 "  nao:prefLabel %s ."
					 "} "
					 "WHERE {"
					 "  OPTIONAL {"
					 "     ?tag a nao:Tag ;"
					 "     nao:prefLabel %s ."
					 "  } ."
					 "  FILTER (!bound(?tag)) "
					 "}",
					 tag_escaped,
					 tag_escaped);
	}

	tracker_resources_sparql_update (client, query, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not add tag"),
		            error->message);
		g_error_free (error);
		g_free (tag_escaped);

		return FALSE;
	}

	g_print ("%s\n",
	         _("Tag was added successfully"));

	uris = get_uris (files);

	if (!uris) {
		/* No URIs to tag */
		g_free (tag_escaped);
		return TRUE;
	}

	urns = get_file_urns (client, uris, NULL);

	/* First we check if the tag is already set and only add if it
	 * is, then we add the urns specified to the new tag.
	 */
	if (urns && urns->len > 0) {
		GStrv urns_strv;
		gchar *filter;

		urns_strv = result_to_strv (urns, 0);
		filter = get_filter_string (urns_strv, TRUE, NULL);

		/* Add tag to specific urns */
		query = g_strdup_printf ("INSERT { "
		                         "  ?urn nao:hasTag ?id "
		                         "} "
		                         "WHERE {"
		                         "  ?urn nie:url ?f ."
		                         "  ?id nao:prefLabel %s "
		                         "  %s "
		                         "}",
		                         tag_escaped,
		                         filter);

		tracker_resources_sparql_update (client, query, &error);
		g_strfreev (urns_strv);
		g_free (filter);
		g_free (query);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not add tag to files"),
			            error->message);
			g_error_free (error);
			g_free (tag_escaped);

			return FALSE;
		}
	}

	if (urns) {
		print_file_report (urns, uris,
		                   _("Tagged"),
		                   _("Not tagged, file is not indexed"));

		g_ptr_array_foreach (urns, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (urns, TRUE);
	}

	g_strfreev (uris);
	g_free (tag_escaped);

	return TRUE;
}

static gboolean
remove_tag_for_urns (TrackerClient *client,
                     GStrv          files,
                     const gchar   *tag)
{
	GPtrArray *urns = NULL;
	GError *error = NULL;
	gchar *tag_escaped;
	gchar *query;
	GStrv uris;

	tag_escaped = get_escaped_sparql_string (tag);
	uris = get_uris (files);

	if (uris && *uris) {
		GPtrArray *results;
		gchar *filter;
		const gchar *urn;
		GStrv urns_strv;

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
			g_strfreev (uris);

			return FALSE;
		}

		if (!results || !results->pdata || !results->pdata[0]) {
			g_print ("%s\n",
			         _("No tags were found by that name"));

			g_free (tag_escaped);
			g_strfreev (uris);

			return TRUE;
		}

		urn = * (GStrv) results->pdata[0];
		urns = get_file_urns (client, uris, urn);

		if (!urns || urns->len == 0) {
			g_print ("%s\n",
			         _("None of the files had this tag set"));

			g_strfreev (uris);
			g_free (tag_escaped);

			return TRUE;
		}

		urns_strv = result_to_strv (urns, 0);
		filter = get_filter_string (urns_strv, TRUE, urn);

		query = g_strdup_printf ("DELETE { "
		                         "  ?urn nao:hasTag ?t "
		                         "} "
		                         "WHERE { "
		                         "  ?urn nao:hasTag ?t . "
		                         "  %s "
		                         "}",
		                         filter);
		g_free (filter);
		g_strfreev (urns_strv);

		g_ptr_array_foreach (results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (results, TRUE);
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
		g_strfreev (uris);

		return FALSE;
	}

	g_print ("%s\n",
	         _("Tag was removed successfully"));

	if (urns) {
		print_file_report (urns, uris,
		                   _("Untagged"),
		                   _("File not indexed or already untagged"));

		g_ptr_array_foreach (urns, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (urns, TRUE);
	}

	g_strfreev (uris);

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
                  const gchar   *uri)
{
	GError *error = NULL;
	GPtrArray *results;
	gchar *query;

	query = g_strdup_printf ("SELECT ?tags ?labels "
	                         "WHERE {"
	                         "  ?urn nao:hasTag ?tags ;"
	                         "  nie:url <%s> ."
	                         "  ?tags a nao:Tag ;"
	                         "  nao:prefLabel ?labels "
	                         "} "
	                         "ORDER BY ASC(?labels)",
	                         uri);

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
	TrackerClient   *client;
	GOptionContext  *context;
	const gchar     *failed = NULL;

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
		g_print ("\n" ABOUT "\n" LICENSE "\n");
		g_option_context_free (context);

		return EXIT_SUCCESS;
	}

	if (!list && show_files) {
		failed = _("The --list option is required for --show-files");
	} else if (add_tag && remove_tag) {
		failed = _("Add and delete actions can not be used together");
	} else if (!list && !add_tag && !remove_tag && !files) {
		failed = _("No arguments were provided");
	} else if (description && !add_tag) {
		failed = _("The --description option can only be used with --add");
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

	client = tracker_client_new (0, G_MAXINT);

	if (!client) {
		g_printerr ("%s\n",
		            _("Could not establish a D-Bus connection to Tracker"));
		return EXIT_FAILURE;
	}

	if (list) {
		gboolean success;

		success = get_all_tags (client, files, offset, limit, or_operator, show_files);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (add_tag) {
		gboolean success;

		success = add_tag_for_urns (client, files, add_tag, description);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (remove_tag) {
		gboolean success;

		success = remove_tag_for_urns (client, files, remove_tag);
		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (files) {
		gboolean success = TRUE;
		gchar **p;

		for (p = files; *p; p++) {
			GFile *file;
			gchar *uri;
			
			file = g_file_new_for_commandline_arg (*p);
			uri = g_file_get_uri (file);
			g_object_unref (file);

			g_print ("%s\n", uri);
			success &= get_tags_by_file (client, uri);
			g_print ("\n");

			g_free (uri);
		}

		g_object_unref (client);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	g_object_unref (client);

	/* This is a failure because we should have done something.
	 * This code should never be reached in practise.
	 */
	return EXIT_FAILURE;
}
