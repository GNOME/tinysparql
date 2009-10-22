/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Edward Duffy (eduffy@gmail.com)
 * Copyright (C) 2006, Laurent Aguerreche (laurent.aguerreche@free.fr)
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

#include <string.h>

#include <glib.h>

#include <gsf/gsf.h>
#include <gsf/gsf-doc-meta-data.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-utils.h>

#include <libtracker-common/tracker-utils.h>

#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-ontology.h>

#include "tracker-main.h"

#define NIE_PREFIX TRACKER_NIE_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

static void extract_msoffice (const gchar *uri,
			      TrackerSparqlBuilder   *metadata);

static TrackerExtractData data[] = {
	{ "application/msword",	  extract_msoffice },
	{ "application/vnd.ms-*", extract_msoffice },
	{ NULL, NULL }
};

typedef struct {
	TrackerSparqlBuilder *metadata;
	const gchar *uri;
} ForeachInfo;

static void
add_gvalue_in_metadata (TrackerSparqlBuilder    *table,
			const gchar  *uri,
			const gchar  *key,
			GValue const *val,
			const gchar *urn,
			const gchar *type,
			const gchar *predicate)
{
	g_return_if_fail (table != NULL);
	g_return_if_fail (key != NULL);

	if (val) {
		gchar *s = g_strdup_value_contents (val);

		if (s) {
			if (!tracker_is_empty_string (s)) {
				gchar *str_val;

				/* Some fun: strings are always
				 * written "str" with double quotes
				 * around, but not numbers!
				 */
				if (s[0] == '"') {
					size_t len;

					len = strlen (s);

					if (s[len - 1] == '"') {
						str_val = (len > 2 ? g_strndup (s + 1, len - 2) : NULL);
					} else {
						/* We have a string
						 * that begins with a
						 * double quote but
						 * which finishes by
						 * something different...
						 * We copy the string
						 * from the
						 * beginning.
						 */
						str_val = g_strdup (s);
					}
				} else {
					/* Here, we probably have a number */
					str_val = g_strdup (s);
				}

				if (str_val) {
					if (urn) {
						tracker_statement_list_insert (table, urn, RDF_TYPE, type);
						tracker_statement_list_insert (table, urn, predicate, str_val);
						tracker_statement_list_insert (table, uri, key, urn);
					} else {
						tracker_statement_list_insert (table, uri, key, str_val);
					}
					g_free (str_val);
				}
			}

			g_free (s);
		}
	}
}

static void
metadata_cb (gpointer key,
	     gpointer value,
	     gpointer user_data)
{
	ForeachInfo  *info = user_data;
	gchar	     *name;
	GsfDocProp   *property;
	TrackerSparqlBuilder    *metadata = info->metadata;
	GValue const *val;
	const gchar  *uri = info->uri;

	name = key;
	property = value;
	metadata = user_data;
	val = gsf_doc_prop_get_val (property);

	if (strcmp (name, "dc:title") == 0) {
		add_gvalue_in_metadata (metadata, uri, NIE_PREFIX "title", val, NULL, NULL, NULL);
	} else if (strcmp (name, "dc:subject") == 0) {
		add_gvalue_in_metadata (metadata, uri, NIE_PREFIX "subject", val, NULL, NULL, NULL);
	} else if (strcmp (name, "dc:creator") == 0) {
		add_gvalue_in_metadata (metadata, uri, NCO_PREFIX "creator", val, ":", NCO_PREFIX "Contact", NCO_PREFIX "fullname");
	} else if (strcmp (name, "dc:keywords") == 0) {
		gchar *keywords = g_strdup_value_contents (val);
		char *lasts, *keyw;
		size_t len;

		keyw = keywords;
		keywords = strchr (keywords, '"');
		if (keywords)
			keywords++;
		else 
			keywords = keyw;

		len = strlen (keywords);
		if (keywords[len - 1] == '"')
			keywords[len - 1] = '\0';

		for (keyw = strtok_r (keywords, ",; ", &lasts); keyw; 
		     keyw = strtok_r (NULL, ",; ", &lasts)) {
			tracker_statement_list_insert (metadata,
					  uri, NIE_PREFIX "keyword",
					  (const gchar*) keyw);
		}

		g_free (keyw);
	} else if (strcmp (name, "dc:description") == 0) {
		add_gvalue_in_metadata (metadata, uri, NIE_PREFIX "comment", val, NULL, NULL, NULL);
	} else if (strcmp (name, "gsf:page-count") == 0) {
		add_gvalue_in_metadata (metadata, uri, NFO_PREFIX "pageCount", val, NULL, NULL, NULL);
	} else if (strcmp (name, "gsf:word-count") == 0) {
		add_gvalue_in_metadata (metadata, uri, NFO_PREFIX "wordCount", val, NULL, NULL, NULL);
	} else if (strcmp (name, "meta:creation-date") == 0) {
		add_gvalue_in_metadata (metadata, uri, NIE_PREFIX "contentCreated", val, NULL, NULL, NULL);
	} else if (strcmp (name, "meta:generator") == 0) {
		add_gvalue_in_metadata (metadata, uri, NIE_PREFIX "generator", val, NULL, NULL, NULL);
	}
}

static void
doc_metadata_cb (gpointer key,
		 gpointer value,
		 gpointer user_data)
{
	ForeachInfo  *info = user_data;
	gchar	     *name;
	GsfDocProp   *property;
	TrackerSparqlBuilder    *metadata = info->metadata;
	GValue const *val;
	const gchar  *uri = info->uri;

	name = key;
	property = value;
	metadata = user_data;
	val = gsf_doc_prop_get_val (property);

	if (strcmp (name, "CreativeCommons_LicenseURL") == 0) {
		add_gvalue_in_metadata (metadata, uri, NIE_PREFIX "license", val, NULL, NULL, NULL);
	}
}

static gchar *
extract_content (const gchar *uri,
		 guint        n_words)
{
	gchar *path, *command, *output, *text;
	GError *error = NULL;

	path = g_filename_from_uri (uri, NULL, NULL);

	if (!path) {
		return NULL;
	}

	command = g_strdup_printf ("wvWare --charset utf-8 -1 -x wvText.xml %s", path);

	g_free (path);

	if (!g_spawn_command_line_sync (command, &output, NULL, NULL, &error)) {
		g_warning ("Could not extract text from '%s': %s", uri, error->message);
		g_error_free (error);
		g_free (command);

		return NULL;
	}

	text = tracker_text_normalize (output, n_words, NULL);

	g_free (command);
	g_free (output);

	return text;
}

static void
extract_msoffice (const gchar *uri,
		  TrackerSparqlBuilder   *metadata)
{
	GsfInput  *input;
	GsfInfile *infile;
	GsfInput  *stream;
	gchar     *filename, *content;
	gboolean   rdf_type_added = FALSE;
	TrackerFTSConfig *fts_config;
	guint n_words;

	gsf_init ();

	filename = g_filename_from_uri (uri, NULL, NULL);

	input = gsf_input_stdio_new (filename, NULL);

	if (!input) {
		g_free (filename);
		gsf_shutdown ();
		return;
	}

	infile = gsf_infile_msole_new (input, NULL);
	g_object_unref (G_OBJECT (input));

	if (!infile) {
		g_free (filename);
		gsf_shutdown ();
		return;
	}

	stream = gsf_infile_child_by_name (infile, "\05SummaryInformation");
	if (stream) {
		GsfDocMetaData *md;
		ForeachInfo info = { metadata, uri };

		md = gsf_doc_meta_data_new ();

		if (gsf_msole_metadata_read (stream, md)) {
			g_object_unref (md);
			g_object_unref (stream);
			g_free (filename);
			gsf_shutdown ();
			return;
		}

		tracker_statement_list_insert (metadata, uri, 
		                          RDF_TYPE, 
		                          NFO_PREFIX "PaginatedTextDocument");

		rdf_type_added = TRUE;

		gsf_doc_meta_data_foreach (md, metadata_cb, &info);

		g_object_unref (md);
		g_object_unref (stream);
	}

	stream = gsf_infile_child_by_name (infile, "\05DocumentSummaryInformation");
	if (stream) {
		GsfDocMetaData *md;
		ForeachInfo info = { metadata, uri };

		md = gsf_doc_meta_data_new ();

		if (gsf_msole_metadata_read (stream, md)) {
			g_object_unref (md);
			g_object_unref (stream);
			gsf_shutdown ();
			g_free (filename);
			return;
		}

		if (!rdf_type_added) {
			tracker_statement_list_insert (metadata, uri, 
			                          RDF_TYPE, 
			                          NFO_PREFIX "PaginatedTextDocument");
			rdf_type_added = TRUE;
		}

		gsf_doc_meta_data_foreach (md, doc_metadata_cb, &info);

		g_object_unref (md);
		g_object_unref (stream);
	}

	fts_config = tracker_main_get_fts_config ();
	n_words = tracker_fts_config_get_max_words_to_index (fts_config);
	content = extract_content (uri, n_words);

	if (content) {
		tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, content);
		g_free (content);
	}

	g_object_unref (infile);
	g_free (filename);

	gsf_shutdown ();
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return data;
}
