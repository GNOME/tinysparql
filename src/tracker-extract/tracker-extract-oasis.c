/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#include <stdio.h>
#include <string.h>

#include <glib.h>

#include <libtracker-common/tracker-os-dependant.h>
#include <libtracker-common/tracker-statement-list.h>
#include <libtracker-common/tracker-ontology.h>

#include "tracker-main.h"

#define NIE_PREFIX TRACKER_NIE_PREFIX
#define NFO_PREFIX TRACKER_NFO_PREFIX
#define NCO_PREFIX TRACKER_NCO_PREFIX

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_TYPE RDF_PREFIX "type"

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
	GPtrArray *metadata;
	tag_type current;
	const gchar *uri;
} ODTParseInfo;

static void start_element_handler (GMarkupParseContext	*context,
				   const gchar		*element_name,
				   const gchar	       **attribute_names,
				   const gchar	       **attribute_values,
				   gpointer		 user_data,
				   GError	       **error);
static void end_element_handler   (GMarkupParseContext	*context,
				   const gchar		*element_name,
				   gpointer		 user_data,
				   GError	       **error);
static void text_handler	  (GMarkupParseContext	*context,
				   const gchar		*text,
				   gsize		 text_len,
				   gpointer		 user_data,
				   GError	       **error);
static void extract_oasis	  (const gchar		*filename,
				   GPtrArray		*metadata);

static TrackerExtractData extract_data[] = {
	{ "application/vnd.oasis.opendocument.*", extract_oasis },
	{ NULL, NULL }
};

static void
extract_oasis (const gchar *uri,
	       GPtrArray   *metadata)
{
	gchar	      *argv[5];
	gchar	      *xml;
	gchar *filename = g_filename_from_uri (uri, NULL, NULL);
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

	if (tracker_spawn (argv, 10, &xml, NULL)) {
		GMarkupParseContext *context;
		GMarkupParser	     parser = {
			start_element_handler,
			end_element_handler,
			text_handler,
			NULL,
			NULL
		};

		tracker_statement_list_insert (metadata, uri, 
		                          RDF_TYPE, 
		                          NFO_PREFIX "Document");

		context = g_markup_parse_context_new (&parser, 0, &info, NULL);
		g_markup_parse_context_parse (context, xml, -1, NULL);

		g_markup_parse_context_free (context);
		g_free (xml);
	}

	g_free (argv[3]);
	g_free (argv[1]);
	g_free (argv[0]);

	g_free (filename);
}

void
start_element_handler (GMarkupParseContext  *context,
		       const gchar	    *element_name,
		       const gchar	   **attribute_names,
		       const gchar	   **attribute_values,
		       gpointer		     user_data,
		       GError		   **error)
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
		GPtrArray *metadata;
		const gchar *uri;
		const gchar **a, **v;

		metadata = data->metadata;
		uri = data->uri;

		for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
			if (strcmp (*a, "meta:word-count") == 0) {
				tracker_statement_list_insert (metadata, uri,
							  NFO_PREFIX "wordCount",
							  *v);
			}
			else if (strcmp (*a, "meta:page-count") == 0) {
				tracker_statement_list_insert (metadata, uri,
							  NFO_PREFIX "pageCount",
							  *v);
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
		     const gchar	  *element_name,
		     gpointer		   user_data,
		     GError		 **error)
{
	((ODTParseInfo*) user_data)->current = -1;
}

void
text_handler (GMarkupParseContext  *context,
	      const gchar	   *text,
	      gsize		    text_len,
	      gpointer		    user_data,
	      GError		  **error)
{
	ODTParseInfo *data;
	GPtrArray    *metadata;
	const gchar        *uri;

	data = user_data;
	metadata = data->metadata;
	uri = data->uri;

	switch (data->current) {
	case READ_TITLE:
		tracker_statement_list_insert (metadata, uri,
					  NIE_PREFIX "title",
					  text);
		break;
	case READ_SUBJECT:
		tracker_statement_list_insert (metadata, uri,
					  NIE_PREFIX "subject",
					  text);
		break;
	case READ_AUTHOR:
		tracker_statement_list_insert (metadata, ":", RDF_TYPE, NCO_PREFIX "Contact");
		tracker_statement_list_insert (metadata, ":", NCO_PREFIX "fullname", text);
		tracker_statement_list_insert (metadata, uri, NCO_PREFIX "creator", ":");
		break;
	case READ_KEYWORDS: {
		gchar *keywords = g_strdup (text);
		char *lasts, *keyw;
		for (keyw = strtok_r (keywords, ",; ", &lasts); keyw; 
		     keyw = strtok_r (NULL, ",; ", &lasts)) {
			tracker_statement_list_insert (metadata,
					  uri, NIE_PREFIX "keyword",
					  (const gchar*) keyw);
		}
		g_free (keywords);
	}
		break;
	case READ_COMMENTS:
		tracker_statement_list_insert (metadata, uri,
					  NIE_PREFIX "comment",
					  text);
		break;
	case READ_CREATED:
		tracker_statement_list_insert (metadata, uri,
					  NIE_PREFIX "contentCreated",
					  text);
		break;
	case READ_GENERATOR:
		tracker_statement_list_insert (metadata, uri,
					  NIE_PREFIX "generator",
					  text);
		break;

	default:
	case READ_STATS:
		break;
	}
}

TrackerExtractData *
tracker_get_extract_data (void)
{
	return extract_data;
}
