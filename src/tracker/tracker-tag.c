/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2014, Lanedo <martyn@lanedo.com>
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

#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-tag.h"

#define TAG_OPTIONS_ENABLED() \
	(resources || \
	 add_tag || \
	 remove_tag || \
	 list)

static gint limit = 512;
static gint offset;
static gchar **resources;
static gboolean and_operator;
static gchar *add_tag;
static gchar *remove_tag;
static gchar *description;
static gboolean *list;
static gboolean show_resources;

static GOptionEntry entries[] = {
	{ "list", 't', 0, G_OPTION_ARG_NONE, &list,
	  N_("List all tags (using FILTER if specified; FILTER always uses logical OR)"),
	  N_("FILTER"),
	},
	{ "show-files", 's', 0, G_OPTION_ARG_NONE, &show_resources,
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
	{ "limit", 'l', 0, G_OPTION_ARG_INT, &limit,
	  N_("Limit the number of results shown"),
	  "512"
	},
	{ "offset", 'o', 0, G_OPTION_ARG_INT, &offset,
	  N_("Offset the results"),
	  "0"
	},
	{ "and-operator", 'n', 0, G_OPTION_ARG_NONE, &and_operator,
	  N_("Use AND for search terms instead of OR (the default)"),
	  NULL
	},
	{ G_OPTION_REMAINING, 0, 0,
	  G_OPTION_ARG_FILENAME_ARRAY, &resources,
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
get_filter_string (GStrv        resources,
                   const gchar *subject,
                   gboolean     resources_are_urns,
                   const gchar *tag)
{
	GString *filter;
	gint i, len;

	if (!resources) {
		return NULL;
	}

	len = g_strv_length (resources);

	if (len < 1) {
		return NULL;
	}

	filter = g_string_new ("");

	g_string_append_printf (filter, "FILTER (");

	if (tag) {
		g_string_append (filter, "(");
	}

	for (i = 0; i < len; i++) {
		g_string_append_printf (filter, "%s = %s%s%s",
		                        subject,
		                        resources_are_urns ? "<" : "\"",
		                        resources[i],
		                        resources_are_urns ? ">" : "\"");

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
get_uris (GStrv resources)
{
	GStrv uris;
	gint len, i;

	if (!resources) {
		return NULL;
	}

	len = g_strv_length (resources);

	if (len < 1) {
		return NULL;
	}

	uris = g_new0 (gchar *, len + 1);

	for (i = 0; resources[i]; i++) {
		GFile *file;

		file = g_file_new_for_commandline_arg (resources[i]);
		uris[i] = g_file_get_uri (file);
		g_object_unref (file);
	}

	return uris;
}

static TrackerSparqlCursor *
get_file_urns (TrackerSparqlConnection *connection,
	       GStrv                    uris,
	       const gchar             *tag)
{
	TrackerSparqlCursor *cursor;
	gchar *query, *filter;
	GError *error = NULL;

	filter = get_filter_string (uris, "?f", FALSE, tag);
	query = g_strdup_printf ("SELECT ?urn ?f "
	                         "WHERE { "
	                         "  ?urn "
	                         "    %s "
	                         "    nie:url ?f . "
	                         "  %s "
	                         "}",
	                         tag ? "nao:hasTag ?t ; " : "",
	                         filter ? filter : "");

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);

	g_free (query);
	g_free (filter);

	if (error) {
		g_print ("    %s, %s\n",
		         _("Could not get file URNs"),
		         error->message);
		g_error_free (error);
		return NULL;
	}

	return cursor;
}

static GStrv
result_to_strv (TrackerSparqlCursor *cursor,
                gint                 n_col)
{
	GStrv strv;
	gint count, i;

	if (!cursor) {
		return NULL;
	}

	i = 0;
	count = 0;

	/* Really no other option here, but we iterate the cursor
	 * first to get the length.
	 */
	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		count++;
	}

	strv = g_new0 (gchar *, count + 1);

	tracker_sparql_cursor_rewind (cursor);

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		const gchar *str;

		str = tracker_sparql_cursor_get_string (cursor, n_col, NULL);
		strv[i++] = g_strdup (str);
	}

	return strv;
}

static void
get_all_tags_show_tag_id (TrackerSparqlConnection *connection,
                          const gchar             *id)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gchar *query;

	/* Get resources associated */
	query = g_strdup_printf ("SELECT ?uri WHERE {"
	                         "  ?urn a rdfs:Resource; "
	                         "  nie:url ?uri ; "
	                         "  nao:hasTag \"%s\" . "
	                         "}",
	                         id);

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);
	g_free (query);

	if (error) {
		g_printerr ("    %s, %s\n",
		            _("Could not get files related to tag"),
		            error->message);
		g_error_free (error);
		return;
	}

	if (!cursor) {
		/* To translators: This is to say there are no
		 * tags found with a particular unique ID. */
		g_print ("    %s\n", _("None"));
		return;
	}


	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		g_print ("    %s\n", tracker_sparql_cursor_get_string (cursor, 0, NULL));
	}

	g_object_unref (cursor);
}

static inline gchar *
get_filter_in_for_strv (GStrv        resources,
                        const gchar *subject)
{
	gchar *filter, *filter_in;

	/* e.g. '?label IN ("foo", "bar")' */
	filter_in = g_strjoinv ("\",\"", resources);
	filter = g_strdup_printf ("FILTER (%s IN (\"%s\"))", subject, filter_in);
	g_free (filter_in);

	return filter;
}

static gboolean
get_all_resources_with_tags (TrackerSparqlConnection *connection,
                             GStrv                    tags,
                             gint                     search_offset,
                             gint                     search_limit)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	GStrv tag_urns, p;
	GString *s;
	gchar *filter, *query;

	if (!tags) {
		return FALSE;
	}

	/* First, get matching tags */
	filter = get_filter_in_for_strv (tags, "?label");
	query = g_strdup_printf ("SELECT ?t "
	                         "WHERE { "
	                         "  ?t a nao:Tag ;"
	                         "     nao:prefLabel ?label ."
	                         "  %s"
	                         "}",
	                         filter);
	g_free (filter);

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not get all tags in the database"),
		            error->message);
		g_error_free (error);

		return FALSE;
	}

	tag_urns = result_to_strv (cursor, 0);
	if (!tag_urns) {
		g_print ("%s\n",
		         _("No files have been tagged"));

		if (cursor) {
			g_object_unref (cursor);
		}

		return TRUE;
	}

	s = g_string_new ("");

	for (p = tag_urns; p && *p; p++) {
		g_string_append_printf (s, "; nao:hasTag <%s>", *p);
	}

	s = g_string_append (s, " .");
	filter = g_string_free (s, FALSE);
	g_strfreev (tag_urns);

	query = g_strdup_printf ("SELECT DISTINCT nie:url(?r) "
	                         "WHERE {"
	                         "  ?r a rdfs:Resource %s"
	                         "} "
	                         "OFFSET %d "
	                         "LIMIT %d",
	                         filter,
	                         search_offset,
	                         search_limit);
	g_free (filter);

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not get files for matching tags"),
		            error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!cursor) {
		g_print ("%s\n",
		         _("No files were found matching ALL of those tags"));
	} else {
		gint count = 0;

		g_print ("%s:\n", _("Files"));

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			g_print ("  %s\n",
			         tracker_sparql_cursor_get_string (cursor, 0, NULL));
			count++;
		}

		if (count == 0) {
			/* To translators: This is to say there are no
			 * files found associated with multiple tags, e.g.:
			 *
			 *   Files:
			 *     None
			 *
			 */
			g_print ("  %s\n", _("None"));
		}

		g_print ("\n");

		if (count >= search_limit) {
			show_limit_warning ();
		}

		g_object_unref (cursor);
	}

	return TRUE;
}


static gboolean
get_all_tags (TrackerSparqlConnection *connection,
              GStrv                    resources,
              gint                     search_offset,
              gint                     search_limit,
              gboolean                 show_resources)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gchar *query;
	gchar *filter = NULL;

	if (resources && g_strv_length (resources) > 0) {
		filter = get_filter_in_for_strv (resources, "?label");
	}

	/* You might be asking, why not logical AND here, why
	 * logical OR for FILTER, well, tags can't have
	 * multiple labels is the simple answer.
	 */
	query = g_strdup_printf ("SELECT ?tag ?label nao:description(?tag) COUNT(?urns) "
	                         "WHERE {"
	                         "  ?tag a nao:Tag ;"
	                         "  nao:prefLabel ?label ."
	                         "  OPTIONAL {"
	                         "     ?urns nao:hasTag ?tag"
	                         "  } ."
	                         "  %s"
	                         "} "
	                         "GROUP BY ?tag "
	                         "ORDER BY ASC(?label) "
	                         "OFFSET %d "
	                         "LIMIT %d",
	                         filter ? filter : "",
	                         search_offset,
	                         search_limit);
	g_free (filter);

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not get all tags"),
		            error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!cursor) {
		g_print ("%s\n",
		         _("No tags were found"));
	} else {
		gint count = 0;

		g_print ("%s:\n", _("Tags (shown by name)"));

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			const gchar *id;
			const gchar *tag;
			const gchar *description;
			const gchar *resources;
			gint n_resources = 0;

			id = tracker_sparql_cursor_get_string (cursor, 0, NULL);
			resources = tracker_sparql_cursor_get_string (cursor, 3, NULL);
			n_resources = atoi (resources);

			tag = tracker_sparql_cursor_get_string (cursor, 1, NULL);
			description = tracker_sparql_cursor_get_string (cursor, 2, NULL);

			if (description && *description == '\0') {
				description = NULL;
			}

			g_print ("  %s %s%s%s\n",
			         tag,
			         description ? "(" : "",
			         description ? description : "",
			         description ? ")" : "");

			if (show_resources && n_resources > 0) {
				get_all_tags_show_tag_id (connection, id);
			} else {
				g_print ("    %s\n", id);
				g_print ("    ");
				g_print (g_dngettext (NULL,
				                      "%d file",
				                      "%d files",
				                      n_resources),
				         n_resources);
				g_print ("\n");
			}

			count++;
		}

		if (count == 0) {
			/* To translators: This is to say there are no
			 * resources found associated with this tag, e.g.:
			 *
			 *   Tags (shown by name):
			 *     None
			 *
			 */
			g_print ("  %s\n", _("None"));
		}

		g_print ("\n");

		if (count >= search_limit) {
			show_limit_warning ();
		}

		g_object_unref (cursor);
	}

	return TRUE;
}

static void
print_file_report (TrackerSparqlCursor *cursor,
                   GStrv                uris,
                   const gchar         *found_msg,
                   const gchar         *not_found_msg)
{
	gint i;

	if (!cursor || !uris) {
		g_print ("  %s\n", _("No files were modified"));
		return;
	}

	for (i = 0; uris[i]; i++) {
		gboolean found = FALSE;

		tracker_sparql_cursor_rewind (cursor);

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			const gchar *str;

			str = tracker_sparql_cursor_get_string (cursor, 1, NULL);

			if (g_strcmp0 (str, uris[i]) == 0) {
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
add_tag_for_urns (TrackerSparqlConnection *connection,
                  GStrv                    resources,
                  const gchar             *tag,
                  const gchar             *description)
{
	TrackerSparqlCursor *cursor = NULL;
	GError *error = NULL;
	GStrv  uris = NULL, urns_strv = NULL;
	gchar *tag_escaped;
	gchar *query;

	tag_escaped = get_escaped_sparql_string (tag);

	if (resources) {
		uris = get_uris (resources);

		if (!uris) {
			return FALSE;
		}

		cursor = get_file_urns (connection, uris, NULL);

		if (!cursor) {
			g_printerr ("%s\n", _("Files do not exist or aren’t indexed"));
			g_strfreev (uris);
			return FALSE;
		}

		urns_strv = result_to_strv (cursor, 0);

		if (!urns_strv || g_strv_length (urns_strv) < 1) {
			g_printerr ("%s\n", _("Files do not exist or aren’t indexed"));
			g_object_unref (cursor);
			g_strfreev (uris);
			return FALSE;
		}
	}

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

	tracker_sparql_connection_update (connection, query, 0, NULL, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not add tag"),
		            error->message);

		if (cursor) {
			g_object_unref (cursor);
		}

		g_error_free (error);
		g_free (tag_escaped);
		g_strfreev (urns_strv);
		g_strfreev (uris);

		return FALSE;
	}

	g_print ("%s\n",
	         _("Tag was added successfully"));

	/* First we check if the tag is already set and only add if it
	 * is, then we add the urns specified to the new tag.
	 */
	if (urns_strv) {
		gchar *filter;

		filter = get_filter_string (urns_strv, "?urn", TRUE, NULL);

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
		                         filter ? filter : "");

		tracker_sparql_connection_update (connection, query, 0, NULL, &error);
		g_strfreev (urns_strv);
		g_free (filter);
		g_free (query);

		if (error) {
			g_printerr ("%s, %s\n",
			            _("Could not add tag to files"),
			            error->message);
			g_object_unref (cursor);
			g_error_free (error);
			g_free (tag_escaped);
			g_strfreev (uris);

			return FALSE;
		}

		print_file_report (cursor, uris, _("Tagged"),
		                   _("Not tagged, file is not indexed"));
	}

	g_strfreev (uris);
	g_free (tag_escaped);

	if (cursor) {
		g_object_unref (cursor);
	}

	return TRUE;
}

static gboolean
remove_tag_for_urns (TrackerSparqlConnection *connection,
                     GStrv                    resources,
                     const gchar             *tag)
{
	TrackerSparqlCursor *urns_cursor = NULL;
	GError *error = NULL;
	gchar *tag_escaped;
	gchar *query;
	GStrv uris;

	tag_escaped = get_escaped_sparql_string (tag);
	uris = get_uris (resources);

	if (uris && *uris) {
		TrackerSparqlCursor *tag_cursor;
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

		tag_cursor = tracker_sparql_connection_query (connection, query, NULL, &error);
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

		if (!tag_cursor || !tracker_sparql_cursor_next (tag_cursor, NULL, NULL)) {
			g_print ("%s\n",
			         _("No tags were found by that name"));

			g_free (tag_escaped);
			g_strfreev (uris);

			if (tag_cursor) {
				g_object_unref (tag_cursor);
			}

			return TRUE;
		}

		urn = tracker_sparql_cursor_get_string (tag_cursor, 0, NULL);
		urns_cursor = get_file_urns (connection, uris, urn);

		if (!urns_cursor || !tracker_sparql_cursor_next (urns_cursor, NULL, NULL)) {
			g_print ("%s\n",
			         _("None of the files had this tag set"));

			g_strfreev (uris);
			g_free (tag_escaped);
			g_object_unref (tag_cursor);

			if (urns_cursor) {
				g_object_unref (urns_cursor);
			}

			return TRUE;
		}

		urns_strv = result_to_strv (urns_cursor, 0);
		filter = get_filter_string (urns_strv, "?urn", TRUE, urn);
		g_strfreev (urns_strv);

		query = g_strdup_printf ("DELETE { "
		                         "  ?urn nao:hasTag ?t "
		                         "} "
		                         "WHERE { "
		                         "  ?urn nao:hasTag ?t . "
		                         "  %s "
		                         "}",
		                         filter ? filter : "");
		g_free (filter);

		g_object_unref (tag_cursor);
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

	tracker_sparql_connection_update (connection, query, 0, NULL, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not remove tag"),
		            error->message);
		g_error_free (error);

		return FALSE;
	}

	g_print ("%s\n", _("Tag was removed successfully"));

	if (urns_cursor) {
		print_file_report (urns_cursor, uris,
		                   _("Untagged"),
		                   _("File not indexed or already untagged"));
		g_object_unref (urns_cursor);
	}

	g_strfreev (uris);

	return TRUE;
}

static gboolean
get_tags_by_file (TrackerSparqlConnection *connection,
                  const gchar             *uri)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
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

	cursor = tracker_sparql_connection_query (connection, query, NULL, &error);
	g_free (query);

	if (error) {
		g_printerr ("%s, %s\n",
		            _("Could not get all tags"),
		            error->message);
		g_error_free (error);

		return FALSE;
	}

	if (!cursor) {
		g_print ("  %s\n",
		         _("No tags were found"));
	} else {
		gint count = 0;

		while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
			g_print ("  %s\n", tracker_sparql_cursor_get_string (cursor, 1, NULL));
			count++;
		}

		if (count == 0) {
			/* To translators: This is to say there are no
			 * tags found for a particular file, e.g.:
			 *
			 *   /path/to/some/file:
			 *     None
			 *
			 */
			g_print ("  %s\n", _("None"));
		}

		g_print ("\n");

		g_object_unref (cursor);
	}

	return TRUE;
}

static int
tag_run (void)
{
	TrackerSparqlConnection *connection;
	GError *error = NULL;

	connection = tracker_sparql_connection_get (NULL, &error);

	if (!connection) {
		g_printerr ("%s: %s\n",
		            _("Could not establish a connection to Tracker"),
		            error ? error->message : _("No error given"));
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	if (list) {
		gboolean success;

		if (G_UNLIKELY (and_operator)) {
			success = get_all_resources_with_tags (connection, resources, offset, limit);
		} else {
			success = get_all_tags (connection, resources, offset, limit, show_resources);
		}
		g_object_unref (connection);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (add_tag) {
		gboolean success;

		success = add_tag_for_urns (connection, resources, add_tag, description);
		g_object_unref (connection);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (remove_tag) {
		gboolean success;

		success = remove_tag_for_urns (connection, resources, remove_tag);
		g_object_unref (connection);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (resources) {
		gboolean success = TRUE;
		gchar **p;

		for (p = resources; *p; p++) {
			GFile *file;
			gchar *uri;
			
			file = g_file_new_for_commandline_arg (*p);
			uri = g_file_get_uri (file);
			g_object_unref (file);

			g_print ("%s\n", uri);
			success &= get_tags_by_file (connection, uri);

			g_free (uri);
		}

		g_object_unref (connection);

		return success ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	g_object_unref (connection);

	/* This is a failure because we should have done something.
	 * This code should never be reached in practise.
	 */
	return EXIT_FAILURE;
}

static int
tag_run_default (void)
{
	GOptionContext *context;
	gchar *help;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);
	help = g_option_context_get_help (context, TRUE, NULL);
	g_option_context_free (context);
	g_printerr ("%s\n", help);
	g_free (help);

	return EXIT_FAILURE;
}

static gboolean
tag_options_enabled (void)
{
	return TAG_OPTIONS_ENABLED ();
}

int
tracker_tag (int argc, const char **argv)
{
	GOptionContext *context;
	GError *error = NULL;
	const gchar *failed;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, NULL);

	argv[0] = "tracker tag";

	if (!g_option_context_parse (context, &argc, (char***) &argv, &error)) {
		g_printerr ("%s, %s\n", _("Unrecognized options"), error->message);
		g_error_free (error);
		g_option_context_free (context);
		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	if (!list && show_resources) {
		failed = _("The --list option is required for --show-files");
	} else if (and_operator && (!list || !resources)) {
		failed = _("The --and-operator option can only be used with --list and tag label arguments");
	} else if (add_tag && remove_tag) {
		failed = _("Add and delete actions can not be used together");
	} else if (description && !add_tag) {
		failed = _("The --description option can only be used with --add");
	} else {
		failed = NULL;
	}

	if (failed) {
		g_printerr ("%s\n\n", failed);
		return EXIT_FAILURE;
	}

	if (tag_options_enabled ()) {
		return tag_run ();
	}

	return tag_run_default ();
}
