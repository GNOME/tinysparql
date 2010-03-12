/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia
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

#include "config.h"

#include <stdio.h>
#include <string.h>

#include <glib.h>

#include <libtracker-common/tracker-os-dependant.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-main.h"

typedef enum {
	READ_TITLE,
	READ_SUBJECT,
	READ_AUTHOR,
	READ_KEYWORDS,
	READ_COMMENTS,
	READ_STATS,
	READ_CREATED,
	READ_GENERATOR
} tag_type;

typedef struct {
	TrackerSparqlBuilder *metadata;
	tag_type current;
	const gchar *uri;
} ODTParseInfo;

static void start_element_handler (GMarkupParseContext   *context,
                                   const gchar           *element_name,
                                   const gchar          **attribute_names,
                                   const gchar          **attribute_values,
                                   gpointer               user_data,
                                   GError               **error);
static void end_element_handler   (GMarkupParseContext   *context,
                                   const gchar           *element_name,
                                   gpointer               user_data,
                                   GError               **error);
static void text_handler          (GMarkupParseContext   *context,
                                   const gchar           *text,
                                   gsize                  text_len,
                                   gpointer               user_data,
                                   GError               **error);
static void extract_oasis         (const gchar           *filename,
                                   TrackerSparqlBuilder  *preupdate,
                                   TrackerSparqlBuilder  *metadata);

static TrackerExtractData extract_data[] = {
	{ "application/vnd.oasis.opendocument.*", extract_oasis },
	{ NULL, NULL }
};

static gchar *
extract_content (const gchar *path,
                 guint        n_words)
{
#ifdef HAVE_ODT2TXT
	gchar *command, *output, *text;
	GError *error = NULL;

	command = g_strdup_printf ("odt2txt --encoding=utf-8 %s", path);

	if (!g_spawn_command_line_sync (command, &output, NULL, NULL, &error)) {
		g_warning ("Could not extract text from '%s': %s", path, error->message);
		g_error_free (error);
		g_free (command);

		return NULL;
	}

	text = tracker_text_normalize (output, n_words, NULL);

	g_free (command);
	g_free (output);

	return text;
#else  /* HAVE_ODT2TXT */
	return NULL;
#endif /* HAVE_ODT2TXT */
}

static void
extract_oasis (const gchar          *uri,
               TrackerSparqlBuilder *preupdate,
               TrackerSparqlBuilder *metadata)
{
	gchar         *argv[5];
	gchar         *xml;
	gchar *filename = g_filename_from_uri (uri, NULL, NULL);
	gchar *content;
	TrackerFTSConfig *fts_config;
	guint n_words;
	ODTParseInfo   info = {
		metadata,
		-1,
		uri
	};

	argv[0] = g_strdup ("unzip");
	argv[1] = g_strdup ("-p");
	argv[2] = filename;
	argv[3] = g_strdup ("meta.xml");
	argv[4] = NULL;

	/* Question: shouldn't we g_unlink meta.xml then? */

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	if (tracker_spawn (argv, 10, &xml, NULL)) {
		GMarkupParseContext *context;
		GMarkupParser        parser = {
			start_element_handler,
			end_element_handler,
			text_handler,
			NULL,
			NULL
		};

		context = g_markup_parse_context_new (&parser, 0, &info, NULL);
		g_markup_parse_context_parse (context, xml, -1, NULL);

		g_markup_parse_context_free (context);
		g_free (xml);
	}

	fts_config = tracker_main_get_fts_config ();
	n_words = tracker_fts_config_get_max_words_to_index (fts_config);
	content = extract_content (filename, n_words);

	if (content) {
		tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, content);
		g_free (content);
	}

	g_free (argv[3]);
	g_free (argv[1]);
	g_free (argv[0]);

	g_free (filename);
}

void
start_element_handler (GMarkupParseContext  *context,
                       const gchar          *element_name,
                       const gchar         **attribute_names,
                       const gchar         **attribute_values,
                       gpointer                      user_data,
                       GError              **error)
{
	ODTParseInfo *data = user_data;

	if (strcmp (element_name, "dc:title") == 0) {
		data->current = READ_TITLE;
	}
	else if (strcmp (element_name, "dc:subject") == 0) {
		data->current = READ_SUBJECT;
	}
	else if (strcmp (element_name, "dc:creator") == 0) {
		data->current = READ_AUTHOR;
	}
	else if (strcmp (element_name, "meta:keyword") == 0) {
		data->current = READ_KEYWORDS;
	}
	else if (strcmp (element_name, "dc:description") == 0) {
		data->current = READ_COMMENTS;
	}
	else if (strcmp (element_name, "meta:document-statistic") == 0) {
		TrackerSparqlBuilder *metadata;
		const gchar *uri;
		const gchar **a, **v;

		metadata = data->metadata;
		uri = data->uri;

		for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
			if (strcmp (*a, "meta:word-count") == 0) {
				tracker_sparql_builder_predicate (metadata, "nfo:wordCount");
				tracker_sparql_builder_object_unvalidated (metadata, *v);
			} else if (strcmp (*a, "meta:page-count") == 0) {
				tracker_sparql_builder_predicate (metadata, "nfo:pageCount");
				tracker_sparql_builder_object_unvalidated (metadata, *v);
			}
		}

		data->current = READ_STATS;
	}
	else if (strcmp (element_name, "meta:creation-date") == 0) {
		data->current = READ_CREATED;
	}
	else if (strcmp (element_name, "meta:generator") == 0) {
		data->current = READ_GENERATOR;
	}
	else {
		data->current = -1;
	}
}

void
end_element_handler (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     gpointer              user_data,
                     GError              **error)
{
	((ODTParseInfo*) user_data)->current = -1;
}

void
text_handler (GMarkupParseContext  *context,
              const gchar          *text,
              gsize                 text_len,
              gpointer              user_data,
              GError              **error)
{
	ODTParseInfo *data;
	TrackerSparqlBuilder *metadata;
	const gchar *uri;
	gchar *date;

	data = user_data;
	metadata = data->metadata;
	uri = data->uri;

	switch (data->current) {
	case READ_TITLE:
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, text);
		break;
	case READ_SUBJECT:
		tracker_sparql_builder_predicate (metadata, "nie:subject");
		tracker_sparql_builder_object_unvalidated (metadata, text);
		break;
	case READ_AUTHOR:
		tracker_sparql_builder_predicate (metadata, "nco:publisher");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");

		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, text);
		tracker_sparql_builder_object_blank_close (metadata);
		break;
	case READ_KEYWORDS: {
		gchar *keywords = g_strdup (text);
		char *lasts, *keyw;
		for (keyw = strtok_r (keywords, ",; ", &lasts); keyw;
		     keyw = strtok_r (NULL, ",; ", &lasts)) {
			tracker_sparql_builder_predicate (metadata, "nie:keyword");
			tracker_sparql_builder_object_unvalidated (metadata, keyw);
		}
		g_free (keywords);
	}
		break;
	case READ_COMMENTS:
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, text);
		break;
	case READ_CREATED:
		date = tracker_date_guess (text);
		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, date);
		g_free (date);
		break;
	case READ_GENERATOR:
		tracker_sparql_builder_predicate (metadata, "nie:generator");
		tracker_sparql_builder_object_unvalidated (metadata, text);
		break;

	default:
	case READ_STATS:
		break;
	}
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return extract_data;
}
