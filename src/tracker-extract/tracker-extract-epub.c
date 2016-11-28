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
	OPF_TAG_TYPE_CREATED,

	OPF_TAG_TYPE_AUTHOR,
	OPF_TAG_TYPE_EDITOR,
	OPF_TAG_TYPE_ILLUSTRATOR,
	OPF_TAG_TYPE_CONTRIBUTOR,

	OPF_TAG_TYPE_LANGUAGE,
	OPF_TAG_TYPE_SUBJECT,
	OPF_TAG_TYPE_DESCRIPTION,
	OPF_TAG_TYPE_UUID,
	OPF_TAG_TYPE_ISBN,
	OPF_TAG_TYPE_PUBLISHER,
	OPF_TAG_TYPE_RATING  /* calibre addition, should it be indexed? how? */
} OPFTagType;

typedef struct {
	TrackerResource *resource;
	gchar *uri;

	OPFTagType element;
	GList *pages;
	guint in_metadata         : 1;
	guint in_manifest         : 1;
	guint has_publisher       : 1;
	guint has_title           : 1;
	guint has_content_created : 1;
	guint has_language        : 1;
	guint has_subject         : 1;
	guint has_description     : 1;
	guint has_identifier      : 1;
	gchar *savedstring;
} OPFData;

typedef struct {
	GString *contents;
	gsize limit;
} OPFContentData;

static inline OPFData *
opf_data_new (const char *uri,
              TrackerResource *resource)
{
	OPFData *data = g_slice_new0 (OPFData);

	data->uri = g_strdup (uri);
	data->resource = g_object_ref (resource);

	return data;
}

static inline void
opf_data_clear_saved_string (OPFData *data)
{
	if (!data || !data->savedstring) {
		return;
	}

	g_free (data->savedstring);
	data->savedstring = NULL;
}

static inline void
opf_data_free (OPFData *data)
{
	if (!data) {
		return;
	}

	g_free (data->savedstring);

	g_list_foreach (data->pages, (GFunc) g_free, NULL);
	g_list_free (data->pages);

	g_object_unref (data->resource);
	g_free (data->uri);

	g_slice_free (OPFData, data);
}

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
	gboolean has_role_attr = FALSE;

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
				if (g_strcmp0 (attribute_names[i], "opf:file-as") == 0) {
					g_debug ("Found creator file-as tag");
					data->savedstring = g_strdup (attribute_values[i]);
				} else if (g_strcmp0 (attribute_names[i], "opf:role") == 0) {
					has_role_attr = TRUE;
					if (g_strcmp0 (attribute_values[i], "aut") == 0) {
						data->element = OPF_TAG_TYPE_AUTHOR;
					} else if (g_strcmp0 (attribute_values[i], "edt") == 0) {
						data->element = OPF_TAG_TYPE_EDITOR;
					} else if (g_strcmp0 (attribute_values[i], "ill") == 0) {
						data->element = OPF_TAG_TYPE_ILLUSTRATOR;
					} else {
						data->element = OPF_TAG_TYPE_UNKNOWN;
						opf_data_clear_saved_string (data);
						g_debug ("Unknown role, skipping");
					}
				}
			}
			if (!has_role_attr) {
				data->element = OPF_TAG_TYPE_AUTHOR;
			}
		} else if (g_strcmp0 (element_name, "dc:date") == 0) {
			for (i = 0; attribute_names[i] != NULL; i++) {
				if (g_strcmp0 (attribute_names[i], "opf:event") == 0 &&
				    g_strcmp0 (attribute_values[i], "original-publication") == 0) {
					data->element = OPF_TAG_TYPE_CREATED;
					break;
				}
			}
		} else if (g_strcmp0 (element_name, "dc:publisher") == 0) {
			data->element = OPF_TAG_TYPE_PUBLISHER;
		} else if (g_strcmp0 (element_name, "dc:description") == 0) {
			data->element = OPF_TAG_TYPE_DESCRIPTION;
		} else if (g_strcmp0 (element_name, "dc:language") == 0) {
			data->element = OPF_TAG_TYPE_LANGUAGE;
		} else if (g_strcmp0 (element_name, "dc:identifier") == 0) {
			data->element = OPF_TAG_TYPE_UUID;
			for (i = 0; attribute_names[i] != NULL; i++) {
				if (g_strcmp0 (attribute_names[i], "opf:scheme") == 0) {
					if (g_ascii_strncasecmp (attribute_values[i], "isbn", 4) == 0) {
						data->element = OPF_TAG_TYPE_ISBN;
					}
				}
			}
		/* } else if (g_strcmp0 (element_name, "meta") == 0) { */
		/* 	for (i = 0; attribute_names[i] != NULL; i++) { */
		/* 		if (g_strcmp0 (attribute_names[i], "name") == 0) { */
		/* 			if (g_strcmp0 (attribute_values[i], "calibre:rating") == 0) { */
		/* 				anybool = TRUE; */
		/* 			} */
		/* 		} else if (anybool && g_strcmp0 (attribute_names[i], "content")) { */
		/* 			data->element = OPF_TAG_TYPE_RATING; */
		/* 			data->savedstring = g_strdup (attribute_values[i]); */
		/* 		} */
		/* 	} */
		/* } else if (g_strcmp0 (element_name, "dc:subject") == 0) { */
		/* 	data->element = OPF_TAG_TYPE_SUBJECT; */
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

	switch (data->element) {
	case OPF_TAG_TYPE_PUBLISHER:
		if (data->has_publisher) {
			g_warning ("Avoiding additional publisher (%s) in EPUB '%s'",
			           text, data->uri);
		} else {
			TrackerResource *publisher;

			publisher = tracker_resource_new (NULL);
			tracker_resource_set_uri (publisher, "rdf:type", "nco:Contact");
			tracker_resource_set_string (publisher, "nco:fullname", text);

			tracker_resource_set_relation (data->resource, "nco:publisher", publisher);
			g_object_unref (publisher);

			data->has_publisher = TRUE;
		}
		break;
	case OPF_TAG_TYPE_AUTHOR:
	case OPF_TAG_TYPE_EDITOR:
	case OPF_TAG_TYPE_ILLUSTRATOR:
	case OPF_TAG_TYPE_CONTRIBUTOR: {
		TrackerResource *contact, *artist = NULL;
		gchar *fname, *gname, *oname;
		const gchar *fullname = NULL;
		gchar *role_uri = NULL;
		const gchar *role_str = NULL;
		gint i, j = 0, len;

		fname = NULL;
		gname = NULL;
		oname = NULL;

		/* parse name.  may not work for dissimilar cultures. */
		if (data->savedstring != NULL) {
			fullname = data->savedstring;

			/* <family name>, <given name> <other name> */
			g_debug ("Parsing 'opf:file-as' attribute:'%s'", data->savedstring);
			len = strlen (data->savedstring);

			for (i = 0; i < len; i++) {
				if (data->savedstring[i] == ',') {
					fname = g_strndup (data->savedstring, i);
					g_debug ("Found family name:'%s'", fname);

					for (; data->savedstring[i] == ',' || data->savedstring[i] == ' '; i++);
					j = i;

					break;
				}
			}

			if (i == len) {
				fname = g_strdup (data->savedstring);
				g_debug ("Found only one name");
			} else {
				for (; i <= len; i++) {
					if (i == len || data->savedstring[i] == ' ') {
						gname = g_strndup (data->savedstring + j, i - j);
						g_debug ("Found given name:'%s'", gname);

						for (; data->savedstring[i] == ',' || data->savedstring[i] == ' '; i++);

						if (i != len) {
							oname = g_strdup (data->savedstring + i);
							g_debug ("Found other name:'%s'", oname);
						}

						break;
					}
				}
			}
		} else {
			fullname = text;

			/* <given name> <other name> <family name> */
			g_debug ("Parsing name, no 'opf:file-as' found: '%s'", text);

			len = strlen (text);

			for (i = 0; i < len; i++) {
				if (text[i] == ' ') {
					gname = g_strndup (text, i);
					g_debug ("Found given name:'%s'", gname);
					j = i + 1;

					break;
				}
			}

			if (j == 0) {
				fname = g_strdup (data->savedstring);
				g_debug ("Found only one name:'%s'", fname);
			} else {
				for (i = len - 1; i >= j - 1; i--) {
					if (text[i] == ' ') {
						fname = g_strdup (text + i + 1);
						g_debug ("Found family name:'%s'", fname);

						if (i > j) {
							oname = g_strndup (text + j, i - j);
							g_debug ("Found other name:'%s'", oname);
						}

						break;
					}
				}
			}
		}

		/* Role details */
		role_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", fullname);

		if (data->element == OPF_TAG_TYPE_AUTHOR) {
			role_str = "nco:creator";
		} else if (data->element == OPF_TAG_TYPE_EDITOR && !data->has_publisher) {
			/* Should this be nco:contributor ?
			 * 'Editor' is a bit vague here.
			 */
			role_str = "nco:publisher";
		} else if (data->element == OPF_TAG_TYPE_ILLUSTRATOR) {
			/* There is no illustrator class, using contributor */
			role_str = "nco:contributor";
		} else {
			g_assert ("Unknown role");
		}

		if (role_uri) {
			artist = tracker_resource_new (role_uri);
			tracker_resource_set_uri (artist, "rdf:type", "nmm:Artist");
			tracker_resource_set_string (artist, "nmm:artistName", fullname);
		}

		/* Creator contact details */
		contact = tracker_resource_new (NULL);
		tracker_resource_set_uri (contact, "rdf:type",  "nco:PersonContact");
		tracker_resource_set_string (contact, "nco:fullname", fullname);

		if (fname) {
			tracker_resource_set_string (contact, "nco:nameFamily", fname);
			g_free (fname);
		}

		if (gname) {
			tracker_resource_set_string (contact, "nco:nameGiven", gname);
			g_free (gname);
		}

		if (oname) {
			tracker_resource_set_string (contact, "nco:nameAdditional", oname);
			g_free (oname);
		}

		if (role_uri) {
			tracker_resource_set_relation (contact, role_str, artist);
			g_free (role_uri);
		}

		tracker_resource_set_relation (data->resource, "nco:creator", contact);
		g_clear_object (&artist);
		g_object_unref (contact);

		break;
	}
	case OPF_TAG_TYPE_TITLE:
		if (data->has_title) {
			g_warning ("Avoiding additional title (%s) in EPUB '%s'",
			           text, data->uri);
		} else {
			data->has_title = TRUE;
			tracker_resource_set_string (data->resource, "nie:title", text);
		}
		break;
	case OPF_TAG_TYPE_CREATED: {
		if (data->has_content_created) {
			g_warning ("Avoiding additional creation time (%s) in EPUB '%s'",
			           text, data->uri);
		} else {
			gchar *date = tracker_date_guess (text);

			if (date) {
				data->has_content_created = TRUE;
				tracker_resource_set_string (data->resource, "nie:contentCreated", date);
				g_free (date);
			} else {
				g_warning ("Could not parse creation time (%s) in EPUB '%s'",
				           text, data->uri);
			}
		}
		break;
	}
	case OPF_TAG_TYPE_LANGUAGE:
		if (data->has_language) {
			g_warning ("Avoiding additional language (%s) in EPUB '%s'",
			           text, data->uri);
		} else {
			data->has_language = TRUE;
			tracker_resource_set_string (data->resource, "nie:language", text);
		}
		break;
	case OPF_TAG_TYPE_SUBJECT:
		if (data->has_subject) {
			g_warning ("Avoiding additional subject (%s) in EPUB '%s'",
			           text, data->uri);
		} else {
			data->has_subject = TRUE;
			tracker_resource_set_string (data->resource, "nie:subject", text);
		}
		break;
	case OPF_TAG_TYPE_DESCRIPTION:
		if (data->has_description) {
			g_warning ("Avoiding additional description (%s) in EPUB '%s'",
			           text, data->uri);
		} else {
			data->has_description = TRUE;
			tracker_resource_set_string (data->resource, "nie:description", text);
		}
		break;
	case OPF_TAG_TYPE_UUID:
	case OPF_TAG_TYPE_ISBN:
		if (data->has_identifier) {
			g_warning ("Avoiding additional identifier (%s) in EPUB '%s'",
			           text, data->uri);
		} else {
			data->has_identifier = TRUE;
			tracker_resource_set_string (data->resource, "nie:identifier", text);
		}
		break;
	/* case OPF_TAG_TYPE_RATING: */
	case OPF_TAG_TYPE_UNKNOWN:
	default:
		break;
	}

	opf_data_clear_saved_string (data);
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
	TrackerConfig *config;
	GError *error = NULL;
	GList *l;
	GMarkupParser xml_parser = {
		NULL, NULL,
		content_xml_text_handler,
		NULL, NULL
	};

	config = tracker_main_get_config ();

	content_data.contents = g_string_new ("");
	content_data.limit = (gsize) tracker_config_get_max_bytes (config);

	g_debug ("Extracting up to %" G_GSIZE_FORMAT " bytes of content", content_data.limit);

	for (l = content_files; l; l = l->next) {
		GMarkupParseContext *context;
		gchar *path;

		context = g_markup_parse_context_new (&xml_parser, 0, &content_data, NULL);

		/* Page file is relative to OPF file location */
		path = g_build_filename (content_prefix, l->data, NULL);
		tracker_gsf_parse_xml_in_zip (uri, path, context, &error);

		if (error) {
			g_warning ("Error extracting EPUB contents (%s): %s",
				   path, error->message);
			g_clear_error (&error);
		}
		g_free (path);

		g_markup_parse_context_free (context);

		if (content_data.limit <= 0) {
			/* Reached plain text extraction limit */
			break;
		}
	}

	return g_string_free (content_data.contents, FALSE);
}

static TrackerResource *
extract_opf (const gchar          *uri,
             const gchar          *opf_path)
{
	TrackerResource *ebook;
	GMarkupParseContext *context;
	OPFData *data = NULL;
	GError *error = NULL;
	gchar *dirname, *contents;
	GMarkupParser opf_parser = {
		opf_xml_start_element_handler,
		opf_xml_end_element_handler,
		opf_xml_text_handler,
		NULL, NULL
	};

	g_debug ("Extracting OPF file contents from EPUB '%s'", uri);

	ebook = tracker_resource_new (NULL);
	tracker_resource_add_uri (ebook, "rdf:type", "nfo:EBook");

	data = opf_data_new (uri, ebook);

	/* Create parsing context */
	context = g_markup_parse_context_new (&opf_parser, 0, data, NULL);

	/* Load the internal container file from the Zip archive,
	 * and parse it to extract the .opf file to get metadata from
	 */
	tracker_gsf_parse_xml_in_zip (uri, opf_path, context, &error);
	g_markup_parse_context_free (context);

	if (error) {
		g_warning ("Could not get EPUB '%s' file: %s\n", opf_path,
		           (error) ? error->message : "No error provided");
		g_error_free (error);
		opf_data_free (data);
		g_object_unref (ebook);
		return NULL;
	}

	dirname = g_path_get_dirname (opf_path);
	contents = extract_opf_contents (uri, dirname, data->pages);
	g_free (dirname);

	if (contents && *contents) {
		tracker_resource_set_string (ebook, "nie:plainTextContent", contents);
	}

	opf_data_free (data);
	g_free (contents);

	return ebook;
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerResource *ebook;
	gchar *opf_path, *uri;
	GFile *file;

	file = tracker_extract_info_get_file (info);
	uri = g_file_get_uri (file);

	opf_path = extract_opf_path (uri);

	if (!opf_path) {
		g_free (uri);
		return FALSE;
	}

	ebook = extract_opf (uri, opf_path);
	g_free (opf_path);
	g_free (uri);

	tracker_extract_info_set_resource (info, ebook);
	g_object_unref (ebook);

	return TRUE;
}
