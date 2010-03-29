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

static gchar *
shortname_to_id (const gchar *name)
{
        gchar *id, *p;

        id = g_strdup (name);
        p = strchr (id, ':');

        if (p) {
                *p = '-';
        }

        return id;
}

static void
print_reference (gpointer item, gpointer user_data)
{
	gchar *shortname, *id;
	FILE *f = (FILE *)user_data;

	shortname = qname_to_shortname ((gchar *) item);
        id = shortname_to_id (shortname);

	g_fprintf (f,"<link linkend='%s'>%s</link>, ", id , shortname);

	g_free (shortname);
	g_free (id);
}

static void
print_variablelist_entry (FILE        *f,
                          const gchar *param,
                          const gchar *value)
{
        g_fprintf (f, "<varlistentry>\n");
        g_fprintf (f, "<term><parameter>%s</parameter>&#160;:</term>\n", param);
        g_fprintf (f, "<listitem><simpara>%s</simpara></listitem>\n", (value) ? value : "--");
        g_fprintf (f, "</varlistentry>\n");
}

static void
print_variablelist_entry_list (FILE        *f,
                               const gchar *param,
                               GList       *list)
{
        g_fprintf (f, "<varlistentry>\n");
        g_fprintf (f, "<term><parameter>%s</parameter>&#160;:</term>\n", param);
        g_fprintf (f, "<listitem><simpara>");

        if (list) {
                g_list_foreach (list, print_reference, f);
        } else {
                g_fprintf (f, "--");
        }

        g_fprintf (f, "</simpara></listitem>\n");
        g_fprintf (f, "</varlistentry>\n");
}


static void
print_deprecated_message (FILE *f)
{
        g_fprintf (f, "<note>\n");
        g_fprintf (f, "<title>Note:</title>\n");
        g_fprintf (f, "<para>This item is deprecated</para>\n");
        g_fprintf (f, "</note>\n");
}

static void
print_sgml_header (FILE *f, OntologyDescription *desc)
{
        gchar *upper_name;

        g_fprintf (f, "<?xml version='1.0' encoding='UTF-8'?>\n");

        g_fprintf (f, "<chapter id='%s-ontology'>\n", desc->localPrefix);

        upper_name = g_ascii_strup (desc->localPrefix, -1);
        g_fprintf (f, "<title>%s Ontology</title>\n", upper_name);
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

static void
print_ontology_class (gpointer key, gpointer value, gpointer user_data)
{
	OntologyClass *def = (OntologyClass *)value;
	gchar *name, *id;
	FILE *f = (FILE *)user_data;

	g_return_if_fail (f != NULL);

	name = qname_to_shortname (def->classname);

        id = shortname_to_id (name);
        g_fprintf (f, "<refsect2 id='%s'>\n", id);

        g_free (id);

        g_fprintf (f, "<title>%s</title>\n", name);

        if (def->description) {
                g_fprintf (f, "<para>%s</para>\n", def->description);
        }

        g_fprintf (f, "<variablelist>\n");

        print_variablelist_entry_list (f, "Superclasses", def->superclasses);
        print_variablelist_entry_list (f, "Subclasses", def->subclasses);
        print_variablelist_entry_list (f, "In domain of", def->in_domain_of);
        print_variablelist_entry_list (f, "In range of", def->in_range_of);

        if (def->instances) {
                print_variablelist_entry_list (f, "Predefined instances", def->instances);
        }

        g_fprintf (f, "</variablelist>\n");

        if (def->notify || def->deprecated) {
                g_fprintf (f, "<note>\n");
                g_fprintf (f, "<title>Note:</title>\n");
                if (def->notify) {
                        g_fprintf (f, "<para>This class notifies about changes</para>\n");
                } 
                if (def->deprecated) {
                        g_fprintf (f, "<para>This class is deprecated</para>\n");
                }
                g_fprintf (f, "</note>\n");
        }

        g_fprintf (f, "</refsect2>\n\n");

        g_free (name);
}

static void
print_ontology_property (gpointer key, gpointer value, gpointer user_data)
{
	OntologyProperty *def = (OntologyProperty *) value;
	gchar *name, *id;
	FILE *f = (FILE *) user_data;

	g_return_if_fail (f != NULL);

	name = qname_to_shortname (def->propertyname);
        id = shortname_to_id (name);

        g_fprintf (f, "<refsect2 id='%s'>\n", id);
        g_free (id);

        g_fprintf (f, "<title>%s</title>\n", name);

        if (def->description) {
                g_fprintf (f, "<para>%s</para>\n", def->description);
        }


        g_fprintf (f, "<variablelist>\n");

        print_variablelist_entry_list (f, "Type", def->type);
        print_variablelist_entry_list (f, "Domain", def->domain);
        print_variablelist_entry_list (f, "Range", def->range);
        print_variablelist_entry_list (f, "Superproperties", def->superproperties);
        print_variablelist_entry_list (f, "Subproperties", def->subproperties);

        if (def->max_cardinality) {
                print_variablelist_entry (f, "Cardinality", def->max_cardinality);
        }

        if (def->fulltextIndexed) {
                print_variablelist_entry (f, "Text indexed", 
                                          "This property is indexed, so it can provide results on text search");
        }

        g_fprintf (f, "</variablelist>\n");

        if (def->deprecated) {
                g_fprintf (f, "<note>\n");
                g_fprintf (f, "<title>Note:</title>\n");
                g_fprintf (f, "<para>This property is deprecated</para>\n");
                g_fprintf (f, "</note>\n");
        }

        g_fprintf (f, "</refsect2>\n\n");
}

static void
print_fts_properties (gpointer key, gpointer value, gpointer user_data)
{
	OntologyProperty *def = (OntologyProperty *) value;
	gchar *name, *id;
	FILE *fts = (FILE *) user_data;

	g_return_if_fail (fts != NULL);
        if (!def->fulltextIndexed) {
                return;
        }

	name = qname_to_shortname (def->propertyname);
        id = shortname_to_id (name);

        g_fprintf (fts, "<tr>\n");
        g_fprintf (fts, "  <td>\n");
        print_reference (def->propertyname, fts);
        g_fprintf (fts, "  </td>\n");
        g_fprintf (fts, "  <td>%s</td>\n", (def->weight ? def->weight : "0"));
        g_fprintf (fts, "</tr>\n");

        g_free (id);

}

void
ttl_sgml_print (OntologyDescription *description,
                Ontology *ontology,
                FILE *f,
                FILE *fts,
                const gchar *explanation_file)
{
        gchar *upper_name;

        upper_name = g_ascii_strup (description->localPrefix, -1);

        qname_init (description->baseUrl, description->localPrefix, NULL);
	print_sgml_header (f, description);

        /* FIXME: make desc files sgml */
	print_sgml_explanation (f, explanation_file);

        g_fprintf (f, "<section id='%s-classes'>\n", description->localPrefix);
	g_fprintf (f, "<title>%s Ontology Classes</title>\n", upper_name);
	g_hash_table_foreach (ontology->classes, print_ontology_class, f);
        g_fprintf (f, "</section>\n");

        g_fprintf (f, "<section id='%s-properties'>\n", description->localPrefix);
	g_fprintf (f, "<title>%s Ontology Properties</title>\n", upper_name);
	g_hash_table_foreach (ontology->properties, print_ontology_property, f);
        g_fprintf (f, "</section>\n");

	print_sgml_footer (f);

        g_free (upper_name);

        if (fts) {
                g_hash_table_foreach (ontology->properties, print_fts_properties, fts);
        }
}
