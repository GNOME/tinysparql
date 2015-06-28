/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include <glib/gprintf.h>
#include <string.h>

#include "ttl_sgml.h"
#include "qname.h"

#define DEFAULT_COPYRIGHT "&copy; 2009, 2010 <ulink url=\"http://www.nokia.com/\">Nokia</ulink>"

#define SIGNALS_DOC "http://live.gnome.org/Tracker/Documentation/SignalsOnChanges"

typedef struct {
	Ontology *ontology;
	OntologyDescription *description;
	FILE *output;
} CallbackInfo;


static void
print_itemized_list (FILE *f, GList *list)
{
	GList *it;

	g_fprintf (f, "<itemizedlist>\n");
	for (it = list; it != NULL; it = it->next) {
		g_fprintf (f, "<listitem>%s</listitem>\n", (gchar *)it->data);
	}
	g_fprintf (f, "</itemizedlist>\n");
}

static void
print_people_list (FILE *f,
                   const gchar *role,
                   GList *list) 
{
        if (!list) {
                return;
        }

        g_fprintf (f, "<varlistentry>\n");
        g_fprintf (f, "  <term>%s</term>\n", role);
        g_fprintf (f, "  <listitem>\n");
        print_itemized_list (f, list);
        g_fprintf (f, "  </listitem>\n");
        g_fprintf (f, "</varlistentry>\n");
}

static void
print_link_as_varlistentry (FILE *f,
                            const gchar *term,
                            const gchar *link_text,
                            const gchar *link)
{
        g_fprintf (f, "  <varlistentry>\n");
        g_fprintf (f,"    <term>%s</term>\n", term);
	if (link) {
                g_fprintf (f, 
                           " <listitem><para><ulink url=\"%s\">%s</ulink></para></listitem>\n",
		           link, link_text);
	} else {
		g_fprintf (f, " <listitem><para>Not available</para></listitem>\n");
	}
        g_fprintf (f, "  </varlistentry>\n");
}

#if 0
static void
print_deprecated_message (FILE *f)
{
        g_fprintf (f, "<note>\n");
        g_fprintf (f, "<title>Note:</title>\n");
        g_fprintf (f, "<para>This item is deprecated</para>\n");
        g_fprintf (f, "</note>\n");
}
#endif

static void
print_sgml_header (FILE *f, OntologyDescription *desc)
{
        gchar *upper_name;

        g_fprintf (f, "<?xml version='1.0' encoding='UTF-8'?>\n");
	g_fprintf (f, "<!DOCTYPE book PUBLIC \"-//OASIS//DTD DocBook XML V4.1.2//EN\"\n"
		   "        \"http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd\" [\n");
	g_fprintf (f, "<!ENTITY %% local.common.attrib \"xmlns:xi  CDATA  #FIXED 'http://www.w3.org/2003/XInclude'\">\n");
	g_fprintf (f, "]>");

        g_fprintf (f, "<chapter id='%s-ontology'>\n", desc->localPrefix);

        upper_name = g_ascii_strup (desc->localPrefix, -1);
        g_fprintf (f, "<title>%s: %s</title>\n", desc->title, desc->description ? desc->description : "");
        g_free (upper_name);

        print_people_list (f, "Authors:", desc->authors);
        print_people_list (f, "Editors:", desc->editors);
        print_people_list (f, "Contributors:", desc->contributors);

        print_link_as_varlistentry (f, "Upstream:", "Upstream version", desc->upstream);
        print_link_as_varlistentry (f, "ChangeLog:", "Tracker changes", desc->gitlog);

        g_fprintf (f, "<varlistentry>\n");
        g_fprintf (f, "  <term>Copyright:</term>\n");
        g_fprintf (f, "  <listitem>\n");
        g_fprintf (f, "<para>%s</para>\n", (desc->copyright ? desc->copyright : DEFAULT_COPYRIGHT));
        g_fprintf (f, "  </listitem>\n");
        g_fprintf (f, "</varlistentry>\n");
}

static void
print_sgml_explanation (FILE *f, const gchar *explanation_file)
{
	gchar *raw_content;
	gsize length;

	if (explanation_file && g_file_test (explanation_file, G_FILE_TEST_EXISTS)) {
		if (!g_file_get_contents (explanation_file, &raw_content, &length, NULL)) {
			g_error ("Unable to load '%s'", explanation_file );
		}
		g_fprintf (f, "%s", raw_content);
	}
}

static void
print_sgml_footer (FILE *f)
{
	g_fprintf (f,"</chapter>\n");
}

static gchar *
name_get_prefix (Ontology    *ontology,
		 const gchar *name)
{
	const gchar *delim;

	delim = g_strrstr (name, "#");

	if (!delim)
		delim = g_strrstr (name, "/");

	if (!delim)
		return NULL;

	delim++;

	return g_strndup (name, delim - name);
}

static gchar *
name_to_shortname (Ontology    *ontology,
		   const gchar *name,
		   const gchar *separator)
{
	gchar *prefix, *short_prefix;
	const gchar *suffix;

	if (!separator)
		separator = ":";

	prefix = name_get_prefix (ontology, name);

	if (!prefix)
		return g_strdup (name);

	short_prefix = g_hash_table_lookup (ontology->prefixes, prefix);

	if (!short_prefix) {
		g_free (prefix);
		return g_strdup (name);
	}

	suffix = &name[strlen (prefix)];
	g_free (prefix);

	return g_strconcat (short_prefix, separator, suffix, NULL);
}

static void
print_ontology_class (Ontology      *ontology,
		      OntologyClass *def,
		      FILE          *f)
{
	gchar *name, *id;

	g_return_if_fail (f != NULL);

	name = name_to_shortname (ontology, def->classname, NULL);
	id = name_to_shortname (ontology, def->classname, "-");
	g_fprintf (f, "<xi:include href='%s.xml'/>\n", id);
	g_free (id);

        g_free (name);
}

void
ttl_sgml_print (OntologyDescription *description,
                Ontology *ontology,
                FILE *f,
                const gchar *explanation_file)
{
	GHashTableIter iter;
        gchar *upper_name;
	OntologyClass *def;

        upper_name = g_ascii_strup (description->localPrefix, -1);

        qname_init (description->baseUrl, description->localPrefix, NULL);
	print_sgml_header (f, description);

        /* FIXME: make desc files sgml */
	print_sgml_explanation (f, explanation_file);

        g_fprintf (f, "<section id='%s-classes'>\n", description->localPrefix);
	g_fprintf (f, "<title>%s Ontology Classes</title>\n", upper_name);
	g_hash_table_iter_init (&iter, ontology->classes);

	while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &def)) {
		print_ontology_class (ontology, def, f);
	}

        g_fprintf (f, "</section>\n");
	print_sgml_footer (f);

        g_free (upper_name);
}
