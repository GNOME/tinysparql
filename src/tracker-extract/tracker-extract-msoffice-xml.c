/*
 * Copyright (C) 2008-2010 Nokia <ivan.frade@nokia.com>
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

#include <string.h>

#include <glib.h>

#include <gsf/gsf.h>
#include <gsf/gsf-doc-meta-data.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-infile-zip.h>

#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-os-dependant.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-main.h"
#include "tracker-gsf.h"

typedef enum {
	MS_OFFICE_XML_TAG_INVALID,
	MS_OFFICE_XML_TAG_TITLE,
	MS_OFFICE_XML_TAG_SUBJECT,
	MS_OFFICE_XML_TAG_AUTHOR,
	MS_OFFICE_XML_TAG_MODIFIED,
	MS_OFFICE_XML_TAG_COMMENTS,
	MS_OFFICE_XML_TAG_CREATED,
	MS_OFFICE_XML_TAG_GENERATOR,
	MS_OFFICE_XML_TAG_NUM_OF_PAGES,
	MS_OFFICE_XML_TAG_NUM_OF_CHARACTERS,
	MS_OFFICE_XML_TAG_NUM_OF_WORDS,
	MS_OFFICE_XML_TAG_NUM_OF_LINES,
	MS_OFFICE_XML_TAG_APPLICATION,
	MS_OFFICE_XML_TAG_NUM_OF_PARAGRAPHS,
	MS_OFFICE_XML_TAG_SLIDE_TEXT,
	MS_OFFICE_XML_TAG_WORD_TEXT,
	MS_OFFICE_XML_TAG_XLS_SHARED_TEXT,
	MS_OFFICE_XML_TAG_DOCUMENT_CORE_DATA,
	MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA
} MsOfficeXMLTagType;

typedef enum {
	FILE_TYPE_INVALID,
	FILE_TYPE_PPTX,
	FILE_TYPE_PPSX,
	FILE_TYPE_DOCX,
	FILE_TYPE_XLSX
} MsOfficeXMLFileType;

typedef struct {
	/* Common constant stuff */
	const gchar *uri;
	MsOfficeXMLFileType file_type;

	/* Tag type, reused by Content and Metadata parsers */
	MsOfficeXMLTagType tag_type;

	/* Metadata-parsing specific things */
	TrackerSparqlBuilder *metadata;
	gboolean title_already_set;
	gboolean generator_already_set;

	/* Content-parsing specific things */
	GString *content;
	gulong bytes_pending;
	gboolean style_element_present;
	gboolean preserve_attribute_present;
} MsOfficeXMLParserInfo;

static void msoffice_xml_content_parse_start       (GMarkupParseContext  *context,
                                                    const gchar          *element_name,
                                                    const gchar         **attribute_names,
                                                    const gchar         **attribute_values,
                                                    gpointer              user_data,
                                                    GError              **error);
static void msoffice_xml_content_parse_stop        (GMarkupParseContext  *context,
                                                    const gchar          *element_name,
                                                    gpointer              user_data,
                                                    GError              **error);
static void msoffice_xml_content_parse             (GMarkupParseContext  *context,
                                                    const gchar          *text,
                                                    gsize                 text_len,
                                                    gpointer              user_data,
                                                    GError              **error);

static void msoffice_xml_metadata_parse_start      (GMarkupParseContext  *context,
                                                    const gchar           *element_name,
                                                    const gchar          **attribute_names,
                                                    const gchar          **attribute_values,
                                                    gpointer               user_data,
                                                    GError               **error);
static void msoffice_xml_metadata_parse_stop       (GMarkupParseContext  *context,
                                                    const gchar          *element_name,
                                                    gpointer              user_data,
                                                    GError              **error);
static void msoffice_xml_metadata_parse            (GMarkupParseContext  *context,
                                                    const gchar          *text,
                                                    gsize                 text_len,
                                                    gpointer              user_data,
                                                    GError              **error);

static void msoffice_xml_content_types_parse_start (GMarkupParseContext  *context,
                                                    const gchar          *element_name,
                                                    const gchar         **attribute_names,
                                                    const gchar         **attribute_values,
                                                    gpointer              user_data,
                                                    GError              **error);

static const GMarkupParser metadata_parser = {
	msoffice_xml_metadata_parse_start,
	msoffice_xml_metadata_parse_stop,
	msoffice_xml_metadata_parse,
	NULL,
	NULL
};

static const GMarkupParser content_parser = {
	msoffice_xml_content_parse_start,
	msoffice_xml_content_parse_stop,
	msoffice_xml_content_parse,
	NULL,
	NULL
};

static const GMarkupParser content_types_parser = {
	msoffice_xml_content_types_parse_start,
	NULL,
	NULL,
	NULL,
	NULL
};

static GQuark maximum_size_error_quark = 0;

/* ------------------------- CONTENT files parsing -----------------------------------*/

static void
msoffice_xml_content_parse_start (GMarkupParseContext  *context,
                                  const gchar          *element_name,
                                  const gchar         **attribute_names,
                                  const gchar         **attribute_values,
                                  gpointer              user_data,
                                  GError              **error)
{
	MsOfficeXMLParserInfo *info = user_data;
	const gchar **a;
	const gchar **v;

	switch (info->file_type) {
	case FILE_TYPE_DOCX:
		if (g_ascii_strcasecmp (element_name, "w:pStyle") == 0) {
			for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
				if (g_ascii_strcasecmp (*a, "w:val") != 0) {
					continue;
				}

				if (g_ascii_strncasecmp (*v, "Heading", 7) == 0) {
					info->style_element_present = TRUE;
				} else if (g_ascii_strncasecmp (*v, "TOC", 3) == 0) {
					info->style_element_present = TRUE;
				} else if (g_ascii_strncasecmp (*v, "Section", 7) == 0) {
					info->style_element_present = TRUE;
				} else if (g_ascii_strncasecmp (*v, "Title", 5) == 0) {
					info->style_element_present = TRUE;
				} else if (g_ascii_strncasecmp (*v, "Subtitle", 8) == 0) {
					info->style_element_present = TRUE;
				}
			}
		} else if (g_ascii_strcasecmp (element_name, "w:rStyle") == 0) {
			for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
				if (g_ascii_strcasecmp (*a, "w:val") != 0) {
					continue;
				}

				if (g_ascii_strncasecmp (*v, "SubtleEmphasis", 14) == 0) {
					info->style_element_present = TRUE;
				} else if (g_ascii_strncasecmp (*v, "SubtleReference", 15) == 0) {
					info->style_element_present = TRUE;
				}
			}
		} else if (g_ascii_strcasecmp (element_name, "w:sz") == 0) {
			for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
				if (g_ascii_strcasecmp (*a, "w:val") != 0) {
					continue;
				}

				if (atoi (*v) >= 38) {
					info->style_element_present = TRUE;
				}
			}
		} else if (g_ascii_strcasecmp (element_name, "w:smartTag") == 0) {
			info->style_element_present = TRUE;
		} else if (g_ascii_strcasecmp (element_name, "w:sdtContent") == 0) {
			info->style_element_present = TRUE;
		} else if (g_ascii_strcasecmp (element_name, "w:hyperlink") == 0) {
			info->style_element_present = TRUE;
		} else if (g_ascii_strcasecmp (element_name, "w:t") == 0) {
			for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
				if (g_ascii_strcasecmp (*a, "xml:space") != 0) {
					continue;
				}

				if (g_ascii_strncasecmp (*v, "preserve", 8) == 0) {
					info->preserve_attribute_present = TRUE;
				}
			}

			info->tag_type = MS_OFFICE_XML_TAG_WORD_TEXT;
		}
		break;

	case FILE_TYPE_XLSX:
		if (g_ascii_strcasecmp (element_name, "sheet") == 0) {
			for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
				if (g_ascii_strcasecmp (*a, "name") == 0) {
					info->tag_type = MS_OFFICE_XML_TAG_XLS_SHARED_TEXT;
				}
			}

		} else if (g_ascii_strcasecmp (element_name, "t") == 0) {
			info->tag_type = MS_OFFICE_XML_TAG_XLS_SHARED_TEXT;
		}
		break;

	case FILE_TYPE_PPTX:
	case FILE_TYPE_PPSX:
		info->tag_type = MS_OFFICE_XML_TAG_SLIDE_TEXT;
		break;

	case FILE_TYPE_INVALID:
		g_message ("Microsoft document type:%d invalid", info->file_type);
		break;
	}
}

static void
msoffice_xml_content_parse_stop (GMarkupParseContext  *context,
                                 const gchar          *element_name,
                                 gpointer              user_data,
                                 GError              **error)
{
	MsOfficeXMLParserInfo *info = user_data;

	if (g_ascii_strcasecmp (element_name, "w:p") == 0) {
		info->style_element_present = FALSE;
		info->preserve_attribute_present = FALSE;
	}

	/* Reset tag */
	info->tag_type = MS_OFFICE_XML_TAG_INVALID;
}

static void
msoffice_xml_content_parse (GMarkupParseContext  *context,
                            const gchar          *text,
                            gsize                 text_len,
                            gpointer              user_data,
                            GError              **error)
{
	MsOfficeXMLParserInfo *info = user_data;
	gsize written_bytes = 0;

	/* If reached max bytes to extract, just return */
	if (info->bytes_pending == 0) {
		g_set_error_literal (error,
		                     maximum_size_error_quark,
		                     0,
		                     "Maximum text limit reached");
		return;
	}

	/* Create content string if not already done before */
	if (G_UNLIKELY (info->content == NULL)) {
		info->content =	g_string_new ("");
	}

	switch (info->tag_type) {
	case MS_OFFICE_XML_TAG_WORD_TEXT:
		tracker_text_validate_utf8 (text,
		                            MIN (text_len, info->bytes_pending),
		                            &info->content,
		                            &written_bytes);
		g_string_append_c (info->content, ' ');
		info->bytes_pending -= written_bytes;
		break;

	case MS_OFFICE_XML_TAG_SLIDE_TEXT:
		tracker_text_validate_utf8 (text,
		                            MIN (text_len, info->bytes_pending),
		                            &info->content,
		                            &written_bytes);
		g_string_append_c (info->content, ' ');
		info->bytes_pending -= written_bytes;
		break;

	case MS_OFFICE_XML_TAG_XLS_SHARED_TEXT:
		if (atoi (text) == 0)  {
			tracker_text_validate_utf8 (text,
			                            MIN (text_len, info->bytes_pending),
			                            &info->content,
			                            &written_bytes);
			g_string_append_c (info->content, ' ');
			info->bytes_pending -= written_bytes;
		}
		break;

	/* Ignore tags that may not happen inside the text subdocument */
	case MS_OFFICE_XML_TAG_TITLE:
	case MS_OFFICE_XML_TAG_SUBJECT:
	case MS_OFFICE_XML_TAG_AUTHOR:
	case MS_OFFICE_XML_TAG_COMMENTS:
	case MS_OFFICE_XML_TAG_CREATED:
	case MS_OFFICE_XML_TAG_GENERATOR:
	case MS_OFFICE_XML_TAG_APPLICATION:
	case MS_OFFICE_XML_TAG_MODIFIED:
	case MS_OFFICE_XML_TAG_NUM_OF_PAGES:
	case MS_OFFICE_XML_TAG_NUM_OF_CHARACTERS:
	case MS_OFFICE_XML_TAG_NUM_OF_WORDS:
	case MS_OFFICE_XML_TAG_NUM_OF_LINES:
	case MS_OFFICE_XML_TAG_NUM_OF_PARAGRAPHS:
	case MS_OFFICE_XML_TAG_DOCUMENT_CORE_DATA:
	case MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA:
	case MS_OFFICE_XML_TAG_INVALID:
		break;
	}
}

/* ------------------------- METADATA files parsing -----------------------------------*/

static void
msoffice_xml_metadata_parse_start (GMarkupParseContext  *context,
                                   const gchar           *element_name,
                                   const gchar          **attribute_names,
                                   const gchar          **attribute_values,
                                   gpointer               user_data,
                                   GError               **error)
{
	MsOfficeXMLParserInfo *info = user_data;

	/* Setup the proper tag type */
	if (g_ascii_strcasecmp (element_name, "dc:title") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_TITLE;
	} else if (g_ascii_strcasecmp (element_name, "dc:subject") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_SUBJECT;
	} else if (g_ascii_strcasecmp (element_name, "dc:creator") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_AUTHOR;
	} else if (g_ascii_strcasecmp (element_name, "dc:description") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_COMMENTS;
	} else if (g_ascii_strcasecmp (element_name, "dcterms:created") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_CREATED;
	} else if (g_ascii_strcasecmp (element_name, "meta:generator") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_GENERATOR;
	} else if (g_ascii_strcasecmp (element_name, "dcterms:modified") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_MODIFIED;
	} else if (g_ascii_strcasecmp (element_name, "Pages") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_NUM_OF_PAGES;
	} else if (g_ascii_strcasecmp (element_name, "Slides") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_NUM_OF_PAGES;
	} else if (g_ascii_strcasecmp (element_name, "Paragraphs") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_NUM_OF_PARAGRAPHS;
	} else if (g_ascii_strcasecmp (element_name, "Characters") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_NUM_OF_CHARACTERS;
	} else if (g_ascii_strcasecmp (element_name, "Words") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_NUM_OF_WORDS;
	} else if (g_ascii_strcasecmp (element_name, "Lines") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_NUM_OF_LINES;
	} else if (g_ascii_strcasecmp (element_name, "Application") == 0) {
		info->tag_type = MS_OFFICE_XML_TAG_APPLICATION;
	} else {
		info->tag_type = MS_OFFICE_XML_TAG_INVALID;
	}
}

static void
msoffice_xml_metadata_parse_stop (GMarkupParseContext  *context,
                                  const gchar          *element_name,
                                  gpointer              user_data,
                                  GError              **error)
{
	/* Reset tag */
	((MsOfficeXMLParserInfo *)user_data)->tag_type = MS_OFFICE_XML_TAG_INVALID;
}

static void
msoffice_xml_metadata_parse (GMarkupParseContext  *context,
                             const gchar          *text,
                             gsize                 text_len,
                             gpointer              user_data,
                             GError              **error)
{
	MsOfficeXMLParserInfo *info = user_data;

	switch (info->tag_type) {
	/* Ignore tags that may not happen inside the core subdocument */
	case MS_OFFICE_XML_TAG_WORD_TEXT:
	case MS_OFFICE_XML_TAG_SLIDE_TEXT:
	case MS_OFFICE_XML_TAG_XLS_SHARED_TEXT:
		break;

	case MS_OFFICE_XML_TAG_TITLE:
		if (info->title_already_set) {
			g_warning ("Avoiding additional title (%s) in MsOffice XML document '%s'",
			           text, info->uri);
		} else {
			info->title_already_set = TRUE;
			tracker_sparql_builder_predicate (info->metadata, "nie:title");
			tracker_sparql_builder_object_unvalidated (info->metadata, text);
		}
		break;

	case MS_OFFICE_XML_TAG_SUBJECT:
		tracker_sparql_builder_predicate (info->metadata, "nie:subject");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		break;

	case MS_OFFICE_XML_TAG_AUTHOR:
		tracker_sparql_builder_predicate (info->metadata, "nco:publisher");

		tracker_sparql_builder_object_blank_open (info->metadata);
		tracker_sparql_builder_predicate (info->metadata, "a");
		tracker_sparql_builder_object (info->metadata, "nco:Contact");

		tracker_sparql_builder_predicate (info->metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		tracker_sparql_builder_object_blank_close (info->metadata);
		break;

	case MS_OFFICE_XML_TAG_COMMENTS:
		tracker_sparql_builder_predicate (info->metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		break;

	case MS_OFFICE_XML_TAG_CREATED: {
		gchar *date;

		date = tracker_date_guess (text);
		tracker_sparql_builder_predicate (info->metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (info->metadata, date);
		g_free (date);
		break;
	}

	case MS_OFFICE_XML_TAG_GENERATOR:
		if (info->generator_already_set) {
			g_warning ("Avoiding additional generator (%s) in MsOffice XML document '%s'",
			           text, info->uri);
		} else {
			info->generator_already_set = TRUE;
			tracker_sparql_builder_predicate (info->metadata, "nie:generator");
			tracker_sparql_builder_object_unvalidated (info->metadata, text);
		}
		break;

	case MS_OFFICE_XML_TAG_APPLICATION:
		/* FIXME: Same code as MS_OFFICE_XML_TAG_GENERATOR should be
		 * used, but nie:generator has max cardinality of 1
		 * and this would cause errors.
		 */
		break;

	case MS_OFFICE_XML_TAG_MODIFIED: {
		gchar *date;

                date = tracker_date_guess (text);
		tracker_sparql_builder_predicate (info->metadata, "nie:contentLastModified");
		tracker_sparql_builder_object_unvalidated (info->metadata, date);
                g_free (date);
		break;
	}

	case MS_OFFICE_XML_TAG_NUM_OF_PAGES:
		tracker_sparql_builder_predicate (info->metadata, "nfo:pageCount");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		break;

	case MS_OFFICE_XML_TAG_NUM_OF_CHARACTERS:
		tracker_sparql_builder_predicate (info->metadata, "nfo:characterCount");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		break;

	case MS_OFFICE_XML_TAG_NUM_OF_WORDS:
		tracker_sparql_builder_predicate (info->metadata, "nfo:wordCount");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		break;

	case MS_OFFICE_XML_TAG_NUM_OF_LINES:
		tracker_sparql_builder_predicate (info->metadata, "nfo:lineCount");
		tracker_sparql_builder_object_unvalidated (info->metadata, text);
		break;

	case MS_OFFICE_XML_TAG_NUM_OF_PARAGRAPHS:
		/* TODO: There is no ontology for this. */
		break;

	case MS_OFFICE_XML_TAG_DOCUMENT_CORE_DATA:
	case MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA:
		/* Nothing as we are using it in defining type of data */
		break;

	case MS_OFFICE_XML_TAG_INVALID:
		/* Here we cant use log otheriwse it will print for other non useful files */
		break;
	}
}

/* ------------------------- CONTENT-TYPES file parsing -----------------------------------*/

static gboolean
xml_read (MsOfficeXMLParserInfo *parser_info,
          const gchar           *xml_filename,
          MsOfficeXMLTagType     type)
{
	GMarkupParseContext *context;

	switch (type) {
	case MS_OFFICE_XML_TAG_DOCUMENT_CORE_DATA: {
		/* Reset these flags before going on */
		parser_info->tag_type = MS_OFFICE_XML_TAG_INVALID;

		context = g_markup_parse_context_new (&metadata_parser,
		                                      0,
		                                      parser_info,
		                                      NULL);
		break;
	}
	case MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA: {
		/* Reset these flags before going on */
		parser_info->tag_type = MS_OFFICE_XML_TAG_INVALID;
		parser_info->style_element_present = FALSE;
		parser_info->preserve_attribute_present = FALSE;

		context = g_markup_parse_context_new (&content_parser,
		                                      0,
		                                      parser_info,
		                                      NULL);
		break;
	}
	default:
		context = NULL;
		break;
	}

	if (context) {
		GError *error = NULL;

		/* Load the internal XML file from the Zip archive, and parse it
		 * using the given context */
		tracker_gsf_parse_xml_in_zip (parser_info->uri,
		                              xml_filename,
		                              context,
		                              &error);
		g_markup_parse_context_free (context);

		if (error) {
			g_debug ("Parsing internal '%s' gave error: '%s'",
			         xml_filename,
			         error->message);
			g_error_free (error);
		}
	}

	return TRUE;
}

static void
msoffice_xml_content_types_parse_start (GMarkupParseContext  *context,
                                        const gchar          *element_name,
                                        const gchar         **attribute_names,
                                        const gchar         **attribute_values,
                                        gpointer              user_data,
                                        GError              **error)
{
	MsOfficeXMLParserInfo *info = user_data;
	const gchar *part_name = NULL;
	const gchar *content_type = NULL;
	gint i;

	if (g_ascii_strcasecmp (element_name, "Override") != 0) {
		return;
	}

	/* Look for part name and content type */
	for (i = 0; attribute_names[i]; i++) {
		if (g_ascii_strcasecmp (attribute_names[i], "PartName") == 0) {
			part_name = attribute_values[i];
		} else if (g_ascii_strcasecmp (attribute_names[i], "ContentType") == 0) {
			content_type = attribute_values[i];
		}
	}

	/* Both part_name and content_type MUST be NON-NULL */
	if (!part_name || !content_type) {
		g_message ("Invalid file (part_name:%s, content_type:%s)",
		           part_name ? part_name : "none",
		           content_type ? content_type : "none");
		return;
	}

	/* Metadata part? */
	if ((g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-package.core-properties+xml") == 0) ||
	    (g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.extended-properties+xml") == 0)) {
		xml_read (info, part_name + 1, MS_OFFICE_XML_TAG_DOCUMENT_CORE_DATA);
		return;
	}

	/* If the file type is unknown, skip trying to extract content */
	if (info->file_type == FILE_TYPE_INVALID) {
		g_message ("Invalid file type, not extracting content from '%s'",
		           part_name + 1);
		return;
	}

	/* Content part? */
	if ((info->file_type == FILE_TYPE_DOCX &&
	     g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml") == 0) ||
	    ((info->file_type == FILE_TYPE_PPTX || info->file_type == FILE_TYPE_PPSX) &&
	     (g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.presentationml.slide+xml") == 0 ||
	      g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.drawingml.diagramData+xml") == 0)) ||
	    (info->file_type == FILE_TYPE_XLSX &&
	     (g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml") == 0 ||
	      g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml") == 0))) {
		/* If reached max bytes to extract, don't event start parsing the file... just return */
		if (info->bytes_pending == 0) {
			g_debug ("Skipping '%s' as already reached max bytes to extract",
			         part_name + 1);
		} else {
			xml_read (info, part_name + 1, MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA);
		}
	}
}

/* ------------------------- Main methods -----------------------------------*/

static MsOfficeXMLFileType
msoffice_xml_get_file_type (const gchar *uri)
{
	GFile *file;
	GFileInfo *file_info;
	const gchar *mime_used;
	MsOfficeXMLFileType file_type;

	/* Get GFile from uri... */
	file = g_file_new_for_uri (uri);
	if (!file) {
		g_warning ("Could not create GFile for URI:'%s'", uri);
		return FILE_TYPE_INVALID;
	}

	/* Get GFileInfo from GFile... (synchronous) */
	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
	                               G_FILE_QUERY_INFO_NONE,
	                               NULL,
	                               NULL);
	g_object_unref (file);
	if (!file_info) {
		g_warning ("Could not get GFileInfo for URI:'%s'", uri);
		return FILE_TYPE_INVALID;
	}

	/* Get Content Type from GFileInfo. The constant string will be valid
	 * as long as the file info reference is valid */
	mime_used = g_file_info_get_content_type (file_info);
	if (g_ascii_strcasecmp (mime_used, "application/vnd.openxmlformats-officedocument.wordprocessingml.document") == 0) {
		/* MsOffice Word document */
		file_type = FILE_TYPE_DOCX;
	} else if (g_ascii_strcasecmp (mime_used, "application/vnd.openxmlformats-officedocument.presentationml.presentation") == 0) {
		/* MsOffice Powerpoint document */
		file_type = FILE_TYPE_PPTX;
	} else if (g_ascii_strcasecmp (mime_used, "application/vnd.openxmlformats-officedocument.presentationml.slideshow") == 0) {
		/* MsOffice Powerpoint (slideshow) document */
		file_type = FILE_TYPE_PPSX;
	} else if (g_ascii_strcasecmp (mime_used, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet") == 0) {
		/* MsOffice Excel document */
		file_type = FILE_TYPE_XLSX;
	} else {
		g_message ("Mime type was not recognised:'%s'", mime_used);
		file_type = FILE_TYPE_INVALID;
	}

	g_object_unref (file_info);

	return file_type;
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (const gchar          *uri,
                              const gchar          *mimetype,
                              TrackerSparqlBuilder *preupdate,
                              TrackerSparqlBuilder *metadata,
                              GString              *where)
{
	MsOfficeXMLParserInfo info;
	MsOfficeXMLFileType file_type;
	TrackerConfig *config;
	GMarkupParseContext *context = NULL;
	GError *error = NULL;

	if (G_UNLIKELY (maximum_size_error_quark == 0)) {
		maximum_size_error_quark = g_quark_from_static_string ("maximum_size_error");
	}

	/* Get current Content Type */
	file_type = msoffice_xml_get_file_type (uri);

	/* Setup conf */
	config = tracker_main_get_config ();

	g_debug ("Extracting MsOffice XML format...");

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	/* Setup Parser info */
	info.metadata = metadata;
	info.file_type = file_type;
	info.tag_type = MS_OFFICE_XML_TAG_INVALID;
	info.style_element_present = FALSE;
	info.preserve_attribute_present = FALSE;
	info.uri = uri;
	info.content = NULL;
	info.title_already_set = FALSE;
	info.generator_already_set = FALSE;
	info.bytes_pending = tracker_config_get_max_bytes (config);

	/* Create content-type parser context */
	context = g_markup_parse_context_new (&content_types_parser,
	                                      0,
	                                      &info,
	                                      NULL);

	/* Load the internal XML file from the Zip archive, and parse it
	 * using the given context */
	tracker_gsf_parse_xml_in_zip (uri,
	                              "[Content_Types].xml",
	                              context,
	                              &error);
	if (error) {
		g_debug ("Parsing the content-types file gave an error: '%s'",
		         error->message);
		g_error_free (error);
	}

	/* If we got any content, add it */
	if (info.content) {
		gchar *content;

		content = g_string_free (info.content, FALSE);
		info.content = NULL;

		if (content) {
			tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
			tracker_sparql_builder_object_unvalidated (metadata, content);
			g_free (content);
		}
	}

	g_markup_parse_context_free (context);

	return TRUE;
}
