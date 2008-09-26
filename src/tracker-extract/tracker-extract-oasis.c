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

#include "tracker-extract.h"

typedef enum {
	READ_TITLE,
	READ_SUBJECT,
	READ_AUTHOR,
	READ_KEYWORDS,
	READ_COMMENTS,
	READ_STATS,
	READ_CREATED,
	READ_FILE_OTHER
} tag_type;

typedef struct {
	GHashTable *metadata;
	tag_type current;
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
				   GHashTable		*metadata);

static TrackerExtractorData data[] = {
	{ "application/vnd.oasis.opendocument.*", extract_oasis },
	{ NULL, NULL }
};

static void
extract_oasis (const gchar *filename,
	       GHashTable  *metadata)
{
	gchar	      *argv[5];
	gchar	      *xml;
	ODTParseInfo   info = {
		metadata,
		-1
	};

	argv[0] = g_strdup ("unzip");
	argv[1] = g_strdup ("-p");
	argv[2] = g_strdup (filename);
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

		context = g_markup_parse_context_new (&parser, 0, &info, NULL);
		g_markup_parse_context_parse (context, xml, -1, NULL);

		g_markup_parse_context_free (context);
		g_free (xml);
	}

	g_free (argv[3]);
	g_free (argv[2]);
	g_free (argv[1]);
	g_free (argv[0]);
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
		GHashTable *metadata;
		const gchar **a, **v;

		metadata = data->metadata;

		for (a = attribute_names, v = attribute_values; *a; ++a, ++v) {
			if (strcmp (*a, "meta:word-count") == 0) {
				g_hash_table_insert (metadata,
						     g_strdup ("Doc:WordCount"),
						     g_strdup (*v));
			}
			else if (strcmp (*a, "meta:page-count") == 0) {
				g_hash_table_insert (metadata,
						     g_strdup ("Doc:PageCount"),
						     g_strdup (*v));
			}
		}

		data->current = READ_STATS;
	}
	else if (strcmp (element_name, "meta:creation-date") == 0) {
		data->current = READ_CREATED;
	}
	else if (strcmp (element_name, "meta:generator") == 0) {
		data->current = READ_FILE_OTHER;
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
	GHashTable   *metadata;

	data = user_data;
	metadata = data->metadata;

	switch (data->current) {
	case READ_TITLE:
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Title"),
				     g_strdup (text));
		break;
	case READ_SUBJECT:
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Subject"),
				     g_strdup (text));
		break;
	case READ_AUTHOR:
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Author"),
				     g_strdup (text));
		break;
	case READ_KEYWORDS: {
		gchar *keywords;

		if ((keywords = g_hash_table_lookup (metadata, "Doc:Keywords"))) {
			g_hash_table_replace (metadata,
					      g_strdup ("Doc:Keywords"),
					      g_strconcat (keywords, ",", text, NULL));
		} else {
			g_hash_table_insert (metadata,
					     g_strdup ("Doc:Keywords"),
					     g_strdup (text));
		}
	}
		break;
	case READ_COMMENTS:
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Comments"),
				     g_strdup (text));
		break;
	case READ_CREATED:
		g_hash_table_insert (metadata,
				     g_strdup ("Doc:Created"),
				     g_strdup (text));
		break;
	case READ_FILE_OTHER:
		g_hash_table_insert (metadata,
				     g_strdup ("File:Other"),
				     g_strdup (text));
		break;

	default:
		break;
	}
}

TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}
