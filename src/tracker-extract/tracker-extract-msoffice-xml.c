/*
 * Copyright (C) 2008-2010 Nokia <ivan.frade@nokia.com>
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
	TrackerSparqlBuilder *metadata;
	MsOfficeXMLFileType file_type;
	MsOfficeXMLTagType tag_type;
	gboolean style_element_present;
	gboolean preserve_attribute_present;
	const gchar *uri;
	GString *content;
	gboolean title_already_set;
	gboolean generator_already_set;
	gulong bytes_pending;
} MsOfficeXMLParserInfo;

static GQuark maximum_size_error_quark = 0;

static void extract_msoffice_xml (const gchar          *uri,
                                  TrackerSparqlBuilder *preupdate,
                                  TrackerSparqlBuilder *metadata);

static TrackerExtractData data[] = {
	/* MSoffice2007*/
	{ "application/vnd.openxmlformats-officedocument.presentationml.presentation", extract_msoffice_xml },
	{ "application/vnd.openxmlformats-officedocument.presentationml.slideshow",    extract_msoffice_xml },
	{ "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",         extract_msoffice_xml },
	{ "application/vnd.openxmlformats-officedocument.wordprocessingml.document",   extract_msoffice_xml },
	{ NULL, NULL }
};

static void
xml_start_element_handler_text_data (GMarkupParseContext  *context,
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
xml_end_element_handler_document_data (GMarkupParseContext  *context,
                                       const gchar          *element_name,
                                       gpointer              user_data,
                                       GError              **error)
{
	MsOfficeXMLParserInfo *info = user_data;

	if (g_ascii_strcasecmp (element_name, "w:p") == 0) {
		info->style_element_present = FALSE;
		info->preserve_attribute_present = FALSE;
	}

	((MsOfficeXMLParserInfo*) user_data)->tag_type = MS_OFFICE_XML_TAG_INVALID;
}

static void
xml_start_element_handler_core_data (GMarkupParseContext  *context,
                                     const gchar           *element_name,
                                     const gchar          **attribute_names,
                                     const gchar          **attribute_values,
                                     gpointer               user_data,
                                     GError               **error)
{
	MsOfficeXMLParserInfo *info = user_data;

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
	} else if (g_ascii_strcasecmp (element_name, "cp:lastModifiedBy") == 0) {
		/* Do nothing ? */
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
xml_core_handler_document_data (GMarkupParseContext  *context,
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

static void
xml_text_handler_document_data (GMarkupParseContext  *context,
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

static gboolean
xml_read (MsOfficeXMLParserInfo *parser_info,
          const gchar           *xml_filename,
          MsOfficeXMLTagType     type)
{
	GMarkupParseContext *context;
	MsOfficeXMLParserInfo info;
	TrackerConfig *config;

	/* Setup conf */
	config = tracker_main_get_config ();

	/* FIXME: Can we use the original info here? */
	info.metadata = parser_info->metadata;
	info.file_type = parser_info->file_type;
	info.tag_type = MS_OFFICE_XML_TAG_INVALID;
	info.style_element_present = FALSE;
	info.preserve_attribute_present = FALSE;
	info.uri = parser_info->uri;
	info.content = parser_info->content;
	info.title_already_set = parser_info->title_already_set;
	info.bytes_pending = tracker_config_get_max_bytes (config);
	switch (type) {
	case MS_OFFICE_XML_TAG_DOCUMENT_CORE_DATA: {
		GMarkupParser parser = {
			xml_start_element_handler_core_data,
			xml_end_element_handler_document_data,
			xml_core_handler_document_data,
			NULL,
			NULL
		};

		context = g_markup_parse_context_new (&parser,
		                                      0,
		                                      &info,
		                                      NULL);
		break;
	}

	case MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA: {
		GMarkupParser parser = {
			xml_start_element_handler_text_data,
			xml_end_element_handler_document_data,
			xml_text_handler_document_data,
			NULL,
			NULL
		};

		context = g_markup_parse_context_new (&parser,
		                                      0,
		                                      &info,
		                                      NULL);
		break;
	}

	default:
		context = NULL;
		break;
	}

	if (context) {
		/* Load the internal XML file from the Zip archive, and parse it
		 * using the given context */
		tracker_gsf_parse_xml_in_zip (parser_info->uri,
		                              xml_filename,
		                              context, NULL);
		g_markup_parse_context_free (context);
	}

	return TRUE;
}

static void
xml_start_element_handler_content_types (GMarkupParseContext  *context,
                                         const gchar          *element_name,
                                         const gchar         **attribute_names,
                                         const gchar         **attribute_values,
                                         gpointer              user_data,
                                         GError              **error)
{
	MsOfficeXMLParserInfo *info;
	const gchar *part_name;
	const gchar *content_type;
	gint i;

	info = user_data;

	if (g_ascii_strcasecmp (element_name, "Override") != 0) {
		info->tag_type = MS_OFFICE_XML_TAG_INVALID;
		return;
	}

	part_name = NULL;
	content_type = NULL;

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

	if ((g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-package.core-properties+xml") == 0) ||
	    (g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.extended-properties+xml") == 0)) {
		xml_read (info, part_name + 1, MS_OFFICE_XML_TAG_DOCUMENT_CORE_DATA);
		return;
	}

	switch (info->file_type) {
	case FILE_TYPE_DOCX:
		if (g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml") == 0) {
			xml_read (info, part_name + 1, MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA);
		}
		break;

	case FILE_TYPE_PPTX:
	case FILE_TYPE_PPSX:
		if ((g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.presentationml.slide+xml") == 0) ||
		    (g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.drawingml.diagramData+xml") == 0)) {
			xml_read (info, part_name + 1, MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA);
		}
		break;

	case FILE_TYPE_XLSX:
		if ((g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml") == 0) ||
		    (g_ascii_strcasecmp (content_type, "application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml") == 0)) {
			xml_read (info, part_name + 1, MS_OFFICE_XML_TAG_DOCUMENT_TEXT_DATA);
		}
		break;

	case FILE_TYPE_INVALID:
		g_message ("Invalid file type:'%d'", info->file_type);
		break;
	}
}

static MsOfficeXMLFileType
msoffice_xml_get_file_type (const gchar *uri)
{
	GFile *file;
	GFileInfo *file_info;
	const gchar *mime_used;

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

	/* Get Content Type from GFileInfo */
	mime_used = g_file_info_get_content_type (file_info);
	g_object_unref (file_info);

	/* MsOffice Word document? */
	if (g_ascii_strcasecmp (mime_used, "application/vnd.openxmlformats-officedocument.wordprocessingml.document") == 0) {
		return FILE_TYPE_DOCX;
	}

	/* MsOffice Powerpoint document? */
	if (g_ascii_strcasecmp (mime_used, "application/vnd.openxmlformats-officedocument.presentationml.presentation") == 0) {
		return FILE_TYPE_PPTX;
	}
	if (g_ascii_strcasecmp (mime_used, "application/vnd.openxmlformats-officedocument.presentationml.slideshow") == 0) {
		return FILE_TYPE_PPSX;
	}

	/* MsOffice Excel document? */
	if (g_ascii_strcasecmp (mime_used, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet") == 0) {
		return FILE_TYPE_XLSX;
	}

	g_message ("Mime type was not recognised:'%s'", mime_used);
	return FILE_TYPE_INVALID;
}

static void
extract_msoffice_xml (const gchar          *uri,
                      TrackerSparqlBuilder *preupdate,
                      TrackerSparqlBuilder *metadata)
{
	MsOfficeXMLParserInfo info;
	MsOfficeXMLFileType file_type;
	TrackerConfig *config;

	GMarkupParseContext *context = NULL;
	GError *error = NULL;
	gulong  total_bytes;
	GMarkupParser parser = {
		xml_start_element_handler_content_types,
		xml_end_element_handler_document_data,
		NULL,
		NULL,
		NULL
	};

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
	total_bytes = tracker_config_get_max_bytes (config);
	info.metadata = metadata;
	info.file_type = file_type;
	info.tag_type = MS_OFFICE_XML_TAG_INVALID;
	info.style_element_present = FALSE;
	info.preserve_attribute_present = FALSE;
	info.uri = uri;
	info.content = NULL;
	info.title_already_set = FALSE;
	info.bytes_pending = total_bytes;
	context = g_markup_parse_context_new (&parser, 0, &info, NULL);

	/* Load the internal XML file from the Zip archive, and parse it
	 * using the given context */
	tracker_gsf_parse_xml_in_zip (uri,
	                              "[Content_Types].xml",
	                              context,
	                              &error);

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
}

TrackerExtractData *
tracker_extract_get_data (void)
{
	return data;
}
