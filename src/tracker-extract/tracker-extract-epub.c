/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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

#include <libtracker-extract/tracker-extract.h>

#include "tracker-main.h"
#include "tracker-gsf.h"
#include "tracker-read.h"

#include <unistd.h>

typedef enum {
	OPF_TAG_TYPE_UNKNOWN,
	OPF_TAG_TYPE_TITLE,
	OPF_TAG_TYPE_AUTHOR,
	OPF_TAG_TYPE_CREATED
} OPFTagType;

typedef struct {
	TrackerSparqlBuilder *preupdate;
	TrackerSparqlBuilder *metadata;
	OPFTagType element;
	GList *pages;
	guint in_metadata : 1;
	guint in_manifest : 1;
} OPFData;

typedef struct {
	GString *contents;
	gsize limit;
} OPFContentData;

/* Methods to parse the container.xml file
 * pointing to the real metadata/content
 */
static void
container_xml_start_element_handler (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     gpointer              user_data,
                                     GError              **error)
{
	gchar **path_out = user_data;
	gint i;

	if (g_strcmp0 (element_name, "rootfile") != 0) {
		return;
	}

	for (i = 0; attribute_names[i] != NULL; i++) {
		if (g_strcmp0 (attribute_names[i], "full-path") == 0) {
			if (!*path_out) {
				*path_out = g_strdup (attribute_values[i]);
			}
			break;
		}
	}
}

/* Methods to parse the OPF document metadata/layout */
static void
opf_xml_start_element_handler (GMarkupParseContext  *context,
                               const gchar          *element_name,
                               const gchar         **attribute_names,
                               const gchar         **attribute_values,
                               gpointer              user_data,
                               GError              **error)
{
	OPFData *data = user_data;
	gint i;

	if (g_strcmp0 (element_name, "metadata") == 0) {
		data->in_metadata = TRUE;
	} else if (g_strcmp0 (element_name, "manifest") == 0) {
		data->in_manifest = TRUE;
	} else if (data->in_metadata) {
		/* epub metadata */
		if (g_strcmp0 (element_name, "dc:title") == 0) {
			data->element = OPF_TAG_TYPE_TITLE;
		} else if (g_strcmp0 (element_name, "dc:creator") == 0) {
			for (i = 0; attribute_names[i] != NULL; i++) {
				if (g_strcmp0 (attribute_names[i], "opf:role") == 0 &&
				    g_strcmp0 (attribute_values[i], "aut") == 0) {
					data->element = OPF_TAG_TYPE_AUTHOR;
					break;
				}
			}
		} else if (g_strcmp0 (element_name, "dc:date") == 0) {
			for (i = 0; attribute_names[i] != NULL; i++) {
				if (g_strcmp0 (attribute_names[i], "opf:event") == 0 &&
				    g_strcmp0 (attribute_values[i], "original-publication") == 0) {
					data->element = OPF_TAG_TYPE_CREATED;
					break;
				}
			}
		}
	} else if (data->in_manifest &&
		   g_strcmp0 (element_name, "item") == 0) {
		const gchar *rel_path = NULL;
		gboolean is_xhtml = FALSE;

		/* Keep list of xhtml documents for plain text extraction */
		for (i = 0; attribute_names[i] != NULL; i++) {
			if (g_strcmp0 (attribute_names[i], "href") == 0) {
				rel_path = attribute_values[i];
			} else if (g_strcmp0 (attribute_names[i], "media-type") == 0 &&
				   g_strcmp0 (attribute_values[i], "application/xhtml+xml") == 0) {
				is_xhtml = TRUE;
			}
		}

		if (is_xhtml && rel_path) {
			data->pages = g_list_append (data->pages, g_strdup (rel_path));
		}
	}
}

static void
opf_xml_end_element_handler (GMarkupParseContext  *context,
                             const gchar          *element_name,
                             gpointer              user_data,
                             GError              **error)
{
	OPFData *data = user_data;

	if (g_strcmp0 (element_name, "metadata") == 0) {
		data->in_metadata = FALSE;
	} else if (g_strcmp0 (element_name, "manifest") == 0) {
		data->in_manifest = FALSE;
	} else {
		data->element = OPF_TAG_TYPE_UNKNOWN;
	}
}

static void
opf_xml_text_handler (GMarkupParseContext   *context,
                      const gchar           *text,
                      gsize                  text_len,
                      gpointer               user_data,
                      GError               **error)
{
	OPFData *data = user_data;
	gchar *date;

	switch (data->element) {
	case OPF_TAG_TYPE_AUTHOR:
		tracker_sparql_builder_predicate (data->metadata, "nco:publisher");

		tracker_sparql_builder_object_blank_open (data->metadata);
		tracker_sparql_builder_predicate (data->metadata, "a");
		tracker_sparql_builder_object (data->metadata, "nco:Contact");

		tracker_sparql_builder_predicate (data->metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (data->metadata, text);
		tracker_sparql_builder_object_blank_close (data->metadata);
		break;
	case OPF_TAG_TYPE_TITLE:
		tracker_sparql_builder_predicate (data->metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (data->metadata, text);
		break;
	case OPF_TAG_TYPE_CREATED:
		date = tracker_date_guess (text);
		tracker_sparql_builder_predicate (data->metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (data->metadata, date);
		g_free (date);
		break;
	case OPF_TAG_TYPE_UNKNOWN:
	default:
		break;
	}
}

/* Methods to extract XHTML text content */
static void
content_xml_text_handler (GMarkupParseContext   *context,
			  const gchar           *text,
			  gsize                  text_len,
			  gpointer               user_data,
			  GError               **error)
{
	OPFContentData *content_data = user_data;
	gsize written_bytes = 0;

	if (text_len <= 0) {
		return;
	}

	if (tracker_text_validate_utf8 (text,
	                                MIN (text_len, content_data->limit),
	                                &content_data->contents,
	                                &written_bytes)) {
		if (content_data->contents->str[content_data->contents->len - 1] != ' ') {
			g_string_append_c (content_data->contents, ' ');
		}
	}

	content_data->limit -= written_bytes;
}

static gchar *
extract_opf_path (const gchar *uri)
{
	GMarkupParseContext *context;
	gchar *path = NULL;
	GError *error = NULL;
	GMarkupParser parser = {
		container_xml_start_element_handler,
		NULL, NULL, NULL, NULL
	};

	/* Create parsing context */
	context = g_markup_parse_context_new (&parser, 0, &path, NULL);

	/* Load the internal container file from the Zip archive,
	 * and parse it to extract the .opf file to get metadata from
	 */
	tracker_gsf_parse_xml_in_zip (uri, "META-INF/container.xml", context, &error);
	g_markup_parse_context_free (context);

	if (error || !path) {
		g_warning ("Could not get EPUB container.xml file: %s\n",
		           (error) ? error->message : "No error provided");
		g_error_free (error);
		return NULL;
	}

	return path;
}

static gchar *
extract_opf_contents (const gchar *uri,
		      const gchar *content_prefix,
		      GList       *content_files)
{
	OPFContentData content_data = { 0 };
	GMarkupParseContext *context;
	TrackerConfig *config;
	GError *error = NULL;
	GList *l;
	GMarkupParser xml_parser = {
		NULL, NULL,
		content_xml_text_handler,
		NULL, NULL
	};

	config = tracker_main_get_config ();
	context = g_markup_parse_context_new (&xml_parser, 0, &content_data, NULL);

	content_data.contents = g_string_new ("");
	content_data.limit = (gsize) tracker_config_get_max_bytes (config);

	g_debug ("Extracting up to %" G_GSIZE_FORMAT " bytes of content", content_data.limit);

	for (l = content_files; l; l = l->next) {
		gchar *path;

		/* Page file is relative to OPF file location */
		path = g_build_filename (content_prefix, l->data, NULL);
		tracker_gsf_parse_xml_in_zip (uri, path, context, &error);
		g_free (path);

		if (error) {
			g_warning ("Error extracting EPUB contents: %s\n",
				   error->message);
			break;
		}

		if (content_data.limit <= 0) {
			/* Reached plain text extraction limit */
			break;
		}
	}

	g_markup_parse_context_free (context);

	return g_string_free (content_data.contents, FALSE);
}

static gboolean
extract_opf (const gchar          *uri,
	     const gchar          *opf_path,
	     TrackerSparqlBuilder *preupdate,
	     TrackerSparqlBuilder *metadata)
{
	GMarkupParseContext *context;
	OPFData data = { 0 };
	GError *error = NULL;
	gchar *dirname, *contents;
	GMarkupParser opf_parser = {
		opf_xml_start_element_handler,
		opf_xml_end_element_handler,
		opf_xml_text_handler,
		NULL, NULL
	};

	g_debug ("Extracting OPF file contents from EPUB '%s'", uri);

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:TextDocument");

	data.metadata = metadata;
	data.preupdate = preupdate;

	/* Create parsing context */
	context = g_markup_parse_context_new (&opf_parser, 0, &data, NULL);

	/* Load the internal container file from the Zip archive,
	 * and parse it to extract the .opf file to get metadata from
	 */
	tracker_gsf_parse_xml_in_zip (uri, opf_path, context, &error);
	g_markup_parse_context_free (context);

	if (error) {
		g_warning ("Could not get EPUB '%s' file: %s\n", opf_path,
		           (error) ? error->message : "No error provided");
		g_error_free (error);
		return FALSE;
	}

	dirname = g_path_get_dirname (opf_path);
	contents = extract_opf_contents (uri, dirname, data.pages);
	g_free (dirname);

	if (contents && *contents) {
		tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, contents);
	}

	g_list_foreach (data.pages, (GFunc) g_free, NULL);
	g_list_free (data.pages);
	g_free (contents);

	return TRUE;
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (const gchar          *uri,
                              const gchar          *mime_used,
                              TrackerSparqlBuilder *preupdate,
                              TrackerSparqlBuilder *metadata,
                              GString              *where)
{
	gchar *opf_path;

	opf_path = extract_opf_path (uri);

	if (!opf_path) {
		return FALSE;
	}

	extract_opf (uri, opf_path, preupdate, metadata);
	g_free (opf_path);

	return TRUE;
}
