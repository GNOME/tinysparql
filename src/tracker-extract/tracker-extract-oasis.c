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
#include "tracker-gsf.h"
#include "tracker-read.h"

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
	ODT_TAG_TYPE_GENERATOR,
	ODT_TAG_TYPE_WORD_TEXT,
	ODT_TAG_TYPE_SLIDE_TEXT,
	ODT_TAG_TYPE_SPREADSHEET_TEXT
} ODTTagType;

typedef enum {
	FILE_TYPE_INVALID,
	FILE_TYPE_ODP,
	FILE_TYPE_ODT,
	FILE_TYPE_ODS
} ODTFileType;

typedef struct {
	TrackerSparqlBuilder *metadata;
	ODTTagType current;
	const gchar *uri;
	gboolean title_already_set;
} ODTMetadataParseInfo;

typedef struct {
	ODTTagType current;
	gboolean styles_present;
	ODTFileType file_type;
	GString *content;
	gulong bytes_pending;
} ODTContentParseInfo;

GQuark maximum_size_error_quark = 0;

static void xml_start_element_handler_metadata (GMarkupParseContext   *context,
                                                const gchar           *element_name,
                                                const gchar          **attribute_names,
                                                const gchar          **attribute_values,
                                                gpointer               user_data,
                                                GError               **error);
static void xml_end_element_handler_metadata   (GMarkupParseContext   *context,
                                                const gchar           *element_name,
                                                gpointer               user_data,
                                                GError               **error);
static void xml_text_handler_metadata          (GMarkupParseContext   *context,
                                                const gchar           *text,
                                                gsize                  text_len,
                                                gpointer               user_data,
                                                GError               **error);
static void xml_start_element_handler_content  (GMarkupParseContext   *context,
                                                const gchar           *element_name,
                                                const gchar          **attribute_names,
                                                const gchar          **attribute_values,
                                                gpointer               user_data,
                                                GError               **error);
static void xml_end_element_handler_content    (GMarkupParseContext   *context,
                                                const gchar           *element_name,
                                                gpointer               user_data,
                                                GError               **error);
static void xml_text_handler_content           (GMarkupParseContext   *context,
                                                const gchar           *text,
                                                gsize                  text_len,
                                                gpointer               user_data,
                                                GError               **error);
static void extract_oasis                      (const gchar           *filename,
                                                TrackerSparqlBuilder  *preupdate,
                                                TrackerSparqlBuilder  *metadata);
static void extract_oasis_content              (const gchar           *uri,
                                                gulong                 total_bytes,
                                                ODTFileType            file_type,
                                                TrackerSparqlBuilder  *metadata);

static TrackerExtractData extract_data[] = {
	{ "application/vnd.oasis.opendocument.*", extract_oasis },
	{ NULL, NULL }
};

static void
extract_oasis_content (const gchar          *uri,
                       gulong                total_bytes,
                       ODTFileType           file_type,
                       TrackerSparqlBuilder *metadata)
{
	gchar *content;
	ODTContentParseInfo info;
	GMarkupParseContext *context;
	GError *error = NULL;
	GMarkupParser parser = {
		xml_start_element_handler_content,
		xml_end_element_handler_content,
		xml_text_handler_content,
		NULL,
		NULL
	};

	/* If no content requested, return */
	if (total_bytes == 0) {
		return;
	}

	/* Create parse info */
	info.current = ODT_TAG_TYPE_UNKNOWN;
	info.file_type = file_type;
	info.styles_present = FALSE;
	info.content = g_string_new ("");
	info.bytes_pending = total_bytes;

	/* Create parsing context */
	context = g_markup_parse_context_new (&parser, 0, &info, NULL);

	/* Load the internal XML file from the Zip archive, and parse it
	 * using the given context */
	tracker_gsf_parse_xml_in_zip (uri, "content.xml", context, &error);

	if (!error || g_error_matches (error, maximum_size_error_quark, 0)) {
		content = g_string_free (info.content, FALSE);
		tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, content);
	} else {
		g_warning ("Got error parsing XML file: %s\n", error->message);
		g_string_free (info.content, TRUE);
	}

	if (error) {
		g_error_free (error);
	}

	g_free (content);
	g_markup_parse_context_free (context);
}

static void
extract_oasis (const gchar          *uri,
               TrackerSparqlBuilder *preupdate,
               TrackerSparqlBuilder *metadata)
{
	TrackerConfig *config;
	ODTMetadataParseInfo info;
	ODTFileType file_type;
	GFile *file = NULL;
	GFileInfo *file_info = NULL;
	const gchar *mime_used;
	GMarkupParseContext *context;
	GMarkupParser parser = {
		xml_start_element_handler_metadata,
		xml_end_element_handler_metadata,
		xml_text_handler_metadata,
		NULL,
		NULL
	};

	if (G_UNLIKELY (maximum_size_error_quark == 0)) {
		maximum_size_error_quark = g_quark_from_static_string ("maximum_size_error");
	}

	/* Setup conf */
	config = tracker_main_get_config ();

	g_debug ("Extracting OASIS metadata and contents from '%s'", uri);

	/* First, parse metadata */

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	/* Create parse info */
	info.metadata = metadata;
	info.current = ODT_TAG_TYPE_UNKNOWN;
	info.uri = uri;
	info.title_already_set = FALSE;

	/* Create parsing context */
	context = g_markup_parse_context_new (&parser, 0, &info, NULL);

	/* Load the internal XML file from the Zip archive, and parse it
	 * using the given context */
	tracker_gsf_parse_xml_in_zip (uri, "meta.xml", context, NULL);
	g_markup_parse_context_free (context);

	/* Next, parse contents */
	file = g_file_new_for_uri (uri);

	if (!file) {
		g_warning ("Could not create GFile for URI:'%s'",
		           uri);
		return;
	}

	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
	                               G_FILE_QUERY_INFO_NONE,
	                               NULL,
	                               NULL);
	g_object_unref (file);

	if (!file_info) {
		g_warning ("Could not get GFileInfo for URI:'%s'",
		           uri);
		return;
	}

	mime_used = g_file_info_get_content_type (file_info);

	if (g_ascii_strcasecmp (mime_used, "application/vnd.oasis.opendocument.text") == 0) {
		file_type = FILE_TYPE_ODT;
	} else if (g_ascii_strcasecmp (mime_used, "application/vnd.oasis.opendocument.presentation") == 0) {
		file_type = FILE_TYPE_ODP;
	} else if (g_ascii_strcasecmp (mime_used, "application/vnd.oasis.opendocument.spreadsheet") == 0) {
		file_type = FILE_TYPE_ODS;
	} else {
		g_message ("Mime type was not recognised:'%s'", mime_used);
		file_type = FILE_TYPE_INVALID;
	}

	g_object_unref (file_info);

	/* Extract content with the given limitations */
	extract_oasis_content (uri,
	                       tracker_config_get_max_bytes (config),
	                       file_type,
	                       metadata);
}

static void
xml_start_element_handler_metadata (GMarkupParseContext  *context,
                                    const gchar          *element_name,
                                    const gchar         **attribute_names,
                                    const gchar         **attribute_values,
                                    gpointer              user_data,
                                    GError              **error)
{
	ODTMetadataParseInfo *data = user_data;

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
xml_end_element_handler_metadata (GMarkupParseContext  *context,
                                  const gchar          *element_name,
                                  gpointer              user_data,
                                  GError              **error)
{
	((ODTMetadataParseInfo*) user_data)->current = -1;
}

static void
xml_text_handler_metadata (GMarkupParseContext  *context,
                           const gchar          *text,
                           gsize                 text_len,
                           gpointer              user_data,
                           GError              **error)
{
	ODTMetadataParseInfo *data;
	TrackerSparqlBuilder *metadata;
	const gchar *uri;
	gchar *date;

	data = user_data;
	metadata = data->metadata;
	uri = data->uri;

	switch (data->current) {
	case ODT_TAG_TYPE_TITLE:
		if (data->title_already_set) {
			g_warning ("Avoiding additional title (%s) in OASIS document '%s'",
			           text, data->uri);
		} else {
			data->title_already_set = TRUE;
			tracker_sparql_builder_predicate (metadata, "nie:title");
			tracker_sparql_builder_object_unvalidated (metadata, text);
		}
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

static void
xml_start_element_handler_content (GMarkupParseContext  *context,
                                   const gchar          *element_name,
                                   const gchar         **attribute_names,
                                   const gchar         **attribute_values,
                                   gpointer              user_data,
                                   GError              **error)
{
	ODTContentParseInfo *data = user_data;
	const gchar **a;
	const gchar **v;

	switch (data->file_type) {
	case FILE_TYPE_ODT:
		if ((g_ascii_strcasecmp (element_name, "text:table-of-content") == 0) ||
		    (g_ascii_strcasecmp (element_name, "text:table-index") == 0) ||
		    (g_ascii_strcasecmp (element_name, "text:illustration-index") == 0) ||
		    (g_ascii_strcasecmp (element_name, "text:section") == 0)) {
			data->styles_present = TRUE;
		} else if (g_ascii_strcasecmp (element_name, "table:table-cell") == 0) {
			data->current = ODT_TAG_TYPE_WORD_TEXT;
		} else if (g_ascii_strcasecmp (element_name, "text:p") == 0) {
			if (data->styles_present) {
				data->current = ODT_TAG_TYPE_WORD_TEXT;
				break;
			}

			for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
				if (g_ascii_strcasecmp (*a, "text:style-name") != 0) {
					continue;
				}

				if ((g_ascii_strcasecmp (*v, "title-article") == 0) ||
				    (g_ascii_strcasecmp (*v, "para-padding") == 0) ||
				    (g_ascii_strcasecmp (*v, "para-screen") == 0)) {
					data->current = ODT_TAG_TYPE_WORD_TEXT;
				}
			}
		} else if (g_ascii_strcasecmp (element_name, "text:h") == 0) {
			for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
				if (g_ascii_strcasecmp (*a, "text:style-name") != 0) {
					continue;
				}

				if (g_ascii_strncasecmp (*v, "Heading", 7) == 0) {
					data->current = ODT_TAG_TYPE_WORD_TEXT;
				}
			}
		} else if (g_ascii_strcasecmp (element_name, "text:span") == 0) {
			data->current = ODT_TAG_TYPE_WORD_TEXT;
		} else if ((g_ascii_strcasecmp (element_name, "text:a") == 0) ||
		           (g_ascii_strcasecmp (element_name, "text:s") == 0)) {
			data->current = ODT_TAG_TYPE_WORD_TEXT;
		} else {
			data->current = -1;
		}
		break;

	case FILE_TYPE_ODP:
		data->current = ODT_TAG_TYPE_SLIDE_TEXT;
		break;

	case FILE_TYPE_ODS:
		if (g_ascii_strncasecmp (element_name, "text", 4) == 0) {
			data->current = ODT_TAG_TYPE_SPREADSHEET_TEXT;
		} else {
			data->current = -1;
		}
		break;

	case FILE_TYPE_INVALID:
		g_message ("Open Office Document type: %d invalid", data->file_type);
		break;
	}
}

static void
xml_end_element_handler_content (GMarkupParseContext  *context,
                                 const gchar          *element_name,
                                 gpointer              user_data,
                                 GError              **error)
{
	ODTContentParseInfo *data = user_data;

	switch (data->file_type) {
	case FILE_TYPE_ODT:
		if ((g_ascii_strcasecmp (element_name, "text:table-of-content") == 0) ||
		    (g_ascii_strcasecmp (element_name, "text:table-index") == 0) ||
		    (g_ascii_strcasecmp (element_name, "text:illustration-index") == 0) ||
		    (g_ascii_strcasecmp (element_name, "text:section") == 0)) {
			data->styles_present = FALSE;
		}
		break;
	default:
		break;
	}

	if ((g_ascii_strcasecmp (element_name, "text:a") != 0) &&
	    (g_ascii_strcasecmp (element_name, "text:s") != 0)) {
		data->current = -1;
	}
}

static void
xml_text_handler_content (GMarkupParseContext  *context,
                          const gchar          *text,
                          gsize                 text_len,
                          gpointer              user_data,
                          GError              **error)
{
	ODTContentParseInfo *data = user_data;
	gsize written_bytes = 0;

	switch (data->current) {
	case ODT_TAG_TYPE_WORD_TEXT:
	case ODT_TAG_TYPE_SLIDE_TEXT:
	case ODT_TAG_TYPE_SPREADSHEET_TEXT:
		if (data->bytes_pending == 0) {
			g_set_error_literal (error,
			                     maximum_size_error_quark, 0,
			                     "Maximum text limit reached");
			break;
		}

		/* Look for valid UTF-8 text */
		if (tracker_text_validate_utf8 (text,
		                                MIN (text_len, data->bytes_pending),
		                                &data->content,
		                                &written_bytes)) {
			if (data->content->str[data->content->len - 1] != ' ') {
				/* If some bytes found to be valid, append an extra whitespace
				 * as separator */
				g_string_append_c (data->content, ' ');
			}
		}

		data->bytes_pending -= written_bytes;
		break;

	default:
		break;
	}
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return extract_data;
}
