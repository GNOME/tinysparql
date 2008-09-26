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

#include "tracker-extract.h"

static void extract_msoffice (const gchar *filename,
			      GHashTable  *metadata);

static TrackerExtractorData data[] = {
	{ "application/msword",	  extract_msoffice },
	{ "application/vnd.ms-*", extract_msoffice },
	{ NULL, NULL }
};

static void
add_gvalue_in_hash_table (GHashTable   *table,
			  const gchar  *key,
			  GValue const *val)
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
					g_hash_table_insert (table, g_strdup (key), str_val);
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
	gchar	     *name;
	GsfDocProp   *property;
	GHashTable   *metadata;
	GValue const *val;

	name = key;
	property = value;
	metadata = user_data;
	val = gsf_doc_prop_get_val (property);

	if (strcmp (name, "dc:title") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:Title", val);
	} else if (strcmp (name, "dc:subject") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:Subject", val);
	} else if (strcmp (name, "dc:creator") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:Author", val);
	} else if (strcmp (name, "dc:keywords") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:Keywords", val);
	} else if (strcmp (name, "dc:description") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:Comments", val);
	} else if (strcmp (name, "gsf:page-count") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:PageCount", val);
	} else if (strcmp (name, "gsf:word-count") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:WordCount", val);
	} else if (strcmp (name, "meta:creation-date") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:Created", val);
	} else if (strcmp (name, "meta:generator") == 0) {
		add_gvalue_in_hash_table (metadata, "File:Other", val);
	}
}

static void
doc_metadata_cb (gpointer key,
		 gpointer value,
		 gpointer user_data)
{
	gchar	     *name;
	GsfDocProp   *property;
	GHashTable   *metadata;
	GValue const *val;

	name = key;
	property = value;
	metadata = user_data;
	val = gsf_doc_prop_get_val (property);

	if (strcmp (name, "CreativeCommons_LicenseURL") == 0) {
		add_gvalue_in_hash_table (metadata, "File:License", val);
	}
}

static void
extract_msoffice (const gchar *filename,
		  GHashTable  *metadata)
{
	GsfInput  *input;
	GsfInfile *infile;
	GsfInput  *stream;

	gsf_init ();

	input = gsf_input_stdio_new (filename, NULL);

	if (!input) {
		gsf_shutdown ();
		return;
	}

	infile = gsf_infile_msole_new (input, NULL);
	g_object_unref (G_OBJECT (input));

	if (!infile) {
		gsf_shutdown ();
		return;
	}

	stream = gsf_infile_child_by_name (infile, "\05SummaryInformation");
	if (stream) {
		GsfDocMetaData *md;

		md = gsf_doc_meta_data_new ();

		if (gsf_msole_metadata_read (stream, md)) {
			gsf_shutdown ();
			return;
		}

		gsf_doc_meta_data_foreach (md, metadata_cb, metadata);

		g_object_unref (G_OBJECT (md));
		g_object_unref (G_OBJECT (stream));
	}

	stream = gsf_infile_child_by_name (infile, "\05DocumentSummaryInformation");
	if (stream) {
		GsfDocMetaData *md;

		md = gsf_doc_meta_data_new ();

		if (gsf_msole_metadata_read (stream, md)) {
			gsf_shutdown ();
			return;
		}

		gsf_doc_meta_data_foreach (md, doc_metadata_cb, metadata);

		g_object_unref (G_OBJECT (md));
		g_object_unref (G_OBJECT (stream));
	}

	g_object_unref (G_OBJECT (infile));

	gsf_shutdown ();
}

TrackerExtractorData *
tracker_get_extractor_data (void)
{
	return data;
}
