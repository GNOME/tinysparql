/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include <libtracker-common/tracker-os-dependant.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-main.h"

#include <unistd.h>

typedef enum {
	ODT_TAG_TYPE_UNKNOWN,
	ODT_TAG_TYPE_TITLE,
	ODT_TAG_TYPE_SUBJECT,
	ODT_TAG_TYPE_AUTHOR,
	ODT_TAG_TYPE_KEYWORDS,
	ODT_TAG_TYPE_COMMENTS,
	ODT_TAG_TYPE_STATS,
	ODT_TAG_TYPE_CREATED,
	ODT_TAG_TYPE_GENERATOR
} ODTTagType;

typedef struct {
	TrackerSparqlBuilder *metadata;
	ODTTagType current;
	const gchar *uri;
} ODTParseInfo;

static void xml_start_element_handler (GMarkupParseContext   *context,
                                       const gchar           *element_name,
                                       const gchar          **attribute_names,
                                       const gchar          **attribute_values,
                                       gpointer               user_data,
                                       GError               **error);
static void xml_end_element_handler   (GMarkupParseContext   *context,
                                       const gchar           *element_name,
                                       gpointer               user_data,
                                       GError               **error);
static void xml_text_handler          (GMarkupParseContext   *context,
                                       const gchar           *text,
                                       gsize                  text_len,
                                       gpointer               user_data,
                                       GError               **error);
static void extract_oasis             (const gchar           *filename,
                                       TrackerSparqlBuilder  *preupdate,
                                       TrackerSparqlBuilder  *metadata);

static TrackerExtractData extract_data[] = {
	{ "application/vnd.oasis.opendocument.*", extract_oasis },
	{ NULL, NULL }
};


#define ODT_BUFFER_SIZE            8193  /* bytes */

static gchar *
extract_content (const gchar *path,
                 guint        n_words,
                 gsize        n_bytes)
{
	const gchar *argv[4];
	gint fdz;
	FILE *fz;
	GError *error = NULL;
	gchar *text = NULL;


	/* Setup command to be executed */
	argv[0] = "odt2txt";
	argv[1] = "--encoding=utf-8";
	argv[2] = path;
	argv[3] = NULL;

	g_debug ("Executing command:'%s %s %s' (max words: %u, "
	         "max_bytes: %" G_GSIZE_FORMAT ")",
	         argv[0], argv[1], argv[2], n_words, n_bytes);

	/* Fork & spawn */
	if (!g_spawn_async_with_pipes (g_get_tmp_dir (),
	                               (gchar **)argv,
	                               NULL,
	                               G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
	                               tracker_spawn_child_func,
	                               GINT_TO_POINTER (10),
	                               NULL,
	                               NULL,
	                               &fdz,
	                               NULL,
	                               &error)) {
		g_warning ("Spawning failed, could not extract text from '%s': %s",
		           path, error ? error->message : NULL);
		g_clear_error (&error);
	}
	/* Open file descriptor for reading */
	else if ((fz = fdopen (fdz, "r")) == NULL) {
		g_warning ("Cannot read child's output... could not extract "
		           "text from '%s'", path);
		close (fdz);
	}
	/* Start buffered reading... */
	else {
		unsigned char buf[ODT_BUFFER_SIZE];
		size_t r, accum;
		guint n_words_remaining = n_words;
		GString *normalized;

		accum = 0;
		normalized = g_string_new ("");

		/* Reading in chunks of ODT_BUFFER_SIZE -1 (8192)
		 *   Loop is halted whenever one of this conditions is met:
		 *     a) Read bytes reached the maximum allowed (n_bytes)
		 *     b) Already read up to the max number of words configured
		 *     c) No more bytes to read
		 */
		while ((accum <= n_bytes) &&
		       (n_words_remaining > 0) &&
		       (r = fread (buf, 1, ODT_BUFFER_SIZE-1, fz))) {
			gchar *normalized_chunk;
			guint n_words_normalized;

			/* Always make sure that the read string will be
			 * NIL-terminated  */
			buf[r] = '\0';
			/* Get normalized chunk */
			normalized_chunk = tracker_text_normalize (buf,
			                                           n_words_remaining,
			                                           &n_words_normalized);
			/* Update number of words remaining.
			 * Note that n_words_normalized should always be less or
			 * equal than n_words_remaining */
			n_words_remaining = (n_words_normalized <= n_words_remaining ?
			                     n_words_remaining - n_words_normalized : 0);
			/* Update accumulated */
			accum += r;

			/* Add normalized chunk to the whole normalized string */
			g_string_append (normalized, normalized_chunk);
			g_free (normalized_chunk);
		}

		/* fclose() the stream, no need to close() the original FD */
		fclose (fz);

		/* Set final normalized contents to return */
		text = g_string_free (normalized, FALSE);
	}

	return text;
}

static void
extract_oasis (const gchar          *uri,
               TrackerSparqlBuilder *preupdate,
               TrackerSparqlBuilder *metadata)
{
	gchar *argv[5];
	gchar *xml;
	gchar *filename;
	gchar *content;
	TrackerFTSConfig *fts_config;
	guint n_words;
	gsize n_bytes;

	filename = g_filename_from_uri (uri, NULL, NULL);

	argv[0] = g_strdup ("unzip");
	argv[1] = g_strdup ("-p");
	argv[2] = filename;
	argv[3] = g_strdup ("meta.xml");
	argv[4] = NULL;

	/* No need to unlink meta.xml, as it goes to stdout of the
	 *  spawned child (-p option in unzip) */

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	if (tracker_spawn (argv, 10, &xml, NULL)) {
		ODTParseInfo info;
		GMarkupParseContext *context;
		GMarkupParser parser = {
			xml_start_element_handler,
			xml_end_element_handler,
			xml_text_handler,
			NULL,
			NULL
		};

		info.metadata = metadata;
		info.current = ODT_TAG_TYPE_UNKNOWN;
		info.uri = uri;

		context = g_markup_parse_context_new (&parser, 0, &info, NULL);
		g_markup_parse_context_parse (context, xml, -1, NULL);

		g_markup_parse_context_free (context);
		g_free (xml);
	}

	fts_config = tracker_main_get_fts_config ();
	/* Set max words to read from content */
	n_words = tracker_fts_config_get_max_words_to_index (fts_config);
	/* Set max bytes to read from content.
	 * Assuming 3 bytes per unicode point in UTF-8, as 4-byte UTF-8 unicode
	 *  points are really pretty rare */
	n_bytes = 3 * n_words * tracker_fts_config_get_max_word_length(fts_config);

	content = extract_content (filename, n_words, n_bytes);

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

static void
xml_start_element_handler (GMarkupParseContext  *context,
                           const gchar          *element_name,
                           const gchar         **attribute_names,
                           const gchar         **attribute_values,
                           gpointer              user_data,
                           GError              **error)
{
	ODTParseInfo *data = user_data;

	if (g_ascii_strcasecmp (element_name, "dc:title") == 0) {
		data->current = ODT_TAG_TYPE_TITLE;
	} else if (g_ascii_strcasecmp (element_name, "dc:subject") == 0) {
		data->current = ODT_TAG_TYPE_SUBJECT;
	} else if (g_ascii_strcasecmp (element_name, "dc:creator") == 0) {
		data->current = ODT_TAG_TYPE_AUTHOR;
	} else if (g_ascii_strcasecmp (element_name, "meta:keyword") == 0) {
		data->current = ODT_TAG_TYPE_KEYWORDS;
	} else if (g_ascii_strcasecmp (element_name, "dc:description") == 0) {
		data->current = ODT_TAG_TYPE_COMMENTS;
	} else if (g_ascii_strcasecmp (element_name, "meta:document-statistic") == 0) {
		TrackerSparqlBuilder *metadata;
		const gchar *uri;
		const gchar **a, **v;

		metadata = data->metadata;
		uri = data->uri;

		for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
			if (g_ascii_strcasecmp (*a, "meta:word-count") == 0) {
				tracker_sparql_builder_predicate (metadata, "nfo:wordCount");
				tracker_sparql_builder_object_unvalidated (metadata, *v);
			} else if (g_ascii_strcasecmp (*a, "meta:page-count") == 0) {
				tracker_sparql_builder_predicate (metadata, "nfo:pageCount");
				tracker_sparql_builder_object_unvalidated (metadata, *v);
			}
		}

		data->current = ODT_TAG_TYPE_STATS;
	} else if (g_ascii_strcasecmp (element_name, "meta:creation-date") == 0) {
		data->current = ODT_TAG_TYPE_CREATED;
	} else if (g_ascii_strcasecmp (element_name, "meta:generator") == 0) {
	 	data->current = ODT_TAG_TYPE_GENERATOR;
	} else {
		data->current = -1;
	}
}

static void
xml_end_element_handler (GMarkupParseContext  *context,
                         const gchar          *element_name,
                         gpointer              user_data,
                         GError              **error)
{
	((ODTParseInfo*) user_data)->current = -1;
}

static void
xml_text_handler (GMarkupParseContext  *context,
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
	case ODT_TAG_TYPE_TITLE:
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, text);
		break;

	case ODT_TAG_TYPE_SUBJECT:
		tracker_sparql_builder_predicate (metadata, "nie:subject");
		tracker_sparql_builder_object_unvalidated (metadata, text);
		break;

	case ODT_TAG_TYPE_AUTHOR:
		tracker_sparql_builder_predicate (metadata, "nco:publisher");

		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");

		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, text);
		tracker_sparql_builder_object_blank_close (metadata);
		break;

	case ODT_TAG_TYPE_KEYWORDS: {
		gchar *keywords;
		gchar *lasts, *keyw;

		keywords = g_strdup (text);

		for (keyw = strtok_r (keywords, ",; ", &lasts);
		     keyw;
		     keyw = strtok_r (NULL, ",; ", &lasts)) {
			tracker_sparql_builder_predicate (metadata, "nie:keyword");
			tracker_sparql_builder_object_unvalidated (metadata, keyw);
		}

		g_free (keywords);

		break;
	}

	case ODT_TAG_TYPE_COMMENTS:
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, text);
		break;

	case ODT_TAG_TYPE_CREATED:
		date = tracker_date_guess (text);
		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, date);
		g_free (date);
		break;

	case ODT_TAG_TYPE_GENERATOR:
		tracker_sparql_builder_predicate (metadata, "nie:generator");
		tracker_sparql_builder_object_unvalidated (metadata, text);
		break;

	default:
	case ODT_TAG_TYPE_STATS:
		break;
	}
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return extract_data;
}
