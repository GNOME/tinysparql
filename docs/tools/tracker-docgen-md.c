/*
 * Copyright (C) 2021, Red Hat Inc.
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
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 */

#include <glib/gprintf.h>
#include <gio/gio.h>

#include "tracker-docgen-md.h"
#include "tracker-utils.h"

typedef struct {
	TrackerOntologyModel *model;
	TrackerOntologyDescription *description;
	FILE *output;
} CallbackInfo;

static gchar *
link_from_shortname (const gchar *shortname)
{
	const gchar *sep;
	gchar *link, *prefix;

	sep = strstr (shortname, ":");
	g_assert (sep != NULL);

	prefix = g_strndup (shortname, sep - shortname);
	link = g_strdup_printf ("%s-ontology.html#%s", prefix, shortname);
	g_free (prefix);

	return link;
}

static void
print_predefined_instances (FILE                 *f,
                            TrackerOntologyClass *klass,
                            TrackerOntologyModel *model)
{
	const gchar *id;
	GList *l;

	if (!klass->instances)
		return;
	if (g_str_equal (klass->shortname, "rdfs:Resource") ||
	    g_str_equal (klass->shortname, "rdfs:DataType") ||
	    g_str_equal (klass->shortname, "rdfs:Class") ||
	    g_str_equal (klass->shortname, "rdf:Property") ||
	    g_str_equal (klass->shortname, "nie:InformationElement"))
		return;

	id = klass->shortname;

	g_fprintf (f, "#### <a name=\"%s.predefined-instances\"></a>Predefined instances\n\n", id);
	g_fprintf (f, "%s has the following predefined instances:\n\n", klass->shortname);

	for (l = klass->instances; l; l = l->next) {
		g_fprintf (f, "- %s\n", (gchar*) l->data);
	}

	g_fprintf (f, "\n");
}

static void
print_class_hierarchy (FILE                 *f,
                       TrackerOntologyClass *klass,
                       TrackerOntologyModel *model)
{
	const gchar *id;

	if (!klass->superclasses && !klass->subclasses)
		return;
	if (g_str_equal (klass->shortname, "nie:InformationElement"))
		return;

	id = klass->shortname;
	g_fprintf (f, "#### <a name=\"%s.hierarchy\"></a>Class hierarchy\n\n", id);
	g_fprintf (f, "<div class=\"docblock\">\n");
	g_fprintf (f,
	           "<style>"
	           "svg .node text { fill: var(--text-color); } "
	           "</style>\n");
	g_fprintf (f, "{{ %s-hierarchy.svg }}\n", id);
	g_fprintf (f, "</div>\n");
}

static gboolean
check_range_non_literals (GList                *properties,
                          TrackerOntologyModel *model)
{
	GList *l;

	for (l = properties; l; l = l->next) {
		TrackerOntologyProperty *prop;
		TrackerOntologyClass *domain;

		prop = tracker_ontology_model_get_property (model, l->data);
		domain = tracker_ontology_model_get_class (model, prop->range->data);

		if (g_str_has_prefix (domain->shortname, "xsd:") ||
		    g_str_equal (domain->shortname, "rdfs:Literal") ||
		    g_str_equal (domain->shortname, "rdf:langString"))
			continue;

		return TRUE;
	}

	return FALSE;
}

static void
print_rdf_diagram (FILE                 *f,
                   TrackerOntologyClass *klass,
                   TrackerOntologyModel *model)
{
	const gchar *id;

	if (!klass->in_range_of &&
	    !check_range_non_literals (klass->in_domain_of, model))
		return;

	id = klass->shortname;
	g_fprintf (f, "#### <a name=\"%s.hierarchy\"></a>RDF Diagram\n\n", id);
	g_fprintf (f, "<div class=\"docblock\">\n");
	g_fprintf (f,
	           "<style>"
	           "svg .node text { text-decoration: none !important; fill: var(--text-color); } "
	           "svg .edge a { text-decoration: none !important; fill: var(--text-color); } "
	           "svg .edge:hover a { text-decoration: none !important; fill: var(--primary); } "
	           "svg .edge path  { stroke: var(--text-color-muted); } "
	           "svg .edge:hover path  { stroke: var(--primary); } "
	           "svg .edge polygon { fill: var(--text-color-muted); stroke: transparent; }"
	           "svg .edge:hover polygon { fill: var(--primary); stroke: var(--primary); }"
	           "</style>\n");
	g_fprintf (f, "{{ %s-diagram.svg }}\n", id);
	g_fprintf (f, "</div>\n");
}

static void
print_flag (FILE        *f,
            const gchar *flag_property_link,
            const gchar *icon_name,
            const gchar *flag_description)
{
	g_fprintf (f, "<svg width='16' height='16' style='display: inline-block;'>"
	           "<a xlink:href='%s' xlink:title='%s'><use xlink:href='#%s'/></a></svg>",
	           flag_property_link, flag_description, icon_name);
}

static void
print_property_table (FILE                 *f,
                      TrackerOntologyModel *model,
                      const char           *id,
                      GList                *properties)
{
	GList *l, *m;
	g_autoptr(GList) properties_sorted = NULL;

	if (!properties)
		return;

	properties_sorted = g_list_sort (g_list_copy (properties), (GCompareFunc) strcmp);

	g_fprintf (f, "|Name|Type|Notes|Description|\n");
	g_fprintf (f, "| --- | --- | --- | --- |\n");

	for (l = properties_sorted; l; l = l->next) {
		TrackerOntologyProperty *prop;
		TrackerOntologyClass *range;
		const gchar *basename = NULL, *type_name = NULL, *type_class_id = NULL, *prop_id = NULL;
		gchar *link;

		prop = tracker_ontology_model_get_property (model, l->data);
		range = tracker_ontology_model_get_class (model, prop->range->data);

		prop_id = prop->shortname;
		basename = prop->basename;
		type_name = range->basename;
		type_class_id = range->shortname;

		/* Property name column */
		g_fprintf (f, "| <a name=\"%s\" /><a name=\"%s\" />%s ",
			   prop_id, basename, basename);

		/* Type column */
		link = link_from_shortname (type_class_id);
		g_fprintf (f, "| [%s](%s) ", type_name, link);
		g_free (link);

		/* Flags column */
		g_fprintf (f, "| ");

		if (prop->deprecated) {
			print_flag (f, "nrl-ontology.html#nrl:deprecated", "icon-deprecated",
			            "This property is deprecated.");
		}

		if (prop->superproperties) {
			for (m = prop->superproperties; m; m = m->next) {
				const gchar *shortname = NULL;
				g_autofree gchar *message = NULL, *link = NULL;
				TrackerOntologyProperty *superprop;

				superprop = tracker_ontology_model_get_property (model, m->data);
				shortname = superprop->shortname;
				link = link_from_shortname (shortname);

				message = g_strdup_printf ("This property extends %s", shortname);

				print_flag (f, link, "icon-superproperty", message);
			}
		}

		if (prop->max_cardinality != NULL && atoi (prop->max_cardinality) == 1) {
			/* Single valued properties are most common, so we don't display this. */
		} else {
			g_autofree gchar *message = NULL;

			if (prop->max_cardinality != NULL && atoi (prop->max_cardinality) > 0) {
				message = g_strdup_printf ("This property can have a maximum of %i values", *prop->max_cardinality);
			} else {
				message = g_strdup_printf ("This property can have multiple values.");
			}
			print_flag (f, "nrl-ontology.html#nrl:maxCardinality", "icon-multivalue", message);
		}

		if (prop->fulltextIndexed) {
			print_flag (f, "nrl-ontology.html#nrl:fulltextIndexed", "icon-fulltextindexed",
			            "This property is full-text-indexed, and can be looked up through fts:match");
		}

		/* Description column */
		g_fprintf (f, "| ");
		if (prop->description) {
			g_fprintf (f, "%s", prop->description);
		}
		g_fprintf (f, "|\n");
	}

	g_fprintf (f, "\n");
}

static void
print_ontology_class (TrackerOntologyModel *model,
                      TrackerOntologyClass *klass,
                      FILE                 *f)
{
	const gchar *name = NULL, *id = NULL;

	g_return_if_fail (f != NULL);

	name = klass->basename;
	id = klass->shortname;

	/* Anchor for external links. */
	g_fprintf (f, "## <a name=\"%s\" /><a name=\"%s\" />%s\n\n",
		   id, name, name);

	if (klass->description || klass->deprecated || klass->notify) {
		if (klass->description) {
			g_fprintf (f, "%s\n\n", klass->description);
		}

		if (klass->deprecated) {
			g_fprintf (f, "**Note:** ");
			print_flag (f, "nrl-ontology.html#nrl:deprecated", "icon-deprecated", "");
			g_fprintf (f, "This class is deprecated.");
			g_fprintf (f, "\n\n");
		}

		if (klass->notify) {
			g_fprintf (f, "**Note:** ");
			print_flag (f, "nrl-ontology.html#nrl:notify", "icon-notify", "");
			g_fprintf (f, "This class emits notifications about changes, and can "
			             "be monitored using [class@Notifier].");
			g_fprintf (f, "\n\n");
		}
	}

	print_class_hierarchy (f, klass, model);
	print_rdf_diagram (f, klass, model);

	if (klass->in_domain_of) {
		g_fprintf (f, "#### <a name=\"%s.properties\"></a>Properties\n\n", id);
		print_property_table (f, model, id, klass->in_domain_of);
	}

	print_predefined_instances (f, klass, model);

	if (klass->specification) {
		g_fprintf (f, "#### Specification\n");
		g_fprintf (f, "<%s>\n", klass->specification);
	}

}

static void
print_ontology_extra_properties (TrackerOntologyModel *model,
                                 const char           *ontology_prefix,
                                 const char           *classname,
                                 GList                *properties_for_class,
                                 FILE                 *f)
{
	TrackerOntologyClass *klass;
	const gchar *short_classname = NULL, *class_id = NULL;
	gchar *section_id = NULL;

	g_return_if_fail (f != NULL);

	klass = tracker_ontology_model_get_class (model, classname);
	short_classname = class_id = klass->shortname;
	section_id = g_strconcat (ontology_prefix, ".", class_id, NULL);

	g_fprintf (f, "# <a name=\"%s\"></a>Additional properties for %s\n", section_id, short_classname);

	g_fprintf (f, "Properties this ontology defines which can describe %s resources.\n\n",
	           short_classname);

	print_property_table (f, model, section_id, properties_for_class);

	g_free (section_id);
}

static void
print_itemized_list (FILE *f, GList *list)
{
	GList *it;

	for (it = list; it != NULL; it = it->next) {
		g_fprintf (f, "- %s\n", (gchar *)it->data);
	}
}

static void
print_people_list (FILE *f,
                   const gchar *role,
                   GList *list) 
{
        if (!list) {
                return;
        }

        g_fprintf (f, "%s\n\n", role);
        print_itemized_list (f, list);
        g_fprintf (f, "\n");
}

static void
print_link (FILE *f,
            const gchar *term,
            const gchar *link_text,
            const gchar *link)
{
        g_fprintf (f,"%s ", term);
	if (link) {
                g_fprintf (f, "[%s](%s)\n\n", link_text, link);
	} else {
		g_fprintf (f, "*Not available*\n\n");
	}
}

static void
print_md_header (FILE *f, TrackerOntologyDescription *desc)
{
	g_fprintf (f, "Title: %s\n", desc->title);
	g_fprintf (f, "Slug: %s-ontology\n\n", desc->localPrefix);
	g_fprintf (f, "%s\n\n", desc->description);
	g_fprintf (f,
	           "<style>"
	           "svg .icon { fill: var(--primary) }"
	           "table { border-collapse: collapse; } "
	           "tr { border: none !important; } "
	           "tr:hover { background: var(--box-bg); } "
	           "td:not(:last-child), th:not(:last-child) { border: none; border-right: solid 1px #888; }"
	           "</style>\n");
}

static void
print_md_footer (FILE *f, TrackerOntologyDescription *desc)
{
	g_fprintf (f, "# Credits and Copyright\n\n");
	print_people_list (f, "Authors:", desc->authors);
	print_people_list (f, "Editors:", desc->editors);
	print_people_list (f, "Contributors:", desc->contributors);

	print_link (f, "Upstream:", "Upstream version", desc->upstream);
	print_link (f, "ChangeLog:", "Tracker changes", desc->gitlog);

	if (desc->copyright) {
		g_fprintf (f, "Copyright:\n");
		g_fprintf (f, "%s\n", desc->copyright);
	}
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
			TrackerOntologyClass *klass;
			gchar *prefix;
			const gchar *sep;

			klass = tracker_ontology_model_get_class (model, c->data);
			sep = strstr (klass->shortname, ":");

			if (sep) {
				prefix = g_strndup (klass->shortname, sep - klass->shortname);
				has_domain_in_this_ontology = g_str_has_prefix (prop->shortname, prefix);
				g_free (prefix);
			}
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
	g_fprintf (f, "```turtle\n");
	g_fprintf (f, "@prefix %s: <%s>\n", desc->localPrefix, desc->baseUrl);
	g_fprintf (f, "```\n\n");
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

	g_fprintf (f, "The following classes are defined:\n\n");

	for (l = classes; l; l = l->next) {
		TrackerOntologyClass *klass;
		const char *basename = NULL, *id = NULL;

		klass = tracker_ontology_model_get_class (model, l->data);
		basename = klass->basename;
		id = klass->shortname;

		if (l != classes) {
			g_fprintf (f, ", ");
		}
		g_fprintf (f, "[%s](#%s)", basename, id);
	}

	g_fprintf (f, "\n\n");
}

static void
print_stock_icons (FILE *f)
{
	/* In order to keep SVGs themeable, but avoid having
	 * repeated icons all over the place, inline them once hidden, so
	 * they can be put in place through <use xlink:href=.../>
	 */
	g_fprintf (f,
	           "<div style='display:none'>\n"
	           "{{ images/icon-deprecated.svg }}\n"
	           "{{ images/icon-superproperty.svg }}\n"
	           "{{ images/icon-multivalue.svg }}\n"
	           "{{ images/icon-fulltextindexed.svg }}\n"
	           "{{ images/icon-notify.svg }}\n"
	           "</div>\n");
}

void
generate_devhelp_keywords (TrackerOntologyModel       *model,
			   TrackerOntologyDescription *description,
			   GFile                      *output_location,
			   GList                      *classes,
			   GList                      *properties)
{
	gchar *path, *filename;
	GFile *file;
	GList *l;
	FILE *f;

	filename = g_strdup_printf ("%s-ontology.keywords", description->localPrefix);
	file = g_file_get_child (output_location, filename);
	g_free (filename);

	path = g_file_get_path (file);
	f = fopen (path, "w");
	g_assert (f != NULL);
	g_free (path);

	for (l = classes; l != NULL; l = l->next) {
		TrackerOntologyClass *klass;

		klass = tracker_ontology_model_get_class (model, l->data);
		g_fprintf (f, "  <keyword type=\"rdf-class\" name=\"%s\" link=\"%s-ontology.html#%s\"/>\n",
			   klass->shortname,
			   description->localPrefix,
			   klass->shortname);
	}

	for (l = properties; l != NULL; l = l->next) {
		TrackerOntologyProperty *prop;

		prop = tracker_ontology_model_get_property (model, l->data);
		g_fprintf (f, "  <keyword type=\"rdf-property\" name=\"%s\" link=\"%s-ontology.html#%s\"/>\n",
			   prop->shortname,
			   description->localPrefix,
			   prop->shortname);
	}

	g_object_unref (file);
	fclose (f);
}

void
generate_search_terms (TrackerOntologyModel       *model,
		       TrackerOntologyDescription *description,
		       GFile                      *output_location,
		       GList                      *classes,
		       GList                      *properties)
{
	gchar *path, *filename;
	GFile *file;
	GList *l;
	FILE *f;
	gboolean first = TRUE;

	filename = g_strdup_printf ("%s-ontology.index.json", description->localPrefix);
	file = g_file_get_child (output_location, filename);
	g_free (filename);

	path = g_file_get_path (file);
	f = fopen (path, "w");
	g_assert (f != NULL);
	g_free (path);

	g_fprintf (f, "{\"symbols\":[");

	for (l = classes; l != NULL; l = l->next) {
		TrackerOntologyClass *klass;
		g_autofree gchar *desc = NULL;

		if (!first)
			g_fprintf (f, ",");

		klass = tracker_ontology_model_get_class (model, l->data);
		if (klass->description)
			desc = g_strescape (klass->description, NULL);

		g_fprintf (f, "{\"type\": \"content\", \"name\":\"%s\",\"href\":\"%s-ontology.html#%s\",\"summary\":\"%s\"}",
			   klass->shortname,
			   description->localPrefix,
			   klass->shortname,
		           desc ? desc : "");
		first = FALSE;
	}

	for (l = properties; l != NULL; l = l->next) {
		TrackerOntologyProperty *prop;
		g_autofree gchar *desc = NULL;

		if (!first)
			g_fprintf (f, ",");

		prop = tracker_ontology_model_get_property (model, l->data);
		if (prop->description)
			desc = g_strescape (prop->description, NULL);

		g_fprintf (f, "{\"type\": \"content\", \"name\":\"%s\",\"href\":\"%s-ontology.html#%s\",\"summary\":\"%s\"}",
			   prop->shortname,
			   description->localPrefix,
			   prop->shortname,
		           desc ? desc : "");
		first = FALSE;
	}

	g_fprintf (f, "]}");

	g_object_unref (file);
	fclose (f);
}

/* Generate markdown document for one ontology. */
void
ttl_md_print (TrackerOntologyDescription *description,
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

	filename = g_strdup_printf ("%s-ontology.md.in", description->localPrefix);
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

	print_md_header (f, description);

	print_synopsis (f, description);
	print_toc_classes (f, model, description->localPrefix, classes);
	print_stock_icons (f);

	basename = g_strdup_printf ("%s-introduction.md", description->localPrefix);
	introduction = g_build_filename (description_dir, basename, NULL);
	g_free (basename);

	if (g_file_test (introduction, G_FILE_TEST_EXISTS)) {
		GMappedFile *mapped_file;

		mapped_file = g_mapped_file_new (introduction, FALSE, NULL);
		fwrite (g_mapped_file_get_contents (mapped_file),
		        g_mapped_file_get_length (mapped_file),
		        1, f);
		g_mapped_file_unref (mapped_file);
	}

	if (classes != NULL) {
		g_fprintf (f, "# <a name=\"%s-classes\"></a>Classes\n\n", description->localPrefix);

		for (l = classes; l; l = l->next) {
			TrackerOntologyClass *klass;

			klass = tracker_ontology_model_get_class (model, l->data);
			print_ontology_class (model, klass, f);
		}
	}

	if (g_hash_table_size (extra_properties) > 0) {
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
	}

	print_md_footer (f, description);

	generate_devhelp_keywords (model, description, output_location, classes, properties);
	generate_search_terms (model, description, output_location, classes, properties);

	g_free (upper_name);
	g_free (introduction);
	g_object_unref (file);
	fclose (f);
}
