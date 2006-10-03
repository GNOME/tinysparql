
#include "config.h"

#ifdef HAVE_LIBGSF

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gsf/gsf.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-doc-meta-data.h>

static void metadata_cb (gpointer key, gpointer value, gpointer user_data)
{
	gchar *name = (gchar *)key;
	GsfDocProp *property = (GsfDocProp *)value;
	GHashTable *metadata = (GHashTable *) user_data;
	GValue const *val = gsf_doc_prop_get_val (property);

	if (strcmp (name, "dc:title") == 0) {
		g_hash_table_insert (metadata, g_strdup ("Doc.Title"), g_strdup_value_contents (val));
	}
	else if (strcmp (name, "dc:subject") == 0) {
		g_hash_table_insert (metadata, g_strdup ("Doc.Subject"), g_strdup_value_contents (val));
	}
	else if (strcmp (name, "dc:creator") == 0) {
		g_hash_table_insert (metadata, g_strdup ("Doc.Author"), g_strdup_value_contents (val));
	}
	else if (strcmp (name, "dc:keywords") == 0) {
		g_hash_table_insert (metadata, g_strdup ("Doc.Keywords"), g_strdup_value_contents (val));
	}
	else if (strcmp (name, "dc:description") == 0) {
		g_hash_table_insert (metadata, g_strdup ("Doc.Comment"), g_strdup_value_contents (val));
	}
	else if (strcmp (name, "gsf:page-count") == 0) {
		g_hash_table_insert (metadata, g_strdup ("Doc.PageCount"), g_strdup_value_contents (val));
	}
	else if (strcmp (name, "gsf:word-count") == 0) {
		g_hash_table_insert (metadata, g_strdup ("Doc.WordCount"), g_strdup_value_contents (val));
	}
	else if (strcmp (name, "meta:creation-date") == 0) {
		g_hash_table_insert (metadata, g_strdup ("Doc.Created"), g_strdup_value_contents (val));
	}
	else if (strcmp (name, "meta:generator") == 0) {
		g_hash_table_insert (metadata, g_strdup ("File.Other"), g_strdup_value_contents (val));
	}
}

void
tracker_extract_msoffice (gchar *filename, GHashTable *metadata)
{
	GsfInput       *input;
	GsfInfile      *infile;
	GsfInput       *stream;
	GsfDocMetaData *md;
	GError         *error = NULL;

	if (!(input = gsf_input_stdio_new (filename, &error)))
		return;
	if (!(infile = gsf_infile_msole_new (input, &error)))
		return;
	if (!(stream = gsf_infile_child_by_name (infile, "\05SummaryInformation")))
		return;
	md = gsf_doc_meta_data_new ();
	if ((error = gsf_msole_metadata_read (stream, md)))
		return;
	gsf_doc_meta_data_foreach (md, metadata_cb, metadata);
}

#else
#warning "Not building Microsoft Office metadata extractor."
#endif  /* HAVE_LIBGSF */
