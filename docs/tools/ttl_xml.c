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

#include "ttl_xml.h"
#include "ttlresource2xml.h"

typedef struct {
	TrackerOntologyModel *model;
	TrackerOntologyDescription *description;
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
print_xml_header (FILE *f, TrackerOntologyDescription *desc)
{
	g_fprintf (f, "<?xml version='1.0' encoding='UTF-8'?>\n");
	g_fprintf (f, "<!DOCTYPE book PUBLIC \"-//OASIS//DTD DocBook XML V4.5//EN\"\n"
	           "        \"http://www.oasis-open.org/docbook/xml/4.5/docbookx.dtd\" [\n");
	g_fprintf (f, "<!ENTITY %% local.common.attrib \"xmlns:xi  CDATA  #FIXED 'http://www.w3.org/2003/XInclude'\">\n");
	g_fprintf (f, "]>");

	g_fprintf (f, "<refentry id='%s' xmlns:xi=\"http://www.w3.org/2003/XInclude\">\n", desc->localPrefix);
	g_fprintf (f, "<refmeta>\n");
	g_fprintf (f, "  <refentrytitle>%s</refentrytitle>\n", desc->title);
	g_fprintf (f, "</refmeta>\n");
	g_fprintf (f, "<refnamediv>\n");
	g_fprintf (f, "<refname>%s</refname>", desc->title);
	g_fprintf (f, "<refpurpose>%s</refpurpose>", desc->description);
	g_fprintf (f, "</refnamediv>\n");
}

static void
print_xml_footer (FILE *f, TrackerOntologyDescription *desc)
{
	g_fprintf (f, "<refsect1>\n");
	g_fprintf (f, "<title>Credits and Copyright</title>\n");
	print_people_list (f, "Authors:", desc->authors);
	print_people_list (f, "Editors:", desc->editors);
	print_people_list (f, "Contributors:", desc->contributors);

	print_link_as_varlistentry (f, "Upstream:", "Upstream version", desc->upstream);
	print_link_as_varlistentry (f, "ChangeLog:", "Tracker changes", desc->gitlog);

	if (desc->copyright) {
		g_fprintf (f, "<varlistentry>\n");
		g_fprintf (f, "  <term>Copyright:</term>\n");
		g_fprintf (f, "  <listitem>\n");
		g_fprintf (f, "<para>%s</para>\n", desc->copyright);
		g_fprintf (f, "  </listitem>\n");
		g_fprintf (f, "</varlistentry>\n");
    }

	g_fprintf (f, "</refsect1>\n");
	g_fprintf (f, "</refentry>\n");
}

/* By default we list properties under their respective class.
 *
 * Ontologies can contain properties whose class is in a different
 * ontology, and we treat these specially as 'extra properties'.
 *
 * This functions returns a hash table mapping class name to the
 * extra properties provided for that class.
 */
static GHashTable *
get_extra_properties (TrackerOntologyModel *model,
                      GList                *classes,
                      GList                *properties)
{
	GList *l, *c;
	GHashTable *extra_properties;
	GHashTableIter iter;
	gchar *classname;
	GList *properties_for_class;

	extra_properties = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (l = properties; l; l = l->next) {
		TrackerOntologyProperty *prop;
		gboolean has_domain_in_this_ontology = FALSE;

		prop = tracker_ontology_model_get_property (model, l->data);

		for (c = prop->domain; c; c = c->next) {
			TrackerOntologyDescription *desc = NULL;
			TrackerOntologyClass *klass;
			gchar *prefix;
			const gchar *sep;

			klass = tracker_ontology_model_get_class (model, c->data);
			sep = strstr (klass->shortname, ":");

			if (sep) {
				prefix = g_strndup (klass->shortname, sep - klass->shortname);
				desc = tracker_ontology_model_get_description (model, prefix);
				g_free (prefix);
			}

			has_domain_in_this_ontology = desc != NULL;
		}

		if (!has_domain_in_this_ontology) {
			for (c = prop->domain; c; c = c->next) {
				const gchar *classname;
				GList *list;

				classname = c->data;
				list = g_hash_table_lookup (extra_properties, classname);
				list = g_list_append (list, prop->propertyname);
				g_hash_table_insert (extra_properties, g_strdup (classname), list);
			}
		}
	}

	g_hash_table_iter_init (&iter, extra_properties);
	while (g_hash_table_iter_next (&iter, (gpointer *)&classname, (gpointer *)&properties_for_class)) {
		properties_for_class = g_list_sort (properties_for_class, (GCompareFunc) strcmp);
		g_hash_table_iter_replace (&iter, properties_for_class);
	}

	return extra_properties;
}

static void
print_synopsis (FILE                       *f,
                TrackerOntologyDescription *desc)
{
	g_fprintf (f, "<refsynopsisdiv>\n");
	g_fprintf (f, "<synopsis>\n");
	g_fprintf (f, "@prefix %s: &lt;%s&gt;\n", desc->localPrefix, desc->baseUrl);
	g_fprintf (f, "</synopsis>\n");
	g_fprintf (f, "</refsynopsisdiv>\n");
}

static void
print_toc_classes (FILE                 *f,
                   TrackerOntologyModel *model,
                   const char           *id,
                   GList                *classes)
{
	GList *l;

	if (!classes)
		return;

	g_fprintf (f, "<refsect1 id=\"%s.classes\">", id);
	g_fprintf (f, "<title>Classes</title>");

	for (l = classes; l; l = l->next) {
		TrackerOntologyClass *klass;
		const char *basename = NULL, *id = NULL;

		klass = tracker_ontology_model_get_class (model, l->data);
		basename = klass->basename;
		id = klass->shortname;

		if (l != classes) {
			g_fprintf (f, ", ");
		}
		g_fprintf (f, "<link linkend=\"%s\">%s</link>", id, basename);
	}

	g_fprintf (f, "</refsect1>");
}

static void
print_toc_extra_properties (FILE                 *f,
                            TrackerOntologyModel *model,
                            const char           *id,
                            GHashTable           *extra_properties)
{
	GList *props_for_class, *c, *l;
	g_autoptr(GList) classes = NULL;
	gboolean print_comma = FALSE;

	if (g_hash_table_size (extra_properties) == 0)
		return;

	g_fprintf (f, "<refsect1 id=\"%s.extra_properties\">", id);
	g_fprintf (f, "<title>Additional Properties</title>");

	classes = g_hash_table_get_keys (extra_properties);
	classes = g_list_sort (classes, (GCompareFunc)strcmp);
	for (c = classes; c; c = c->next) {
		gchar *classname;

		classname = c->data;
		props_for_class = g_hash_table_lookup (extra_properties, classname);
		for (l = props_for_class; l; l = l->next) {
			TrackerOntologyProperty *prop;
			const char *basename = NULL, *prop_id = NULL;

			prop = tracker_ontology_model_get_property (model, l->data);
			basename = prop->basename;
			prop_id = prop->shortname;

			if (print_comma) {
				g_fprintf (f, ", ");
			} else {
				print_comma = TRUE;
			}

			g_fprintf (f, "<link linkend=\"%s\">%s</link>", prop_id, basename);
		}
	}

	g_fprintf (f, "</refsect1>");
}

/* Generate docbook XML document for one ontology. */
void
ttl_xml_print (TrackerOntologyDescription *description,
               TrackerOntologyModel       *model,
	       const gchar                *prefix,
               GFile                      *output_location,
               const gchar                *description_dir)
{
	gchar *upper_name, *path, *introduction, *basename, *filename;
	g_autoptr(GList) classes = NULL, properties = NULL, extra_classes = NULL;
	g_autoptr(GHashTable) extra_properties = NULL;
	GFile *file;
	GList *l;
	FILE *f;

	filename = g_strdup_printf ("%s-ontology.xml", description->localPrefix);
	file = g_file_get_child (output_location, filename);
	g_free (filename);

	path = g_file_get_path (file);
	f = fopen (path, "w");
	g_assert (f != NULL);
	g_free (path);

	upper_name = g_ascii_strup (description->localPrefix, -1);
	classes = tracker_ontology_model_list_classes (model, prefix);
	properties = tracker_ontology_model_list_properties (model, prefix);
	extra_properties = get_extra_properties (model, classes, properties);

	print_xml_header (f, description);

	print_synopsis (f, description);
	print_toc_classes (f, model, description->localPrefix, classes);
	print_toc_extra_properties (f, model, description->localPrefix, extra_properties);

	basename = g_strdup_printf ("%s-introduction.xml", description->localPrefix);
	introduction = g_build_filename (description_dir, basename, NULL);
	g_free (basename);

	if (g_file_test (introduction, G_FILE_TEST_EXISTS)) {
		g_fprintf (f, "<xi:include href='%s'><xi:fallback/></xi:include>",
		           introduction);
	}

	if (classes != NULL) {
		g_fprintf (f, "<refsect1 id='%s-classes'>\n", description->localPrefix);
		g_fprintf (f, "<title>Class Details</title>\n");

		for (l = classes; l; l = l->next) {
			TrackerOntologyClass *klass;

			klass = tracker_ontology_model_get_class (model, l->data);
			print_ontology_class (model, klass, f);
		}

		g_fprintf (f, "</refsect1>\n");
	}

	if (g_hash_table_size (extra_properties) > 0) {
		g_fprintf (f, "<refsect1 id='%s-extra-properties'>\n", description->localPrefix);
		g_fprintf (f, "<title>Property Details</title>\n");

		extra_classes = g_hash_table_get_keys (extra_properties);
		extra_classes = g_list_sort (extra_classes, (GCompareFunc)strcmp);
		for (l = extra_classes; l; l = l->next) {
			gchar *classname;
			GList *properties_for_class;

			classname = l->data;

			properties_for_class = g_hash_table_lookup (extra_properties, classname);
			if (properties_for_class) {
				print_ontology_extra_properties (model, description->localPrefix, classname, properties_for_class, f);
			}
		}

		g_fprintf (f, "</refsect1>\n");
	}

	print_xml_footer (f, description);

	g_free (upper_name);
	g_free (introduction);
	g_object_unref (file);
	fclose (f);
}
