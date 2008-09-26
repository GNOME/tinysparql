/* Tracker Xmp - Xmp helper functions
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <locale.h>
#include <string.h>
#include <glib.h>

#include "config.h"
#include "tracker-xmp.h"

#ifdef HAVE_EXEMPI

#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>

static void tracker_xmp_iter	    (XmpPtr	     xmp,
				     XmpIteratorPtr  iter,
				     GHashTable     *metadata,
				     gboolean	     append);

static void tracker_xmp_iter_simple (GHashTable  *metadata,
				     const gchar *schema,
				     const gchar *path,
				     const gchar *value,
				     gboolean	  append);


static void
tracker_append_string_to_hash_table (GHashTable *metadata, const gchar *key, const gchar *value, gboolean append)
{
	gchar *new_value;

	if (append) {
		gchar *orig;
		if (g_hash_table_lookup_extended (metadata, key, NULL, (gpointer)&orig )) {
			new_value = g_strconcat (orig, " ", value, NULL);
		} else {
			new_value = g_strdup (value);
		}
	} else {
		new_value = g_strdup (value);
	}

	g_hash_table_insert (metadata, g_strdup (key), new_value);
}


/* We have an array, now recursively iterate over it's children.  Set 'append' to true so that all values of the array are added
   under one entry. */
static void
tracker_xmp_iter_array (XmpPtr xmp, GHashTable *metadata, const gchar *schema, const gchar *path)
{
		XmpIteratorPtr iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN);
		tracker_xmp_iter (xmp, iter, metadata, TRUE);
		xmp_iterator_free (iter);
}


/* We have an array, now recursively iterate over it's children.  Set 'append' to false so that only one item is used. */
static void
tracker_xmp_iter_alt_text (XmpPtr xmp, GHashTable *metadata, const gchar *schema, const gchar *path)
{
		XmpIteratorPtr iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN);
		tracker_xmp_iter (xmp, iter, metadata, FALSE);
		xmp_iterator_free (iter);
}


/* We have a simple element, but need to iterate over the qualifiers */
static void
tracker_xmp_iter_simple_qual (XmpPtr xmp, GHashTable *metadata,
			      const gchar *schema, const gchar *path, const gchar *value, gboolean append)
{
	XmpIteratorPtr iter = xmp_iterator_new (xmp, schema, path, XMP_ITER_JUSTCHILDREN | XMP_ITER_JUSTLEAFNAME);

	XmpStringPtr the_path = xmp_string_new ();
	XmpStringPtr the_prop = xmp_string_new ();

	gchar *locale = setlocale (LC_ALL, NULL);
	gchar *sep = strchr (locale,'.');
	if (sep) {
		locale[sep - locale] = '\0';
	}
	sep = strchr (locale, '_');
	if (sep) {
		locale[sep - locale] = '-';
	}

	gboolean ignore_element = FALSE;

	while (xmp_iterator_next (iter, NULL, the_path, the_prop, NULL)) {
		const gchar *qual_path = xmp_string_cstr (the_path);
		const gchar *qual_value = xmp_string_cstr (the_prop);

		if (strcmp (qual_path, "xml:lang") == 0) {
			/* is this a language we should ignore? */
			if (strcmp (qual_value, "x-default") != 0 && strcmp (qual_value, "x-repair") != 0 && strcmp (qual_value, locale) != 0) {
				ignore_element = TRUE;
				break;
			}
		}
	}

	if (!ignore_element) {
		tracker_xmp_iter_simple (metadata, schema, path, value, append);
	}

	xmp_string_free (the_prop);
	xmp_string_free (the_path);

	xmp_iterator_free (iter);
}


/* We have a simple element. Add any metadata we know about to the hash table  */
static void
tracker_xmp_iter_simple (GHashTable *metadata,
			 const gchar *schema, const gchar *path, const gchar *value, gboolean append)
{
	gchar *name = g_strdup (strchr (path, ':') + 1);
	const gchar *index = strrchr (name, '[');
	if (index) {
		name[index-name] = '\0';
	}

	/* Dublin Core */
	if (strcmp (schema, NS_DC) == 0) {
		if (strcmp (name, "title") == 0) {
			tracker_append_string_to_hash_table (metadata, "Image:Title", value, append);
		}
		else if (strcmp (name, "rights") == 0) {
			tracker_append_string_to_hash_table (metadata, "File:Copyright", value, append);
		}
		else if (strcmp (name, "creator") == 0) {
			tracker_append_string_to_hash_table (metadata, "Image:Creator", value, append);
		}
		else if (strcmp (name, "description") == 0) {
			tracker_append_string_to_hash_table (metadata, "Image:Description", value, append);
		}
		else if (strcmp (name, "date") == 0) {
			tracker_append_string_to_hash_table (metadata, "Image:Date", value, append);
		}
		else if (strcmp (name, "keywords") == 0) {
			tracker_append_string_to_hash_table (metadata, "Image:Keywords", value, append);
		}
		else if (strcmp (name, "subject") == 0) {
			tracker_append_string_to_hash_table (metadata, "DC:Subject", value, append);
		}
		else if (strcmp (name, "publisher") == 0) {
			tracker_append_string_to_hash_table (metadata, "DC:Publisher", value, append);
		}
		else if (strcmp (name, "contributor") == 0) {
			tracker_append_string_to_hash_table (metadata, "DC:Contributor", value, append);
		}
		else if (strcmp (name, "type") == 0) {
			tracker_append_string_to_hash_table (metadata, "DC:Type", value, append);
		}
		else if (strcmp (name, "format") == 0) {
			tracker_append_string_to_hash_table (metadata, "DC:Format", value, append);
		}
		else if (strcmp (name, "identifier") == 0) {
			tracker_append_string_to_hash_table (metadata, "DC:Identifier", value, append);
		}
		else if (strcmp (name, "source") == 0) {
			tracker_append_string_to_hash_table (metadata, "DC:Source", value, append);
		}
		else if (strcmp (name, "language") == 0) {
			tracker_append_string_to_hash_table (metadata, "DC:Language", value, append);
		}
		else if (strcmp (name, "relation") == 0) {
			tracker_append_string_to_hash_table (metadata, "DC:Relation", value, append);
		}
		else if (strcmp (name, "coverage") == 0) {
			tracker_append_string_to_hash_table (metadata, "DC:Coverage", value, append);
		}

	}
	/* Creative Commons */
	else if (strcmp (schema, NS_CC) == 0) {
		if (strcmp (name, "license") == 0) {
			tracker_append_string_to_hash_table (metadata, "File:License", value, append);
		}
	}
	/* Exif basic scheme */
	else if (strcmp (schema, NS_EXIF) == 0) {
		if (strcmp (name, "title") == 0) {
			tracker_append_string_to_hash_table (metadata, "Image:Title", value, append);
		}
	}

	g_free (name);
}


/* Iterate over the XMP, dispatching to the appropriate element type (simple, simple w/qualifiers, or an array) handler */
void
tracker_xmp_iter (XmpPtr xmp, XmpIteratorPtr iter, GHashTable *metadata, gboolean append)
{
	XmpStringPtr the_schema = xmp_string_new ();
	XmpStringPtr the_path = xmp_string_new ();
	XmpStringPtr the_prop = xmp_string_new ();

	uint32_t opt;
	while (xmp_iterator_next (iter, the_schema, the_path, the_prop, &opt)) {
		const gchar *schema = xmp_string_cstr (the_schema);
		const gchar *path = xmp_string_cstr (the_path);
		const gchar *value = xmp_string_cstr (the_prop);

		if (XMP_IS_PROP_SIMPLE (opt)) {
			if (strcmp (path,"") != 0) {
				if (XMP_HAS_PROP_QUALIFIERS (opt)) {
					tracker_xmp_iter_simple_qual (xmp, metadata, schema, path, value, append);
				} else {
					tracker_xmp_iter_simple (metadata, schema, path, value, append);
				}
			}
		}
		else if (XMP_IS_PROP_ARRAY (opt)) {
			if (XMP_IS_ARRAY_ALTTEXT (opt)) {
				tracker_xmp_iter_alt_text (xmp, metadata, schema, path);
				xmp_iterator_skip (iter, XMP_ITER_SKIPSUBTREE);
			} else {
				tracker_xmp_iter_array (xmp, metadata, schema, path);
				xmp_iterator_skip (iter, XMP_ITER_SKIPSUBTREE);
			}
		}
	}

	xmp_string_free (the_prop);
	xmp_string_free (the_path);
	xmp_string_free (the_schema);
}

#endif /* HAVE_EXEMPI */


void
tracker_read_xmp (const gchar *buffer, size_t len, GHashTable *metadata)
{
#ifdef HAVE_EXEMPI
	xmp_init ();

	XmpPtr xmp = xmp_new_empty ();
	xmp_parse (xmp, buffer, len);
	if (xmp != NULL) {
		XmpIteratorPtr iter = xmp_iterator_new (xmp, NULL, NULL, XMP_ITER_PROPERTIES);
		tracker_xmp_iter (xmp, iter, metadata, FALSE);
		xmp_iterator_free (iter);

		xmp_free (xmp);
	}

	xmp_terminate ();
#endif
}
