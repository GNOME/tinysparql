/* Tracker - metadata extraction of Word files with libgsf
 * Copyright (C) 2006, Edward Duffy (eduffy@gmail.com)
 * Copyright (C) 2006, Laurent Aguerreche (laurent.aguerreche@free.fr)
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

#ifdef HAVE_LIBGSF

#include <string.h>
#include <glib.h>
#include <gsf/gsf.h>
#include <gsf/gsf-doc-meta-data.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-utils.h>


static void
add_gvalue_in_hash_table (GHashTable *table, const gchar *key, GValue const *val)
{
	g_return_if_fail (table && key);

	if (val) {
		gchar *s;

		s = g_strdup_value_contents (val);

		if (s) {
			if (s[0] != '\0') {
				gchar *str_val;

				/* Some fun: strings are always written "str" with double quotes around, but not numbers! */
				if (s && s[0] == '"') {
					size_t len;

					len = strlen (s);

					if (s[len - 1] == '"') {
						str_val = (len > 2 ? g_strndup (s + 1, len - 2) : NULL);
					} else {
						/* We have a string that begins with a double quote but which finishes
						   by something different... We copy the string from the beginning. */
						str_val = g_strdup (s);
					}
				} else {
					/* here, we probably have a number */
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
metadata_cb (gpointer key, gpointer value, gpointer user_data)
{
	gchar		*name;
	GsfDocProp	*property;
	GHashTable	*metadata;
	GValue const	*val;

	name = (gchar *) key;
	property = (GsfDocProp *) value;
	metadata = (GHashTable *) user_data;

	val = gsf_doc_prop_get_val (property);

	if (strcmp (name, "dc:title") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:Title", val);
	}
	else if (strcmp (name, "dc:subject") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:Subject", val);
	}
	else if (strcmp (name, "dc:creator") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:Author", val);
	}
	else if (strcmp (name, "dc:keywords") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:Keywords", val);
	}
	else if (strcmp (name, "dc:description") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:Comments", val);
	}
	else if (strcmp (name, "gsf:page-count") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:PageCount", val);
	}
	else if (strcmp (name, "gsf:word-count") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:WordCount", val);
	}
	else if (strcmp (name, "meta:creation-date") == 0) {
		add_gvalue_in_hash_table (metadata, "Doc:Created", val);
	}
	else if (strcmp (name, "meta:generator") == 0) {
		add_gvalue_in_hash_table (metadata, "File:Other", val);
	}
}

static void
doc_metadata_cb (gpointer key, gpointer value, gpointer user_data)
{
	gchar		*name;
	GsfDocProp	*property;
	GHashTable	*metadata;
	GValue const	*val;

	name = (gchar *) key;
	property = (GsfDocProp *) value;
	metadata = (GHashTable *) user_data;

	val = gsf_doc_prop_get_val (property);

	if (strcmp (name, "CreativeCommons_LicenseURL") == 0) {
		add_gvalue_in_hash_table (metadata, "File:License", val);
	}
}


void
tracker_extract_msoffice (gchar *filename, GHashTable *metadata)
{
	GsfInput	*input;
	GsfInfile	*infile;
	GsfInput	*stream;
	GsfDocMetaData	*md;

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


#else
#warning "Not building Microsoft Office metadata extractor."
#endif  /* HAVE_LIBGSF */
