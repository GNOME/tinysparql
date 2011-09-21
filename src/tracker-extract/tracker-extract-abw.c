/*
 * Copyright (C) 2007, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-file-utils.h>

#include <libtracker-extract/tracker-extract.h>

typedef struct AbwParserData AbwParserData;
typedef enum {
	ABW_PARSER_TAG_UNHANDLED,
	ABW_PARSER_TAG_TITLE,
	ABW_PARSER_TAG_SUBJECT,
	ABW_PARSER_TAG_CREATOR,
	ABW_PARSER_TAG_KEYWORDS,
	ABW_PARSER_TAG_DESCRIPTION,
	ABW_PARSER_TAG_GENERATOR
} AbwParserTag;

struct AbwParserData {
	TrackerSparqlBuilder *metadata;
	TrackerSparqlBuilder *preupdate;
	GString *content;

	guint cur_tag;
	guint in_text : 1;
};

static void
abw_parser_start_elem (GMarkupParseContext *context,
                       const gchar         *element_name,
                       const gchar        **attribute_names,
                       const gchar        **attribute_values,
                       gpointer             user_data,
                       GError             **error)
{
	AbwParserData *data = user_data;

	if (g_strcmp0 (element_name, "m") == 0 &&
	    g_strcmp0 (attribute_names[0], "key") == 0) {
		if (g_strcmp0 (attribute_values[0], "dc.title") == 0) {
			data->cur_tag = ABW_PARSER_TAG_TITLE;
		} else if (g_strcmp0 (attribute_values[0], "dc.subject") == 0) {
			data->cur_tag = ABW_PARSER_TAG_SUBJECT;
		} else if (g_strcmp0 (attribute_values[0], "dc.creator") == 0) {
			data->cur_tag = ABW_PARSER_TAG_CREATOR;
		} else if (g_strcmp0 (attribute_values[0], "abiword.keywords") == 0) {
			data->cur_tag = ABW_PARSER_TAG_KEYWORDS;
		} else if (g_strcmp0 (attribute_values[0], "dc.description") == 0) {
			data->cur_tag = ABW_PARSER_TAG_DESCRIPTION;
		} else if (g_strcmp0 (attribute_values[0], "abiword.generator") == 0) {
			data->cur_tag = ABW_PARSER_TAG_GENERATOR;
		}
	} else if (g_strcmp0 (element_name, "section") == 0) {
		data->in_text = TRUE;
	}
}

static void
abw_parser_text (GMarkupParseContext *context,
                 const gchar         *text,
                 gsize                text_len,
                 gpointer             user_data,
                 GError             **error)
{
	AbwParserData *data = user_data;
	gchar *str;

	str = g_strndup (text, text_len);

	switch (data->cur_tag) {
	case ABW_PARSER_TAG_TITLE:
		tracker_sparql_builder_predicate (data->metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (data->metadata, str);
		break;
	case ABW_PARSER_TAG_SUBJECT:
		tracker_sparql_builder_predicate (data->metadata, "nie:subject");
		tracker_sparql_builder_object_unvalidated (data->metadata, str);
		break;
	case ABW_PARSER_TAG_CREATOR:
		tracker_sparql_builder_predicate (data->metadata, "nco:creator");

		tracker_sparql_builder_object_blank_open (data->metadata);
		tracker_sparql_builder_predicate (data->metadata, "a");
		tracker_sparql_builder_object (data->metadata, "nco:Contact");

		tracker_sparql_builder_predicate (data->metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (data->metadata, str);
		tracker_sparql_builder_object_blank_close (data->metadata);
		break;
	case ABW_PARSER_TAG_DESCRIPTION:
		tracker_sparql_builder_predicate (data->metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (data->metadata, str);
		break;
	case ABW_PARSER_TAG_GENERATOR:
		tracker_sparql_builder_predicate (data->metadata, "nie:generator");
		tracker_sparql_builder_object_unvalidated (data->metadata, str);
		break;
	case ABW_PARSER_TAG_KEYWORDS:
	{
		char *lasts, *keyword;

		for (keyword = strtok_r (str, ",; ", &lasts); keyword;
		     keyword = strtok_r (NULL, ",; ", &lasts)) {
			tracker_sparql_builder_predicate (data->metadata, "nie:keyword");
			tracker_sparql_builder_object_unvalidated (data->metadata, keyword);
		}
	}
		break;
	default:
		break;
	}

	if (data->in_text) {
		if (G_UNLIKELY (!data->content)) {
			data->content = g_string_new ("");
		}

		g_string_append_len (data->content, text, text_len);
	}

	data->cur_tag = ABW_PARSER_TAG_UNHANDLED;
	g_free (str);
}

static GMarkupParser parser = {
	abw_parser_start_elem,
	NULL,
	abw_parser_text,
	NULL, NULL
};

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerSparqlBuilder *preupdate, *metadata;
	int fd;
	gchar *filename, *contents;
	gboolean retval = FALSE;
	GFile *f;
	gsize len;
	struct stat st;

	preupdate = tracker_extract_info_get_preupdate_builder (info);
	metadata = tracker_extract_info_get_metadata_builder (info);

	f = tracker_extract_info_get_file (info);
	filename = g_file_get_path (f);

	fd = g_open (filename, O_RDONLY | O_NOATIME, 0);

	if (fd == -1) {
		g_warning ("Could not open abw file '%s': %s\n",
		           filename,
		           g_strerror (errno));
		g_free (filename);
		return retval;
	}

	if (fstat (fd, &st) == -1) {
		g_warning ("Could not fstat abw file '%s': %s\n",
		           filename,
		           g_strerror (errno));
		close (fd);
		g_free (filename);
		return retval;
	}

	if (st.st_size == 0) {
		contents = NULL;
		len = 0;
	} else {
		contents = (gchar *) mmap (NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (contents == NULL) {
			g_warning ("Could not mmap abw file '%s': %s\n",
			           filename,
			           g_strerror (errno));
			close (fd);
			g_free (filename);
			return retval;
		}
		len = st.st_size;
	}

	g_free (filename);

	if (contents) {
		GError *error = NULL;
		GMarkupParseContext *context;
		AbwParserData data = { 0 };

		data.metadata = metadata;
		data.preupdate = preupdate;

		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nfo:Document");

		context = g_markup_parse_context_new (&parser, 0, &data, NULL);
		g_markup_parse_context_parse (context, contents, len, &error);

		if (error) {
			g_warning ("Could not parse abw file: %s\n", error->message);
			g_error_free (error);
		} else {
			if (data.content) {
				tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
				tracker_sparql_builder_object_unvalidated (metadata, data.content->str);
				g_string_free (data.content, TRUE);
			}

			retval = TRUE;
		}

		g_markup_parse_context_free (context);
	}


	if (contents) {
		munmap (contents, len);
	}

	close (fd);

	return retval;
}
