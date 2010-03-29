/*
 * Copyright (C) 2009, Nokia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "qname.h"
#include <glib/gstdio.h>
#include <string.h>

//static gchar *local_uri = NULL;
//static gchar *local_prefix = NULL;

typedef struct {
	gchar *namespace;
	gchar *uri;
} Namespace;

static GHashTable *class_deffile = NULL;

Namespace NAMESPACES [] = {
	{NULL, NULL}, /* Save this for the local_uri and prefix */
	{"dc", "http://purl.org/dc/elements/1.1/"},
	{"xsd", "http://www.w3.org/2001/XMLSchema#"},
	{"fts", "http://www.tracker-project.org/ontologies/fts#"},
	{"mto", "http://www.tracker-project.org/temp/mto#"},
	{"mlo", "http://www.tracker-project.org/temp/mlo#"},
	{"nao", "http://www.semanticdesktop.org/ontologies/2007/08/15/nao#"},
	{"ncal", "http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#"},
	{"nco", "http://www.semanticdesktop.org/ontologies/2007/03/22/nco#"},
	{"nfo", "http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#"},
	{"nid3", "http://www.semanticdesktop.org/ontologies/2007/05/10/nid3#"},
	{"nie", "http://www.semanticdesktop.org/ontologies/2007/01/19/nie#"},
	{"nmm", "http://www.tracker-project.org/temp/nmm#"},
	{"nmo", "http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#"},
	{"nrl", "http://www.semanticdesktop.org/ontologies/2007/08/15/nrl#"},
	{"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	{"rdfs", "http://www.w3.org/2000/01/rdf-schema#"},
	{"tracker", "http://www.tracker-project.org/ontologies/tracker#"},
	{NULL, NULL}
};


void
qname_init (const gchar *luri, const gchar *lprefix, const gchar *class_location)
{
	g_return_if_fail (luri != NULL);

	if (NAMESPACES[0].namespace || NAMESPACES[0].uri) {
		g_warning ("Reinitializing qname_module");
		g_free (NAMESPACES[0].namespace);
		g_free (NAMESPACES[0].uri);
		if (class_deffile) {
			g_hash_table_destroy (class_deffile);
		}
	}

	NAMESPACES[0].uri = g_strdup (luri);
	NAMESPACES[0].namespace = (lprefix != NULL ? g_strdup (lprefix) : g_strdup (""));

	if (class_location) {
		/* Process a file that contains: dir class pairs by line
		 */
		gint   i;
		gchar  *raw_content = NULL;
		gchar **lines;
		gsize   length;

		if (!g_file_get_contents (class_location, &raw_content, &length, NULL)) {
			g_error ("Unable to load '%s'", class_location);
		}

		class_deffile = g_hash_table_new_full (g_str_hash,
		                                       g_str_equal,
		                                       g_free,
		                                       g_free);

		lines = g_strsplit (raw_content, "\n", -1);
		for (i = 0; lines[i] != NULL; i++) {
			if (strlen (lines[i]) < 1) {
				continue;
			}

			gchar **pieces = NULL;

			pieces = g_strsplit (lines[i], " ", -1);
			g_assert (g_strv_length (pieces) == 2);
			g_hash_table_insert (class_deffile,
			                     g_strdup(pieces[1]),
			                     g_strdup(pieces[0]));
			g_strfreev (pieces);

		}
		g_strfreev (lines);
		g_free (raw_content);
	}

}

void
qname_shutdown (void)
{
	g_free (NAMESPACES[0].namespace);
	NAMESPACES[0].namespace = NULL;

	g_free (NAMESPACES[0].uri);
	NAMESPACES[0].uri = NULL;

	if (class_deffile) {
		g_hash_table_destroy (class_deffile);
		class_deffile = NULL;
	}
}

static gchar **
split_qname (const gchar *qname, GError **error)
{
	gchar **pieces;
	gint    i;

	/* Try by '#' */
	pieces = g_strsplit (qname, "#", 2);
	if (g_strv_length (pieces) == 2) {
		return pieces;
	}
	g_strfreev (pieces);
	pieces = NULL;

	/* Try by last '/' */
	for (i = strlen (qname); i >= 0; i--) {
		if (qname[i] == '/') {
			pieces = g_new0 (gchar*, 3);
			pieces[0] = g_strndup (qname, i);
			pieces[1] = g_strdup (&qname[i+1]);
			pieces[2] = NULL;
			break;
		}
	}

	if (pieces == NULL) {
		g_warning ("Unable to split '%s' in prefix and class", qname);
	}
	return pieces;
}

gchar *
qname_to_link (const gchar *qname)
{
	gchar **pieces;
	gchar *name;

	if (NAMESPACES[0].uri) {

		/* There is a local URI! */
		if (g_str_has_prefix (qname, NAMESPACES[0].uri)) {
			pieces = split_qname (qname, NULL);
			name = g_strdup_printf ("#%s", pieces[1]);
			g_strfreev (pieces);
			return name;
		}
	}

	if (class_deffile) {
		gchar *dir, *shortname;
		shortname = qname_to_shortname (qname);
		dir = g_hash_table_lookup (class_deffile, shortname);
		g_free (shortname);
		if (dir) {
			return g_strdup_printf ("../%s/index.html#%s",
			                        dir, qname_to_classname (qname));
		}
	}

	return g_strdup (qname);
}


gchar *
qname_to_shortname (const gchar *qname)
{
	gchar **pieces;
	gchar  *name = NULL;
	gint    i;

	for (i = 0; NAMESPACES[i].namespace != NULL; i++) {
		if (g_str_has_prefix (qname, NAMESPACES[i].uri)) {

			pieces = split_qname (qname, NULL);
			name = g_strdup_printf ("%s:%s",
			                        NAMESPACES[i].namespace,
			                        pieces[1]);
			g_strfreev (pieces);
			break;
		}
	}

	if (!name) {
		return g_strdup (qname);
	} else {
		return name;
	}
}

gchar *
qname_to_classname (const gchar *qname) {

	gchar  *shortname;
	gchar **pieces;
	gchar  *classname = NULL;

	shortname = qname_to_shortname (qname);
	if (g_strcmp0 (qname, shortname) == 0) {
		return shortname;
	}
	pieces = g_strsplit (shortname, ":", -1);
	g_assert (g_strv_length (pieces) == 2);

	classname = g_strdup (pieces[1]);
	g_strfreev (pieces);
	g_free (shortname);
	return classname;
}

gboolean
qname_is_basic_type (const gchar *qname)
{
	gint i;
	/* dc: or xsd: are basic types */
	for (i = 1; NAMESPACES[i].namespace != NULL && i < 3; i++) {
		if (g_str_has_prefix (qname, NAMESPACES[i].uri)) {
			return TRUE;
		}
	}
	return FALSE;
}
